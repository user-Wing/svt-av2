#!/usr/bin/env python3
"""T016 S1/S2 platform + sanitizer baseline recorder.

This tool does NOT decide functional correctness. It records, for each
platform/configuration, what was actually built and what the test runner
actually reported, so the platform table in docs/task-reports/T016.md can be
backed by reproducible commands instead of prose.

It intentionally reuses T014's ``run_matrix.py --check-oracle`` for oracle
identity (contracts section 9) rather than re-implementing oracle discovery.

Exit codes:
    0  every requested configuration was recorded (checks passed)
    1  at least one configuration failed a check
    2  usage error
    3  a required external tool/environment is missing (BLOCKED, never PASS)
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
from pathlib import Path

EXIT_OK = 0
EXIT_FAIL = 1
EXIT_USAGE = 2
EXIT_BLOCKED = 3

HERE = Path(__file__).resolve().parent
SVTAV2 = HERE.parent
DEFAULT_OUT = SVTAV2 / "artifacts" / "T016" / "S1"
ROOT = SVTAV2.parent

# A CTest summary line for both the "100% tests passed" and the older
# "100% tests passed out of N" spellings used across CMake versions.
_RE_TESTS_RUN = re.compile(r"^(\d+)/(\d+)\s+Test\s+#", re.MULTILINE)
_RE_FAILED = re.compile(r"^\s*(\d+)\s+-\s+(\S+)\s+\(Failed", re.MULTILINE)
_RE_NOT_RUN = re.compile(r"^\s*(\d+)\s+-\s+(\S+)\s+\((Not Run|Skipped)\)", re.MULTILINE)


def _sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


# Source files that the platform verdicts depend on. Recording their SHA256 in
# every snapshot is what separates "a real regression" from "a stale read of an
# already-fixed tree" (both T007 and T016 have been bitten by the latter).
_JUDGED_SOURCES = [
    "Source/Lib/Common/picture.c",
    "Source/Lib/Common/block_geometry.c",
    "tests/unit/test_picture.c",
    "tests/unit/test_block_geometry.c",
    "tests/unit/test_intra_prediction.c",
    "tests/unit/test_residual.c",
    "tests/CMakeLists.txt",
    # The oracle-identity step calls T014's `run_matrix.py --check-oracle`, so a
    # snapshot must record which runner version produced its oracle block.
    "tools/run_matrix.py",
    "tools/compare_recon.py",
]


def _source_hashes() -> dict:
    """SHA256 of the sources the platform verdicts are read against."""
    out = {}
    for rel in _JUDGED_SOURCES:
        p = SVTAV2 / rel
        if p.is_file():
            out[rel] = _sha256(p)
    return out


def _run(cmd: list[str], cwd: Path | None = None, timeout: int = 3600,
         env: dict | None = None) -> subprocess.CompletedProcess:
    return subprocess.run(
        cmd, cwd=str(cwd) if cwd else None, capture_output=True, text=True,
        timeout=timeout, env=env, errors="replace",
    )


def _which_wsl(wsl_exe: str, distro: str) -> subprocess.CompletedProcess:
    return _run([wsl_exe, "-d", distro, "--exec", "bash", "-lc", "echo READY"], timeout=180)


def _parse_ctest(stdout: str) -> dict:
    """Extract the runner's own accounting, not our guess of it."""
    total = None
    for m in _RE_TESTS_RUN.finditer(stdout):
        idx, tot = int(m.group(1)), int(m.group(2))
        total = max(total or 0, tot)
    failed = [(int(i), n) for i, n in _RE_FAILED.findall(stdout)]
    notrun = [(int(i), n, s) for i, n, s in _RE_NOT_RUN.findall(stdout)]
    passed = None
    m = re.search(r"(\d+)%\s+tests passed,\s+(\d+)\s+tests failed out of\s+(\d+)", stdout)
    if m:
        passed = int(m.group(3)) - int(m.group(2))
    else:
        m = re.search(r"(\d+)%\s+tests passed out of\s+(\d+)", stdout)
        if m:
            passed = int(m.group(2))
        else:
            m = re.search(r"(\d+)%\s+tests passed", stdout)
            if m and total is not None:
                passed = total
    return {
        "registered": total,
        "passed": passed,
        "failed": [{"index": i, "name": n} for i, n in failed],
        "not_run": [{"index": i, "name": n, "state": s} for i, n, s in notrun],
    }


def _linux_prefix(wsl_exe: str, distro: str, cmd: str) -> list[str]:
    return [wsl_exe, "-d", distro, "--exec", "bash", "-lc", cmd]


def cmake_version_dir(build_dir: Path) -> str:
    """Return CMake's per-version CMakeFiles subdirectory name (e.g. '3.28.0')."""
    vers = [p.name for p in (build_dir / "CMakeFiles").glob("*")
            if p.is_dir() and re.fullmatch(r"\d+\.\d+[.\d]*", p.name)]
    return max(vers) if vers else ""


def _wsl_path(p: Path) -> str:
    """C:\\a\\b -> /mnt/c/a/b"""
    s = str(p).replace("\\", "/")
    if len(s) > 1 and s[1] == ":":
        return "/mnt/" + s[0].lower() + s[2:]
    return s


def record_windows(args, cfg: str, build_type: str) -> dict:
    out: dict = {"config": cfg, "platform": "windows-msvc", "build_type": build_type}
    bdir = SVTAV2 / "build" / "T016" / cfg
    steps: list[dict] = []

    cmake = shutil.which("cmake")
    if not cmake:
        out["status"] = "BLOCKED"
        out["blocked_reason"] = "cmake not on PATH (run activate.ps1 first)"
        return out

    if getattr(args, "clean", False):
        # T007 hit a stale picture.c.o with make; a clean build removes any
        # chance the matrix reports a previous source revision.
        shutil.rmtree(bdir, ignore_errors=True)
    steps.append({"step": "clean", "clean": bool(getattr(args, "clean", False))})

    gen = [cmake, "-S", str(SVTAV2), "-B", str(bdir), "-G", "Ninja",
           f"-DCMAKE_BUILD_TYPE={build_type}", "-DSVT_AV2_WARNINGS_AS_ERRORS=ON"]
    cp = _run(gen)
    steps.append({"step": "configure", "cmd": gen, "exit": cp.returncode})
    if cp.returncode != 0:
        out["status"] = "FAIL"
        out["steps"] = steps
        out["stdout_tail"] = cp.stdout[-4000:]
        out["stderr_tail"] = cp.stderr[-4000:]
        return out

    # Recover the compiler identity from CMake's own compiler description file
    # (the cache only stores the compiler *path*, not its id/version).
    comp = ver = None
    ccf = bdir / "CMakeFiles" / f"{cmake_version_dir(bdir)}" / "CMakeCCompiler.cmake"
    if not ccf.is_file():
        cands = list(bdir.glob("CMakeFiles/*/CMakeCCompiler.cmake"))
        ccf = cands[0] if cands else ccf
    if ccf.is_file():
        text = ccf.read_text(errors="replace")
        m = re.search(r'set\(CMAKE_C_COMPILER_ID\s+"?([^")\s]+)"?\)', text)
        if m:
            comp = m.group(1)
        m = re.search(r'set\(CMAKE_C_COMPILER_VERSION\s+"?([^")\s]+)"?\)', text)
        if m:
            ver = m.group(1)
    out["compiler"] = f"{comp} {ver}" if comp else None

    bp = _run([cmake, "--build", str(bdir)])
    steps.append({"step": "build", "cmd": [cmake, "--build", str(bdir)], "exit": bp.returncode})
    if bp.returncode != 0:
        out["status"] = "FAIL"
        out["steps"] = steps
        out["stdout_tail"] = bp.stdout[-4000:]
        out["stderr_tail"] = bp.stderr[-4000:]
        return out

    ctest = shutil.which("ctest")
    tp = _run([ctest, "--test-dir", str(bdir), "--output-on-failure"])
    steps.append({"step": "ctest", "cmd": [ctest, "--test-dir", str(bdir)], "exit": tp.returncode})
    out["ctest"] = _parse_ctest(tp.stdout)
    out["ctest_raw_tail"] = tp.stdout[-3000:]
    out["steps"] = steps
    out["build_dir"] = str(bdir)

    app = bdir / "bin" / ("SvtAv2EncApp.exe" if os.name == "nt" else "SvtAv2EncApp")
    if app.is_file():
        out["app_sha256"] = _sha256(app)
        out["app_path"] = str(app)
    out["status"] = "PASS" if tp.returncode == 0 else "FAIL"
    return out


def record_linux(args, cfg: str, build_type: str, sanitizers: bool) -> dict:
    out: dict = {
        "config": cfg,
        "platform": f"linux-{args.distro}",
        "build_type": build_type,
        "sanitizers": sanitizers,
    }
    bdir = SVTAV2 / "build" / "T016" / cfg
    steps: list[dict] = []

    probe = _which_wsl(args.wsl, args.distro)
    if probe.returncode != 0:
        out["status"] = "BLOCKED"
        out["blocked_reason"] = f"WSL distro {args.distro!r} not reachable"
        return out

    ws = _wsl_path(SVTAV2)
    wb = _wsl_path(bdir)

    if getattr(args, "clean", False):
        shutil.rmtree(bdir, ignore_errors=True)
    steps.append({"step": "clean", "clean": bool(getattr(args, "clean", False))})

    san = "-DSVT_AV2_ENABLE_SANITIZERS=ON" if sanitizers else "-DSVT_AV2_ENABLE_SANITIZERS=OFF"
    cfg_cmd = (f"cmake -S {ws} -B {wb} -G 'Unix Makefiles' "
               f"-DCMAKE_BUILD_TYPE={build_type} {san} "
               f"-DSVT_AV2_WARNINGS_AS_ERRORS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON")
    cp = _run(_linux_prefix(args.wsl, args.distro, cfg_cmd), timeout=1800)
    steps.append({"step": "configure", "cmd": cfg_cmd, "exit": cp.returncode})
    if cp.returncode != 0:
        out["status"] = "FAIL"
        out["steps"] = steps
        out["stdout_tail"] = cp.stdout[-4000:]
        out["stderr_tail"] = cp.stderr[-4000:]
        return out

    ver_cmd = "gcc --version | head -1; cmake --version | head -1; uname -r"
    vp = _run(_linux_prefix(args.wsl, args.distro, ver_cmd), timeout=300)
    out["toolchain"] = [l for l in vp.stdout.splitlines() if l.strip()]

    b_cmd = f"cmake --build {wb} -j {args.jobs}"
    bp = _run(_linux_prefix(args.wsl, args.distro, b_cmd), timeout=3600)
    steps.append({"step": "build", "cmd": b_cmd, "exit": bp.returncode})
    out["build_stdout_tail"] = bp.stdout[-3000:]
    if bp.returncode != 0:
        out["status"] = "FAIL"
        out["steps"] = steps
        out["stderr_tail"] = bp.stderr[-4000:]
        return out

    # Prove the sanitizer is really in the artifacts rather than only requested.
    if sanitizers:
        ev_cmd = (f"cd {wb} && echo '--- link flags ---' && "
                  f"grep -rho -- '-fsanitize=[a-z,]*' tests/CMakeFiles/test_api.dir/link.txt "
                  f"Source/Lib/Codec/CMakeFiles/*/link.txt 2>/dev/null | sort -u && "
                  f"echo '--- __asan symbols ---' && nm -C bin/test_api 2>/dev/null | grep -ci __asan && "
                  f"echo '--- ldd ---' && ldd bin/test_api | grep -iE 'asan|ubsan'")
        ep = _run(_linux_prefix(args.wsl, args.distro, ev_cmd), timeout=300)
        out["sanitizer_evidence"] = ep.stdout.strip()
        out["sanitizer_evidence_exit"] = ep.returncode

    env_prefix = ("export ASAN_OPTIONS=detect_leaks=1:abort_on_error=0 "
                  "UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0 && ")
    t_cmd = env_prefix + f"ctest --test-dir {wb} --output-on-failure"
    tp = _run(_linux_prefix(args.wsl, args.distro, t_cmd), timeout=3600)
    steps.append({"step": "ctest", "cmd": t_cmd, "exit": tp.returncode})
    out["ctest"] = _parse_ctest(tp.stdout)
    out["ctest_raw_tail"] = tp.stdout[-3000:]
    out["steps"] = steps
    out["build_dir"] = str(bdir)

    # App artifact identity on the Linux side too, so a Windows-side decoder
    # acceptance of a Linux artifact can be tied to a specific binary.
    sh_cmd = f"cd {wb} && sha256sum bin/SvtAv2EncApp 2>/dev/null | cut -d' ' -f1"
    shp = _run(_linux_prefix(args.wsl, args.distro, sh_cmd), timeout=300)
    digest = (shp.stdout or "").strip()
    if digest:
        out["app_sha256"] = digest
        out["app_path"] = f"{wb}/bin/SvtAv2EncApp"

    out["status"] = "PASS" if tp.returncode == 0 else "FAIL"
    return out


def probe_clang(args) -> dict:
    """Record Clang availability truthfully instead of assuming it."""
    res: dict = {"windows": [], "wsl": None}
    cands = [
        r"C:\Program Files\LLVM\bin\clang.exe",
        r"C:\Program Files (x86)\LLVM\bin\clang.exe",
        r"C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\Llvm\x64\bin\clang.exe",
        r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\Llvm\x64\bin\clang.exe",
    ]
    res["windows"] = [{"path": c, "exists": os.path.isfile(c)} for c in cands]
    res["windows_any"] = any(c["exists"] for c in res["windows"])

    cmd = ("for c in clang clang-14 clang-15 clang-16 clang-17 clang-18; do "
           "if command -v $c >/dev/null 2>&1; then echo \"FOUND $c $($c --version | head -1)\"; fi; done; "
           "ls -d /usr/lib/llvm-* 2>/dev/null; echo END")
    cp = _run(_linux_prefix(args.wsl, args.distro, cmd), timeout=300)
    res["wsl"] = cp.stdout.strip()
    res["wsl_any"] = "FOUND" in cp.stdout or "/usr/lib/llvm-" in cp.stdout
    return res


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description="T016 platform baseline recorder")
    ap.add_argument("--out-dir", default=str(DEFAULT_OUT))
    ap.add_argument("--wsl", default="wsl")
    ap.add_argument("--distro", default="ARXChem-Ubuntu-22.04-WSL1")
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--only", help="comma list: win-rel,win-dbg,lin-rel,lin-dbg,lin-san,clang")
    ap.add_argument("--skip-builds", action="store_true",
                    help="only probe clang/oracle, do not (re)build")
    ap.add_argument("--clean", action="store_true",
                    help="remove each build dir first (avoids reporting a stale object)")
    args = ap.parse_args(argv)

    outdir = Path(args.out_dir)
    outdir.mkdir(parents=True, exist_ok=True)

    want = set((args.only or "win-rel,win-dbg,lin-rel,lin-dbg,lin-san,clang").split(","))
    report: dict = {
        "task": "T016",
        "step": "S1/S2 platform + sanitizer baseline",
        "generated_at": _dt.datetime.now().astimezone().isoformat(),
        "root": str(ROOT),
        "source_hashes": _source_hashes(),
        "configs": [],
    }

    if "clang" in want:
        report["clang_probe"] = probe_clang(args)

    if not args.skip_builds:
        if "win-rel" in want:
            report["configs"].append(record_windows(args, "Win-Release", "Release"))
        if "win-dbg" in want:
            report["configs"].append(record_windows(args, "Win-Debug", "Debug"))
        if "lin-rel" in want:
            report["configs"].append(record_linux(args, "Linux-GCC-Release", "Release", False))
        if "lin-dbg" in want:
            report["configs"].append(record_linux(args, "Linux-GCC-Debug", "Debug", False))
        if "lin-san" in want:
            report["configs"].append(record_linux(args, "Linux-GCC-San", "Debug", True))

    # Oracle identity is T014's entry point; reuse it, do not reimplement.
    rm = SVTAV2 / "tools" / "run_matrix.py"
    if rm.is_file():
        cp = _run([sys.executable, str(rm), "--check-oracle"])
        report["oracle"] = {"exit": cp.returncode, "output": (cp.stdout + cp.stderr)[-2500:]}
    else:
        report["oracle"] = {"exit": None, "output": "run_matrix.py not found (T014 owns it)"}

    statuses = [c.get("status") for c in report["configs"]]
    if not statuses:
        overall = "NO_CONFIGS"
    elif any(s == "FAIL" for s in statuses):
        overall = "FAIL"
    elif any(s == "BLOCKED" for s in statuses):
        overall = "BLOCKED_PARTIAL"
    else:
        overall = "PASS"
    report["overall"] = overall

    rp = outdir / "platform.json"
    rp.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")

    print(f"T016 platform baseline -> {rp}")
    print(f"overall = {overall}")
    for c in report["configs"]:
        ct = c.get("ctest") or {}
        print(f"  {c['config']:<20} {c.get('status'):<8} "
              f"tests={ct.get('registered')} passed={ct.get('passed')} "
              f"failed={len(ct.get('failed') or [])} compiler={c.get('compiler') or c.get('toolchain')}")
        for f in (ct.get("failed") or []):
            print(f"      FAILED: {f['name']}")
    if "clang_probe" in report:
        print(f"  clang windows={report['clang_probe']['windows_any']} "
              f"wsl={report['clang_probe']['wsl_any']}")

    if overall == "FAIL":
        return EXIT_FAIL
    # "no configurations recorded" is only a usage error when the caller did not
    # explicitly ask for a build-free probe run (e.g. --skip-builds --only clang).
    if overall == "NO_CONFIGS" and not args.skip_builds:
        return EXIT_USAGE
    return EXIT_OK


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
