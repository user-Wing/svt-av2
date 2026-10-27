#!/usr/bin/env python3
"""Conformance check: generate an M0 benchmark record and validate its schema.

Owner: T002. This is the CTest entry point that proves the M0 benchmark path
works end to end: SvtAv2EncApp parses a corpus Y4M, tools/benchmark.py emits a
versioned record with encoded fps = null, and the record passes schema and
semantic validation.

Exit: 0 pass, 1 fail, 3 blocked (missing app/input).
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.normpath(os.path.join(HERE, "..", "..", "tools"))
SCHEMA_DIR = os.path.normpath(os.path.join(HERE, "..", "..", "benchmarks", "schema"))


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", required=True)
    parser.add_argument("--input", required=True)
    parser.add_argument("--out-dir", default=None)
    args = parser.parse_args(argv)

    if not os.path.isfile(args.app):
        print("BLOCKED: app not found: %s" % args.app)
        return 3
    if not os.path.isfile(args.input):
        print("BLOCKED: input not found: %s" % args.input)
        return 3

    out_dir = args.out_dir or tempfile.mkdtemp(prefix="svt_av2_bench_")
    os.makedirs(out_dir, exist_ok=True)
    record = os.path.join(out_dir, "bench_m0.json")

    gen = [sys.executable, os.path.join(TOOLS, "benchmark.py"),
           "--app", args.app, "--input", args.input, "--out", record]
    result = subprocess.run(gen, capture_output=True)
    sys.stdout.write(result.stdout.decode("utf-8", "replace"))
    sys.stderr.write(result.stderr.decode("utf-8", "replace"))
    if result.returncode != 0:
        print("FAIL: benchmark.py exit %d" % result.returncode)
        return 1

    validate = [sys.executable, os.path.join(SCHEMA_DIR, "validate_benchmark.py"), record]
    result = subprocess.run(validate, capture_output=True)
    sys.stdout.write(result.stdout.decode("utf-8", "replace"))
    sys.stderr.write(result.stderr.decode("utf-8", "replace"))
    if result.returncode != 0:
        print("FAIL: benchmark record failed schema validation")
        return 1

    print("PASS: M0 benchmark record generated and validated")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
