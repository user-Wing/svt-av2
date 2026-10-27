#!/usr/bin/env python3
"""T005 determinism regression: 3x encode must be byte-identical.

Compiles the repository-resident minimal-stream generator
(tests/unit/gen_m1_stream.c) against the already-built M1 libraries, runs it
three times, and requires all three outputs to have the same SHA256.

This is the automated form of the T005 self-test
(see docs/task-reports/T005.md). It does not touch the read-only AVM oracle.

Usage:
  python tools/check_m1_stream_determinism.py --build <build-dir>

Exit codes:
  0 = three encodes produced identical bytes (and, with --expect-sha256, match)
  1 = outputs differ, or the generator failed to build/run
  2 = usage error
  3 = required inputs missing (build dir / libs / source)
"""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, ".."))

FROZEN_SHA256 = "7aab59865d8423a128adbb8d65cf5d08087ce35061ff5324b0d9f620842ce0f7"


def fail(code: int, msg: str) -> int:
    print(f"FAIL: {msg}", file=sys.stderr)
    return code


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def find_lib(build_dir: str, name: str) -> str | None:
    for root, _dirs, files in os.walk(build_dir):
        for fn in files:
            if fn.lower() == name.lower():
                return os.path.join(root, fn)
    return None


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--build", required=True, help="CMake build directory (e.g. build/T005-full)")
    ap.add_argument("--compiler", default="cl", help="C compiler (default: cl)")
    ap.add_argument("--expect-sha256", default=FROZEN_SHA256,
                    help="expected SHA256 (default: frozen M1 value; '' to skip)")
    args = ap.parse_args(argv)

    src = os.path.join(REPO, "tests", "unit", "gen_m1_stream.c")
    if not os.path.isfile(src):
        return fail(3, f"generator source not found: {src}")

    build = os.path.abspath(args.build)
    if not os.path.isdir(build):
        return fail(3, f"build dir not found: {build}")

    lib_bs = find_lib(build, "svt_av2_bitstream.lib")
    lib_ent = find_lib(build, "svt_av2_entropy.lib")
    if not lib_bs or not lib_ent:
        return fail(3, f"libs not found under {build} (svt_av2_bitstream/svt_av2_entropy)")

    inc_bs = os.path.join(REPO, "Source", "Lib", "Bitstream")
    inc_ent = os.path.join(REPO, "Source", "Lib", "EntropyCoding")

    with tempfile.TemporaryDirectory(prefix="t005_det_") as tmp:
        exe = os.path.join(tmp, "gen_m1_stream.exe")
        cmd = [args.compiler, "/nologo", "/W3", "/O2",
               f"/I{inc_bs}", f"/I{inc_ent}", f"/Fe:{exe}", src, lib_bs, lib_ent]
        print("build:", " ".join(cmd))
        rc = subprocess.run(cmd, capture_output=True, text=True)
        if rc.returncode != 0 or not os.path.isfile(exe):
            return fail(1, f"generator build failed (rc={rc.returncode})\n{rc.stdout}\n{rc.stderr}")

        hashes = []
        for i in range(1, 4):
            out = os.path.join(tmp, f"run{i}.bin")
            r = subprocess.run([exe, out], capture_output=True, text=True)
            if r.returncode != 0 or not os.path.isfile(out):
                return fail(1, f"encode run{i} failed (rc={r.returncode})\n{r.stdout}\n{r.stderr}")
            h = sha256_file(out)
            hashes.append(h)
            print(f"run{i}: {h}  ({os.path.getsize(out)} bytes)")

        if len(set(hashes)) != 1:
            return fail(1, f"3 encodes differ: {hashes}")

        if args.expect_sha256 and hashes[0] != args.expect_sha256:
            return fail(1, f"sha256 {hashes[0]} != expected {args.expect_sha256}")

    print(f"PASS: 3 encodes identical (sha256={hashes[0]})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
