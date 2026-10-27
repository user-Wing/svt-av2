#!/usr/bin/env python3
"""Exercise the public CLI and compare every reconstructed frame with AVM.

Source pixels are deliberately varied; their exact values are not expected to
survive lossy coding. Decoder geometry and actual per-frame reconstruction are
the acceptance criteria. An ASan/LSan build can use the same continuous run.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path

from run_matrix import _parse_y4m_header
from array import array
from compare_recon import plane_layout, frame_size
from verify_m2 import write_input as write_m2_input


def write_input(path, width, height, frames, bit_depth, chroma):
    if bit_depth == 8:
        return write_m2_input(path, width, height, frames)
    with path.open("wb") as out:
        out.write(f"YUV4MPEG2 W{width} H{height} F25:1 Ip C{chroma}p10\n".encode())
        for frame in range(frames):
            out.write(b"FRAME\n")
            for plane, (_, w, h) in enumerate(plane_layout(width, height, chroma)):
                for y in range(h):
                    if frame % 4 == 0:
                        # Extremes and values around 8-bit / midpoint boundaries.
                        values = (0, 1, 255, 256, 511, 512, 1022, 1023)
                        row = [values[(frame // 4 + plane + x // 8 + y // 8) % 8]
                               for x in range(w)]
                    elif frame % 4 == 1:
                        row = [(x * 13 + y * 21 + frame * 53 + plane * 283) % 1024
                               for x in range(w)]
                    elif frame % 4 == 2:
                        row = [900 if (x // 8 + y // 8 + frame + plane) % 2 else 123
                               for x in range(w)]
                    else:
                        row = [((x * 1103515245 + y * 12345 + frame * 7919
                                 + plane * 104729) ^ (x * y * 97)) % 1024
                               for x in range(w)]
                    if frames >= 1000 and plane == 0 and y < 16:
                        for x in range(min(w, 64)):
                            row[x] = 120 + 240 * ((frame >> (2 * (x // 8))) & 3)
                    samples = array("H", row)
                    if sys.byteorder != "little":
                        samples.byteswap()
                    out.write(samples.tobytes())


def run(command, timeout, report):
    start = time.monotonic()
    proc = subprocess.run([str(item) for item in command], capture_output=True,
                          text=True, errors="replace", timeout=timeout)
    report.append({"command": [str(item) for item in command],
                   "exit": proc.returncode, "seconds": time.monotonic() - start,
                   "stdout": proc.stdout[-4000:], "stderr": proc.stderr[-4000:]})
    if proc.returncode:
        detail = proc.stderr[-2000:] or proc.stdout[-2000:]
        raise RuntimeError(f"exit {proc.returncode}: {command[0]}\n{detail}")
    return proc


def unpack_y4m(path, raw, width, height, frames, bit_depth, chroma):
    size = frame_size(width, height, bit_depth, chroma)
    hashes = []
    with path.open("rb") as src, raw.open("wb") as out:
        header = src.readline().decode("ascii").strip()
        if _parse_y4m_header(header) != (width, height, chroma, bit_depth):
            raise RuntimeError(f"decoder geometry mismatch: {header}")
        while True:
            marker = src.readline()
            if not marker:
                break
            if marker.strip() != b"FRAME":
                raise RuntimeError(f"unexpected Y4M frame marker: {marker!r}")
            data = src.read(size)
            if len(data) != size:
                raise RuntimeError("truncated decoded frame")
            if bit_depth == 10:
                samples = array("H")
                samples.frombytes(data)
                if sys.byteorder != "little":
                    samples.byteswap()
                if max(samples, default=0) > 1023:
                    raise RuntimeError("decoder 10-bit sample exceeds 1023 (check representation)")
            hashes.append(hashlib.sha256(data).hexdigest())
            out.write(data)
    if len(hashes) != frames:
        raise RuntimeError(f"decoder frames {len(hashes)} != {frames}")
    return header, hashes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--formats", default="8:420,10:420,10:444")
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--decoder", type=Path)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--sizes", default="176x144,1280x720,1920x1080")
    parser.add_argument("--frames", default="1,2,3,17")
    parser.add_argument("--timeout", type=int, default=7200)
    parser.add_argument("--encode-only", action="store_true",
                        help="Run an instrumented app without claiming decoder acceptance")
    args = parser.parse_args()
    if not args.encode_only and args.decoder is None:
        parser.error("--decoder is required unless --encode-only")
    args.out_dir.mkdir(parents=True, exist_ok=True)
    results = []
    failed = False
    for fmt in args.formats.split(","):
        bit_depth_text, chroma = fmt.split(":")
        bit_depth = int(bit_depth_text)
        if (bit_depth, chroma) not in ((8, "420"), (10, "420"), (10, "444")):
            parser.error("supported formats: 8:420,10:420,10:444")
        for geometry in args.sizes.split(","):
            width, height = map(int, geometry.split("x"))
            if width <= 0 or height <= 0 or width % 2 or height % 2:
                parser.error("sizes must be positive even 4:2:0 dimensions")
            for frames in map(int, args.frames.split(",")):
                case = args.out_dir / f"{bit_depth}bit{chroma}_{geometry}_{frames}f"
                case.mkdir(exist_ok=True)
                report = {"width": width, "height": height, "frames": frames,
                          "bit_depth": bit_depth, "chroma": chroma, "commands": []}
                results.append(report)
                source, stream, recon = (case / name for name in
                                         ("input.y4m", "stream.av2", "internal.yuv"))
                try:
                    # Refuse stale artifacts rather than accepting another build's output.
                    if stream.exists() or recon.exists():
                        raise RuntimeError("output already exists; choose a fresh --out-dir")
                    write_input(source, width, height, frames, bit_depth, chroma)
                    run([args.app, "--input", source, "--output", stream, "--export-recon",
                         recon, "--frames", frames], args.timeout, report["commands"])
                    expected = frame_size(width, height, bit_depth, chroma) * frames
                    if not stream.stat().st_size or recon.stat().st_size != expected:
                        raise RuntimeError("empty bitstream or wrong internal reconstruction size")
                    report["stream_sha256"] = hashlib.sha256(stream.read_bytes()).hexdigest()
                    if args.encode_only:
                        report["status"] = "ENCODE_PASS_DECODER_NOT_CHECKED"
                    else:
                        decoded, raw = case / "decoded.y4m", case / "decoded.yuv"
                        run([args.decoder, "--summary", "-o", decoded, stream],
                            args.timeout, report["commands"])
                        header, hashes = unpack_y4m(decoded, raw, width, height, frames, bit_depth, chroma)
                        report.update(decoder_header=header, frame_sha256=hashes)
                        required_unique = frames if frames >= 1000 else min(frames, 3)
                        if len(set(hashes)) < required_unique:
                            raise RuntimeError("distinct inputs produced repeated reconstruction frames")
                        proc = run([sys.executable, Path(__file__).with_name("compare_recon.py"),
                                    "--a", recon, "--b", raw, "--width", width,
                                    "--height", height, "--frames", frames,
                                    "--bit-depth", bit_depth, "--chroma", chroma, "--json"],
                                   args.timeout, report["commands"])
                        report["compare"] = json.loads(proc.stdout)
                        report["status"] = "PASS"
                except (OSError, RuntimeError, subprocess.TimeoutExpired) as exc:
                    report.update(status="FAIL", error=str(exc))
                    failed = True
                (args.out_dir / "acceptance.json").write_text(
                    json.dumps(results, indent=2), encoding="utf-8")
                print(f"{bit_depth}bit{chroma} {geometry} {frames}f: {report['status']}", flush=True)
    return int(failed)


if __name__ == "__main__":
    sys.exit(main())
