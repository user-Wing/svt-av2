"""Frozen, resumable M6 CQP/RD round trips and quality/time measurements."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import statistics
import time

import numpy as np
from m53_batch import (atomic_json, bd_rate, compare_quality, digest, dimensions,
                       execute, inspect_source, reusable, run_lock)


def job_id(job):
    return hashlib.sha256(json.dumps(job, sort_keys=True).encode()).hexdigest()[:20]


def validate_frozen(config):
    files = config['sources'] + config['frozen_files']
    files += [config[k] for k in ['app', 'baseline_app', 'decoder', 'ffmpeg', 'vmaf_model'] if config[k]]
    for item in files:
        if digest(item['path']) != item['sha256']:
            raise ValueError('frozen dependency changed: ' + item['path'])


def rd_trace(path, expected_frames, mode):
    rows = []
    for line in path.read_text(encoding='utf-8', errors='replace').splitlines():
        if not line.startswith('rd frame='):
            continue
        rows.append({k: int(v) for k, v in re.findall(r'(\w+)=(\d+)', line)})
    if mode in ['m5', 'off']:
        if rows:
            raise ValueError('RD-off unexpectedly emitted RD decisions')
        return rows
    if len(rows) != expected_frames or [r['frame'] for r in rows] != list(range(expected_frames)):
        raise ValueError('missing, extra or unordered RD frame records')
    expected_mode = 1 if mode == 'exhaustive' else 2
    required = ['mode', 'candidates', 'forward', 'replay', 'D', 'Rq12', 'J', 'lambdaq8', 'full', 'partial', 'winner']
    for row in rows:
        if any(k not in row for k in required) or row['mode'] != expected_mode:
            raise ValueError('invalid RD record')
        if not row['candidates'] or row['J'] < row['D'] or not row['replay']:
            raise ValueError('invalid RD candidate/cost/replay statistics')
    return rows


def run_job(job, config, out, run_id, timeout):
    folder = out / 'cases' / job_id(job)
    folder.mkdir(parents=True, exist_ok=True)
    result_path = folder / 'result.json'
    if reusable(result_path, run_id, job):
        return json.loads(result_path.read_text(encoding='utf-8'))
    attempt = 1
    while (folder / f'attempt-{attempt:04}').exists():
        attempt += 1
    work = folder / f'attempt-{attempt:04}'
    work.mkdir()
    source = config['sources'][job['source']]
    result = {'run_id': run_id, 'job': job, 'status': 'FAIL', 'commands': []}
    try:
        raw_bytes = sum(dimensions(source)) * (2 if source['depth'] > 8 else 1) * source['frames']
        if shutil.disk_usage(out).free < 4 * raw_bytes + 16 * 1024 * 1024:
            raise OSError('insufficient free disk for job scratch')
        first = first_trace = None
        app = config['baseline_app'] if job['rd_mode'] == 'm5' else config['app']
        for repeat in range(config['repeats']):
            stream, recon = work / f'encode-{repeat}.av2', work / 'internal.yuv'
            command = [app['path'], '--input', source['path'], '--output', stream,
                '--export-recon', recon, '--qp', job['qp'], '--pred-structure', job['prediction'],
                '--intra-select', 'adaptive' if job['intra'] else 'dc',
                '--partition-select', 'adaptive' if job['partition'] else 'fixed']
            if job['rd_mode'] != 'm5':
                command += ['--rd-mode', job['rd_mode']]
            record = execute(command, work / f'encode-{repeat}.log', timeout, out)
            record.update(stage='encode', repeat=repeat)
            result['commands'].append(record)
            trace = rd_trace(Path(record['log']), source['frames'], job['rd_mode'])
            current = digest(stream), digest(recon)
            if first is None:
                first, first_trace = current, trace
            elif first != current or first_trace != trace:
                raise ValueError('repeated encode or RD statistics differ')
            if repeat:
                stream.unlink()
        stream, decoded = work / 'encode-0.av2', work / 'decoded.y4m'
        record = execute([config['decoder']['path'], '--summary', '-o', decoded, stream],
                         work / 'decode.log', timeout, out)
        record['stage'] = 'decode'
        result['commands'].append(record)
        result['quality'] = compare_quality(Path(source['path']), recon, decoded, source)
        if digest(recon) != first[1]:
            raise ValueError('reconstruction changed during decode')
        result.update(stream_sha256=first[0], recon_sha256=first[1], bytes=stream.stat().st_size,
                      frames=source['frames'], actual_qindex=job['qp'] + (48 if source['depth'] == 10 else 0),
                      vmaf=None, rd_frames=first_trace)
        if config['ffmpeg']:
            vmaf_log = work / 'vmaf.json'
            pixel = 'yuv' + source['chroma'] + ('p10le' if source['depth'] == 10 else 'p')
            filters = ('[0:v]setpts=PTS-STARTPTS,format=yuv420p10le[d];'
                       '[1:v]setpts=PTS-STARTPTS,format=yuv420p10le[r];'
                       '[d][r]libvmaf=model=path=model.json:log_fmt=json:log_path='
                       + vmaf_log.relative_to(out).as_posix() + ':n_threads=1')
            record = execute([config['ffmpeg']['path'], '-hide_banner', '-nostdin', '-v', 'error',
                '-filter_complex_threads', '1', '-f', 'rawvideo', '-pixel_format', pixel,
                '-video_size', f"{source['width']}x{source['height']}",
                '-framerate', source['fps'].replace(':', '/'), '-i', recon,
                '-i', source['path'], '-filter_complex', filters, '-f', 'null', '-'],
                work / 'vmaf.log', timeout, out)
            record['stage'] = 'vmaf'
            result['commands'].append(record)
            data = json.loads(vmaf_log.read_text(encoding='utf-8'))
            if len(data['frames']) != source['frames']:
                raise ValueError('VMAF frame count mismatch')
            result['vmaf'] = data['pooled_metrics']['vmaf']['mean']
        timings = [c['seconds'] for c in result['commands'] if c['stage'] == 'encode']
        result.update(median_seconds=statistics.median(timings),
                      encoding_seconds=timings, seconds_min=min(timings), seconds_max=max(timings))
        result['fps'] = source['frames'] / result['median_seconds']
        n, d = map(int, source['fps'].split(':'))
        result['duration_seconds'] = source['frames'] * d / n
        result['bitrate_kbps'] = result['bytes'] * 8 / result['duration_seconds'] / 1000
        result['peak_rss_bytes'] = max((c['sampled_peak_rss_bytes'] or 0) for c in result['commands'] if c['stage'] == 'encode') or None
        result['retained_artifacts'] = [{'path': str(p), 'sha256': digest(p)}
            for p in sorted(work.iterdir()) if p not in [recon, decoded]]
        recon.unlink()
        decoded.unlink()
        result['status'] = 'PASS'
    except (OSError, ValueError, RuntimeError, TimeoutError, KeyError) as exc:
        result['error'] = str(exc)
    atomic_json(result_path, result)
    print(job_id(job), result['status'], job, result.get('error', ''), flush=True)
    return result


def summarize(rows, config):
    reports = []
    modes = config['rd_modes']
    reference = 'm5' if 'm5' in modes else 'off' if 'off' in modes else modes[0]
    pairs = [(reference, m) for m in modes if m != reference]
    if 'pruned' in modes and 'exhaustive' in modes and reference != 'exhaustive':
        pairs.append(('exhaustive', 'pruned'))
    for source in range(len(config['sources'])):
        for prediction in config['predictions']:
            for tool in config['tools']:
                for baseline, tested in pairs:
                    selected = [r for r in rows if r['job']['source'] == source and
                        r['job']['prediction'] == prediction and
                        str(r['job']['partition']) + str(r['job']['intra']) == tool]
                    for metric in ['psnr_y', 'vmaf']:
                        def curve(mode):
                            values = [r for r in selected if r['job']['rd_mode'] == mode]
                            if len(values) != len(config['qps']) or any(r['status'] != 'PASS' for r in values):
                                return None
                            return [(r['quality']['psnr_yuv_db'][0] if metric == 'psnr_y' else r['vmaf'], r['bytes']) for r in values]
                        a, b = curve(baseline), curve(tested)
                        value = bd_rate(a, b) if a and b and all(q is not None for q, _ in a+b) else {
                            'percent': None, 'reason': 'missing/failed/infinite quality point'}
                        reports.append(dict(source=source, prediction=prediction, tools=tool,
                            reference=baseline, test=tested, metric=metric, **value))
    return reports


def expected_jobs(config):
    return [dict(source=s, qp=q, prediction=p, partition=int(t[0]), intra=int(t[1]), rd_mode=m)
        for s in range(len(config['sources'])) for q in config['qps']
        for p in config['predictions'] for t in config['tools'] for m in config['rd_modes']]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['app', 'decoder', 'corpus', 'out-dir']:
        parser.add_argument('--' + name, type=Path, required=True)
    for name in ['baseline-app', 'ffmpeg', 'vmaf-model', 'contract']:
        parser.add_argument('--' + name, type=Path)
    parser.add_argument('--qps', default='40,80,100,160,220')
    parser.add_argument('--predictions', default='all-intra,low-delay-p')
    parser.add_argument('--tools', default='00,01,10,11', help='partition/intra pairs')
    parser.add_argument('--rd-modes', default='off,exhaustive,pruned')
    parser.add_argument('--jobs', type=int, default=1)
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--timeout', type=int, default=86400)
    parser.add_argument('--resume', action='store_true')
    args = parser.parse_args()
    qps = list(map(int, args.qps.split(',')))
    predictions, tools, modes = args.predictions.split(','), args.tools.split(','), args.rd_modes.split(',')
    if (not qps or len(set(qps)) != len(qps) or not all(1 <= q <= 255 for q in qps) or
        any(len(set(v)) != len(v) or not v for v in [predictions, tools, modes]) or
        not all(p in ['all-intra', 'low-delay-p'] for p in predictions) or
        not all(t in ['00', '01', '10', '11'] for t in tools) or
        not all(m in ['off', 'exhaustive', 'pruned'] for m in modes) or
        min(args.jobs, args.repeats, args.timeout) < 1 or bool(args.ffmpeg) != bool(args.vmaf_model)):
        parser.error('invalid batch configuration')
    if args.baseline_app:
        modes = ['m5'] + modes
    out = args.out_dir.resolve()
    if out.exists() and not args.resume:
        parser.error('output exists; use a fresh directory or --resume')
    out.mkdir(parents=True, exist_ok=True)
    with run_lock(out / 'run.lock'):
        def frozen(path):
            return {'path': str(path.resolve()), 'sha256': digest(path)} if path else None
        sources = []
        for item in json.loads(args.corpus.read_text(encoding='utf-8')):
            path = Path(item['path'])
            path = path if path.is_absolute() else args.corpus.resolve().parent / path
            sources.append(dict(inspect_source(path), **{k: v for k, v in item.items() if k != 'path'}))
        if not sources or len({s['path'] for s in sources}) != len(sources):
            parser.error('empty or duplicate corpus')
        dependencies = [Path(__file__), Path(__file__).with_name('m53_batch.py'),
                        Path(__file__).with_name('verify_m4.py'), Path(__file__).with_name('audit_m6_batch.py')]
        dependencies += [Path(s['provenance_record']) for s in sources if s.get('provenance_record')]
        if args.contract:
            dependencies.append(args.contract)
        # MSVC app uses a sidecar DLL. It must be frozen alongside the EXE.
        for app in [args.app, args.baseline_app]:
            if app:
                dependencies.extend(sorted(app.resolve().parent.glob('*.dll')))
        config = {'schema': 6, 'script_sha256': digest(Path(__file__)),
            'frozen_files': [frozen(p) for p in dict.fromkeys(dependencies)],
            'host': {'platform': platform.platform(), 'processor': platform.processor(),
                     'logical_cpus': os.cpu_count(), 'python': platform.python_version()},
            'numpy_version': np.__version__, 'app': frozen(args.app), 'baseline_app': frozen(args.baseline_app),
            'decoder': frozen(args.decoder), 'ffmpeg': frozen(args.ffmpeg), 'vmaf_model': frozen(args.vmaf_model),
            'sources': sources, 'qps': qps, 'predictions': predictions, 'tools': tools, 'rd_modes': modes,
            'repeats': args.repeats, 'jobs': args.jobs,
            'vmaf_input_rule': 'both inputs converted to yuv420p10le; PTS reset; no scaling',
            'timing_scope': 'CLI encode includes input read and recon export; excludes decoder and VMAF',
            'retention': 'keep stream/JSON/log; verify and remove recon/decoded; remove repeated streams after hash match'}
        manifest = out / 'run-manifest.json'
        if manifest.exists() and json.loads(manifest.read_text(encoding='utf-8')) != config:
            parser.error('resume fingerprint/configuration mismatch')
        if not manifest.exists():
            atomic_json(manifest, config)
            if args.vmaf_model:
                shutil.copyfile(args.vmaf_model, out / 'model.json')
        validate_frozen(config)
        if args.vmaf_model and digest(out / 'model.json') != config['vmaf_model']['sha256']:
            parser.error('saved VMAF model changed')
        run_id, started = digest(manifest), time.time()
        atomic_json(out / 'summary.json', {'status': 'RUNNING', 'run_id': run_id, 'pid': os.getpid(), 'started_unix': started})
        maximum = max(sum(dimensions(s)) * (2 if s['depth'] > 8 else 1) * s['frames'] for s in sources)
        reserve = args.jobs * (4 * maximum + 16 * 1024 * 1024)
        free = shutil.disk_usage(out).free
        jobs = expected_jobs(config)
        atomic_json(out / 'resource-estimate.json', {'free_bytes_at_start': free, 'jobs': len(jobs),
            'temporary_reserve_bytes': reserve, 'retained_streams_are_additional': True})
        if free < reserve:
            parser.error('insufficient temporary disk reserve')
        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            rows = list(pool.map(lambda job: run_job(job,config,out,run_id,args.timeout),jobs))
        atomic_json(out / 'acceptance.json', rows)
        status, error = ('PASS' if all(r['status'] == 'PASS' for r in rows) else 'FAIL'), None
        try:
            validate_frozen(config)
        except (OSError, ValueError) as exc:
            status, error = 'FAIL', str(exc)
        atomic_json(out / 'summary.json', {'status': status, 'run_id': run_id, 'jobs': len(jobs),
            'passed': sum(r['status'] == 'PASS' for r in rows), 'started_unix': started,
            'finished_unix': time.time(), 'frozen_dependency_error': error,
            'speed_note': 'one encoder at a time within this runner; external load must be controlled' if args.jobs == 1
                else 'concurrent jobs; not isolated encoder speed', 'bd_rate': summarize(rows,config)})
        return int(status != 'PASS')


if __name__ == '__main__':
    raise SystemExit(main())
