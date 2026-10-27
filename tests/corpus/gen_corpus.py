#!/usr/bin/env python3
"""Deterministic Y4M test corpus generator for the SVT-AV2 validation harness.

Owner: T002 (tests/corpus). Scope: M0/M1 verification infrastructure only. This
tool produces tiny, byte-reproducible 8-bit 4:2:0 single-frame Y4M inputs plus a
manifest that pins their geometry and SHA256. It does NOT touch codec code.

Regenerating the corpus must yield identical files and therefore identical
manifest hashes; the manifest check in tests/corpus/check_manifest.py enforces
that. Random content uses a fixed seed derived from the pattern name so the
result never depends on the Python version's RNG state beyond random.seed.

Usage:
    python gen_corpus.py [--out DIR] [--width W] [--height H]
                         [--frames N] [--patterns flat,gradient,...]
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import random
import sys

# Provisional M0 development geometry. The M1 fixed geometry is confirmed by
# T004/T005 (docs/m0-m1-contracts.md section 6) and is still open; regenerate
# the corpus with --width/--height once it is frozen. 4:2:0 needs even sizes.
DEFAULT_WIDTH = 64
DEFAULT_HEIGHT = 64
DEFAULT_FRAMES = 1
DEFAULT_PATTERNS = ["flat", "gradient", "checkerboard", "noise"]

MANIFEST_VERSION = 1
CHROMA = "420"
BIT_DEPTH = 8


def _flat(w: int, h: int, frame: int) -> tuple[bytes, bytes, bytes]:
    # Mid-grey luma, neutral chroma. Independent of frame index for frame 0.
    y = bytes([128]) * (w * h)
    c = bytes([128]) * ((w // 2) * (h // 2))
    return y, c, c


def _gradient(w: int, h: int, frame: int) -> tuple[bytes, bytes, bytes]:
    y = bytearray(w * h)
    for j in range(h):
        for i in range(w):
            y[j * w + i] = (i * 255) // max(1, w - 1) if w > 1 else 0
    cw, ch = w // 2, h // 2
    u = bytearray(cw * ch)
    v = bytearray(cw * ch)
    for j in range(ch):
        for i in range(cw):
            u[j * cw + i] = (j * 255) // max(1, ch - 1) if ch > 1 else 0
            v[j * cw + i] = ((i + j) * 255) // max(1, cw + ch - 2) if (cw + ch) > 2 else 0
    return bytes(y), bytes(u), bytes(v)


def _checkerboard(w: int, h: int, frame: int) -> tuple[bytes, bytes, bytes]:
    block = 8
    y = bytearray(w * h)
    for j in range(h):
        for i in range(w):
            y[j * w + i] = 235 if ((i // block) + (j // block)) % 2 == 0 else 16
    cw, ch = w // 2, h // 2
    u = bytearray(cw * ch)
    v = bytearray(cw * ch)
    for j in range(ch):
        for i in range(cw):
            u[j * cw + i] = 128
            v[j * cw + i] = 128
    return bytes(y), bytes(u), bytes(v)


def _noise(w: int, h: int, frame: int) -> tuple[bytes, bytes, bytes]:
    rng = random.Random(0x5EED + frame)
    y = bytes(rng.randrange(0, 256) for _ in range(w * h))
    cw, ch = w // 2, h // 2
    u = bytes(rng.randrange(0, 256) for _ in range(cw * ch))
    v = bytes(rng.randrange(0, 256) for _ in range(cw * ch))
    return y, u, v


PATTERNS = {
    "flat": _flat,
    "gradient": _gradient,
    "checkerboard": _checkerboard,
    "noise": _noise,
}


def _y4m_header(w: int, h: int, frames: int, color_space: str) -> bytes:
    # YUV4MPEG2 W<w> H<h> F25:1 Ip A1:1 C420\n
    header = "YUV4MPEG2 W%d H%d F25:1 Ip A1:1 C%s\n" % (w, h, color_space)
    return header.encode("ascii")


def _frame_bytes(pattern: str, w: int, h: int, frames: int) -> bytes:
    out = bytearray()
    for frame in range(frames):
        y, u, v = PATTERNS[pattern](w, h, frame)
        out += b"FRAME\n"
        out += y + u + v
    return bytes(out)


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def expected_raw_size(w: int, h: int, frames: int) -> int:
    """Tightly packed 8-bit 4:2:0 raw size: W*H + 2*(W/2)*(H/2) per frame."""
    return (w * h + 2 * (w // 2) * (h // 2)) * frames


def generate(out_dir: str, width: int, height: int, frames: int, patterns: list[str]) -> dict:
    if width <= 0 or height <= 0:
        raise ValueError("width/height must be positive")
    if width % 2 != 0 or height % 2 != 0:
        raise ValueError("8-bit 4:2:0 requires even width and height")
    if frames <= 0:
        raise ValueError("frames must be positive")

    os.makedirs(out_dir, exist_ok=True)
    header = _y4m_header(width, height, frames, "420")
    raw_frame = expected_raw_size(width, height, 1)

    entries = []
    for pattern in patterns:
        if pattern not in PATTERNS:
            raise ValueError("unknown pattern: %s" % pattern)
        file_id = "%s_%dx%d_%df_%s" % (pattern, width, height, frames, CHROMA)
        data = header + _frame_bytes(pattern, width, height, frames)
        filename = file_id + ".y4m"
        path = os.path.join(out_dir, filename)
        with open(path, "wb") as fh:
            fh.write(data)

        entries.append({
            "id": file_id,
            "path": filename,
            "width": width,
            "height": height,
            "bit_depth": BIT_DEPTH,
            "chroma": CHROMA,
            "frames": frames,
            "pattern": pattern,
            "seed": (0x5EED if pattern == "noise" else None),
            "y4m_bytes": len(data),
            "expected_raw_bytes": raw_frame * frames,
            "sha256": _sha256(data),
        })

    manifest = {
        "manifest_version": MANIFEST_VERSION,
        "generator": "tests/corpus/gen_corpus.py",
        "width": width,
        "height": height,
        "frames": frames,
        "bit_depth": BIT_DEPTH,
        "chroma": CHROMA,
        "entries": entries,
    }
    return manifest


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", default=os.path.dirname(os.path.abspath(__file__)))
    parser.add_argument("--width", type=int, default=DEFAULT_WIDTH)
    parser.add_argument("--height", type=int, default=DEFAULT_HEIGHT)
    parser.add_argument("--frames", type=int, default=DEFAULT_FRAMES)
    parser.add_argument("--patterns", default=",".join(DEFAULT_PATTERNS))
    args = parser.parse_args(argv)

    patterns = [p.strip() for p in args.patterns.split(",") if p.strip()]
    manifest = generate(args.out, args.width, args.height, args.frames, patterns)

    manifest_path = os.path.join(args.out, "manifest.json")
    with open(manifest_path, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(manifest, fh, indent=2, sort_keys=True)
        fh.write("\n")

    for entry in manifest["entries"]:
        print("%s  %s  raw=%d" % (entry["sha256"], entry["path"], entry["expected_raw_bytes"]))
    print("manifest: %s (%d entries)" % (manifest_path, len(manifest["entries"])))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
