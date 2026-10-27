"""Record a coarse runtime budget from the isolated one-second real pilot."""
import argparse
import json
from pathlib import Path
from audit_m6_batch import audit
from m53_batch import atomic_json


def budget(run):
    root = run / 'pilot-real'
    audit(root)
    rows = json.loads((root / 'acceptance.json').read_text())
    times = {mode: [r['median_seconds'] for r in rows if r['job']['rd_mode'] == mode]
             for mode in ['m5', 'pruned']}
    maximum = {k: max(v) for k, v in times.items()}
    # 8 sources * 5 QPs * 2 structures * 3 repeats, five seconds per clip.
    estimate = sum(maximum.values()) * 8 * 5 * 2 * 3 * 5
    atomic_json(run / 'pilot-budget.json', dict(status='PASS', sample_seconds=1,
        max_encode_seconds=maximum, real_quality_encode_estimate_seconds=estimate,
        note='Coarse extrapolation from two pilot clips at QP100/P; other clips/QPs/AI may differ greatly. Excludes metrics/decode and the other campaign stages.'))
    print('Pilot budget recorded; approximate real-curve encode seconds:', estimate)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--campaign', type=Path, required=True)
    budget(parser.parse_args().campaign.resolve())
