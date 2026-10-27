"""M5.1 independent AI/P on/off matrix with frozen-M4 comparison."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import math
from pathlib import Path
import subprocess
import time

import numpy as np
from m4_stream import inspect_stream
from verify_m3 import unpack_y4m


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def generate(path, width, height, frames, depth, chroma, pattern):
    token = '420jpeg' if depth == 8 else chroma + 'p10'
    dtype = np.dtype('u1' if depth == 8 else '<u2')
    maximum = (1 << depth) - 1
    with path.open('wb') as f:
        f.write(f'YUV4MPEG2 W{width} H{height} F25:1 Ip C{token}\n'.encode())
        for frame in range(frames):
            f.write(b'FRAME\n')
            for plane in range(3):
                divisor = 2 if plane and chroma == '420' else 1
                y, x = np.indices((height // divisor, width // divisor), dtype=np.int32)
                if pattern == 'flat':
                    pixels = np.full_like(x, 1 << (depth - 1))
                elif pattern == 'directional':
                    pixels = (x if frame % 2 else y) * 4 + plane * 37
                else:
                    region = ((x * divisor // 64) + (y * divisor // 64)) % 3
                    fixed = (x * 7 + y * 11 + plane * 31) & maximum
                    axis = (x if frame % 2 else y) * 4 + plane * 37
                    cut = (x * 13 + y * 17 + frame // 3 * 97 + plane * 43) & maximum
                    pixels = np.where(region == 0, fixed, np.where(region == 1, axis, cut))
                f.write((pixels & maximum).astype(dtype).tobytes())


def invoke(command, folder, label, timeout):
    start = time.perf_counter()
    result = subprocess.run(list(map(str, command)), capture_output=True, timeout=timeout)
    (folder / (label + '.log')).write_bytes(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(f'{label}: exit={result.returncode}, {result.stderr[-1000:]!r}')
    return {'argv': list(map(str, command)), 'exit': result.returncode,
            'seconds': time.perf_counter() - start}


def quality(source, recon, width, height, frames, depth, chroma):
    dtype = np.dtype('u1' if depth == 8 else '<u2')
    dims = [width*height] + [width*height // (4 if chroma == '420' else 1)] * 2
    total = sum(dims)
    errors = np.zeros(3, dtype=np.float64)
    decoded = np.frombuffer(recon.read_bytes(), dtype=dtype).reshape(frames, total)
    with source.open('rb') as f:
        f.readline()
        for frame in range(frames):
            assert f.readline().startswith(b'FRAME')
            raw = np.frombuffer(f.read(total * dtype.itemsize), dtype=dtype)
            offset = 0
            for p, count in enumerate(dims):
                diff = raw[offset:offset+count].astype(np.float64) - decoded[frame, offset:offset+count]
                errors[p] += np.sum(diff*diff)
                offset += count
    mse = [float(errors[p] / (frames*dims[p])) for p in range(3)]
    psnr = [None if value == 0 else 10*math.log10(((1 << depth)-1)**2/value) for value in mse]
    return {'mse_yuv': mse, 'psnr_yuv_db': psnr, 'psnr_null_means_infinite': True}


def case(args, item):
    depth, chroma, width, height, frames, prediction, pattern = item
    label = f'{depth}bit{chroma}_{width}x{height}_{frames}f_{prediction}_{pattern}'
    folder = args.out_dir / label
    folder.mkdir()
    source = folder / 'input.y4m'
    result = {'case': label, 'bit_depth': depth, 'chroma': chroma, 'width': width,
              'height': height, 'frames': frames, 'prediction': prediction, 'pattern': pattern}
    try:
        generate(source, width, height, frames, depth, chroma, pattern)
        result['source_sha256'] = digest(source)
        for selection in ['dc', 'adaptive']:
            stream, recon, decoded, raw = [folder / (selection + suffix) for suffix in
                                           ['.av2', '.yuv', '.decoded.y4m', '.decoded.yuv']]
            commands = []
            commands.append(invoke([args.app, '--input', source, '--output', stream,
                '--export-recon', recon, '--pred-structure', prediction,
                '--intra-select', selection], folder, selection + '-encode', args.timeout))
            before = digest(recon)
            commands.append(invoke([args.decoder, '--summary', '-o', decoded, stream],
                folder, selection + '-decode', args.timeout))
            header, frame_hashes = unpack_y4m(decoded, raw, width, height, frames, depth, chroma)
            assert digest(raw) == before == digest(recon), 'internal/decoder reconstruction mismatch'
            syntax = inspect_stream(stream.read_bytes(), frames, prediction == 'low-delay-p')
            result[selection] = {'stream_sha256': digest(stream), 'recon_sha256': before,
                'frame_sha256': frame_hashes, 'bytes': stream.stat().st_size,
                'commands': commands, 'syntax': syntax, 'decoder_header': header,
                'quality': quality(source, recon, width, height, frames, depth, chroma)}
        if args.baseline:
            commands = invoke([args.baseline, '--input', source, '--output', folder / 'm4.av2',
                '--export-recon', folder / 'm4.yuv', '--pred-structure', prediction],
                folder, 'm4-encode', args.timeout)
            assert digest(folder / 'm4.av2') == result['dc']['stream_sha256'], 'M4 off stream changed'
            assert digest(folder / 'm4.yuv') == result['dc']['recon_sha256'], 'M4 off recon changed'
            result['baseline'] = {'off_identical': True, 'command': commands}
        result['adaptive_stream_changed'] = result['dc']['stream_sha256'] != result['adaptive']['stream_sha256']
        result['adaptive_recon_changed'] = result['dc']['recon_sha256'] != result['adaptive']['recon_sha256']
        if pattern == 'flat':
            assert not result['adaptive_stream_changed'] and not result['adaptive_recon_changed']
        if width >= 176 and height >= 144 and pattern in ('directional', 'mixed'):
            assert result['adaptive_stream_changed'], 'controlled feature did not change the stream'
            assert result['adaptive_recon_changed'], 'controlled feature did not change reconstruction'
        result['status'] = 'PASS'
    except (AssertionError, RuntimeError, OSError, subprocess.TimeoutExpired, ValueError) as exc:
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
    parser.add_argument('--patterns', default='flat,directional,mixed')
    parser.add_argument('--predictions', default='all-intra,low-delay-p')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--timeout', type=int, default=7200)
    args = parser.parse_args()
    if args.out_dir.exists():
        parser.error('choose a fresh output directory')
    args.out_dir.mkdir(parents=True)
    items = [(depth, chroma, *map(int, geometry.split('x')), int(frames), prediction, pattern)
             for depth, chroma in [(8, '420'), (10, '420'), (10, '444')]
             for geometry in args.sizes.split(',') for frames in args.frames.split(',')
             for prediction in args.predictions.split(',') for pattern in args.patterns.split(',')]
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(lambda item: case(args, item), items))
    (args.out_dir / 'acceptance.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
    return int(any(result['status'] != 'PASS' for result in results))


if __name__ == '__main__':
    raise SystemExit(main())
