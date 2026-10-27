#!/usr/bin/env python3
"""Verify the M2 material matrix against its manifest (T014).

Owner: T014 (tests/corpus). Checks, for every manifest entry:
  * the file exists, its byte size and SHA256 match the manifest,
  * the Y4M header geometry/chroma and the real frame count match,
  * the raw size equals the tightly packed 8-bit 4:2:0 formula,
  * multi-frame cases really contain distinct frames (per-frame SHA256), so a
    static or first-frame-duplicated stream cannot masquerade as multi-frame,
  * the declared per-frame hashes match the file's actual frames.

Exit code 0 = all entries valid, 1 = at least one mismatch, 2 = usage error.
No third-party dependencies.
"""

from __future__ import annotations

import hashlib
import json
import os
import sys

# Frames of these patterns are identical by construction (degenerate controls).
STATIC_PATTERNS = {"flat_black", "flat_grey", "flat_white"}


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _expected_raw_size(width: int, height: int, frames: int) -> int:
    return (width * height + 2 * (width // 2) * (height // 2)) * frames


def _parse_y4m(data: bytes):
    """Returns (width, height, chroma, [frame_payloads])."""
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
        raise ValueError("missing W/H in Y4M header")

    raw_frame = width * height + 2 * (width // 2) * (height // 2)
    pos = newline + 1
    payloads = []
    while pos < len(data):
        tag_end = data.find(b"\n", pos)
        if tag_end < 0 or not data[pos:tag_end].startswith(b"FRAME"):
            raise ValueError("bad frame tag at offset %d" % pos)
        start = tag_end + 1
        end = start + raw_frame
        if end > len(data):
            raise ValueError("frame payload truncated at end of file")
        payloads.append(data[start:end])
        pos = end
    return width, height, chroma, payloads


def check(matrix_dir: str, strict_frames: bool = True) -> int:
    manifest_path = os.path.join(matrix_dir, "matrix.json")
    if not os.path.isfile(manifest_path):
        print("FAIL: matrix manifest not found: %s" % manifest_path)
        return 1
    with open(manifest_path, "r", encoding="utf-8") as fh:
        manifest = json.load(fh)

    failures = 0
    checked_static = 0
    checked_dynamic = 0
    for entry in manifest.get("entries", []):
        path = os.path.join(matrix_dir, entry["path"])
        if not os.path.isfile(path):
            print("FAIL: missing file %s" % entry["path"])
            failures += 1
            continue
        with open(path, "rb") as fh:
            data = fh.read()

        if len(data) != entry["y4m_bytes"]:
            print("FAIL: %s size %d != manifest %d" % (entry["path"], len(data), entry["y4m_bytes"]))
            failures += 1
        actual_hash = _sha256(data)
        if actual_hash != entry["sha256"]:
            print("FAIL: %s sha256 %s != manifest %s" % (entry["path"], actual_hash, entry["sha256"]))
            failures += 1

        try:
            width, height, chroma, payloads = _parse_y4m(data)
        except ValueError as exc:
            print("FAIL: %s unparsable: %s" % (entry["path"], exc))
            failures += 1
            continue

        if (width, height) != (entry["width"], entry["height"]):
            print("FAIL: %s geometry %dx%d != manifest %dx%d"
                  % (entry["path"], width, height, entry["width"], entry["height"]))
            failures += 1
        if chroma != entry["chroma"]:
            print("FAIL: %s chroma %s != manifest %s" % (entry["path"], chroma, entry["chroma"]))
            failures += 1
        if len(payloads) != entry["frames"]:
            print("FAIL: %s frames %d != manifest %d" % (entry["path"], len(payloads), entry["frames"]))
            failures += 1
        expected = _expected_raw_size(entry["width"], entry["height"], entry["frames"])
        if expected != entry["expected_raw_bytes"]:
            print("FAIL: %s expected_raw_bytes %d != formula %d"
                  % (entry["path"], entry["expected_raw_bytes"], expected))
            failures += 1

        frame_hashes = [_sha256(p) for p in payloads]
        declared = entry.get("frame_sha256")
        if declared is not None and frame_hashes != declared:
            print("FAIL: %s per-frame hashes differ from manifest" % entry["path"])
            failures += 1

        pattern = entry.get("pattern")
        if strict_frames and entry["frames"] > 1 and pattern not in STATIC_PATTERNS:
            distinct = len(set(frame_hashes))
            checked_dynamic += 1
            if distinct != entry["frames"]:
                print("FAIL: %s has %d distinct frames out of %d; multi-frame cases "
                      "must differ per frame" % (entry["path"], distinct, entry["frames"]))
                failures += 1
        elif pattern in STATIC_PATTERNS:
            checked_static += 1

    total = len(manifest.get("entries", []))
    if failures == 0:
        print("matrix OK: %d entries verified (%d dynamic multi-frame, %d static controls)"
              % (total, checked_dynamic, checked_static))
        return 0
    print("matrix FAILED: %d problem(s) over %d entries" % (failures, total))
    return 1


def main(argv: list[str]) -> int:
    if argv and argv[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    matrix_dir = argv[0] if argv else os.path.dirname(os.path.abspath(__file__))
    return check(matrix_dir)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
