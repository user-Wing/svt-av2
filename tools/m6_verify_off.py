"""Check the M5/off pair in a completed M6 batch without changing old evidence."""
import argparse
import json
from pathlib import Path
from audit_m6_batch import audit
from m53_batch import atomic_json


def verify(out):
    audit(out)
    rows = json.loads((out / 'acceptance.json').read_text())
    index = {}
    for row in rows:
        job = row['job']
        key = tuple(job[k] for k in ['source', 'qp', 'prediction', 'partition', 'intra'])
        index.setdefault(key, {})[job['rd_mode']] = row
    count = 0
    for key, pair in index.items():
        assert 'm5' in pair and 'off' in pair, ('missing pair', key)
        for field in ['stream_sha256', 'recon_sha256']:
            assert pair['m5'][field] == pair['off'][field], (key, field)
        count += 1
    result = {'status': 'PASS', 'pairs': count, 'note': 'RD off preserves complete M5 stream and reconstruction.'}
    atomic_json(out / 'off-verification.json', result)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out-dir', type=Path, required=True)
    print(json.dumps(verify(parser.parse_args().out_dir.resolve()), indent=2))
