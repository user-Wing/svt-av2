#!/usr/bin/env python3
"""Prove actual inter/MV/MC with independent fixed-decoder counterfactuals.

Frames 2 and 3 are successive normative predictions from the previous frame,
so their residuals must be zero. Replace frame 1 with an independently encoded
different keyframe: both decoded P frames must follow that reference with the
same MV. This also proves the P-to-P reference refresh for nonzero motion.
"""
import argparse
import json
import subprocess
import sys
from pathlib import Path
import numpy as np
from compare_recon import frame_size, file_sha256
from m4_model import texture, predict
from m4_stream import obus, inspect_stream
from verify_m3 import unpack_y4m
from verify_m4 import run, write_frames, raw_planes, frame_bytes, encode, validate_decode


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", required=True, type=Path)
    parser.add_argument("--decoder", required=True, type=Path)
    parser.add_argument("--out-dir", required=True, type=Path)
    parser.add_argument("--timeout", type=int, default=7200)
    args = parser.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    results, failed = [], False
    for bd, chroma in ((8, "420"), (10, "420"), (10, "444")):
        for mv in ((0, 0), (-4, 2), (1, -3), (-3, 1)):
            case = args.out_dir / f"{bd}bit{chroma}_mv{mv[0]}_{mv[1]}"
            case.mkdir(exist_ok=True)
            report = {"bit_depth": bd, "chroma": chroma, "expected_mv": mv, "commands": []}
            results.append(report)
            try:
                width = height = 128
                dims = [(128, 128), (64, 64), (64, 64)] if chroma == "420" else [(128, 128)]*3
                original = [texture(w, h, bd, plane) for plane, (w, h) in enumerate(dims)]
                alternate = [((1 << bd)-1)-plane for plane in original]
                warm, alt = case / "warm", case / "alternate"
                warm.mkdir(exist_ok=True); alt.mkdir(exist_ok=True)
                refs = []
                key_streams = []
                for folder, source_planes in ((warm, original), (alt, alternate)):
                    source = folder / "input.y4m"
                    write_frames(source, width, height, bd, chroma, [source_planes])
                    stream, recon = encode(folder, args.app, source, 1, "low-delay-p", args.timeout, report["commands"])
                    refs.append(raw_planes(recon.read_bytes(), width, height, bd, chroma))
                    key_streams.append(stream)
                expected = [predict(plane, mv, int(p > 0 and chroma == "420"))
                            for p, plane in enumerate(refs[0])]
                expected_third = [predict(plane, mv, int(p > 0 and chroma == "420"))
                                  for p, plane in enumerate(expected)]
                source = case / "input.y4m"
                write_frames(source, width, height, bd, chroma, [original, expected, expected_third])
                stream, recon = encode(case, args.app, source, 3, "low-delay-p", args.timeout, report["commands"])
                report["original"] = validate_decode(case, stream, recon, args.decoder,
                    width, height, 3, bd, chroma, args.timeout, report["commands"])
                size = frame_size(width, height, bd, chroma)
                reconstructed = recon.read_bytes()
                if reconstructed[:size] != frame_bytes(refs[0], bd):
                    raise RuntimeError("warm key reconstruction differs from sequence key")
                if reconstructed[size:] != frame_bytes(expected, bd) + frame_bytes(expected_third, bd):
                    raise RuntimeError("zero residual fixture did not reconstruct exact normative prediction")
                # Header-only boundary parsing is independent of packet writer.
                units = list(obus(stream.read_bytes()))
                tail_offset = next(offset for kind, _, offset in units if kind == 2 and offset > 0 and
                                   any(k == 4 and prev < offset for k, _, prev in units))
                probe = case / "counterfactual.av2"
                probe.write_bytes(key_streams[1].read_bytes() + stream.read_bytes()[tail_offset:])
                inspect_stream(probe.read_bytes(), 3, True)
                decoded, raw = case / "counterfactual.y4m", case / "counterfactual.yuv"
                run([args.decoder, "--summary", "-o", decoded, probe], args.timeout, report["commands"])
                _, hashes = unpack_y4m(decoded, raw, width, height, 3, bd, chroma)
                alternative_expected = [predict(plane, mv, int(p > 0 and chroma == "420"))
                                        for p, plane in enumerate(refs[1])]
                alternative_third = [predict(plane, mv, int(p > 0 and chroma == "420"))
                                     for p, plane in enumerate(alternative_expected)]
                if mv != (0, 0) and frame_bytes(alternative_third, bd) == frame_bytes(alternative_expected, bd):
                    raise RuntimeError("fixture cannot distinguish P refresh from stale reference")
                probe_bytes = raw.read_bytes()
                if probe_bytes[:size] != frame_bytes(refs[1], bd):
                    raise RuntimeError("alternate key reconstruction does not match oracle")
                probe_frame = probe_bytes[size:]
                if probe_frame != frame_bytes(alternative_expected, bd) + frame_bytes(alternative_third, bd):
                    raise RuntimeError("counterfactual P does not use the expected reference/MV/MC")
                if probe_frame == frame_bytes(expected, bd) + frame_bytes(expected_third, bd):
                    raise RuntimeError("counterfactual key did not affect P; inter proof absent")
                report.update(status="PASS", counterfactual_sha256=file_sha256(probe),
                              counterfactual_frame_sha256=hashes,
                              zero_residual_exact=True, independent_reference_mv_proof=True,
                              independent_p_refresh_proof=mv != (0, 0))
            except (OSError, RuntimeError, ValueError, subprocess.TimeoutExpired) as exc:
                report.update(status="FAIL", error=str(exc))
                failed = True
            (args.out_dir / "acceptance.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
            print(f"{case.name}: {report['status']}", flush=True)
    return int(failed)


if __name__ == "__main__":
    sys.exit(main())
