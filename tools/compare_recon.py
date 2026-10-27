#!/usr/bin/env python3
"""compare_recon.py - bit-exact per-frame / per-plane comparison of raw YUV.

M3 scope: tightly packed 8-bit 4:2:0 and 10-bit 4:2:0 / 4:4:4.
High-bit-depth samples are unsigned 16-bit little-endian, low ten bits valid.
Strided/padded files and other configurations are rejected explicitly.

--a is the expected (reference) file, --b is the actual (tested) file.

Output: concise text by default or with --text; --json emits a fixed JSON
document on stdout. The two are mutually exclusive.

Exit codes:
    0  files are byte-identical over the declared layout
    1  pixel mismatch (lengths were valid)
    2  argument / usage error (bad dimensions, unsupported format)
    3  file or length error (missing path, truncated, extra bytes)

This tool never crops, resamples, converts colour or applies a tolerance.
"""

import argparse
from array import array
import hashlib
import json
import os
import sys

SCHEMA_VERSION = 1
LAYOUT = "yuv420p8"

EXIT_MATCH = 0
EXIT_MISMATCH = 1
EXIT_ARG = 2
EXIT_FILE = 3

CHUNK = 1 << 20


class CompareError(Exception):
    """A failure that maps to a specific status / exit code."""

    def __init__(self, status, message, exit_code):
        super().__init__(message)
        self.status = status
        self.message = message
        self.exit_code = exit_code


def plane_layout(width, height, chroma="420"):
    """Return visible plane dimensions in samples."""
    cw, ch = (width // 2, height // 2) if chroma == "420" else (width, height)
    return (("Y", width, height), ("U", cw, ch), ("V", cw, ch))


def frame_size(width, height, bit_depth=8, chroma="420"):
    return sum(w * h for _, w, h in plane_layout(width, height, chroma)) * (1 if bit_depth == 8 else 2)


def locate(offset, fsize, planes, bytes_per_sample=1):
    """Map a byte offset to (frame, plane, x, y) within that plane."""
    frame = offset // fsize
    within = offset - frame * fsize
    for name, w, h in planes:
        psize = w * h * bytes_per_sample
        if within < psize:
            sample = within // bytes_per_sample
            return frame, name, sample % w, sample // w
        within -= psize
    raise AssertionError("offset outside frame")  # pragma: no cover


def file_sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(CHUNK), b""):
            h.update(chunk)
    return h.hexdigest()


def validate_args(args):
    if args.width <= 0 or args.height <= 0:
        raise CompareError(
            "argument_error",
            "width and height must be positive (got %dx%d)" % (args.width, args.height),
            EXIT_ARG,
        )
    if args.chroma == "420" and (args.width % 2 or args.height % 2):
        raise CompareError(
            "argument_error",
            "4:2:0 requires even width and height (got %dx%d)" % (args.width, args.height),
            EXIT_ARG,
        )
    if args.frames <= 0:
        raise CompareError(
            "argument_error", "frames must be >= 1 (got %d)" % args.frames, EXIT_ARG
        )
    if (args.bit_depth, args.chroma) not in ((8, "420"), (10, "420"), (10, "444")):
        raise CompareError("argument_error", "supported: 8-bit420, 10-bit420, 10-bit444", EXIT_ARG)


def base_result(args):
    return {
        "schema_version": SCHEMA_VERSION,
        "status": None,
        "match": False,
        "a": args.path_a,
        "b": args.path_b,
        "width": args.width,
        "height": args.height,
        "frames": args.frames,
        "layout": LAYOUT if args.bit_depth == 8 else "yuv%sp10le" % args.chroma,
        "expected_bytes": None,
        "a_bytes": None,
        "b_bytes": None,
        "first_mismatch": None,
        "mismatch_counts": {"Y": 0, "U": 0, "V": 0, "total": 0},
        "sha256": {"a": None, "b": None},
        "error": None,
    }


def run_compare(args):
    validate_args(args)

    for path in (args.path_a, args.path_b):
        if not os.path.isfile(path):
            raise CompareError("file_error", "file not found: %s" % path, EXIT_FILE)

    planes = plane_layout(args.width, args.height, args.chroma)
    bps = 1 if args.bit_depth == 8 else 2
    fsize = frame_size(args.width, args.height, args.bit_depth, args.chroma)
    expected = fsize * args.frames

    result = base_result(args)
    result["expected_bytes"] = expected

    size_a = os.path.getsize(args.path_a)
    size_b = os.path.getsize(args.path_b)
    result["a_bytes"] = size_a
    result["b_bytes"] = size_b

    # Length check first: identical hashes must not rescue a wrong length.
    if size_a != expected or size_b != expected:
        result["status"] = "length_error"
        result["sha256"]["a"] = file_sha256(args.path_a)
        result["sha256"]["b"] = file_sha256(args.path_b)
        result["error"] = (
            "length mismatch: expected %d bytes per side; a=%d b=%d"
            % (expected, size_a, size_b)
        )
        return result, EXIT_FILE

    ha = hashlib.sha256()
    hb = hashlib.sha256()
    first = None
    counts = {"Y": 0, "U": 0, "V": 0, "total": 0}
    offset = 0

    with open(args.path_a, "rb") as fa, open(args.path_b, "rb") as fb:
        while True:
            ca = fa.read(CHUNK)
            cb = fb.read(CHUNK)
            if not ca and not cb:
                break
            ha.update(ca)
            hb.update(cb)
            if bps == 2:
                va, vb = array("H"), array("H")
                va.frombytes(ca)
                vb.frombytes(cb)
                if sys.byteorder != "little":
                    va.byteswap()
                    vb.byteswap()
                if max(va, default=0) > 1023 or max(vb, default=0) > 1023:
                    raise CompareError("sample_error", "10-bit sample exceeds 1023", EXIT_FILE)
            else:
                va, vb = ca, cb
            if ca != cb:
                for i, (expected_sample, actual_sample) in enumerate(zip(va, vb)):
                    if expected_sample == actual_sample:
                        continue
                    counts["total"] += 1
                    byte_offset = offset + i * bps
                    frame, plane, x, y = locate(byte_offset, fsize, planes, bps)
                    counts[plane] += 1
                    if first is None:
                        first = {"frame": frame, "plane": plane, "x": x, "y": y,
                                 "a_offset": byte_offset, "b_offset": byte_offset,
                                 "expected": expected_sample, "actual": actual_sample}
            offset += len(ca)

    result["sha256"]["a"] = ha.hexdigest()
    result["sha256"]["b"] = hb.hexdigest()
    result["mismatch_counts"] = counts
    result["first_mismatch"] = first

    if counts["total"] == 0:
        result["status"] = "match"
        result["match"] = True
        return result, EXIT_MATCH

    result["status"] = "mismatch"
    result["match"] = False
    return result, EXIT_MISMATCH


def format_text(result):
    head = "compare_recon: %s a=%s b=%s %dx%d %s frames=%d" % (
        result["status"].upper(),
        result["a"],
        result["b"],
        result["width"],
        result["height"],
        result["layout"],
        result["frames"],
    )
    lines = [head]
    if result["status"] == "length_error":
        lines.append("  %s" % result["error"])
        lines.append("  expected_bytes=%d a_bytes=%d b_bytes=%d" % (
            result["expected_bytes"], result["a_bytes"], result["b_bytes"]))
    elif result["status"] in ("mismatch",):
        m = result["first_mismatch"]
        lines.append(
            "  first mismatch frame=%d plane=%s x=%d y=%d offset=%d expected=0x%02x actual=0x%02x"
            % (m["frame"], m["plane"], m["x"], m["y"], m["a_offset"],
               m["expected"], m["actual"]))
        c = result["mismatch_counts"]
        lines.append("  counts Y=%d U=%d V=%d total=%d" % (
            c["Y"], c["U"], c["V"], c["total"]))
    elif result["error"]:
        lines.append("  %s" % result["error"])
    return "\n".join(lines)


def build_parser():
    p = argparse.ArgumentParser(
        description="Bit-exact per-frame/per-plane raw YUV comparison (8-bit420 / 10-bit420 / 10-bit444, tightly packed)."
    )
    p.add_argument("--a", dest="path_a", required=True,
                   help="expected / reference raw file")
    p.add_argument("--b", dest="path_b", required=True,
                   help="actual / tested raw file")
    p.add_argument("--width", type=int, required=True)
    p.add_argument("--height", type=int, required=True)
    p.add_argument("--frames", type=int, required=True)
    p.add_argument("--bit-depth", dest="bit_depth", type=int, default=8,
                   help="8 or 10 (default 8)")
    p.add_argument("--chroma", default="420",
                   help="420, or 444 with 10-bit (default 420)")
    fmt = p.add_mutually_exclusive_group()
    fmt.add_argument("--json", action="store_true",
                     help="emit a fixed JSON document on stdout")
    fmt.add_argument("--text", action="store_true",
                     help="emit concise text (default)")
    return p


def main(argv=None):
    parser = build_parser()
    args = parser.parse_args(argv)

    try:
        result, code = run_compare(args)
    except CompareError as exc:
        result = base_result(args)
        result["status"] = exc.status
        result["error"] = exc.message
        code = exc.exit_code

    if args.json:
        json.dump(result, sys.stdout, indent=2)
        sys.stdout.write("\n")
    else:
        if result["status"] in ("argument_error", "file_error", "length_error"):
            sys.stderr.write(format_text(result) + "\n")
        else:
            sys.stdout.write(format_text(result) + "\n")
    return code


if __name__ == "__main__":
    sys.exit(main())
