"""Replay a verified Windows batch with a WSL encoder and compare complete stream/recon hashes."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import subprocess

from m53_batch import atomic_json, digest, execute


def linux_path(path):
    absolute = Path(path).resolve().as_posix()
    if len(absolute) < 3 or absolute[1:3] != ':/':
        raise ValueError('expected an absolute Windows drive path')
    return '/mnt/' + absolute[0].lower() + absolute[2:]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--kind', choices=['GCC', 'Clang', 'SAN'], required=True)
    parser.add_argument('--out-dir', type=Path, required=True)
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--linux-app', help='optional private WSL executable snapshot')
    parser.add_argument('--resume', action='store_true')
    args = parser.parse_args()
    if (args.out_dir.exists() and not args.resume) or args.jobs < 1:
        parser.error('choose a fresh output directory and positive jobs')
    out = args.out_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    meta = json.loads((args.baseline / 'run-manifest.json').read_text())
    rows = json.loads((args.baseline / 'acceptance.json').read_text())
    assert rows and all(r['status'] == 'PASS' for r in rows)
    for source in meta['sources']:
        assert digest(source['path']) == source['sha256']
    app = args.linux_app or f'/root/svt-av2-m53/{args.kind}/bin/SvtAv2EncApp'
    prefix = ['wsl', '-d', 'ARXChem-Ubuntu-22.04-WSL1', '--']
    app_hash = subprocess.check_output(prefix + ['sha256sum', app], text=True).split()[0]
    frozen = {'kind': args.kind, 'app': app, 'app_sha256': app_hash,
              'baseline_manifest_sha256': digest(args.baseline / 'run-manifest.json')}
    if (out / 'manifest.json').exists():
        assert json.loads((out / 'manifest.json').read_text()) == frozen, 'cross-platform resume fingerprint mismatch'
    else:
        atomic_json(out / 'manifest.json', frozen)
    atomic_json(out / 'summary.json', {'status': 'RUNNING', 'kind': args.kind})
    def check(item):
        index, row = item
        job = row['job']
        folder = out / str(index)
        folder.mkdir(exist_ok=True)
        stream, recon = folder / 'output.av2', folder / 'recon.yuv'
        result_path = folder / 'result.json'
        if result_path.exists():
            try:
                cached = json.loads(result_path.read_text())
                if (cached['status'] == 'PASS' and cached['job'] == job and stream.exists() and
                        cached['stream_sha256'] == row['stream_sha256'] == digest(stream) and
                        cached['recon_sha256'] == row['recon_sha256'] and
                        Path(cached['log']).exists() and digest(cached['log']) == cached['log_sha256']):
                    return cached
            except (OSError, ValueError, KeyError, TypeError):
                pass
        command = prefix + ['env', 'ASAN_OPTIONS=detect_leaks=1:halt_on_error=1',
            'UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1', app,
            '--input', linux_path(meta['sources'][job['source']]['path']),
            '--output', linux_path(stream), '--export-recon', linux_path(recon),
            '--qp', str(job['qp']), '--pred-structure', job['prediction'],
            '--intra-select', 'adaptive' if job['intra'] else 'dc',
            '--partition-select', 'adaptive' if job['partition'] else 'fixed']
        record = execute(command, folder / 'encode.log', 86400, out)
        record.update(job=job, stream_sha256=digest(stream), recon_sha256=digest(recon))
        assert record['stream_sha256'] == row['stream_sha256'], (args.kind, job, 'stream')
        assert record['recon_sha256'] == row['recon_sha256'], (args.kind, job, 'recon')
        record['status'] = 'PASS'
        record['log_sha256'] = digest(folder / 'encode.log')
        recon.unlink()
        atomic_json(result_path, record)
        return record
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(check, enumerate(rows)))
    assert app_hash == subprocess.check_output(prefix + ['sha256sum', app], text=True).split()[0]
    atomic_json(out / 'acceptance.json', results)
    atomic_json(out / 'summary.json', {'status': 'PASS', 'kind': args.kind, 'jobs': len(results)})
    print(args.kind, len(results), 'PASS')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
