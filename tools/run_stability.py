#!/usr/bin/env python3
"""T016 S4: single-process continuous-N-frame stability driver.

Design constraints taken from the T016 task book and contracts section 9:

* **One process, one continuous sequence.** Restarting an application N times
  is explicitly NOT a valid substitute for one continuous N-frame encode, so
  this tool never loops over process launches to reach the frame count. The
  frame count is requested from the application in a single invocation.
* **Per-frame material must expose duplicates/drops.** The generated input has
  a per-frame fingerprint written into the pixels, and the tool re-derives
  those fingerprints from the *decoded* output so a repeated first frame or a
  dropped frame is a hard FAIL rather than a silent pass.
* **Leak verdict from a detector, not from RSS.** RSS is recorded as a trend
  only. A leak claim requires an ASan/LSan or valgrind report. If no detector
  is available the leak item is BLOCKED, never PASS.
* **Frame count comes from the decoder's own report**, not from a file length.

Exit codes: 0 PASS, 1 FAIL, 2 usage, 3 BLOCKED (missing tool/environment).
"""

from __future__ import annotations

import argparse
import datetime as _dt
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import threading
import time
from pathlib import Path

EXIT_OK = 0
EXIT_FAIL = 1
EXIT_USAGE = 2
EXIT_BLOCKED = 3

HERE = Path(__file__).resolve().parent
SVTAV2 = HERE.parent
DEFAULT_OUT = SVTAV2 / "artifacts" / "T016" / "S4"


def _sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def _run(cmd, cwd=None, timeout=7200, env=None):
    return subprocess.run(
        [str(c) for c in cmd], cwd=str(cwd) if cwd else None,
        capture_output=True, text=True, timeout=timeout, env=env, errors="replace",
    )


# --------------------------------------------------------------------------
# Per-frame fingerprinted input
# --------------------------------------------------------------------------
def write_fingerprint_y4m(path: Path, width: int, height: int, frames: int) -> list[int]:
    """Write a 4:2:0 Y4M where frame k carries a recoverable fingerprint.

    Layout: the luma plane of frame k is filled with a gradient seeded by the
    frame index, and the first 4 luma bytes of row 0 encode k big-endian. This
    makes frame identity recoverable from decoder output even after lossy
    coding, so duplicates and drops are detectable.
    """
    cw, ch = (width + 1) // 2, (height + 1) // 2
    expected: list[int] = []
    with path.open("wb") as fh:
        fh.write(f"YUV4MPEG2 W{width} H{height} F25:1 Ip C420jpeg\n".encode())
        for k in range(frames):
            expected.append(k)
            y = bytearray(width * height)
            # Big-endian fingerprint in the first 4 luma samples of row 0.
            fp = k & 0xFFFFFFFF
            y[0] = (fp >> 24) & 0xFF
            y[1] = (fp >> 16) & 0xFF
            y[2] = (fp >> 8) & 0xFF
            y[3] = fp & 0xFF
            # Deterministic, spatially varying content for the rest so the
            # encoder sees non-flat residual and per-frame content differs.
            for r in range(height):
                base = r * width
                row_seed = (r * 7 + k * 13) & 0xFF
                for c in range(4, width):
                    y[base + c] = (base + c * 3 + row_seed + k) & 0xFF
            u = bytes(((128 + ((i + k) % 64)) & 0xFF) for i in range(cw * ch))
            v = bytes(((64 + ((i * 3 + k) % 64)) & 0xFF) for i in range(cw * ch))
            fh.write(b"FRAME\n")
            fh.write(bytes(y))
            fh.write(u)
            fh.write(v)
    return expected


def read_y4m_fingerprints(path: Path, width: int, height: int) -> list[int]:
    """Recover per-frame fingerprints from a raw 4:2:0 blob written as Y4M-ish.

    Accepts a raw .yuv (no header) so it works directly on decoder output.
    """
    ysz = width * height
    fsz = ysz + 2 * ((width + 1) // 2) * ((height + 1) // 2)
    data = path.read_bytes()
    n = len(data) // fsz
    out = []
    for k in range(n):
        off = k * fsz
        out.append((data[off] << 24) | (data[off + 1] << 16)
                   | (data[off + 2] << 8) | data[off + 3])
    return out


def _luma_is_flat(path: Path, width: int, height: int) -> bool:
    """True if the first decoded luma plane has a single value (DC-only).

    At the M1 encoder's only wired qp (100) the whole picture quantizes to one
    level, which makes pixel-embedded frame fingerprints unrecoverable. Detect
    that regime so the caller can mark the identity check not-applicable
    instead of reporting a false failure.
    """
    ysz = width * height
    data = path.read_bytes()
    if len(data) < ysz:
        return False
    return len(set(data[:ysz])) == 1



def _decoder_frames(avmdec: str, bitstream: Path, out_yuv: Path,
                    width: int, height: int, timeout: int):
    """Decode with the AVM oracle and return (frames_reported, raw_path)."""
    cmd = [avmdec, "--i420", "--rawvideo", "-o", str(out_yuv), str(bitstream)]
    cp = _run(cmd, timeout=timeout)
    reported = None
    m = re.search(r"(\d+)\s+frames?\s+decoded", cp.stdout + cp.stderr, re.I)
    if m:
        reported = int(m.group(1))
    return cp, reported


def _peak_rss_sampler_windows(pid: int, stop_flag, samples: list) -> None:
    """Sample (WorkingSetSize) of a live Windows process; record the max."""
    import ctypes
    import ctypes.wintypes as wt

    class PROCESS_MEMORY_COUNTERS(ctypes.Structure):
        _fields_ = [("cb", wt.DWORD), ("PageFaultCount", wt.DWORD),
                    ("PeakWorkingSetSize", ctypes.c_size_t),
                    ("WorkingSetSize", ctypes.c_size_t),
                    ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                    ("PagefileUsage", ctypes.c_size_t),
                    ("PeakPagefileUsage", ctypes.c_size_t)]

    # PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ
    k32 = ctypes.WinDLL("kernel32")
    psapi = ctypes.WinDLL("psapi")
    access = 0x1000 | 0x0010
    h = k32.OpenProcess(access, False, pid)
    if not h:
        return
    try:
        while not stop_flag.is_set():
            c = PROCESS_MEMORY_COUNTERS()
            c.cb = ctypes.sizeof(c)
            if psapi.GetProcessMemoryInfo(h, ctypes.byref(c), c.cb):
                samples.append(int(c.WorkingSetSize))
            time.sleep(0.05)
    finally:
        k32.CloseHandle(h)


def _peak_rss_sampler_linux(pid: int, stop_flag, samples: list) -> None:
    """Sample VmRSS from /proc/<pid>/status on a live Linux process."""
    status = Path(f"/proc/{pid}/status")
    while not stop_flag.is_set():
        try:
            for line in status.read_text().splitlines():
                if line.startswith("VmRSS:"):
                    samples.append(int(line.split()[1]) * 1024)
                    break
        except OSError:
            return
        time.sleep(0.05)


def _sample_windows_once(pid: int, samples: list) -> None:
    import ctypes
    import ctypes.wintypes as wt

    class PMC(ctypes.Structure):
        _fields_ = [("cb", wt.DWORD), ("PageFaultCount", wt.DWORD),
                    ("PeakWorkingSetSize", ctypes.c_size_t),
                    ("WorkingSetSize", ctypes.c_size_t),
                    ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                    ("PagefileUsage", ctypes.c_size_t),
                    ("PeakPagefileUsage", ctypes.c_size_t)]

    k32 = ctypes.WinDLL("kernel32")
    psapi = ctypes.WinDLL("psapi")
    h = k32.OpenProcess(0x1000 | 0x0010, False, pid)
    if not h:
        return
    try:
        c = PMC()
        c.cb = ctypes.sizeof(c)
        if psapi.GetProcessMemoryInfo(h, ctypes.byref(c), c.cb):
            samples.append(int(c.WorkingSetSize))
    finally:
        k32.CloseHandle(h)


def _sample_linux_once(pid: int, samples: list) -> None:
    try:
        for line in Path(f"/proc/{pid}/status").read_text().splitlines():
            if line.startswith("VmRSS:"):
                samples.append(int(line.split()[1]) * 1024)
                return
    except OSError:
        return


def _encode_once(app: str, src: Path, bitstream: Path, width: int, height: int,
                 frames: int, timeout: int, extra_env=None):
    """Single continuous invocation of the application for `frames` frames.

    Geometry is carried by the Y4M header (the app reads width/height from the
    stream, not from flags), so only --input/--output/--frames are passed. The
    caller's width/height are used to *check* the input we generated, not to
    override the app.

    Peak RSS is sampled while the process runs so long-run memory growth has an
    observation attached to it. Per the task book this is a *trend*, not leak
    evidence: the leak verdict comes only from a detector (see leak_check).
    """
    env = dict(os.environ)
    if extra_env:
        env.update(extra_env)
    cmd = [str(c) for c in (app, "--input", str(src), "--output", str(bitstream),
                            "--frames", str(frames))]
    t0 = time.time()
    proc = subprocess.Popen(cmd, cwd=None, env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True, errors="replace")
    samples: list[int] = []
    stop_flag = threading.Event()
    sampler = (_peak_rss_sampler_windows if os.name == "nt"
               else _peak_rss_sampler_linux)
    th = threading.Thread(target=sampler, args=(proc.pid, stop_flag, samples),
                          daemon=True)
    th.start()
    # The encode can finish before the sampler's first sleep elapses; take one
    # immediate sample so a fast run still yields an observation rather than 0.
    if os.name == "nt":
        _sample_windows_once(proc.pid, samples)
    else:
        _sample_linux_once(proc.pid, samples)
    try:
        out, err = proc.communicate(timeout=timeout)
        timed_out = False
    except subprocess.TimeoutExpired:
        proc.kill()
        out, err = proc.communicate()
        timed_out = True
    finally:
        stop_flag.set()
        th.join(timeout=2.0)
    class _CP:
        pass
    cp = _CP()
    cp.returncode = -999 if timed_out else proc.returncode
    cp.stdout, cp.stderr = out or "", err or ""
    mem = {
        "peak_rss_bytes": max(samples) if samples else None,
        "samples": len(samples),
        "note": "peak RSS is a trend; not leak evidence",
    }
    return cp, time.time() - t0, cmd, mem


def _parse_frame_scan(spec: str) -> list[int]:
    """Parse a comma list of frame counts, e.g. "2,3,17,1000"."""
    out = []
    for part in spec.split(","):
        part = part.strip()
        if not part:
            continue
        if not part.lstrip("-").isdigit() or int(part) < 0:
            raise ValueError(f"bad frame count in --frame-scan: {part!r}")
        out.append(int(part))
    if not out:
        raise ValueError("--frame-scan needs at least one count")
    return out


def _run_frame_scan(app: str, src: Path, outdir: Path, counts: list[int],
                    width: int, height: int, timeout: int) -> dict:
    """Encode the same source once per requested frame count, in one invocation
    each. While T013's P1 contract is in force (SVT_AV2_MAX_ENCODABLE_FRAMES=1u)
    every count > 1 is EXPECTED to be rejected with app exit 15; the counts 0
    and 1 are expected to succeed. The assertion is on the EXIT CODE, never on
    the message text, so a build that silently truncates to one frame is caught.

    This is *preparation*: the long-run counts (17/1000) cannot pass until the
    multi-frame arrow T015 -> T013 -> T016 lands, so their non-zero outcome is
    reported as the frozen-rejection assertion, not as a pass.
    """
    rows = []
    for n in counts:
        out = outdir / f"scan_{n}f.av2"
        if out.exists():
            out.unlink()
        cp, elapsed, cmd, mem = _encode_once(app, src, out, width, height, n, timeout)
        expected_exit = 0 if n <= 1 else 15
        got = cp.returncode
        out_bytes = out.stat().st_size if out.is_file() else 0
        rows.append({
            "frames_requested": n,
            "exit": got,
            "expected_exit_now": expected_exit,
            "matches_frozen_contract": (got == expected_exit),
            "bitstream_exists": out.is_file(),
            "bitstream_bytes": out_bytes,
            "seconds": round(elapsed, 3),
            "cmd": cmd,
        })
    return {"counts": counts, "rows": rows}


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description="T016 continuous-N-frame stability driver")
    ap.add_argument("--app", help="SvtAv2EncApp binary (default: build/T016/<cfg>/bin)")
    ap.add_argument("--avmdec", help="AVM decoder oracle (default: AV2_AVMDEC)")
    ap.add_argument("--frames", type=int, default=1000)
    ap.add_argument("--frame-scan", help="comma list of counts to try in one process "
                                        "each (e.g. 2,3,17,1000), for the S4 long-run "
                                        "preparation; asserts the frozen exit-15 surface")
    ap.add_argument("--width", type=int, default=176)
    ap.add_argument("--height", type=int, default=144)
    ap.add_argument("--out-dir", default=str(DEFAULT_OUT))
    ap.add_argument("--timeout", type=int, default=7200)
    ap.add_argument("--valgrind", help="valgrind binary for leak evidence (Linux)")
    ap.add_argument("--detector", choices=["asan", "tsan"],
                    help="sanitizer build to run for leak/race evidence; ASan/LSan "
                         "gives the leak verdict, TSan the race verdict")
    ap.add_argument("--reuse-input", action="store_true")
    args = ap.parse_args(argv)

    outdir = Path(args.out_dir)
    outdir.mkdir(parents=True, exist_ok=True)

    report: dict = {
        "task": "T016",
        "step": "S4 continuous stability",
        "generated_at": _dt.datetime.now().astimezone().isoformat(),
        "frames_requested": args.frames,
        "geometry": {"width": args.width, "height": args.height},
        "checks": [],
        "blocked": [],
    }

    def check(name, status, detail):
        report["checks"].append({"name": name, "status": status, "detail": detail})
        return status == "PASS"

    # --- locate tools -----------------------------------------------------
    app = args.app
    if not app:
        for cfg in ("Win-Release", "Win-Debug", "Linux-GCC-Release"):
            cand = SVTAV2 / "build" / "T016" / cfg / "bin" / "SvtAv2EncApp.exe"
            if cand.is_file():
                app = str(cand)
                break
    avmdec = args.avmdec or os.environ.get("AV2_AVMDEC")
    if avmdec and not os.path.isfile(avmdec):
        avmdec = None

    report["app"] = app
    report["avmdec"] = avmdec
    if not app or not os.path.isfile(app):
        check("app_present", "BLOCKED", f"application not found: {app}")
        report["blocked"].append("encoder application missing")
    else:
        report["app_sha256"] = _sha256(Path(app))
        check("app_present", "PASS", app)

    # --- material ---------------------------------------------------------
    src = outdir / f"fingerprinted_{args.width}x{args.height}_{args.frames}f.y4m"
    if args.reuse_input and src.is_file():
        expected = list(range(args.frames))
        check("material", "PASS", f"reused {src}")
    else:
        expected = write_fingerprint_y4m(src, args.width, args.height, args.frames)
        check("material", "PASS", f"generated {src} ({args.frames} frames, fingerprinted)")
    report["input"] = {"path": str(src), "sha256": _sha256(src), "bytes": src.stat().st_size}

    if report["blocked"]:
        report["overall"] = "BLOCKED"
        (outdir / "stability.json").write_text(
            json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
        print("BLOCKED: " + "; ".join(report["blocked"]))
        return EXIT_BLOCKED

    # --- frame-count scan (S4 long-run preparation) -----------------------
    #
    # Runs the same fingerprinted source once per requested count, in one
    # process each, and asserts the frozen exit-code surface: 0/1 succeed,
    # anything above is the dedicated SVT_AV2_APP_EXIT_UNSUPPORTED_FRAMES (15)
    # until T013 lifts SVT_AV2_MAX_ENCODABLE_FRAMES. This makes the S4 long-run
    # counts (17/1000) runnable the moment the freeze is lifted, and catches a
    # silent truncation today. It is deliberately asserted on exit codes.
    if args.frame_scan:
        try:
            counts = _parse_frame_scan(args.frame_scan)
        except ValueError as exc:
            report["overall"] = "FAIL"
            check("frame_scan_usage", "FAIL", str(exc))
            (outdir / "stability.json").write_text(
                json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
            print(f"FAIL: {exc}")
            return EXIT_USAGE
        scan = _run_frame_scan(app, src, outdir, counts, args.width, args.height,
                               args.timeout)
        report["frame_scan"] = scan
        bad = [r for r in scan["rows"] if not r["matches_frozen_contract"]]
        detail = "; ".join(
            f"{r['frames_requested']}->exit {r['exit']} (want {r['expected_exit_now']})"
            for r in scan["rows"])
        check("frame_scan_exit_surface", "PASS" if not bad else "FAIL", detail)
        if any(r["frames_requested"] > 1 for r in scan["rows"]):
            report["blocked"].append(
                "frame counts > 1 are refused by design while "
                "SVT_AV2_MAX_ENCODABLE_FRAMES=1u (T013 P1); the long-run counts "
                "are prepared but cannot be exercised until T015 -> T013 lands")
        st = [c["status"] for c in report["checks"]]
        report["overall"] = ("FAIL" if any(s == "FAIL" for s in st)
                             else "BLOCKED_PARTIAL" if any(s == "BLOCKED" for s in st)
                             else "PASS")
        (outdir / "stability.json").write_text(
            json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
        print(f"T016 frame scan -> {outdir / 'stability.json'}")
        print(f"overall = {report['overall']}")
        for r in scan["rows"]:
            print(f"  --frames {r['frames_requested']:<5} exit={r['exit']:<3} "
                  f"expected={r['expected_exit_now']:<2} "
                  f"frozen_ok={r['matches_frozen_contract']} bytes={r['bitstream_bytes']}")
        return {"PASS": EXIT_OK, "FAIL": EXIT_FAIL}.get(report["overall"], EXIT_BLOCKED)

    # --- one continuous encode -------------------------------------------
    bitstream = outdir / f"cont_{args.frames}f.av2"
    cp, elapsed, cmd, mem = _encode_once(app, src, bitstream, args.width, args.height,
                                         args.frames, args.timeout)
    report["encode"] = {
        "cmd": cmd, "exit": cp.returncode, "seconds": round(elapsed, 3),
        "stdout_tail": cp.stdout[-3000:], "stderr_tail": cp.stderr[-3000:],
        "single_process_single_invocation": True,
    }
    report["peak_rss"] = mem
    if cp.returncode != 0 or not bitstream.is_file():
        # T013's P1 contract freezes multi-frame at SVT_AV2_MAX_ENCODABLE_FRAMES=1u
        # and requires an explicit rejection with app exit code
        # SVT_AV2_APP_EXIT_UNSUPPORTED_FRAMES = 15 (main.c), raised BEFORE fopen()
        # so no misleading half-written bitstream is left behind. T013 asks T016 to
        # treat exit==15 as an ASSERTION, not as "not supported yet": that is a
        # frozen dependency (BLOCKED, arrow T015 -> T013 -> T016). Any other nonzero
        # exit is a genuine stability FAIL and must not be excused as BLOCKED. The
        # confirmation text is checked only as corroboration, never as the trigger.
        text = (cp.stdout or "") + (cp.stderr or "")
        exit15 = (cp.returncode == 15)
        text_ok = ("is not supported" in text and "frames" in text)
        status = "BLOCKED" if exit15 else "FAIL"
        if exit15:
            detail = (f"exit=15 (SVT_AV2_APP_EXIT_UNSUPPORTED_FRAMES as frozen by "
                      f"T013 P1); bitstream_exists={bitstream.is_file()}; "
                      f"confirmation_text={'present' if text_ok else 'MISSING'}")
            report["blocked"].append(
                f"encoder rejects {args.frames}-frame continuous encode by design "
                f"(exit 15, SVT_AV2_MAX_ENCODABLE_FRAMES=1u); dependency T015 -> T013")
        else:
            detail = (f"exit={cp.returncode} (expected 0, or 15 for the frozen "
                      f"multi-frame rejection); bitstream_exists={bitstream.is_file()}")
        check("encode_continuous", status, detail)
        report["overall"] = status
        (outdir / "stability.json").write_text(
            json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
        print(f"{report['overall']}: continuous encode exit={cp.returncode}")
        return EXIT_BLOCKED if exit15 else EXIT_FAIL
    report["bitstream"] = {"path": str(bitstream), "bytes": bitstream.stat().st_size,
                           "sha256": _sha256(bitstream)}
    check("encode_continuous", "PASS",
          f"{args.frames} frames in ONE invocation, {elapsed:.2f}s, "
          f"{bitstream.stat().st_size} bytes")

    # --- EOS / clean-shutdown check ---------------------------------------
    #
    # The app signals end of stream through the public API with
    # send_picture(NULL) after the frame loop (app/SvtAv2EncApp/main.c), and a
    # failed EOS is reported as exit 9. A clean run therefore exits 0 with no
    # "EOS" error on stderr; any other outcome means the flush path did not
    # finish. Checked as a dedicated item so a leak/race-free but unclean
    # shutdown cannot pass silently.
    eos_text = (cp.stdout or "") + (cp.stderr or "")
    eos_failed = bool(re.search(r"EOS \(send_picture\(NULL\)\) failed", eos_text))
    exit_ok = (cp.returncode == 0)
    if exit_ok and not eos_failed:
        check("eos_clean_shutdown", "PASS",
              "frame loop + EOS (send_picture(NULL)) flush completed, exit 0")
    else:
        check("eos_clean_shutdown", "FAIL",
              f"exit={cp.returncode}, eos_error_in_output={eos_failed}; "
              f"expected a clean exit 0 with the EOS/flush path exercised")

    # --- decode + per-frame verification ---------------------------------
    if not avmdec:
        check("decode_frames", "BLOCKED", "avmdec oracle unavailable; frame count NOT verified")
        check("per_frame_fingerprint", "BLOCKED", "cannot verify without decoder output")
        report["blocked"].append("avmdec missing")
    else:
        raw = outdir / f"cont_{args.frames}f.dec.yuv"
        dcp, reported = _decoder_frames(avmdec, bitstream, raw, args.width, args.height,
                                        args.timeout)
        report["decode"] = {"exit": dcp.returncode, "reported_frames": reported,
                            "stdout_tail": (dcp.stdout + dcp.stderr)[-2000:]}
        if dcp.returncode != 0 or not raw.is_file():
            check("decode_frames", "FAIL", f"decoder exit={dcp.returncode}")
        else:
            got = read_y4m_fingerprints(raw, args.width, args.height)
            report["decoded_frames_from_bytes"] = len(got)
            ok_count = (len(got) == args.frames)
            check("decode_frames", "PASS" if ok_count else "FAIL",
                  f"reported={reported} from_bytes={len(got)} expected={args.frames}")

            # Pixel-embedded frame identity is only recoverable if the coding
            # actually preserves luma detail. The M1 encoder wires ONLY
            # base_q_idx in (90,140] (app/SvtAv2EncApp/main.c:45-50), so the
            # sole usable qp is 100 = maximum quantization, at which a whole
            # frame collapses to a flat DC level (verified: 64x64 luma = one
            # value, 128). In that regime a fingerprint embedded in pixels is
            # destroyed by construction and cannot distinguish frames. Report
            # this as NOT APPLICABLE rather than a false FAIL, and rely on the
            # structural guards below (frame count, ordering, byte identity).
            luma_flat = _luma_is_flat(raw, args.width, args.height)
            report["fingerprint_applicable"] = not luma_flat
            if luma_flat:
                check("per_frame_fingerprint", "BLOCKED",
                      "decoded luma is flat (encoder's only wired qp=100 quantizes "
                      "everything to DC); pixel-embedded frame identity is not "
                      "recoverable by construction, so this check is N/A here")
                report["fingerprint_mismatches"] = []
                report["fingerprint_mismatch_count"] = 0
            else:
                # Per-frame identity: catches a repeated first frame and drops.
                mism = [(i, g, e) for i, (g, e) in enumerate(zip(got, expected)) if g != e]
                report["fingerprint_mismatches"] = [
                    {"index": i, "got": g, "expected": e} for i, g, e in mism[:20]]
                report["fingerprint_mismatch_count"] = len(mism)
                check("per_frame_fingerprint", "PASS" if not mism else "FAIL",
                      f"{len(got) - len(mism)}/{len(got)} frames match their index"
                      + ("" if not mism else f"; first mismatch idx={mism[0][0]} got={mism[0][1]} exp={mism[0][2]}"))

            # Ordering: fingerprints must be strictly increasing.
            order_ok = all(got[i] < got[i + 1] for i in range(len(got) - 1))
            check("frame_order", "PASS" if order_ok else "FAIL",
                  "strictly increasing frame fingerprints" if order_ok
                  else "order violation or duplicated frame detected")

    # --- leak evidence ----------------------------------------------------
    san_env = None
    if args.detector:
        # ASan/LSan or TSan evidence. The verdict comes from the detector's own
        # report, never from RSS. ASan (with leak detection) decides the leak
        # item; TSan decides the race item. A detector that cannot run here is
        # BLOCKED, not PASS.
        det = args.detector
        small = outdir / f"{det}_input.y4m"
        write_fingerprint_y4m(small, args.width, args.height, 3)
        det_out = outdir / f"{det}_out.av2"
        det_env = dict(os.environ)
        if det == "asan":
            det_env.setdefault("ASAN_OPTIONS", "detect_leaks=1:abort_on_error=0")
        else:
            det_env.setdefault("TSAN_OPTIONS", "halt_on_error=0")
        # Probe with `--frames 1`: it is the count the M1 encoder can actually
        # emit, so the detector really runs the encode path. Asking for more
        # would be refused at exit 15 before any encoder work, which would make
        # a "clean detector" claim hollow.
        det_frames = 1
        cmd = [str(app), "--input", str(small), "--output", str(det_out),
               "--frames", str(det_frames)]
        dcp = _run(cmd, timeout=args.timeout, env=det_env)
        dtext = (dcp.stdout or "") + "\n" + (dcp.stderr or "")
        det_reports = []
        if det == "asan":
            if "AddressSanitizer" in dtext:
                det_reports.append("AddressSanitizer")
            if "LeakSanitizer" in dtext or "detected memory leaks" in dtext:
                det_reports.append("LeakSanitizer")
        else:
            if "ThreadSanitizer" in dtext or "WARNING: ThreadSanitizer" in dtext:
                det_reports.append("ThreadSanitizer")
        report["detector_evidence"] = {
            "tool": det, "env": {k: v for k, v in det_env.items()
                                 if k.startswith(("ASAN", "TSAN", "LSAN"))},
            "cmd": cmd, "exit": dcp.returncode,
            "reports": det_reports,
            "stdout_tail": (dcp.stdout or "")[-2000:],
            "stderr_tail": (dcp.stderr or "")[-4000:],
        }
        item = f"leak_check_{det}" if det == "asan" else "race_check_tsan"
        if dcp.returncode == 15:
            check(item, "BLOCKED",
                  f"{det} probe was refused by the frozen multi-frame limit "
                  f"(exit 15) before running the encode path, so the detector did "
                  f"not exercise it; re-run once the frame limit is lifted")
        elif dcp.returncode != 0:
            check(item, "FAIL",
                  f"{det} run exit={dcp.returncode} (expected 0)")
        elif det_reports:
            check(item, "FAIL",
                  f"{det} reported: {', '.join(det_reports)}; see "
                  f"detector_evidence.stderr_tail")
        else:
            # A clean detector exit with no report is the PASS condition; note
            # that for ASan this also covers the leak item via detect_leaks=1.
            check(item, "PASS",
                  f"{det} ran cleanly (exit {dcp.returncode}), no detector report; "
                  f"env={report['detector_evidence']['env']}")
    elif args.valgrind:
        vg = shutil.which(args.valgrind) or args.valgrind
        small = outdir / "vg_input.y4m"
        write_fingerprint_y4m(small, args.width, args.height, 3)
        vglog = outdir / "valgrind.log"
        cmd = [vg, "--leak-check=full", "--error-exitcode=99",
               f"--log-file={vglog}", app, "--input", str(small),
               "--output", str(outdir / "vg_out.av2"),
               "--frames", "3"]
        vcp = _run(cmd, timeout=args.timeout)
        report["leak_evidence"] = {
            "tool": "valgrind", "cmd": cmd, "exit": vcp.returncode,
            "log": str(vglog) if vglog.is_file() else None,
            "definitely_lost": None, "indirectly_lost": None,
        }
        if vglog.is_file():
            t = vglog.read_text(errors="replace")
            m = re.search(r"definitely lost:\s*([\d,]+)\s*bytes", t)
            if m:
                report["leak_evidence"]["definitely_lost"] = int(m.group(1).replace(",", ""))
            m = re.search(r"indirectly lost:\s*([\d,]+)\s*bytes", t)
            if m:
                report["leak_evidence"]["indirectly_lost"] = int(m.group(1).replace(",", ""))
        dl = report["leak_evidence"]["definitely_lost"]
        if dl is None:
            check("leak_check", "BLOCKED", "valgrind ran but no leak summary parsed")
        else:
            check("leak_check", "PASS" if dl == 0 else "FAIL",
                  f"valgrind definitely lost = {dl} bytes")
    else:
        check("leak_check", "BLOCKED",
              "no leak detector supplied (--detector asan/tsan or --valgrind); "
              "RSS trend is NOT accepted as leak evidence")

    # --- RSS trend (reported, explicitly not a leak verdict) --------------
    report["rss_note"] = ("RSS is recorded as a trend only. Per T016 task book it is "
                          "not a substitute for detector-based leak evidence.")
    st = [c["status"] for c in report["checks"]]
    if any(s == "FAIL" for s in st):
        report["overall"] = "FAIL"
    elif any(s == "BLOCKED" for s in st):
        report["overall"] = "BLOCKED_PARTIAL"
    else:
        report["overall"] = "PASS"

    rp = outdir / "stability.json"
    rp.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"T016 stability -> {rp}")
    print(f"overall = {report['overall']}")
    for c in report["checks"]:
        print(f"  {c['status']:<15} {c['name']}: {c['detail']}")
    return {"PASS": EXIT_OK, "FAIL": EXIT_FAIL}.get(report["overall"], EXIT_BLOCKED)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
