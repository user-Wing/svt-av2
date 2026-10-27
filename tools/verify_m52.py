"""Independent partition fixtures, rational SSE model and four-way round trips."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from fractions import Fraction
import json
from pathlib import Path
import subprocess

import numpy as np
from m4_stream import inspect_stream
from verify_m3 import unpack_y4m
from verify_m51 import digest, invoke, quality


def generate(path, width, height, frames, depth, chroma, pattern):
    dtype = np.dtype('u1' if depth == 8 else '<u2')
    scale, maximum = 1 << (depth - 8), (1 << depth) - 1
    token = '420jpeg' if depth == 8 else chroma + 'p10'
    plans = []
    with path.open('wb') as f:
        f.write(f'YUV4MPEG2 W{width} H{height} F25:1 Ip C{token}\n'.encode())
        for frame in range(frames):
            f.write(b'FRAME\n')
            for plane in range(3):
                divisor = 2 if plane and chroma == '420' else 1
                y, x = np.indices((height // divisor, width // divisor), dtype=np.int64)
                if pattern == 'flat':
                    pixels = np.full_like(x, (128 + frame % 7) * scale)
                elif plane:
                    pixels = ((x * 3 + y * 5 + frame * 11 + plane * 31) * scale) & maximum
                else:
                    q = ((x % 64) // 32) ^ ((y % 64) // 32)
                    ramp = (x % 32) if frame % 2 else (y % 32)
                    pixels = ((32 + q * 128 + ramp + frame * 7) * scale) & maximum
                    if pattern == 'mixed':
                        keep = ((x // 64) + (y // 64) + frame // 3) % 2 == 0
                        pixels = np.where(keep, 128 * scale, pixels)
                f.write(pixels.astype(dtype).tobytes())
                if plane == 0:
                    decisions = []
                    for by in range(0, height, 64):
                        for bx in range(0, width, 64):
                            block = pixels[by:by+64, bx:bx+64]
                            if block.shape != (64, 64):
                                decisions.append(0)
                                continue
                            def sse(a):
                                return int((a*a).sum()) - Fraction(int(a.sum()) ** 2, a.size)
                            gain = sse(block) - sum(sse(block[y:y+32, x:x+32])
                                for y in (0, 32) for x in (0, 32))
                            decisions.append(int(gain > 4096 * 16 * scale * scale))
                    plans.append(decisions)
    return plans


def check_case(args, item):
    depth, chroma, width, height, frames, prediction, pattern = item
    label = f'{depth}bit{chroma}_{width}x{height}_{frames}f_{prediction}_{pattern}'
    folder = args.out_dir / label
    folder.mkdir()
    source = folder / 'input.y4m'
    result = {'case': label, 'bit_depth': depth, 'chroma': chroma, 'width': width,
              'height': height, 'frames': frames, 'prediction': prediction, 'pattern': pattern}
    try:
        plans = generate(source, width, height, frames, depth, chroma, pattern)
        (folder / 'expected-plans.json').write_text(json.dumps(plans), encoding='utf-8')
        result.update(source_sha256=digest(source), split_roots=sum(map(sum, plans)))
        for p in (0, 1):
            for i in (0, 1):
                mode = f'p{p}i{i}'
                stream, recon, decoded, raw = [folder / (mode + suffix) for suffix in
                    ['.av2', '.yuv', '.decoded.y4m', '.decoded.yuv']]
                encode = [args.app, '--input', source, '--output', stream, '--export-recon', recon,
                    '--pred-structure', prediction, '--partition-select', 'adaptive' if p else 'fixed',
                    '--intra-select', 'adaptive' if i else 'dc']
                commands = [invoke(encode, folder, mode + '-encode', args.timeout)]
                frozen = digest(recon)
                commands.append(invoke([args.decoder, '--summary', '-o', decoded, stream],
                                       folder, mode + '-decode', args.timeout))
                header, hashes = unpack_y4m(decoded, raw, width, height, frames, depth, chroma)
                assert digest(raw) == frozen == digest(recon), f'{mode}: reconstruction mismatch'
                syntax = inspect_stream(stream.read_bytes(), frames, prediction == 'low-delay-p')
                result[mode] = {'stream_sha256': digest(stream), 'recon_sha256': frozen,
                    'frame_sha256': hashes, 'bytes': stream.stat().st_size, 'commands': commands,
                    'syntax': syntax, 'decoder_header': header,
                    'quality': quality(source, recon, width, height, frames, depth, chroma)}
                if not p and args.baseline:
                    invoke([args.baseline, '--input', source, '--output', folder / f'm51i{i}.av2',
                        '--export-recon', folder / f'm51i{i}.yuv', '--pred-structure', prediction,
                        '--intra-select', 'adaptive' if i else 'dc'], folder, mode + '-baseline', args.timeout)
                    assert digest(folder / f'm51i{i}.av2') == result[mode]['stream_sha256'], 'M51 fixed tree changed'
                    assert digest(folder / f'm51i{i}.yuv') == frozen, 'M51 fixed recon changed'
        for i in (0, 1):
            changed = result[f'p0i{i}']['stream_sha256'] != result[f'p1i{i}']['stream_sha256']
            result[f'partition_changed_i{i}'] = changed
            if not result['split_roots']:
                assert not changed, 'no split requested but fixed-tree bytes changed'
            else:
                assert changed, 'partition feature did not change a controlled stream'
        result['status'] = 'PASS'
    except (AssertionError, RuntimeError, OSError, ValueError, subprocess.TimeoutExpired) as exc:
        result.update(status='FAIL', error=str(exc))
    (folder / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(label, result['status'], result.get('error', ''), flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--decoder', type=Path, required=True)
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--out-dir', type=Path, required=True)
    parser.add_argument('--sizes', default='64x64,176x144')
    parser.add_argument('--frames', default='1,2,3,17')
    parser.add_argument('--patterns', default='flat,quads,mixed')
    parser.add_argument('--predictions', default='all-intra,low-delay-p')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--timeout', type=int, default=7200)
    args = parser.parse_args()
    if args.out_dir.exists():
        parser.error('choose a fresh output directory')
    args.out_dir.mkdir(parents=True)
    items = [(depth, chroma, *map(int, size.split('x')), int(frames), pred, pattern)
        for depth, chroma in [(8, '420'), (10, '420'), (10, '444')]
        for size in args.sizes.split(',') for frames in args.frames.split(',')
        for pred in args.predictions.split(',') for pattern in args.patterns.split(',')]
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(lambda item: check_case(args, item), items))
    (args.out_dir / 'acceptance.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
    return int(any(x['status'] != 'PASS' for x in results))


if __name__ == '__main__':
    raise SystemExit(main())
