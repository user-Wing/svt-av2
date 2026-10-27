"""Sequential, repeated single-QP measurements of the verified M5.1 inputs."""
import argparse
import hashlib
import json
from pathlib import Path
import statistics

from verify_m4 import run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--input-results', type=Path, required=True)
    parser.add_argument('--out-dir', type=Path, required=True)
    args = parser.parse_args()
    if args.out_dir.exists():
        parser.error('choose a fresh output directory')
    args.out_dir.mkdir(parents=True)
    records = json.loads((args.input_results / 'acceptance.json').read_text())
    results = []
    for case in records:
        assert case['status'] == 'PASS'
        folder = args.out_dir / case['case']
        folder.mkdir()
        result = {'case': case['case'], 'frames': case['frames'], 'modes': {}}
        for selection in ['dc', 'adaptive']:
            commands = []
            for _ in range(3):
                run([args.app, '--input', args.input_results / case['case'] / 'input.y4m',
                     '--output', folder / (selection + '.av2'), '--export-recon', folder / (selection + '.yuv'),
                     '--pred-structure', case['prediction'], '--intra-select', selection], 7200, commands)
                assert hashlib.sha256((folder / (selection + '.av2')).read_bytes()).hexdigest() == case[selection]['stream_sha256']
                assert hashlib.sha256((folder / (selection + '.yuv')).read_bytes()).hexdigest() == case[selection]['recon_sha256']
            median = statistics.median(c['seconds'] for c in commands)
            peaks = [c['peak_rss_bytes'] for c in commands if c['peak_rss_bytes'] is not None]
            result['modes'][selection] = {'median_seconds': median, 'fps': case['frames'] / median,
                'sampled_peak_rss_bytes': max(peaks, default=None), 'commands': commands,
                'bytes': case[selection]['bytes'], 'quality': case[selection]['quality']}
        result['byte_ratio'] = result['modes']['adaptive']['bytes'] / result['modes']['dc']['bytes']
        result['time_ratio'] = result['modes']['adaptive']['median_seconds'] / result['modes']['dc']['median_seconds']
        results.append(result)
        print(case['case'], 'PASS', flush=True)
    report = {'status': 'PASS', 'qp': 100, 'threads': 1, 'repeats': 3, 'bd_rate': None,
        'bd_rate_note': 'N/A: only one QP supported', 'cases': results}
    (args.out_dir / 'measurements.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
