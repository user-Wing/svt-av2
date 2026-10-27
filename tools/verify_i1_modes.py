"""Force all 13x7 Y/UV modes and compare the fixed AVM visible samples."""
import argparse
import hashlib
import json
from pathlib import Path
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
    command = [str(args.emitter.resolve()), str(args.out.resolve())]
    with (args.out/'emit.log').open('wb') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
    result = dict(emitter_sha256=sha(args.emitter), decoder_sha256=sha(args.decoder),
                  emit_command=command, rows=[], status='RUNNING')
    for stream in sorted(args.out.glob('*.av2')):
        raw = stream.with_suffix('.decoded.yuv')
        depth = 8 if stream.name.startswith('f0-') else 10
        cmd = [str(args.decoder.resolve()), '--rawvideo', f'--output-bit-depth={depth}',
               '-o', str(raw.resolve()), str(stream.resolve())]
        with stream.with_suffix('.decode.log').open('wb') as log:
            subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=30)
        expected = stream.with_suffix('.yuv')
        if raw.read_bytes() != expected.read_bytes():
            raise ValueError('fixed AVM mismatch: ' + str(stream))
        result['rows'].append(dict(case=stream.stem, command=cmd, bytes=stream.stat().st_size,
                                  stream_sha256=sha(stream), recon_sha256=sha(expected),
                                  decoded_sha256=sha(raw), decoded_bytes=raw.stat().st_size))
    if len(result['rows']) != 3*6*2*2*13*7:
        raise ValueError('incomplete forced mode matrix')
    result['status'] = 'PASS'
    (args.out/'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print('PASS', len(result['rows']), 'fixed AVM streams')


if __name__ == '__main__':
    main()
