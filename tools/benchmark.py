#!/usr/bin/env python3
"""Emit a versioned SVT-AV2 benchmark record (T002).

Owner: T002. M0 mode drives SvtAv2EncApp in metadata-only ("parse_init") mode
and records wall time for parsing the Y4M header. M0 does NOT encode, so
encoded_frames and fps are null and carry an explicit reason; this tool never
fabricates encode fps.

Missing app/binary => BLOCKED (exit 3), never a PASS record.

Usage:
    python benchmark.py --app <SvtAv2EncApp> --input <file.y4m> --out <record.json>
                        [--milestone M0] [--task T002] [--mode parse_init]
                        [--qp 32] [--threads 1] [--preset 4] [--no-deterministic]
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import re
import subprocess
import sys
import time

SCHEMA_VERSION = 1
EXIT_OK = 0
EXIT_FAIL = 1
EXIT_USAGE = 2
EXIT_BLOCKED = 3


def _sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def _parse_y4m(path: str):
    with open(path, "rb") as fh:
        data = fh.read()
    newline = data.find(b"\n")
    if newline < 0:
        raise ValueError("no Y4M header newline")
    header = data[:newline].decode("ascii", "strict")
    if not header.startswith("YUV4MPEG2"):
        raise ValueError("not a YUV4MPEG2 stream")
    width = height = None
    chroma = "420"
    for field in header.split()[1:]:
        if field.startswith("W"):
            width = int(field[1:])
        elif field.startswith("H"):
            height = int(field[1:])
        elif field.startswith("C"):
            chroma = field[1:]
    if width is None or height is None:
        raise ValueError("missing W/H")
    depth = 8
    if chroma.endswith("p10"):
        depth = 10
    elif chroma.endswith("p12"):
        depth = 12
    chroma_short = chroma.replace("jpeg", "").replace("mpeg2", "").replace("paldv", "")
    chroma_short = chroma_short.replace("p10", "").replace("p12", "")
    chroma_short = chroma_short.replace("mono", "400")
    chroma_short = chroma_short.replace("C", "")
    raw_frame = width * height + 2 * (width // 2) * (height // 2)
    pos = newline + 1
    frames = 0
    while pos < len(data):
        tag_end = data.find(b"\n", pos)
        if tag_end < 0 or not data[pos:tag_end].startswith(b"FRAME"):
            raise ValueError("bad frame tag at %d" % pos)
        pos = tag_end + 1 + raw_frame
        frames += 1
    return width, height, depth, chroma_short, frames


def _app_version(app: str):
    try:
        result = subprocess.run([app, "--version"], capture_output=True, timeout=30)
    except (OSError, subprocess.TimeoutExpired):
        return None, ""
    text = (result.stdout + result.stderr).decode("utf-8", "replace")
    return text, text


def _git_commit(repo_dir: str):
    try:
        result = subprocess.run(["git", "-C", repo_dir, "rev-parse", "HEAD"],
                                capture_output=True, timeout=30)
        if result.returncode == 0:
            return result.stdout.decode("ascii").strip()
    except (OSError, subprocess.TimeoutExpired):
        pass
    return None


def _cpu_name() -> str:
    return os.environ.get("PROCESSOR_IDENTIFIER") or platform.processor() or "unknown"


def _parse_version_fields(version_text: str) -> tuple[str, str]:
    compiler = "unknown"
    build_type = "unknown"
    for line in version_text.splitlines():
        stripped = line.strip()
        if stripped.startswith("compiler"):
            compiler = stripped.split(":", 1)[1].strip() if ":" in stripped else stripped
        elif stripped.startswith("build type"):
            build_type = stripped.split(":", 1)[1].strip() if ":" in stripped else stripped
    return compiler, build_type


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", required=True)
    parser.add_argument("--input", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--milestone", default="M0")
    parser.add_argument("--task", default="T002")
    parser.add_argument("--mode", default="parse_init", choices=["parse_init", "encode"])
    parser.add_argument("--qp", type=int, default=32)
    parser.add_argument("--threads", type=int, default=1)
    parser.add_argument("--preset", type=int, default=4)
    parser.add_argument("--no-deterministic", dest="deterministic", action="store_false")
    parser.add_argument("--log", default=None)
    parser.set_defaults(deterministic=True)
    args = parser.parse_args(argv)

    if not os.path.isfile(args.app):
        print("BLOCKED: app not found: %s" % args.app)
        return EXIT_BLOCKED
    if not os.path.isfile(args.input):
        print("BLOCKED: input not found: %s" % args.input)
        return EXIT_BLOCKED
    if args.mode != "parse_init":
        print("BLOCKED: M0 benchmark only supports mode=parse_init; encode mode arrives with M1")
        return EXIT_BLOCKED

    try:
        width, height, bit_depth, chroma, frames = _parse_y4m(args.input)
    except ValueError as exc:
        print("FAIL: cannot parse input Y4M: %s" % exc)
        return EXIT_FAIL

    version_text, _ = _app_version(args.app)
    compiler, build_type = _parse_version_fields(version_text or "")
    snapshot = _git_commit(os.path.dirname(os.path.abspath(args.app)))
    if snapshot:
        snapshot_obj = {"type": "git", "id": snapshot}
    else:
        snapshot_obj = {"type": "non-git", "id": "app-sha256:" + _sha256_file(args.app)}

    log_path = args.log or (args.out + ".log")
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)

    # M0 metadata-only run: no --output, so the app parses and reports.
    command = [args.app, "--input", args.input]
    start = time.perf_counter()
    try:
        result = subprocess.run(command, capture_output=True, timeout=120)
        wall = time.perf_counter() - start
        exit_code = result.returncode
        combined = (result.stdout + result.stderr).decode("utf-8", "replace")
        status = "PASS" if exit_code == 0 else "FAIL"
    except subprocess.TimeoutExpired:
        wall = None
        exit_code = None
        combined = "TIMEOUT"
        status = "FAIL"

    with open(log_path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("command: %s\n" % " ".join(command))
        fh.write("exit: %s\n" % exit_code)
        fh.write("---- output ----\n")
        fh.write(combined)

    record = {
        "schema_version": SCHEMA_VERSION,
        "milestone": args.milestone,
        "task": args.task,
        "mode": args.mode,
        "snapshot": snapshot_obj,
        "environment": {
            "os": "%s %s" % (platform.system(), platform.release()),
            "cpu": _cpu_name(),
            "compiler": compiler,
            "compiler_version": compiler,
            "build_type": build_type,
            "python": platform.python_version(),
        },
        "input": {
            "path": args.input,
            "sha256": _sha256_file(args.input),
            "width": width,
            "height": height,
            "bit_depth": bit_depth,
            "chroma": chroma,
            "frame_count": frames,
        },
        "config": {
            "qp": args.qp,
            "threads": args.threads,
            "preset": args.preset,
            "deterministic": args.deterministic,
        },
        "timing": {
            "wall_time_s": wall,
            "cpu_time_s": None,
            "cpu_time_reason": "not collected in M0 (no portable child CPU-time API used)",
            "peak_rss_bytes": None,
            "rss_reason": "not collected in M0 (no psutil/resource available; no dependency allowed)",
        },
        "encoded": {
            "encoded_frames": None,
            "fps": None,
            "reason": "M0 performs no encoding; only Y4M metadata parsing and API init are exercised",
        },
        "status": status,
        "correctness": {
            "checked": False,
            "detail": "M0: no bitstream and no pixel correctness; metadata parse only",
        },
        "logs": {"path": log_path},
    }

    with open(args.out, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(record, fh, indent=2, sort_keys=True)
        fh.write("\n")

    print("wrote %s (status=%s, wall=%.6fs)" % (args.out, status, wall if wall else -1))
    return EXIT_OK if status == "PASS" else EXIT_FAIL


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
