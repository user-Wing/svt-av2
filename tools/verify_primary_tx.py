"""Force the binary primary TX subset against the frozen AVM decoder."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--emitter', type=Path, required=True)
    p.add_argument('--decoder', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--jobs', type=int, default=4)
    a = p.parse_args()
    a.out.mkdir(parents=True, exist_ok=False)
    command = [str(a.emitter.resolve()), str(a.out.resolve())]
    with (a.out / 'emit.log').open('wb') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
    result = dict(emitter_sha256=sha(a.emitter), decoder_sha256=sha(a.decoder),
                  emit_command=command, rows=[], status='RUNNING')

    def check(stream):
        raw = stream.with_suffix('.decoded.yuv')
        depth = 8 if stream.name.startswith('f0-') else 10
        cmd = [str(a.decoder.resolve()), '--threads=1', '--rawvideo', f'--output-bit-depth={depth}',
               '-o', str(raw.resolve()), str(stream.resolve())]
        with stream.with_suffix('.decode.log').open('wb') as log:
            subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=30)
        expected = stream.with_suffix('.yuv')
        if raw.read_bytes() != expected.read_bytes():
            raise ValueError('fixed AVM mismatch: ' + str(stream))
        return dict(case=stream.stem, command=cmd, bytes=stream.stat().st_size,
                    stream_sha256=sha(stream), recon_sha256=sha(expected),
                    decoded_sha256=sha(raw), decoded_bytes=raw.stat().st_size)

    try:
        with ThreadPoolExecutor(max_workers=a.jobs) as pool:
            for row in pool.map(check, sorted(a.out.glob('*.av2'))):
                result['rows'].append(row)
                if len(result['rows']) % 300 == 0:
                    print('verified', len(result['rows']), flush=True)
        if len(result['rows']) != 3 * 3 * 2 * 2 * 13 * 7:
            raise ValueError('incomplete forced TX matrix')
        result['status'] = 'PASS'
    except Exception as exc:
        result.update(status='FAIL', error=str(exc))
        raise
    finally:
        (a.out / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print('PASS', len(result['rows']), 'fixed AVM streams')


if __name__ == '__main__':
    main()
