"""Re-encode every frozen M4 AI/P fixture with the current default path."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--out-dir', type=Path, required=True)
    parser.add_argument('--jobs', type=int, default=6)
    args = parser.parse_args()
    if args.out_dir.exists():
        parser.error('choose a fresh output directory')
    args.out_dir.mkdir(parents=True)
    cases = []
    for name, prediction in [('all-intra-final', 'all-intra'), ('matrix-r13', 'low-delay-p')]:
        base = ROOT / 'artifacts/m4-root' / name
        records = json.loads((base / 'acceptance.json').read_text())
        for record in records:
            label = f"{record['bit_depth']}bit{record['chroma']}_{record['width']}x{record['height']}_{record['frames']}f"
            if prediction == 'low-delay-p':
                label += '_' + record['pattern']
            cases.append((base / label, prediction))

    def check(item):
        source, prediction = item
        folder = args.out_dir / (prediction + '-' + source.name)
        folder.mkdir()
        stream_name = 'stream.av2' if prediction == 'all-intra' else 'low-delay-p.av2'
        recon_name = 'internal.yuv' if prediction == 'all-intra' else 'low-delay-p.yuv'
        command = list(map(str, [args.app, '--input', source / 'input.y4m',
            '--output', folder / 'stream.av2', '--export-recon', folder / 'internal.yuv',
            '--pred-structure', prediction]))
        p = subprocess.run(command, capture_output=True, timeout=7200)
        (folder / 'encode.log').write_bytes(p.stdout + p.stderr)
        result = {'case': folder.name, 'command': command, 'exit': p.returncode}
        if p.returncode == 0:
            result['stream_sha256'] = sha(folder / 'stream.av2')
            result['recon_sha256'] = sha(folder / 'internal.yuv')
            result['frozen_stream_sha256'] = sha(source / stream_name)
            result['frozen_recon_sha256'] = sha(source / recon_name)
            result['status'] = 'PASS' if (
                result['stream_sha256'] == result['frozen_stream_sha256'] and
                result['recon_sha256'] == result['frozen_recon_sha256']) else 'FAIL'
        else:
            result['status'] = 'FAIL'
        print(result['case'], result['status'], flush=True)
        return result

    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(check, cases))
    (args.out_dir / 'acceptance.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
    return int(any(x['status'] != 'PASS' for x in results))


if __name__ == '__main__':
    raise SystemExit(main())
