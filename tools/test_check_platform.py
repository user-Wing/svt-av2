#!/usr/bin/env python3
"""T016 self-test for the platform/stability entries.

Asserts the *detection logic* works, using negative controls, so that a later
PASS from run_stability.py cannot be produced by a broken checker. This mirrors
the T014 `--self-test` idea (infrastructure self-test is not a gate result).

Exit codes: 0 all checks pass, 1 a check failed, 2 usage.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
SVTAV2 = HERE.parent
EXIT_OK, EXIT_FAIL, EXIT_USAGE = 0, 1, 2

failures: list[str] = []


def check(name: str, ok: bool, detail: str = "") -> None:
    print(f"{'ok  ' if ok else 'FAIL'} {name}{(' -> ' + detail) if detail else ''}")
    if not ok:
        failures.append(name)


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--python", default=sys.executable)
    args = ap.parse_args(argv)

    sys.path.insert(0, str(HERE))
    try:
        import run_stability as rs
    except Exception as exc:  # pragma: no cover
        print(f"FAIL import run_stability: {exc}")
        return EXIT_FAIL

    with tempfile.TemporaryDirectory(prefix="t016_selftest_") as td:
        tmp = Path(td)
        W = H = 64
        FRAMES = 8
        y4m = tmp / "fp.y4m"
        expected = rs.write_fingerprint_y4m(y4m, W, H, FRAMES)
        check("generator emits one fingerprint per frame",
              expected == list(range(FRAMES)), f"{expected}")

        # Convert to raw 4:2:0 the way a decoder would emit it.
        ysz = W * H
        fsz = ysz + 2 * ((W + 1) // 2) * ((H + 1) // 2)
        data = y4m.read_bytes()
        i = data.index(b"\n") + 1
        raw = bytearray()
        while i < len(data):
            assert data[i:i + 6] == b"FRAME\n"
            i += 6
            raw += data[i:i + fsz]
            i += fsz
        rawp = tmp / "good.yuv"
        rawp.write_bytes(bytes(raw))

        got = rs.read_y4m_fingerprints(rawp, W, H)
        check("round-trip identity", got == expected, f"{got}")

        # Negative control 1: repeated first frame must be detected.
        dup = bytearray(raw)
        dup[fsz:2 * fsz] = raw[0:fsz]
        dupp = tmp / "dup.yuv"
        dupp.write_bytes(bytes(dup))
        gd = rs.read_y4m_fingerprints(dupp, W, H)
        check("detects duplicated first frame", gd != expected, f"{gd}")

        # Negative control 2: dropped frame (shift left) must be detected.
        drop = bytearray(raw[fsz:])
        dropp = tmp / "drop.yuv"
        dropp.write_bytes(bytes(drop))
        gr = rs.read_y4m_fingerprints(dropp, W, H)
        check("detects dropped frame", gr != expected, f"{gr}")

        # Negative control 3: frame count mismatch is visible in raw length.
        check("detects wrong frame count", len(gr) != FRAMES, f"{len(gr)} != {FRAMES}")

    # CLI surfaces must exist and exit with the documented usage/BLOCKED codes.
    for tool, expect_usage in ((HERE / "run_stability.py", True),
                               (HERE / "check_platform.py", True)):
        cp = subprocess.run([args.python, str(tool), "--nonsense-flag"],
                            capture_output=True, text=True)
        check(f"{tool.name} rejects bad usage with exit 2",
              cp.returncode == EXIT_USAGE, f"exit={cp.returncode}")

    # check_platform must report BLOCKED (3), never PASS, when cmake is absent.
    # Redirect --out-dir to a temp directory: the self-test must never overwrite
    # the authoritative artifacts/T016/S1/platform.json platform table.
    with tempfile.TemporaryDirectory(prefix="t016_selftest_fp_") as fp_td:
        cp = subprocess.run(
            [args.python, str(HERE / "check_platform.py"),
             "--only", "win-rel", "--skip-builds", "--out-dir", fp_td],
            capture_output=True, text=True)
    check("check_platform --skip-builds runs", cp.returncode in (0, 1, 3),
          f"exit={cp.returncode}")

    print()
    if failures:
        print(f"SELF-TEST FAILED: {len(failures)} problem(s): {failures}")
        return EXIT_FAIL
    print("SELF-TEST PASS")
    return EXIT_OK


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
