"""Force rectangular DCT coding leaves and compare the fixed AVM pixels."""
import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--emitter', type=Path, required=True)
    parser.add_argument('--decoder', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    frozen = args.out / ('emitter' + args.emitter.suffix)
    shutil.copy2(args.emitter, frozen)
    command = [str(frozen.resolve()), str(args.out.resolve())]
    with (args.out / 'emit.log').open('wb') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
    streams = sorted(args.out.glob('*.av2'))
    if len(streams) != 3*4*2*2*7*7*2*2:
        raise ValueError('incomplete forced rectangle matrix')

    def decode(stream):
        decoded = stream.with_suffix('.decoded.yuv')
        depth = 8 if stream.name.startswith('f0-') else 10
        command = [str(args.decoder.resolve()), '--rawvideo', f'--output-bit-depth={depth}',
                   '-o', str(decoded.resolve()), str(stream.resolve())]
        with stream.with_suffix('.decode.log').open('wb') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=30)
        expected = stream.with_suffix('.yuv')
        if decoded.read_bytes() != expected.read_bytes():
            raise ValueError('fixed AVM mismatch: ' + str(stream))
        return {'case': stream.stem, 'command': command, 'bytes': stream.stat().st_size,
                'stream_sha256': sha(stream), 'recon_sha256': sha(expected),
                'decoded_sha256': sha(decoded), 'decoded_bytes': decoded.stat().st_size}

    # Fail early on one complete 64x64 DC block set before queueing the matrix.
    rows = [decode(stream) for stream in streams[:4]]
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        rows += list(pool.map(decode, streams[4:]))
    result = {'status': 'PASS', 'emitter_sha256': sha(frozen), 'decoder_sha256': sha(args.decoder),
              'emit_command': command, 'rows': rows}
    (args.out / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf8')
    print('PASS', len(rows), 'fixed AVM rectangle streams')


if __name__ == '__main__':
    main()
