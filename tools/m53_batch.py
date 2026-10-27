"""Resumable CQP round trips and quality measurements; see docs/m53-validation-contracts.md."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from contextlib import contextmanager
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import shutil
import subprocess
import time

import numpy as np
from verify_m4 import memory_snapshot


def digest(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def atomic_json(path, value):
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps(value, indent=2, allow_nan=False), encoding='utf-8')
    temporary.replace(path)


def header(f):
    tokens = f.readline().decode('ascii').split()
    if not tokens or tokens[0] != 'YUV4MPEG2':
        raise ValueError('expected Y4M input')
    fields = {t[0]: t[1:] for t in tokens[1:]}
    w, h = int(fields['W']), int(fields['H'])
    chroma = fields.get('C', '420jpeg')
    if chroma not in ['420jpeg', '420', '420mpeg2', '420paldv', '420p10', '444p10']:
        raise ValueError('unsupported Y4M format: ' + chroma)
    depth = 10 if chroma.endswith('p10') else 8
    chroma = chroma[:3]
    if w % 8 or h % 8 or not 0 < w <= 1920 or not 0 < h <= 1080:
        raise ValueError('unsupported geometry')
    return {'width': w, 'height': h, 'depth': depth, 'chroma': chroma,
            'fps': fields.get('F', '25:1')}


def dimensions(meta):
    y = meta['width'] * meta['height']
    return [y, y // 4, y // 4] if meta['chroma'] == '420' else [y, y, y]


def y4m_frame(f, size):
    tag = f.readline()
    if not tag:
        return None
    if not tag.startswith(b'FRAME') or not tag.endswith(b'\n'):
        raise ValueError('invalid frame marker')
    data = f.read(size)
    if len(data) != size:
        raise ValueError('truncated Y4M frame')
    return data


def inspect_source(path):
    with path.open('rb') as f:
        meta = header(f)
        size = sum(dimensions(meta)) * (2 if meta['depth'] > 8 else 1)
        frames = 0
        while y4m_frame(f, size) is not None:
            frames += 1
    if not frames:
        raise ValueError('empty source')
    return dict(meta, frames=frames, sha256=digest(path), path=str(path.resolve()))


def compare_quality(source, recon, decoded, meta):
    dims = dimensions(meta)
    size = sum(dims) * (2 if meta['depth'] > 8 else 1)
    dtype = np.dtype('u1' if meta['depth'] == 8 else '<u2')
    sums = np.zeros(3, dtype=np.float64)
    hashes = []
    with source.open('rb') as src, recon.open('rb') as rec, decoded.open('rb') as dec:
        original, external = header(src), header(dec)
        for key in ['width', 'height', 'depth', 'chroma']:
            if original[key] != external[key] or original[key] != meta[key]:
                raise ValueError('decoder geometry/format mismatch')
        for _ in range(meta['frames']):
            inp, internal, actual = y4m_frame(src, size), rec.read(size), y4m_frame(dec, size)
            if inp is None or len(internal) != size or actual != internal:
                raise ValueError('frame count, length or decoder reconstruction mismatch')
            hashes.append(hashlib.sha256(internal).hexdigest())
            a, b = np.frombuffer(inp, dtype=dtype), np.frombuffer(internal, dtype=dtype)
            pos = 0
            for p, count in enumerate(dims):
                delta = a[pos:pos+count].astype(np.float64) - b[pos:pos+count]
                sums[p] += np.sum(delta * delta)
                pos += count
        if src.read(1) or rec.read(1) or dec.read(1):
            raise ValueError('extra frames or trailing data')
    mse = [float(sums[p] / (dims[p] * meta['frames'])) for p in range(3)]
    maximum = (1 << meta['depth']) - 1
    return {'mse_yuv': mse, 'psnr_yuv_db': [10 * math.log10(maximum**2 / x) if x else None for x in mse],
            'psnr_null_means_infinite': True, 'frame_sha256': hashes}


def execute(command, log, timeout, cwd):
    start, peak = time.perf_counter(), None
    with log.open('wb') as output, subprocess.Popen(list(map(str, command)), cwd=cwd,
            stdout=output, stderr=subprocess.STDOUT) as proc:
        try:
            while True:
                sample = memory_snapshot(proc)
                if sample is not None:
                    peak = max(peak or 0, sample['peak_rss_bytes'])
                try:
                    status = proc.wait(timeout=0.2)
                    break
                except subprocess.TimeoutExpired:
                    if time.perf_counter() - start > timeout:
                        raise TimeoutError('command timeout: ' + str(command[0]))
        except BaseException:
            proc.kill()
            proc.wait()
            raise
    record = {'command': list(map(str, command)), 'exit': status,
                  'seconds': time.perf_counter() - start, 'sampled_peak_rss_bytes': peak,
              'log': str(log)}
    if status:
        raise RuntimeError(f'exit {status}: {command[0]}; see {log}')
    return record


@contextmanager
def run_lock(path):
    with path.open('a+b') as f:
        f.seek(0)
        if os.name == 'nt':
            import msvcrt
            if path.stat().st_size == 0:
                f.write(b'0')
                f.flush()
            f.seek(0)
            msvcrt.locking(f.fileno(), msvcrt.LK_NBLCK, 1)
        else:
            import fcntl
            fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
        yield


def reusable(path, run_id, job):
    if not path.exists():
        return False
    try:
        result = json.loads(path.read_text(encoding='utf-8'))
        return (result['status'] == 'PASS' and result['run_id'] == run_id and result['job'] == job
                and bool(result['retained_artifacts'])
                and any(Path(x['path']).name == 'encode-0.av2' and x['sha256'] == result['stream_sha256']
                        for x in result['retained_artifacts'])
                and all(Path(x['path']).is_file() and digest(x['path']) == x['sha256']
                        for x in result['retained_artifacts']))
    except (OSError, ValueError, KeyError, TypeError):
        return False


def run_job(job, config, out, run_id, timeout):
    ident = hashlib.sha256(json.dumps(job, sort_keys=True).encode()).hexdigest()[:20]
    folder = out / 'cases' / ident
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
        if shutil.disk_usage(out).free < 4*raw_bytes + 16*1024*1024:
            raise OSError('insufficient free disk for this job; keep failed attempts and choose a larger output volume')
        first = None
        for repeat in range(config['repeats']):
            stream, recon = work / f'encode-{repeat}.av2', work / 'internal.yuv'
            command = [config['app']['path'], '--input', source['path'], '--output', stream,
                       '--export-recon', recon, '--qp', str(job['qp']), '--pred-structure', job['prediction'],
                       '--intra-select', 'adaptive' if job['intra'] else 'dc',
                       '--partition-select', 'adaptive' if job['partition'] else 'fixed']
            record = execute(command, work / f'encode-{repeat}.log', timeout, out)
            record.update(stage='encode', repeat=repeat)
            result['commands'].append(record)
            current = (digest(stream), digest(recon))
            if first is None:
                first = current
            elif first != current:
                raise ValueError('repeated encode is not deterministic')
            if repeat:
                stream.unlink()
        stream = work / 'encode-0.av2'
        decoded = work / 'decoded.y4m'
        record = execute([config['decoder']['path'], '--summary', '-o', decoded, stream],
                         work / 'decode.log', timeout, out)
        record['stage'] = 'decode'
        result['commands'].append(record)
        result['quality'] = compare_quality(Path(source['path']), recon, decoded, source)
        if digest(recon) != first[1]:
            raise ValueError('internal reconstruction changed during decode')
        result.update(stream_sha256=first[0], recon_sha256=first[1], bytes=stream.stat().st_size,
                      frames=source['frames'], vmaf=None)
        if config['ffmpeg']:
            vmaf_log = work / 'vmaf.json'
            pix_fmt = 'yuv' + source['chroma'] + ('p10le' if source['depth'] == 10 else 'p')
            relative = vmaf_log.relative_to(out).as_posix()
            filters = ('[0:v]setpts=PTS-STARTPTS,format=yuv420p10le[d];'
                       '[1:v]setpts=PTS-STARTPTS,format=yuv420p10le[r];'
                       f'[d][r]libvmaf=model=path=model.json:log_fmt=json:log_path={relative}:n_threads=1')
            record = execute([config['ffmpeg']['path'], '-hide_banner', '-nostdin', '-v', 'error',
                '-filter_complex_threads', '1', '-f', 'rawvideo', '-pixel_format', pix_fmt,
                '-video_size', f"{source['width']}x{source['height']}", '-framerate', source['fps'].replace(':', '/'),
                '-i', recon, '-i', source['path'], '-filter_complex', filters, '-f', 'null', '-'],
                work / 'vmaf.log', timeout, out)
            record['stage'] = 'vmaf'
            result['commands'].append(record)
            data = json.loads(vmaf_log.read_text())
            if len(data['frames']) != source['frames']:
                raise ValueError('VMAF frame count mismatch')
            result['vmaf'] = data['pooled_metrics']['vmaf']['mean']
        timings = [x['seconds'] for x in result['commands'] if x['stage'] == 'encode']
        result['median_seconds'] = float(np.median(timings))
        result['fps'] = source['frames'] / result['median_seconds']
        result['retained_artifacts'] = [{'path': str(p), 'sha256': digest(p)}
            for p in work.iterdir() if p not in [recon, decoded]]
        recon.unlink()
        decoded.unlink()
        result['status'] = 'PASS'
    except (OSError, ValueError, RuntimeError, TimeoutError, KeyError) as exc:
        result['error'] = str(exc)
    atomic_json(result_path, result)
    print(ident, result['status'], job, result.get('error', ''), flush=True)
    return result


def bd_rate(reference, test):
    """Shape-preserving PCHIP log-rate integration over the common quality interval."""
    curves = []
    for points in [reference, test]:
        points = sorted(points)
        if len(points) < 4 or any(not math.isfinite(q) or not math.isfinite(rate) or rate <= 0 for q, rate in points):
            return {'percent': None, 'reason': 'need at least four finite quality/rate points'}
        quality, rate = zip(*points)
        if any(quality[i] >= quality[i+1] or rate[i] >= rate[i+1] for i in range(len(points)-1)):
            return {'percent': None, 'reason': 'quality/rate curve is not strictly monotonic'}
        curves.append((quality, [math.log(r) for r in rate]))
    lo, hi = max(c[0][0] for c in curves), min(c[0][-1] for c in curves)
    if lo >= hi:
        return {'percent': None, 'reason': 'no overlapping quality interval'}
    def integrate(x, y):
        h = [x[i+1]-x[i] for i in range(len(x)-1)]
        d = [(y[i+1]-y[i])/h[i] for i in range(len(h))]
        m = [0.0] * len(x)
        m[0] = max(0.0, min(3*d[0], ((2*h[0]+h[1])*d[0]-h[0]*d[1])/(h[0]+h[1])))
        m[-1] = max(0.0, min(3*d[-1], ((2*h[-1]+h[-2])*d[-1]-h[-1]*d[-2])/(h[-1]+h[-2])))
        for i in range(1, len(x)-1):
            a, b = 2*h[i]+h[i-1], h[i]+2*h[i-1]
            m[i] = (a+b)/(a/d[i-1]+b/d[i])
        total = 0.0
        for i, length in enumerate(h):
            left, right = max(lo, x[i]), min(hi, x[i+1])
            if left >= right:
                continue
            def primitive(t):
                return length * ((t**4/2-t**3+t)*y[i] +
                    (t**4/4-2*t**3/3+t*t/2)*length*m[i] +
                    (-t**4/2+t**3)*y[i+1] + (t**4/4-t**3/3)*length*m[i+1])
            total += primitive((right-x[i])/length)-primitive((left-x[i])/length)
        return total
    means = [integrate(x, y)/(hi-lo) for x, y in curves]
    return {'percent': float(100 * math.expm1(means[1]-means[0])), 'quality_interval': [lo, hi],
            'method': 'monotonic PCHIP in log(rate), common quality interval'}


def summarize(results):
    groups = {}
    for row in results:
        job = row['job']
        groups.setdefault((job['source'], job['prediction']), []).append(row)
    reports = []
    for (source, prediction), rows in groups.items():
        for metric in ['psnr_y', 'vmaf']:
            def curve(p, i):
                selected = [r for r in rows if r['status'] == 'PASS' and
                            r['job']['partition'] == p and r['job']['intra'] == i]
                return [(r['quality']['psnr_yuv_db'][0] if metric == 'psnr_y' else r['vmaf'], r['bytes'])
                        for r in selected]
            baseline = curve(0, 0)
            for p, i in [(0, 1), (1, 0), (1, 1)]:
                tested = curve(p, i)
                value = (bd_rate(baseline, tested) if baseline and tested and
                         all(q is not None for q, _ in baseline+tested) else
                         {'percent': None, 'reason': 'missing or infinite quality metric'})
                reports.append(dict(source=source, prediction=prediction, metric=metric,
                                    partition=p, intra=i, **value))
    return reports


def validate_frozen(config):
    for item in config['sources'] + [config[k] for k in ['app', 'decoder', 'ffmpeg', 'vmaf_model'] if config[k]]:
        if digest(item['path']) != item['sha256']:
            raise ValueError('input or executable changed during the run: ' + item['path'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--decoder', type=Path, required=True)
    parser.add_argument('--corpus', type=Path, required=True, help='JSON list of {path, scene}')
    parser.add_argument('--out-dir', type=Path, required=True)
    parser.add_argument('--qps', default='40,100,160,220')
    parser.add_argument('--predictions', default='all-intra,low-delay-p')
    parser.add_argument('--tools', default='00,01,10,11', help='partition/intra pairs')
    parser.add_argument('--jobs', type=int, default=1)
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--timeout', type=int, default=86400, help='seconds per command')
    parser.add_argument('--ffmpeg', type=Path)
    parser.add_argument('--vmaf-model', type=Path)
    parser.add_argument('--resume', action='store_true')
    args = parser.parse_args()
    qps = list(map(int, args.qps.split(',')))
    predictions, tools = args.predictions.split(','), args.tools.split(',')
    if (len(set(qps)) != len(qps) or not all(1 <= q <= 255 for q in qps) or
            not all(p in ['all-intra', 'low-delay-p'] for p in predictions) or
            len(set(predictions)) != len(predictions) or len(set(tools)) != len(tools) or
            not all(t in ['00', '01', '10', '11'] for t in tools) or
            min(args.jobs, args.repeats, args.timeout) < 1 or bool(args.ffmpeg) != bool(args.vmaf_model)):
        parser.error('invalid QP, prediction, tool, count or VMAF configuration')
    out = args.out_dir.resolve()
    if out.exists() and not args.resume:
        parser.error('output exists; use a fresh directory or --resume')
    out.mkdir(parents=True, exist_ok=True)
    with run_lock(out / 'run.lock'):
        def binary(path):
            return {'path': str(path.resolve()), 'sha256': digest(path)} if path else None
        sources = []
        for item in json.loads(args.corpus.read_text(encoding='utf-8')):
            path = Path(item['path'])
            if not path.is_absolute():
                path = args.corpus.resolve().parent / path
            sources.append(dict(inspect_source(path), scene=item['scene']))
        if not sources or len({s['path'] for s in sources}) != len(sources):
            parser.error('empty or duplicate corpus')
        config = {'schema': 1, 'script_sha256': digest(Path(__file__)),
                  'memory_sampler_sha256': digest(Path(__file__).with_name('verify_m4.py')),
                  'host': {'platform': platform.platform(), 'processor': platform.processor(),
                           'logical_cpus': os.cpu_count(), 'python': platform.python_version()},
                  'app': binary(args.app), 'decoder': binary(args.decoder),
                  'ffmpeg': binary(args.ffmpeg), 'vmaf_model': binary(args.vmaf_model),
                  'numpy_version': np.__version__, 'sources': sources, 'qps': qps,
                  'predictions': predictions, 'tools': tools, 'repeats': args.repeats,
                  'jobs': args.jobs, 'vmaf_input_rule': 'both inputs converted to yuv420p10le; PTS reset'}
        manifest = out / 'run-manifest.json'
        if manifest.exists() and json.loads(manifest.read_text(encoding='utf-8')) != config:
            parser.error('resume fingerprint/configuration mismatch; choose a fresh directory')
        if not manifest.exists():
            atomic_json(manifest, config)
            if args.vmaf_model:
                shutil.copyfile(args.vmaf_model, out / 'model.json')
        if args.vmaf_model and digest(out / 'model.json') != config['vmaf_model']['sha256']:
            parser.error('saved VMAF model was changed')
        run_id = digest(manifest)
        started = time.time()
        atomic_json(out / 'summary.json', {'status': 'RUNNING', 'run_id': run_id,
                                          'started_unix': started, 'pid': os.getpid()})
        maximum_raw = max(sum(dimensions(s)) * (2 if s['depth'] > 8 else 1) * s['frames'] for s in sources)
        free = shutil.disk_usage(out).free
        reserve = args.jobs * (4*maximum_raw + 16*1024*1024)
        atomic_json(out / 'resource-estimate.json', {'free_bytes_at_start': free,
                    'temporary_reserve_bytes': reserve, 'retained_streams_are_additional': True})
        if free < reserve:
            parser.error('insufficient disk reserve for requested jobs/frames; reduce jobs or use a larger output volume')
        jobs = [dict(source=s, qp=q, prediction=p, partition=int(t[0]), intra=int(t[1]))
                for s in range(len(sources)) for q in qps for p in predictions for t in tools]
        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            results = list(pool.map(lambda job: run_job(job, config, out, run_id, args.timeout), jobs))
        atomic_json(out / 'acceptance.json', results)
        status = 'PASS' if all(r['status'] == 'PASS' for r in results) else 'FAIL'
        frozen_error = None
        try:
            validate_frozen(config)
        except (OSError, ValueError) as exc:
            status, frozen_error = 'FAIL', str(exc)
        atomic_json(out / 'summary.json', {'status': status, 'run_id': run_id, 'jobs': len(jobs),
                    'started_unix': started, 'finished_unix': time.time(),
                    'frozen_dependency_error': frozen_error,
                    'passed': sum(r['status'] == 'PASS' for r in results),
                    'speed_note': 'isolated jobs within this runner' if args.jobs == 1 else
                                  'concurrent jobs; timings are not isolated encoder speed',
                    'bd_rate': summarize(results)})
        return int(status != 'PASS')


if __name__ == '__main__':
    raise SystemExit(main())
