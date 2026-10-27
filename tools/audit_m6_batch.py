"""Audit completed M6 batches, including RD mode/trace and frozen dependencies."""
import argparse
import json
import math
from pathlib import Path
import statistics

from m53_batch import atomic_json, digest, reusable
from m6_batch import expected_jobs, job_id, rd_trace, summarize, validate_frozen


def audit(out):
    out = Path(out).resolve()
    meta = json.loads((out / 'run-manifest.json').read_text(encoding='utf-8'))
    summary = json.loads((out / 'summary.json').read_text(encoding='utf-8'))
    run_id = digest(out / 'run-manifest.json')
    assert meta['schema'] == 6
    assert summary['status'] == 'PASS' and summary['run_id'] == run_id
    assert summary['frozen_dependency_error'] is None
    validate_frozen(meta)
    if meta['vmaf_model']:
        assert digest(out / 'model.json') == meta['vmaf_model']['sha256']
    jobs = expected_jobs(meta)
    expected = {job_id(job): job for job in jobs}
    assert len(expected) == len(jobs)
    rows = json.loads((out / 'acceptance.json').read_text(encoding='utf-8'))
    assert len(rows) == len(jobs) == summary['jobs'] == summary['passed']
    seen = set()
    for row in rows:
        job = row['job']
        ident = job_id(job)
        assert ident in expected and ident not in seen and expected[ident] == job
        seen.add(ident)
        result_file = out / 'cases' / ident / 'result.json'
        assert reusable(result_file,run_id,job)
        assert json.loads(result_file.read_text(encoding='utf-8')) == row
        source = meta['sources'][job['source']]
        assert row['frames'] == source['frames'] == len(row['quality']['frame_sha256'])
        assert row['actual_qindex'] == job['qp'] + (48 if source['depth'] == 10 else 0)
        assert all(len(h) == 64 and all(c in '0123456789abcdef' for c in h) for h in row['quality']['frame_sha256'])
        commands = row['commands']
        assert all(c['exit'] == 0 for c in commands)
        encodes = [c for c in commands if c['stage'] == 'encode']
        assert len(encodes) == meta['repeats']
        assert sum(c['stage'] == 'decode' for c in commands) == 1
        assert sum(c['stage'] == 'vmaf' for c in commands) == int(bool(meta['ffmpeg']))
        app = meta['baseline_app'] if job['rd_mode'] == 'm5' else meta['app']
        for repeat, command in enumerate(encodes):
            argv = command['command']
            assert argv[0] == app['path'] and command['repeat'] == repeat
            for flag, value in [('--input',source['path']), ('--qp',str(job['qp'])),
                ('--pred-structure',job['prediction']),
                ('--intra-select','adaptive' if job['intra'] else 'dc'),
                ('--partition-select','adaptive' if job['partition'] else 'fixed')]:
                assert argv.count(flag) == 1 and argv[argv.index(flag)+1] == value
            if job['rd_mode'] == 'm5':
                assert '--rd-mode' not in argv
            else:
                assert argv.count('--rd-mode') == 1 and argv[argv.index('--rd-mode')+1] == job['rd_mode']
            assert rd_trace(Path(command['log']),source['frames'],job['rd_mode']) == row['rd_frames']
        timing = [c['seconds'] for c in encodes]
        assert all(t > 0 and math.isfinite(t) for t in timing)
        assert row['encoding_seconds'] == timing and row['median_seconds'] == statistics.median(timing)
        assert math.isclose(row['fps'],source['frames']/row['median_seconds'],rel_tol=1e-12)
        streams = [Path(a['path']) for a in row['retained_artifacts'] if Path(a['path']).name == 'encode-0.av2']
        assert len(streams) == 1 and streams[0].stat().st_size == row['bytes']
        assert len(row['quality']['mse_yuv']) == len(row['quality']['psnr_yuv_db']) == 3
        for mse, psnr in zip(row['quality']['mse_yuv'],row['quality']['psnr_yuv_db']):
            assert mse >= 0 and math.isfinite(mse)
            assert psnr is None if mse == 0 else math.isclose(psnr,10*math.log10(((1 << source['depth'])-1)**2/mse),abs_tol=1e-8)
        if meta['ffmpeg']:
            logs = [Path(a['path']) for a in row['retained_artifacts'] if Path(a['path']).name == 'vmaf.json']
            assert len(logs) == 1
            vmaf = json.loads(logs[0].read_text(encoding='utf-8'))
            assert len(vmaf['frames']) == source['frames']
            assert math.isfinite(row['vmaf']) and row['vmaf'] == vmaf['pooled_metrics']['vmaf']['mean']
    assert summary['bd_rate'] == summarize(rows,meta), 'summary BD-rate differs from raw records'
    return {'status':'PASS','schema':6,'run_id':run_id,'jobs':len(rows),
        'total_frames':sum(r['frames'] for r in rows),'repeats':meta['repeats'],
        'rd_modes':meta['rd_modes'],'vmaf':bool(meta['ffmpeg']),
        'note':'Frozen source/tool/contract hashes, exact job coverage, retained files, RD mode/trace, decoder and metrics verified.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out-dir',type=Path,required=True)
    args = parser.parse_args()
    report = audit(args.out_dir)
    atomic_json(args.out_dir.resolve() / 'audit.json',report)
    print(json.dumps(report,indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
