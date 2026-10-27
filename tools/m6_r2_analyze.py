"""Read-only paired analysis of a completed M6 small exhaustive batch."""
import argparse
from collections import defaultdict
import json
from pathlib import Path
import statistics


def describe(values):
    return dict(min=min(values), median=statistics.median(values), max=max(values),
                mean=statistics.mean(values))


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--batch', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    rows = [json.loads(p.read_text()) for p in sorted(args.batch.glob('cases/*/result.json'))]
    assert len(rows) == 1512 and all(r['status'] == 'PASS' for r in rows)
    groups, pairs = defaultdict(list), defaultdict(dict)
    for row in rows:
        job = row['job']
        groups[(job['prediction'], str(job['partition']) + str(job['intra']), job['rd_mode'])].append(row)
        pairs[tuple((k, v) for k, v in sorted(job.items()) if k != 'rd_mode')][job['rd_mode']] = row
    result = dict(batch=str(args.batch), count=len(rows), timing_scope='jobs4 CLI including recon export; not isolated speed', groups=[])
    for (prediction, tools, mode), group in sorted(groups.items()):
        item = dict(prediction=prediction, tools=tools, mode=mode, count=len(group),
                    seconds=describe([r['median_seconds'] for r in group]))
        for key in ['candidates', 'forward', 'replay', 'full', 'partial']:
            if mode != 'off':
                item[key + '_per_frame'] = describe([f[key] for r in group for f in r['rd_frames']])
        result['groups'].append(item)
    comparisons = []
    for key, modes in sorted(pairs.items()):
        full, pruned = modes['exhaustive'], modes['pruned']
        comparisons.append(dict(job=dict(key), stream_equal=full['stream_sha256'] == pruned['stream_sha256'],
            recon_equal=full['recon_sha256'] == pruned['recon_sha256'],
            byte_delta=pruned['bytes'] - full['bytes'], vmaf_delta=pruned['vmaf'] - full['vmaf'],
            seconds_ratio=pruned['median_seconds'] / full['median_seconds'],
            candidate_ratio=sum(f['candidates'] for f in pruned['rd_frames']) / sum(f['candidates'] for f in full['rd_frames']),
            forward_ratio=sum(f['forward'] for f in pruned['rd_frames']) / sum(f['forward'] for f in full['rd_frames'])))
    result['pairs'] = comparisons
    result['paired_summary'] = dict(count=len(comparisons),
        identical_streams=sum(r['stream_equal'] for r in comparisons),
        identical_recons=sum(r['recon_equal'] for r in comparisons),
        **{k: describe([r[k] for r in comparisons]) for k in ['byte_delta', 'vmaf_delta', 'seconds_ratio', 'candidate_ratio', 'forward_ratio']})
    result['limits'] = ['Whole-frame J differs after earlier decisions diverge; it is not same-state local pruning regret.',
                        'Paired quality deltas describe this small resized corpus only; no general bitrate claim.']
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result['paired_summary'], indent=2))


if __name__ == '__main__':
    main()
