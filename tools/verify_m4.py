#!/usr/bin/env python3
"""M4 independent decoder matrix, exact motion fixtures and rate/quality checks."""
import argparse
import hashlib
import json
import math
import os
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
from compare_recon import frame_size, plane_layout, file_sha256
from verify_m3 import unpack_y4m
from m4_model import predict, texture

if os.name == "nt":
    import ctypes
    from ctypes import wintypes

    class _MemoryCounters(ctypes.Structure):
        _fields_ = [("cb", wintypes.DWORD), ("faults", wintypes.DWORD)] + [
            (name, ctypes.c_size_t) for name in
            ("peak", "rss", "quota_peak_paged", "quota_paged",
             "quota_peak_nonpaged", "quota_nonpaged", "commit", "peak_commit")]

    _read_process_memory = ctypes.WinDLL("psapi", use_last_error=True).GetProcessMemoryInfo
    _read_process_memory.argtypes = [wintypes.HANDLE, ctypes.POINTER(_MemoryCounters), wintypes.DWORD]
    _read_process_memory.restype = wintypes.BOOL


def memory_snapshot(proc):
    """Read OS memory counters; these are not cumulative malloc counts."""
    try:
        if os.name == "nt":
            data = _MemoryCounters()
            data.cb = ctypes.sizeof(data)
            if _read_process_memory(int(proc._handle), ctypes.byref(data), data.cb):
                return {"rss_bytes": data.rss, "peak_rss_bytes": data.peak,
                        "commit_bytes": data.commit}
        else:
            fields = {}
            for line in Path(f"/proc/{proc.pid}/status").read_text().splitlines():
                parts = line.split()
                if parts[0] in ("VmRSS:", "VmHWM:", "VmData:"):
                    fields[parts[0]] = int(parts[1]) * 1024
            if "VmRSS:" in fields:
                return {"rss_bytes": fields["VmRSS:"], "peak_rss_bytes": fields["VmHWM:"],
                        "data_bytes": fields.get("VmData:", 0)}
    except OSError:
        pass
    return None


def run(command, timeout, report):
    start, samples = time.monotonic(), []
    cmd = [str(item) for item in command]
    with subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          text=True, errors="replace") as proc:
        while True:
            sample = memory_snapshot(proc)
            if sample is not None:
                sample["seconds"] = time.monotonic() - start
                samples.append(sample)
            try:
                stdout, stderr = proc.communicate(timeout=min(0.1, timeout))
                break
            except subprocess.TimeoutExpired:
                if time.monotonic() - start >= timeout:
                    proc.kill()
                    proc.communicate()
                    raise subprocess.TimeoutExpired(cmd, timeout)
        record = {"command": cmd, "exit": proc.returncode,
                  "seconds": time.monotonic() - start, "stdout": stdout[-4000:],
                  "stderr": stderr[-4000:], "memory_samples": samples,
                  "peak_rss_bytes": max((s["peak_rss_bytes"] for s in samples), default=None)}
        report.append(record)
        if proc.returncode:
            raise RuntimeError(f"exit {proc.returncode}: {cmd[0]}\n{stderr[-2000:] or stdout[-2000:]}")
        return subprocess.CompletedProcess(cmd, proc.returncode, stdout, stderr)


def sample_type(bit_depth):
    return np.dtype("u1" if bit_depth == 8 else "<u2")


def frame_bytes(planes, bit_depth):
    return b"".join(p.astype(sample_type(bit_depth)).tobytes() for p in planes)


def raw_planes(data, width, height, bit_depth, chroma):
    values = np.frombuffer(data, dtype=sample_type(bit_depth)).astype(np.int64)
    offset, planes = 0, []
    for _, w, h in plane_layout(width, height, chroma):
        planes.append(values[offset:offset + w*h].reshape(h, w))
        offset += w*h
    if offset != values.size:
        raise ValueError("raw frame size mismatch")
    return planes


def write_frames(path, width, height, bit_depth, chroma, planes):
    token = "420jpeg" if bit_depth == 8 else chroma + "p10"
    with path.open("wb") as out:
        out.write(f"YUV4MPEG2 W{width} H{height} F25:1 Ip C{token}\n".encode())
        for frame in planes:
            out.write(b"FRAME\n")
            out.write(frame_bytes(frame, bit_depth))


def generate_frames(width, height, frames, bit_depth, chroma, pattern):
    bases = [texture(w, h, bit_depth, p) for p, (_, w, h) in
             enumerate(plane_layout(width, height, chroma))]
    shifts = [(4, -2), (1, 3), (-3, -1), (-4, 2), (0, 0)]
    # Each two-frame case contains one pair; larger sequences alternate shifts.
    start = [64, 176, 1280, 1920].index(width) % 3 if frames == 2 and width in (64, 176, 1280, 1920) else 0
    dx = dy = 0
    for frame in range(frames):
        if pattern == "static":
            yield bases
            continue
        if pattern == "mixed" and frame and frame % 13 == 0:
            bases = [texture(w, h, bit_depth, p, frame // 13) for p, (_, w, h)
                     in enumerate(plane_layout(width, height, chroma))]
            dx = dy = 0
        if frame and not (pattern == "mixed" and frame % 7 in (0, 1)):
            sx, sy = shifts[(start + frame - 1) % len(shifts)]
            dx, dy = dx + sx, dy + sy
        # Shift content by +d means reference sampling at -d.
        yield [predict(b, (-dx, -dy), int(p > 0 and chroma == "420"))
               for p, b in enumerate(bases)]


def read_y4m_frames(path, width, height, frames, bit_depth, chroma):
    size = frame_size(width, height, bit_depth, chroma)
    with path.open("rb") as src:
        src.readline()
        for _ in range(frames):
            if src.readline().strip() != b"FRAME":
                raise RuntimeError("source frame marker missing")
            data = src.read(size)
            if len(data) != size:
                raise RuntimeError("source frame truncated")
            yield raw_planes(data, width, height, bit_depth, chroma)
        if src.read(1):
            raise RuntimeError("source has extra frames/bytes")


def quality(source, recon, width, height, frames, bit_depth, chroma):
    sums = np.zeros(3, dtype=np.float64)
    counts = np.zeros(3, dtype=np.int64)
    per_frame = []
    size = frame_size(width, height, bit_depth, chroma)
    with recon.open("rb") as rec:
        for original in read_y4m_frames(source, width, height, frames, bit_depth, chroma):
            data = rec.read(size)
            if len(data) != size:
                raise RuntimeError("reconstruction frame truncated")
            decoded = raw_planes(data, width, height, bit_depth, chroma)
            frame_mse = []
            for p, (a, b) in enumerate(zip(original, decoded)):
                diff = a - b
                error = float(np.square(diff).sum())
                sums[p] += error
                counts[p] += a.size
                frame_mse.append(error / a.size)
            per_frame.append(frame_mse)
        if rec.read(1):
            raise RuntimeError("reconstruction has extra bytes")
    mse = sums / counts
    peak = (1 << bit_depth) - 1
    return {"mse": float(sums.sum() / counts.sum()),
            "plane_mse": mse.tolist(), "frame_plane_mse": per_frame,
            "plane_psnr": [None if v == 0 else 10*math.log10(peak*peak/v) for v in mse]}


def encode(case, app, source, frames, mode, timeout, commands):
    stream, recon = case / f"{mode}.av2", case / f"{mode}.yuv"
    if stream.exists() or recon.exists():
        raise RuntimeError("output exists; use a fresh output directory")
    run([app, "--input", source, "--output", stream, "--export-recon", recon,
         "--frames", frames, "--pred-structure", mode], timeout, commands)
    return stream, recon


def validate_decode(case, stream, recon, decoder, width, height, frames,
                    bit_depth, chroma, timeout, commands):
    size = frame_size(width, height, bit_depth, chroma) * frames
    if recon.stat().st_size != size or not stream.stat().st_size:
        raise RuntimeError("empty stream or reconstruction length mismatch")
    # Hash before the oracle ever runs.
    before = file_sha256(recon)
    raw, decoded = case / (stream.stem + "-decoded.yuv"), case / (stream.stem + "-decoded.y4m")
    run([decoder, "--summary", "-o", decoded, stream], timeout, commands)
    header, hashes = unpack_y4m(decoded, raw, width, height, frames, bit_depth, chroma)
    if file_sha256(recon) != before or file_sha256(raw) != before:
        raise RuntimeError("decoder/internal reconstruction mismatch")
    from m4_stream import inspect_stream
    syntax = inspect_stream(stream.read_bytes(), frames, stream.stem == "low-delay-p")
    return {"stream_sha256": file_sha256(stream), "internal_sha256": before,
            "frame_sha256": hashes, "decoder_header": header, "syntax": syntax,
            "bytes": stream.stat().st_size, "mismatch": 0}


def compare_rate(ai, inter):
    return (inter["bytes"] <= ai["bytes"] * 0.5 and
            inter["quality"]["mse"] <= ai["quality"]["mse"] * 1.05 + 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--decoder", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--formats", default="8:420,10:420,10:444")
    parser.add_argument("--sizes", default="64x64,176x144,1280x720,1920x1080")
    parser.add_argument("--frames", default="2,3,17")
    parser.add_argument("--patterns", default="static,translation,mixed")
    parser.add_argument("--timeout", type=int, default=7200)
    parser.add_argument("--rate", action="store_true", help="128x128 static 64f rate/quality comparison")
    args = parser.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    results, failed = [], False
    for fmt in args.formats.split(","):
        bd_text, chroma = fmt.split(":")
        bd = int(bd_text)
        if (bd, chroma) not in ((8, "420"), (10, "420"), (10, "444")):
            parser.error("supported: 8:420,10:420,10:444")
        sizes = ["128x128"] if args.rate else args.sizes.split(",")
        lengths = [64] if args.rate else list(map(int, args.frames.split(",")))
        patterns = ["static"] if args.rate else args.patterns.split(",")
        for size in sizes:
            width, height = map(int, size.split("x"))
            for frames in lengths:
                for pattern in patterns:
                    if pattern not in ("static", "translation", "mixed"):
                        parser.error("unknown pattern")
                    name = f"{bd}bit{chroma}_{size}_{frames}f_{pattern}"
                    case = args.out_dir / name
                    case.mkdir(exist_ok=True)
                    report = {"bit_depth": bd, "chroma": chroma, "width": width,
                              "height": height, "frames": frames, "pattern": pattern, "commands": []}
                    results.append(report)
                    try:
                        source = case / "input.y4m"
                        write_frames(source, width, height, bd, chroma,
                                     generate_frames(width, height, frames, bd, chroma, pattern))
                        report["source_sha256"] = file_sha256(source)
                        modes = ("all-intra", "low-delay-p") if args.rate else ("low-delay-p",)
                        for mode in modes:
                            stream, recon = encode(case, args.app, source, frames, mode,
                                                   args.timeout, report["commands"])
                            result = validate_decode(case, stream, recon, args.decoder,
                                width, height, frames, bd, chroma, args.timeout, report["commands"])
                            if args.rate:
                                result["quality"] = quality(source, recon, width, height, frames, bd, chroma)
                            report[mode] = result
                        if args.rate:
                            ai, inter = report["all-intra"], report["low-delay-p"]
                            report["byte_ratio"] = inter["bytes"] / ai["bytes"]
                            if not compare_rate(ai, inter):
                                raise RuntimeError("rate/quality threshold failed")
                        report["status"] = "PASS"
                    except (OSError, RuntimeError, ValueError, subprocess.TimeoutExpired) as exc:
                        report.update(status="FAIL", error=str(exc))
                        failed = True
                    (args.out_dir / "acceptance.json").write_text(json.dumps(results, indent=2),
                                                                 encoding="utf-8")
                    print(f"{name}: {report['status']}", flush=True)
    return int(failed)


if __name__ == "__main__":
    sys.exit(main())
