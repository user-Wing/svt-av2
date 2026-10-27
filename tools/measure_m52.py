"""Sequential single-QP measurements of the four verified M5.2 combinations."""
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
    results = []
    for case in json.loads((args.input_results / 'acceptance.json').read_text()):
        assert case['status'] == 'PASS'
        folder = args.out_dir / case['case']
        folder.mkdir()
        result = {'case': case['case'], 'frames': case['frames'], 'modes': {}}
        for p, i in [(0, 0), (0, 1), (1, 0), (1, 1)]:
            mode = f'p{p}i{i}'
            commands = []
            for _ in range(3):
                run([args.app, '--input', args.input_results / case['case'] / 'input.y4m',
                     '--output', folder / (mode + '.av2'), '--export-recon', folder / (mode + '.yuv'),
                     '--pred-structure', case['prediction'], '--intra-select', 'adaptive' if i else 'dc',
                     '--partition-select', 'adaptive' if p else 'fixed'], 7200, commands)
                for extension, field in [('av2', 'stream_sha256'), ('yuv', 'recon_sha256')]:
                    assert hashlib.sha256((folder / f'{mode}.{extension}').read_bytes()).hexdigest() == case[mode][field]
            median = statistics.median(c['seconds'] for c in commands)
            peaks = [c['peak_rss_bytes'] for c in commands if c['peak_rss_bytes'] is not None]
            result['modes'][mode] = {'median_seconds': median, 'fps': case['frames'] / median,
                'sampled_peak_rss_bytes': max(peaks, default=None), 'commands': commands,
                'bytes': case[mode]['bytes'], 'quality': case[mode]['quality']}
        results.append(result)
        print(case['case'], 'PASS', flush=True)
    report = {'status': 'PASS', 'qp': 100, 'threads': 1, 'repeats': 3, 'bd_rate': None,
        'bd_rate_note': 'N/A: only one QP supported', 'cases': results}
    (args.out_dir / 'measurements.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
