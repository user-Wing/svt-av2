"""Bounded deterministic stream/reconstruction/decision differential checks."""
import argparse
from array import array
import hashlib
import json
from pathlib import Path
import re
import subprocess
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def guard(status):
    state = json.loads(status.read_text())
    if state.get('stage') == 'isolated-speed':
        raise RuntimeError('isolated-speed active; computation paused')


def run(command, folder, name, status):
    guard(status)
    start = time.perf_counter()
    result = subprocess.run([str(x) for x in command], capture_output=True, timeout=120)
    log = folder / (name + '.log')
    log.write_bytes(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError('command failed: ' + str(log))
    return dict(command=[str(x) for x in command], seconds=time.perf_counter() - start,
                log=str(log), log_sha256=digest(log))


def source(path, width, height, depth, chroma):
    with path.open('wb') as output:
        ctag = '420jpeg' if depth == 8 else chroma + 'p10'
        output.write(f'YUV4MPEG2 W{width} H{height} F24:1 Ip A1:1 C{ctag}\n'.encode())
        for frame in range(3):
            output.write(b'FRAME\n')
            for plane in range(3):
                w = width if plane == 0 or chroma == '444' else width // 2
                h = height if plane == 0 or chroma == '444' else height // 2
                values = [((x * 13 + y * 19 + ((x + frame * 3) // 8) * 47 +
                            ((y - frame * 2) // 8) * 29 + plane * 71 +
                            (frame == 2) * ((x * y) % 127)) % 256) << (depth - 8)
                          for y in range(h) for x in range(w)]
                output.write(bytes(values) if depth == 8 else array('H', values).tobytes())


def trace(path):
    return [{k: int(v) for k, v in re.findall(r'(\w+)=(\d+)', line)}
            for line in path.read_text().splitlines() if line.startswith('rd frame=')]


def decoded_raw(path, size):
    raw = bytearray()
    with path.open('rb') as decoded:
        if not decoded.readline().startswith(b'YUV4MPEG2 '):
            raise ValueError('invalid decoded header')
        for _ in range(3):
            if not decoded.readline().startswith(b'FRAME'):
                raise ValueError('missing decoded frame')
            frame = decoded.read(size)
            if len(frame) != size:
                raise ValueError('truncated decoded frame')
            raw += frame
        if decoded.read(1):
            raise ValueError('extra decoded data')
    return raw


def main():
    parser = argparse.ArgumentParser(__doc__)
    for key in ['baseline', 'candidate', 'm5', 'decoder', 'status', 'out']:
        parser.add_argument('--' + key, type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    results = dict(status='RUNNING', ignored_execution_counters=['forward'],
                   binaries={k: dict(path=str(getattr(args, k)), sha256=digest(getattr(args, k)))
                             for k in ['baseline', 'candidate', 'm5', 'decoder']}, cases=[])
    results['adjacent_dlls'] = {str(p): digest(p) for key in ['baseline', 'candidate', 'm5']
                               for p in getattr(args, key).parent.glob('*.dll')}
    target = args.out / 'result.json'
    try:
        for fmt_index, (depth, chroma) in enumerate([(8, '420'), (10, '420'), (10, '444')]):
            for geom_index, (width, height, qp) in enumerate([(64, 64, 1), (128, 64, 100), (72, 72, 255)]):
                src = args.out / f'{width}x{height}-{depth}-{chroma}.y4m'
                source(src, width, height, depth, chroma)
                for pred_index, prediction in enumerate(['all-intra', 'low-delay-p']):
                    tools = (fmt_index + geom_index + pred_index) % 4
                    for mode in ['off', 'exhaustive', 'pruned']:
                        name = f'{depth}-{chroma}-{width}x{height}-{prediction}-{tools}-{mode}'
                        folder = args.out / name
                        folder.mkdir()
                        item = dict(name=name, qp=qp, source_sha256=digest(src), commands=[])
                        for label, app in [('baseline', args.baseline), ('candidate', args.candidate)]:
                            command = [app, '--input', src, '--output', folder / (label + '.av2'),
                                '--export-recon', folder / (label + '.yuv'), '--qp', qp,
                                '--pred-structure', prediction, '--intra-select', 'adaptive' if tools & 1 else 'dc',
                                '--partition-select', 'adaptive' if tools & 2 else 'fixed', '--rd-mode', mode]
                            item['commands'].append(run(command, folder, label, args.status))
                        for suffix in ['av2', 'yuv']:
                            if digest(folder / ('baseline.' + suffix)) != digest(folder / ('candidate.' + suffix)):
                                raise ValueError(name + ': ' + suffix + ' mismatch')
                        before, after = trace(folder / 'baseline.log'), trace(folder / 'candidate.log')
                        semantic = lambda rows: [{k: v for k, v in row.items() if k != 'forward'} for row in rows]
                        if semantic(before) != semantic(after):
                            raise ValueError(name + ': decision trace mismatch')
                        if any(b['forward'] < a['forward'] for b, a in zip(before, after)):
                            raise ValueError(name + ': forward transform count increased')
                        if len(before) != (0 if mode == 'off' else 3):
                            raise ValueError(name + ': missing trace')
                        item.update(baseline_trace=before, candidate_trace=after,
                                    stream_sha256=digest(folder / 'candidate.av2'),
                                    recon_sha256=digest(folder / 'candidate.yuv'))
                        if mode == 'off':
                            command[0] = args.m5
                            command[command.index('--output') + 1] = folder / 'm5.av2'
                            command[command.index('--export-recon') + 1] = folder / 'm5.yuv'
                            item['commands'].append(run(command[:-2], folder, 'm5', args.status))
                            if any(digest(folder / ('m5.' + s)) != digest(folder / ('candidate.' + s)) for s in ['av2', 'yuv']):
                                raise ValueError(name + ': M5/off mismatch')
                        decoded = folder / 'decoded.y4m'
                        item['commands'].append(run([args.decoder, '--summary', '-o', decoded, folder / 'candidate.av2'], folder, 'decode', args.status))
                        size = width * height * (3 if chroma == '444' else 3 / 2) * (1 if depth == 8 else 2)
                        if decoded_raw(decoded, int(size)) != (folder / 'candidate.yuv').read_bytes():
                            raise ValueError(name + ': oracle reconstruction mismatch')
                        item['status'] = 'PASS'
                        results['cases'].append(item)
                        target.write_text(json.dumps(results, indent=2) + '\n')
        results['status'] = 'PASS'
    except Exception as error:
        results.update(status='FAIL', error=str(error))
        raise
    finally:
        target.write_text(json.dumps(results, indent=2) + '\n')
    print(f"PASS: {len(results['cases'])} cases")


if __name__ == '__main__':
    main()
