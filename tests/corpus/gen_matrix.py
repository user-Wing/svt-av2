#!/usr/bin/env python3
"""Deterministic M2 material matrix generator (T014).

Owner: T014 (tests/corpus). Scope: M2 verification material only; this tool
never touches codec code. It extends the M1 corpus (gen_corpus.py) with the
patterns, sizes and frame counts the M2 milestone needs, and writes a manifest
that pins geometry, format, frame count, QP, seed and source hash per case.

Design rules:
  * Fixed seeds. Regenerating must yield byte-identical files and therefore an
    identical manifest; check_matrix.py enforces that.
  * Every frame of a multi-frame case MUST differ from every other frame. This
    is what lets the matrix catch first-frame duplication, frame misordering
    and cross-frame context pollution; a static pattern would pass a broken
    multi-frame encoder silently. check_matrix.py verifies this property.
  * Stdlib only, no third-party dependency.

Usage:
    python gen_matrix.py [--out DIR] [--sizes 64x64,176x144] [--frames 1,2,3,17]
    python gen_matrix.py --out DIR --list-cases
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import random
import sys

MANIFEST_VERSION = 1
MATRIX_VERSION = 1
CHROMA = "420"
BIT_DEPTH = 8
QP = 100
SEED_BASE = 0x5EED

# Sizes are the M2 DoD sizes in ascending cost order. --sizes can override.
M2_SIZES = ["64x64", "176x144", "1280x720", "1920x1080"]
# M2 DoD frame counts.
M2_FRAMES = [1, 2, 3, 17]

# Patterns required by the T014 task book. Each takes (w, h, frame, seed).
PATTERNS = [
    "flat_black",
    "flat_grey",
    "flat_white",
    "gradient",
    "checkerboard",
    "noise",
    "chroma_stress",
    "thin_lines",
]


def _clip(value: int) -> int:
    return 0 if value < 0 else (255 if value > 255 else value)


def _flat(w: int, h: int, frame: int, level: int) -> tuple[bytes, bytes, bytes]:
    """Constant luma at `level`; chroma neutral. Identical across frames by
    design -- these are the degenerate controls, not the multi-frame cases."""
    return bytes([level]) * (w * h), bytes([128]) * ((w // 2) * (h // 2)), bytes([128]) * ((w // 2) * (h // 2))


def _gradient(w: int, h: int, frame: int, seed: int) -> tuple[bytes, bytes, bytes]:
    """Diagonal ramp, offset per frame so multi-frame cases differ."""
    y = bytearray(w * h)
    shift = frame * 7
    denom = max(1, (w - 1) + (h - 1))
    for j in range(h):
        for i in range(w):
            y[j * w + i] = ((i + j + shift) * 255) // denom % 256
    cw, ch = w // 2, h // 2
    u = bytearray(cw * ch)
    v = bytearray(cw * ch)
    for j in range(ch):
        for i in range(cw):
            u[j * cw + i] = ((i + shift) * 255) // max(1, cw - 1) % 256 if cw > 1 else shift % 256
            v[j * cw + i] = ((j + shift) * 255) // max(1, ch - 1) % 256 if ch > 1 else shift % 256
    return bytes(y), bytes(u), bytes(v)


def _checkerboard(w: int, h: int, frame: int, seed: int) -> tuple[bytes, bytes, bytes]:
    """8x8 luma checkerboard with a per-frame luma DC offset.

    The phase flips every frame AND the luma level is offset by the frame index,
    so all frames of a 17-frame case stay distinct. A pure 0/1 phase flip would
    make frames 0/2/4... identical and hide frame duplication.
    """
    block = 8
    phase = frame
    delta = frame
    y = bytearray(w * h)
    for j in range(h):
        for i in range(w):
            base = 235 if ((i // block) + (j // block) + phase) % 2 == 0 else 16
            y[j * w + i] = _clip(base + delta)
    cw, ch = w // 2, h // 2
    u = bytearray(cw * ch)
    v = bytearray(cw * ch)
    for j in range(ch):
        for i in range(cw):
            u[j * cw + i] = _clip(128 + (24 if phase % 2 else -24) + delta)
            v[j * cw + i] = _clip(128 - (24 if phase % 2 else -24) - delta)
    return bytes(y), bytes(u), bytes(v)


def _noise(w: int, h: int, frame: int, seed: int) -> tuple[bytes, bytes, bytes]:
    """Full-range pseudo-random noise. Seed derives from the case seed + frame,
    so frames differ and regeneration is exact."""
    rng = random.Random(seed + frame)
    y = bytes(rng.randrange(0, 256) for _ in range(w * h))
    cw, ch = w // 2, h // 2
    u = bytes(rng.randrange(0, 256) for _ in range(cw * ch))
    v = bytes(rng.randrange(0, 256) for _ in range(cw * ch))
    return y, u, v


def _chroma_stress(w: int, h: int, frame: int, seed: int) -> tuple[bytes, bytes, bytes]:
    """Neutral-ish luma with saturated chroma ramps. Exercises the chroma path
    independently of luma, which a luma-only gradient cannot do.

    All three planes carry a per-frame term with no short period, so a 17-frame
    case keeps every frame distinct.
    """
    y = bytearray(w * h)
    for j in range(h):
        for i in range(w):
            y[j * w + i] = _clip(128 + ((i + j + frame) % 16) - 8)
    cw, ch = w // 2, h // 2
    u = bytearray(cw * ch)
    v = bytearray(cw * ch)
    span = max(1, cw - 1)
    for j in range(ch):
        for i in range(cw):
            u[j * cw + i] = _clip(((i * 255) // span) + frame)
            v[j * cw + i] = _clip((255 - ((j * 255) // max(1, ch - 1))) - frame if ch > 1 else 255 - frame * 3)
    return bytes(y), bytes(u), bytes(v)


def _thin_lines(w: int, h: int, frame: int, seed: int) -> tuple[bytes, bytes, bytes]:
    """1-pixel lines. Worst case for the transform/DC-only prediction path:
    high-frequency content that a DC_PRED + flat model cannot represent.

    The phase term alone has period 4, so it would make frames 0/4/8/... identical
    in a 17-frame case and hide duplication. A linear per-frame level shift that
    never repeats inside the frame count keeps every frame distinct.
    """
    offset = frame % 4
    delta = frame * 5
    y = bytearray(w * h)
    for j in range(h):
        for i in range(w):
            base = 235 if (i + offset) % 4 == 0 or (j + offset) % 4 == 0 else 16
            y[j * w + i] = _clip(base + delta if base == 16 else base - delta)
    cw, ch = w // 2, h // 2
    u = bytearray([_clip(128 + delta)] * (cw * ch))
    v = bytearray([_clip(128 - delta)] * (cw * ch))
    return bytes(y), bytes(u), bytes(v)


def _flat_black(w, h, frame, seed):
    return _flat(w, h, frame, 0)


def _flat_grey(w, h, frame, seed):
    return _flat(w, h, frame, 128)


def _flat_white(w, h, frame, seed):
    return _flat(w, h, frame, 255)


GENERATORS = {
    "flat_black": _flat_black,
    "flat_grey": _flat_grey,
    "flat_white": _flat_white,
    "gradient": _gradient,
    "checkerboard": _checkerboard,
    "noise": _noise,
    "chroma_stress": _chroma_stress,
    "thin_lines": _thin_lines,
}

# Patterns whose frames are identical by construction (degenerate controls).
STATIC_PATTERNS = {"flat_black", "flat_grey", "flat_white"}


def parse_size(text: str) -> tuple[int, int]:
    width, _, height = text.lower().partition("x")
    w, h = int(width), int(height)
    if w <= 0 or h <= 0:
        raise ValueError("size must be positive: %s" % text)
    if w % 2 or h % 2:
        raise ValueError("8-bit 4:2:0 requires even dimensions: %s" % text)
    return w, h


def expected_raw_size(width: int, height: int, frames: int) -> int:
    return (width * height + 2 * (width // 2) * (height // 2)) * frames


def case_id(pattern: str, width: int, height: int, frames: int) -> str:
    return "%s_%dx%d_%df_%s" % (pattern, width, height, frames, CHROMA)


def case_seed(pattern: str, width: int, height: int, frames: int) -> int:
    """Stable per-case seed derived from the case identity, so adding a case
    never changes an existing case's bytes."""
    digest = hashlib.sha256(("%s|%d|%d|%d" % (pattern, width, height, frames)).encode("ascii")).digest()
    return SEED_BASE ^ int.from_bytes(digest[:4], "big")


def _frame_bytes(pattern: str, w: int, h: int, frames: int, seed: int) -> bytes:
    gen = GENERATORS[pattern]
    out = bytearray()
    for frame in range(frames):
        y, u, v = gen(w, h, frame, seed)
        out += b"FRAME\n" + y + u + v
    return bytes(out)


def _frame_hashes(pattern: str, w: int, h: int, frames: int, seed: int) -> list[str]:
    gen = GENERATORS[pattern]
    return [hashlib.sha256(b"".join(gen(w, h, f, seed))).hexdigest() for f in range(frames)]


def build_cases(sizes: list[tuple[int, int]], frame_counts: list[int], patterns: list[str]) -> list[dict]:
    cases = []
    for pattern in patterns:
        for (w, h) in sizes:
            for frames in frame_counts:
                cases.append({
                    "id": case_id(pattern, w, h, frames),
                    "pattern": pattern,
                    "width": w,
                    "height": h,
                    "frames": frames,
                    "seed": case_seed(pattern, w, h, frames),
                })
    return cases


def generate(out_dir: str, cases: list[dict]) -> dict:
    os.makedirs(out_dir, exist_ok=True)
    entries = []
    for case in cases:
        pattern, w, h, frames = case["pattern"], case["width"], case["height"], case["frames"]
        seed = case["seed"]
        header = ("YUV4MPEG2 W%d H%d F25:1 Ip A1:1 C%s\n" % (w, h, CHROMA)).encode("ascii")
        data = header + _frame_bytes(pattern, w, h, frames, seed)
        filename = case["id"] + ".y4m"
        with open(os.path.join(out_dir, filename), "wb") as fh:
            fh.write(data)

        frame_hashes = _frame_hashes(pattern, w, h, frames, seed)
        if pattern not in STATIC_PATTERNS and frames > 1:
            if len(set(frame_hashes)) != frames:
                raise ValueError(
                    "case %s: frames are not all distinct; a multi-frame case that "
                    "repeats a frame cannot detect duplication/misordering" % case["id"])

        entries.append({
            "id": case["id"],
            "path": filename,
            "pattern": pattern,
            "width": w,
            "height": h,
            "bit_depth": BIT_DEPTH,
            "chroma": CHROMA,
            "frames": frames,
            "qp": QP,
            "seed": seed,
            "distinct_frames": len(set(frame_hashes)),
            "frame_sha256": frame_hashes,
            "y4m_bytes": len(data),
            "expected_raw_bytes": expected_raw_size(w, h, frames),
            "sha256": hashlib.sha256(data).hexdigest(),
        })

    manifest = {
        "manifest_version": MANIFEST_VERSION,
        "matrix_version": MATRIX_VERSION,
        "generator": "tests/corpus/gen_matrix.py",
        "qp": QP,
        "bit_depth": BIT_DEPTH,
        "chroma": CHROMA,
        "sizes": sorted({"%dx%d" % (c["width"], c["height"]) for c in cases}),
        "frame_counts": sorted({c["frames"] for c in cases}),
        "patterns": sorted({c["pattern"] for c in cases}),
        "entries": entries,
    }
    return manifest


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", default=os.path.dirname(os.path.abspath(__file__)))
    parser.add_argument("--sizes", default="64x64,176x144",
                        help="comma-separated WxH list (default: the two fast M2 sizes)")
    parser.add_argument("--frames", default="1,2,3,17",
                        help="comma-separated frame counts")
    parser.add_argument("--patterns", default=",".join(PATTERNS))
    parser.add_argument("--list-cases", action="store_true",
                        help="print the case ids that would be generated, write nothing")
    args = parser.parse_args(argv)

    sizes = [parse_size(s.strip()) for s in args.sizes.split(",") if s.strip()]
    frame_counts = [int(f.strip()) for f in args.frames.split(",") if f.strip()]
    patterns = [p.strip() for p in args.patterns.split(",") if p.strip()]
    for pattern in patterns:
        if pattern not in GENERATORS:
            parser.error("unknown pattern: %s" % pattern)
    for frames in frame_counts:
        if frames <= 0:
            parser.error("frame counts must be positive")

    cases = build_cases(sizes, frame_counts, patterns)
    if args.list_cases:
        for case in cases:
            print(case["id"])
        print("total: %d case(s)" % len(cases), file=sys.stderr)
        return 0

    manifest = generate(args.out, cases)
    with open(os.path.join(args.out, "matrix.json"), "w", encoding="utf-8", newline="\n") as fh:
        json.dump(manifest, fh, indent=2, sort_keys=True)
        fh.write("\n")

    print("cases=%d sizes=%s frames=%s"
          % (len(manifest["entries"]), manifest["sizes"], manifest["frame_counts"]))
    print("manifest: %s" % os.path.join(args.out, "matrix.json"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
