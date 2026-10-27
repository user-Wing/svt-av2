#!/usr/bin/env python3
"""Verify the deterministic Y4M test corpus against its manifest.

Owner: T002. Checks, for every manifest entry:
  * the file exists and its byte size matches the recorded value,
  * its SHA256 matches (this is what makes corpus regeneration meaningful),
  * the Y4M header geometry/chroma/frames match the manifest,
  * expected_raw_bytes equals the tightly packed 8-bit 4:2:0 formula.

Exit code 0 = all entries valid, 1 = at least one mismatch, 2 = usage error.
No third-party dependencies.
"""

from __future__ import annotations

import hashlib
import json
import os
import sys


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _expected_raw_size(width: int, height: int, frames: int) -> int:
    return (width * height + 2 * (width // 2) * (height // 2)) * frames


def _parse_y4m(data: bytes):
    """Returns (width, height, chroma, frames) or raises ValueError."""
    newline = data.find(b"\n")
    if newline < 0:
        raise ValueError("no Y4M header newline")
    header = data[:newline].decode("ascii", "strict")
    if not header.startswith("YUV4MPEG2"):
        raise ValueError("not a YUV4MPEG2 stream")
    fields = header.split()[1:]
    width = height = None
    chroma = "420"
    for field in fields:
        if field.startswith("W"):
            width = int(field[1:])
        elif field.startswith("H"):
            height = int(field[1:])
        elif field.startswith("C"):
            chroma = field[1:]
    if width is None or height is None:
        raise ValueError("missing W/H in Y4M header")
    # Walk frames sequentially using the tight 8-bit 4:2:0 payload size so the
    # count cannot be fooled by payload bytes that happen to spell "FRAME".
    raw_frame = width * height + 2 * (width // 2) * (height // 2)
    pos = newline + 1
    frames = 0
    while pos < len(data):
        tag_end = data.find(b"\n", pos)
        if tag_end < 0 or not data[pos:tag_end].startswith(b"FRAME"):
            raise ValueError("bad frame tag at offset %d" % pos)
        pos = tag_end + 1 + raw_frame
        frames += 1
    if pos != len(data):
        raise ValueError("frame payload truncated at end of file")
    return width, height, chroma, frames


def check(corpus_dir: str) -> int:
    manifest_path = os.path.join(corpus_dir, "manifest.json")
    if not os.path.isfile(manifest_path):
        print("FAIL: manifest not found: %s" % manifest_path)
        return 1
    with open(manifest_path, "r", encoding="utf-8") as fh:
        manifest = json.load(fh)

    failures = 0
    for entry in manifest.get("entries", []):
        path = os.path.join(corpus_dir, entry["path"])
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
            width, height, chroma, frames = _parse_y4m(data)
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
        if frames != entry["frames"]:
            print("FAIL: %s frames %d != manifest %d" % (entry["path"], frames, entry["frames"]))
            failures += 1
        expected = _expected_raw_size(entry["width"], entry["height"], entry["frames"])
        if expected != entry["expected_raw_bytes"]:
            print("FAIL: %s expected_raw_bytes %d != formula %d"
                  % (entry["path"], entry["expected_raw_bytes"], expected))
            failures += 1

    total = len(manifest.get("entries", []))
    if failures == 0:
        print("manifest OK: %d entries verified" % total)
        return 0
    print("manifest FAILED: %d problem(s) over %d entries" % (failures, total))
    return 1


def main(argv: list[str]) -> int:
    corpus_dir = argv[0] if argv else os.path.dirname(os.path.abspath(__file__))
    return check(corpus_dir)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
