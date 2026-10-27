"""Frozen bounded coefficient-refinement rate/distortion experiment, one process."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import time

import numpy as np
from m53_batch import bd_rate


CAMPAIGN = Path('C:/Private/ffmpeg-native-m6-tests/campaign-local-r2-20261017')
MEDIA = Path('C:/Private/ffmpeg-native-m6-local-media')
QPS = [80, 100, 140, 220]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def guard():
    if json.loads((CAMPAIGN / 'campaign-status.json').read_text())['stage'] == 'isolated-speed':
        raise RuntimeError('isolated-speed active; stop computation')


def sizes(source):
    return [source['width'] * source['height']] + [source['width'] * source['height'] //
        (4 if source['chroma'] == '420' else 1)] * 2


def read_y4m(path, source):
    size = sum(sizes(source)) * (1 if source['depth'] == 8 else 2)
    with path.open('rb') as stream:
        stream.readline()
        frames = []
        for _ in range(source['frames']):
            if not stream.readline().startswith(b'FRAME'):
                raise ValueError('missing Y4M frame')
            frame = stream.read(size)
            if len(frame) != size:
                raise ValueError('truncated Y4M frame')
            frames.append(frame)
    return frames


def write_y4m(path, source, frames):
    token = '420jpeg' if source['depth'] == 8 else source['chroma'] + 'p10'
    with path.open('wb') as stream:
        stream.write(f"YUV4MPEG2 W{source['width']} H{source['height']} F24000:1001 Ip C{token}\n".encode())
        for frame in frames:
            stream.write(b'FRAME\n' + frame)


def prepare(out):
    out.mkdir(parents=True, exist_ok=False)
    sources = []
    for index, (depth, chroma, pattern) in enumerate([(8, '420', 'edges'), (10, '420', 'motion'), (10, '444', 'noise')]):
        source = dict(name='synthetic-' + pattern, width=128, height=128, depth=depth, chroma=chroma, frames=3, native=False)
        frames = []
        rng = np.random.default_rng(481516 + index)
        for frame in range(3):
            planes = []
            for plane in range(3):
                width = 128 if plane == 0 or chroma == '444' else 64
                y, x = np.indices((width, width))
                if pattern == 'noise':
                    pixels = rng.integers(32, 224, (width, width))
                elif pattern == 'motion':
                    pixels = ((x - frame * 2) * 7 + (y + frame * 2) * 11 + ((x - frame * 2) // 8) * 47 + plane * 31) % 224 + 16
                else:
                    pixels = (32 + x // 2 + y // 4 + ((x + y) % 17 == 0) * 32 + (x >= 64) * 40 + plane * 13) % 224 + 16
                planes.append((pixels << (depth - 8)).astype('u1' if depth == 8 else '<u2').tobytes())
            frames.append(b''.join(planes))
        path = out / (source['name'] + '.y4m')
        write_y4m(path, source, frames)
        source.update(path=str(path), sha256=digest(path), origin='independent deterministic mathematical fixture')
        sources.append(source)
    for native in [False, True]:
        for scene in ['2D', '3D', 'Real']:
            source = dict(name=scene + ('-native1' if native else '-small3'), width=1920 if native else 256,
                          height=1080 if native else 144, depth=10, chroma='420', frames=1 if native else 3, native=native)
            original = MEDIA / ('pilot' if native else 'small') / (scene + '-yuv420p10le.y4m')
            frames = read_y4m(original, source)
            path = out / (source['name'] + '.y4m')
            write_y4m(path, source, frames)
            source.update(path=str(path), sha256=digest(path), origin=str(original), origin_sha256=digest(original))
            sources.append(source)
    manifest = dict(qps=QPS, predictions=['all-intra', 'low-delay-p'], sources=sources,
                    selection='frozen before candidate results; no lambda adjustment; native one frame QP100 P only')
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('prepared', out / 'manifest.json')


def run_command(command, log):
    guard()
    start = time.perf_counter()
    result = subprocess.run([str(x) for x in command], capture_output=True, timeout=240)
    log.write_bytes(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError('failed command: ' + str(log))
    return time.perf_counter() - start


def quality(source, raw):
    dtype = 'u1' if source['depth'] == 8 else '<u2'
    reference = np.frombuffer(b''.join(read_y4m(Path(source['path']), source)), dtype=dtype).astype(np.int64)
    recon = np.frombuffer(raw, dtype=dtype).astype(np.int64)
    if reference.size != recon.size:
        raise ValueError('reconstruction size mismatch')
    counts = sizes(source)
    errors = (reference - recon).reshape(source['frames'], sum(counts))
    sums, offset = [], 0
    for count in counts:
        delta = errors[:, offset:offset + count]
        sums.append(int(np.sum(delta * delta)))
        offset += count
    scale = 1 << (2 * (source['depth'] - 8))
    weight = 4 if source['chroma'] == '420' else 1
    distortion = (4 * sums[0] + weight * (sums[1] + sums[2])) / scale
    psnr = [None if error == 0 else 10 * math.log10(((1 << source['depth']) - 1) ** 2 * count * source['frames'] / error)
            for error, count in zip(sums, counts)]
    return dict(sse_yuv=sums, weighted_D=distortion, psnr_yuv_db=psnr)


def encode(app, source, qp, prediction, mode, folder, decoder, coeff=False):
    folder.mkdir()
    stream, recon = folder / 'stream.av2', folder / 'recon.yuv'
    command = [app, '--input', source['path'], '--output', stream, '--export-recon', recon,
               '--qp', qp, '--pred-structure', prediction, '--intra-select', 'adaptive',
               '--partition-select', 'adaptive', '--rd-mode', mode]
    if coeff:
        command += ['--coeff-opt', 'rd']
    seconds = run_command(command, folder / 'encode.log')
    raw = recon.read_bytes()
    decoded = folder / 'decoded.y4m'
    run_command([decoder, '--summary', '-o', decoded, stream], folder / 'decode.log')
    if b''.join(read_y4m(decoded, source)) != raw:
        raise ValueError('AVM sample reconstruction differs')
    log = (folder / 'encode.log').read_text()
    traces = [{k: int(v) for k, v in re.findall(r'(\w+)=(\d+)', line)} for line in log.splitlines() if line.startswith('rd frame=')]
    coeff_traces = [{k: int(v) for k, v in re.findall(r'(\w+)=(\d+)', line)} for line in log.splitlines() if line.startswith('coeff frame=')]
    if len(traces) != (0 if mode == 'off' else source['frames']):
        raise ValueError('missing RD decision trace')
    if coeff:
        if len(coeff_traces) != source['frames'] or any(r['frame'] != i or r['trials'] < r['accepts']
                for i, r in enumerate(coeff_traces)):
            raise ValueError('missing/invalid coefficient statistics')
    elif coeff_traces:
        raise ValueError('default coeff-off emitted coefficient statistics')
    item = dict(bytes=stream.stat().st_size, stream_sha256=digest(stream), recon_sha256=digest(recon),
                seconds=seconds, quality=quality(source, raw), rd_frames=traces, coeff_frames=coeff_traces)
    (folder / 'result.json').write_text(json.dumps(item, indent=2) + '\n')
    decoded.unlink()
    recon.unlink()
    return item


def experiment(out, baseline, candidate):
    guard()
    manifest = json.loads((out / 'manifest.json').read_text())
    assert manifest['qps'] == QPS
    for source in manifest['sources']:
        if digest(Path(source['path'])) != source['sha256']:
            raise ValueError('frozen source changed')
    decoder = CAMPAIGN / 'snapshot/decoder.exe'
    result = dict(status='RUNNING', manifest_sha256=digest(out / 'manifest.json'),
                  programs={str(p): digest(p) for p in [baseline, candidate, decoder]}, cases=[], compatibility=[])
    target = out / 'result.json'
    try:
        for source in manifest['sources']:
            for prediction in (['low-delay-p'] if source['native'] else manifest['predictions']):
                for qp in ([100] if source['native'] else QPS):
                    folder = out / f"{source['name']}-{prediction}-{qp}"
                    folder.mkdir()
                    pair = dict(source=source['name'], prediction=prediction, qp=qp)
                    pair['baseline'] = encode(baseline, source, qp, prediction, 'pruned', folder / 'baseline', decoder)
                    pair['candidate'] = encode(candidate, source, qp, prediction, 'pruned', folder / 'candidate', decoder, coeff=True)
                    if qp == 100 and (source['name'].startswith('synthetic-') or
                                      (source['name'].endswith('-small3') and prediction == 'low-delay-p')):
                        compat = encode(candidate, source, qp, prediction, 'pruned', folder / 'default-off', decoder)
                        if any(compat[k] != pair['baseline'][k] for k in ['stream_sha256', 'recon_sha256', 'rd_frames']):
                            raise ValueError('default coeff-off compatibility mismatch')
                        result['compatibility'].append(dict(source=source['name'], prediction=prediction, status='PASS'))
                    result['cases'].append(pair)
                    target.write_text(json.dumps(result, indent=2) + '\n')
        result['bd_rate_psnr_y'] = []
        for source in manifest['sources'][:6]:
            for prediction in manifest['predictions']:
                rows = [p for p in result['cases'] if p['source'] == source['name'] and p['prediction'] == prediction]
                curves = [[(p[mode]['quality']['psnr_yuv_db'][0], p[mode]['bytes']) for p in rows] for mode in ['baseline', 'candidate']]
                bd = bd_rate(*curves) if all(q is not None for c in curves for q, _ in c) else dict(percent=None, reason='infinite PSNR')
                result['bd_rate_psnr_y'].append(dict(source=source['name'], prediction=prediction, **bd))
        result['status'] = 'PASS'
    except Exception as error:
        result.update(status='FAIL', error=str(error))
        raise
    finally:
        target.write_text(json.dumps(result, indent=2) + '\n')
    print('PASS', len(result['cases']), 'paired cases')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--candidate', type=Path)
    args = parser.parse_args()
    if args.candidate:
        experiment(args.out, args.baseline, args.candidate)
    else:
        prepare(args.out)
