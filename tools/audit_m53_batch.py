"""Audit a completed unattended batch; never accept RUNNING or stale summary files."""
import argparse
import itertools
import json
import math
from pathlib import Path

from m53_batch import atomic_json, digest, reusable, validate_frozen


def audit(out):
    meta = json.loads((out / 'run-manifest.json').read_text(encoding='utf-8'))
    summary = json.loads((out / 'summary.json').read_text(encoding='utf-8'))
    run_id = digest(out / 'run-manifest.json')
    assert summary['status'] == 'PASS' and summary['run_id'] == run_id, 'batch has not completed successfully'
    validate_frozen(meta)
    if meta['vmaf_model']:
        assert digest(out / 'model.json') == meta['vmaf_model']['sha256'], 'saved VMAF model changed'
    expected = set(itertools.product(range(len(meta['sources'])), meta['qps'], meta['predictions'], meta['tools']))
    rows = json.loads((out / 'acceptance.json').read_text(encoding='utf-8'))
    assert len(rows) == len(expected) == summary['jobs'] == summary['passed']
    seen = set()
    for row in rows:
        job = row['job']
        key = (job['source'], job['qp'], job['prediction'], str(job['partition'])+str(job['intra']))
        assert key in expected and key not in seen, 'missing/duplicate/unexpected job'
        seen.add(key)
        assert reusable_result(row, run_id), key
        source = meta['sources'][job['source']]
        assert row['frames'] == source['frames'] == len(row['quality']['frame_sha256'])
        assert all(len(x) == 64 and all(c in '0123456789abcdef' for c in x)
                   for x in row['quality']['frame_sha256'])
        commands = row['commands']
        assert all(c['exit'] == 0 for c in commands)
        assert sum(c['stage'] == 'encode' for c in commands) == meta['repeats']
        assert sum(c['stage'] == 'decode' for c in commands) == 1
        assert sum(c['stage'] == 'vmaf' for c in commands) == int(bool(meta['ffmpeg']))
        streams = [Path(a['path']) for a in row['retained_artifacts'] if Path(a['path']).name == 'encode-0.av2']
        assert len(streams) == 1 and streams[0].stat().st_size == row['bytes']
        assert len(row['quality']['mse_yuv']) == len(row['quality']['psnr_yuv_db']) == 3
        for mse, psnr in zip(row['quality']['mse_yuv'], row['quality']['psnr_yuv_db']):
            assert mse >= 0 and math.isfinite(mse)
            if mse == 0:
                assert psnr is None
            else:
                expected_psnr = 10*math.log10(((1 << source['depth'])-1)**2/mse)
                assert math.isclose(psnr, expected_psnr, abs_tol=1e-8)
        if meta['ffmpeg']:
            logs = [Path(a['path']) for a in row['retained_artifacts'] if Path(a['path']).name == 'vmaf.json']
            assert len(logs) == 1
            vmaf = json.loads(logs[0].read_text())
            assert len(vmaf['frames']) == source['frames']
            assert math.isfinite(row['vmaf']) and row['vmaf'] == vmaf['pooled_metrics']['vmaf']['mean']
    return {'status': 'PASS', 'run_id': run_id, 'jobs': len(rows),
            'total_frames': sum(r['frames'] for r in rows),
            'vmaf': bool(meta['ffmpeg']), 'repeats': meta['repeats'],
            'note': 'Validated frozen dependencies, job coverage, retained hashes and metric consistency.'}


def reusable_result(row, run_id):
    # Reuse the exact worker cache predicate against a temporary-free row view:
    # a stored result.json must itself agree with the aggregate acceptance row.
    job = row['job']
    import hashlib
    ident = hashlib.sha256(json.dumps(job, sort_keys=True).encode()).hexdigest()[:20]
    artifact = Path(row['retained_artifacts'][0]['path'])
    result_file = artifact.parent.parent / 'result.json'
    assert result_file.parent.name == ident
    assert json.loads(result_file.read_text(encoding='utf-8')) == row
    return reusable(result_file, run_id, job)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out-dir', type=Path, required=True)
    args = parser.parse_args()
    report = audit(args.out_dir.resolve())
    atomic_json(args.out_dir / 'audit.json', report)
    print(json.dumps(report, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
