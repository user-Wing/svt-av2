"""Re-encode the frozen M5.2 four-way matrix at QP100 without overwriting it."""
import argparse
import json
from pathlib import Path

from m53_batch import atomic_json, digest, execute


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--baseline', type=Path, default=Path('artifacts/m52-root/matrix144'))
    parser.add_argument('--out-dir', type=Path, required=True)
    args = parser.parse_args()
    if args.out_dir.exists():
        parser.error('choose a fresh output directory')
    out = args.out_dir.resolve()
    out.mkdir(parents=True)
    records = []
    baseline = args.baseline.resolve()
    app = args.app.resolve()
    for case in json.loads((baseline / 'acceptance.json').read_text()):
        assert case['status'] == 'PASS'
        source = baseline / case['case'] / 'input.y4m'
        assert digest(source) == case['source_sha256']
        folder = out / case['case']
        folder.mkdir()
        for p, i in [(0, 0), (0, 1), (1, 0), (1, 1)]:
            mode = f'p{p}i{i}'
            stream, recon = folder / f'{mode}.av2', folder / f'{mode}.yuv'
            record = execute([app, '--input', source, '--output', stream, '--export-recon', recon,
                '--qp', '100', '--pred-structure', case['prediction'],
                '--intra-select', 'adaptive' if i else 'dc',
                '--partition-select', 'adaptive' if p else 'fixed'], folder / f'{mode}.log', 7200, out)
            record.update(case=case['case'], mode=mode, stream_sha256=digest(stream), recon_sha256=digest(recon))
            assert record['stream_sha256'] == case[mode]['stream_sha256'], (case['case'], mode, 'stream')
            assert record['recon_sha256'] == case[mode]['recon_sha256'], (case['case'], mode, 'recon')
            record['status'] = 'PASS'
            recon.unlink()
            records.append(record)
        print(case['case'], 'PASS', flush=True)
    atomic_json(out / 'acceptance.json', records)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
