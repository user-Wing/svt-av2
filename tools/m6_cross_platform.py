"""Replay every M6 batch job with a frozen native WSL encoder and compare hashes."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import subprocess

from m53_batch import atomic_json, digest, execute, run_lock
from m53_cross_platform import linux_path
from audit_m6_batch import audit


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--out-dir', type=Path, required=True)
    parser.add_argument('--linux-app', required=True)
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--timeout', type=int, default=14400)
    parser.add_argument('--resume', action='store_true')
    args = parser.parse_args()
    if args.jobs < 1 or args.timeout < 1 or (args.out_dir.exists() and not args.resume):
        parser.error('positive jobs/timeout and fresh output or --resume required')
    audit(args.baseline.resolve())
    out = args.out_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    prefix = ['wsl', '-d', 'ARXChem-Ubuntu-22.04-WSL1', '--']
    def native_sha():
        return subprocess.check_output(prefix + ['sha256sum', args.linux_app], text=True).split()[0]
    frozen = {'linux_app': args.linux_app, 'app_sha256': native_sha(),
              'baseline_manifest_sha256': digest(args.baseline / 'run-manifest.json'),
              'script_sha256': digest(Path(__file__))}
    with run_lock(out / 'run.lock'):
        manifest = out / 'manifest.json'
        if manifest.exists():
            assert json.loads(manifest.read_text()) == frozen, 'replay fingerprint mismatch'
        else:
            atomic_json(manifest, frozen)
        atomic_json(out / 'summary.json', {'status': 'RUNNING'})
        meta = json.loads((args.baseline / 'run-manifest.json').read_text())
        rows = json.loads((args.baseline / 'acceptance.json').read_text())
        def replay(item):
            index, ref = item
            folder = out / str(index)
            folder.mkdir(exist_ok=True)
            result_path = folder / 'result.json'
            stream, recon, log = folder / 'output.av2', folder / 'recon.yuv', folder / 'encode.log'
            job = ref['job']
            if result_path.exists():
                try:
                    old = json.loads(result_path.read_text())
                    if (old['status'] == 'PASS' and old['job'] == job and
                            old['stream_sha256'] == ref['stream_sha256'] == digest(stream) and
                            old['recon_sha256'] == ref['recon_sha256'] and old['log_sha256'] == digest(log)):
                        return old
                except (OSError, ValueError, KeyError, TypeError):
                    pass
            command = prefix + ['env', 'ASAN_OPTIONS=detect_leaks=1:halt_on_error=1',
                'UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1', args.linux_app,
                '--input', linux_path(meta['sources'][job['source']]['path']),
                '--output', linux_path(stream), '--export-recon', linux_path(recon),
                '--qp', str(job['qp']), '--pred-structure', job['prediction'],
                '--intra-select', 'adaptive' if job['intra'] else 'dc',
                '--partition-select', 'adaptive' if job['partition'] else 'fixed',
                '--rd-mode', 'off' if job['rd_mode'] == 'm5' else job['rd_mode']]
            result = execute(command, log, args.timeout, out)
            result.update(job=job, stream_sha256=digest(stream), recon_sha256=digest(recon),
                          log_sha256=digest(log))
            assert result['stream_sha256'] == ref['stream_sha256'], (index, job, 'stream')
            assert result['recon_sha256'] == ref['recon_sha256'], (index, job, 'recon')
            result['status'] = 'PASS'
            recon.unlink()
            atomic_json(result_path, result)
            return result
        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            results = list(pool.map(replay, enumerate(rows)))
        assert native_sha() == frozen['app_sha256']
        atomic_json(out / 'acceptance.json', results)
        atomic_json(out / 'summary.json', {'status': 'PASS', 'jobs': len(results)})
        print('cross-platform PASS', len(results), flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
