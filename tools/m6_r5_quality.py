"""Frozen native24 motion-RD paired quality experiment; separate from timing."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import subprocess
import time

import numpy as np
from m53_batch import bd_rate
from m6_r5_prepare import CAMPAIGN, FRAME_BYTES, guard, sha


def command(args, log, cwd, deadline):
    guard()
    started = time.perf_counter()
    with log.open('wb') as output, subprocess.Popen([str(x) for x in args], stdout=output, stderr=subprocess.STDOUT, cwd=cwd) as process:
        try:
            while process.poll() is None:
                guard()
                if time.perf_counter() > deadline:
                    raise TimeoutError('quality matrix exceeded six-hour budget')
                if time.perf_counter() - started > 1800:
                    raise TimeoutError('quality command exceeded 1800 seconds')
                try:
                    process.wait(timeout=1)
                except subprocess.TimeoutExpired:
                    pass
            if process.returncode:
                raise RuntimeError('quality command failed: ' + str(log))
        except Exception:
            process.terminate()
            process.wait()
            raise
    return time.perf_counter() - started


def check_quality(source, recon, decoded):
    sums = [0, 0, 0]
    counts = [1920 * 1080, 960 * 540, 960 * 540]
    with Path(source['path']).open('rb') as original, recon.open('rb') as internal, decoded.open('rb') as oracle:
        original.readline()
        oracle.readline()
        for _ in range(source['frames']):
            if not original.readline().startswith(b'FRAME') or not oracle.readline().startswith(b'FRAME'):
                raise ValueError('quality/decoder frame marker missing')
            reference, raw, decoded_raw = original.read(FRAME_BYTES), internal.read(FRAME_BYTES), oracle.read(FRAME_BYTES)
            if len(reference) != FRAME_BYTES or raw != decoded_raw or len(raw) != FRAME_BYTES:
                raise ValueError('source frame truncated or AVM samples differ')
            a, b = np.frombuffer(reference, dtype='<u2'), np.frombuffer(raw, dtype='<u2')
            offset = 0
            for plane, count in enumerate(counts):
                delta = a[offset:offset+count].astype(np.int64) - b[offset:offset+count].astype(np.int64)
                sums[plane] += int(np.sum(delta * delta))
                offset += count
        if original.read(1) or internal.read(1) or oracle.read(1):
            raise ValueError('extra quality/decoder frame bytes')
    return dict(sse_yuv=sums, weighted_D=(4 * sum(sums)) / 16,
        psnr_yuv_db=[None if error == 0 else 10 * math.log10(1023**2 * count * source['frames'] / error)
                     for error, count in zip(sums, counts)])


def pair(source, qp, config, out, deadline):
    guard()
    if time.perf_counter() > deadline:
        raise TimeoutError('quality matrix exceeded six-hour budget')
    folder = out / f"{source['scene']}-{qp}"
    target = folder / 'result.json'
    if target.exists():
        cached = json.loads(target.read_text())
        if cached['status'] == 'PASS' and cached['config_sha256'] == config['config_sha256']:
            if all(sha(folder / mode / 'stream.av2') == cached[mode]['stream_sha256'] for mode in ['baseline', 'candidate']):
                return cached
        raise ValueError('existing case cannot be safely reused')
    folder.mkdir()
    result = dict(status='RUNNING', scene=source['scene'], qp=qp, source_sha256=source['sha256'], config_sha256=config['config_sha256'])
    try:
        for mode in ['baseline', 'candidate']:
            if sha(Path(config[mode]['path'])) != config[mode]['sha256']:
                raise ValueError('encoder changed during frozen quality experiment')
            work = folder / mode
            work.mkdir()
            stream, recon, decoded = work / 'stream.av2', work / 'recon.yuv', work / 'decoded.y4m'
            args = [config[mode]['path'], '--input', source['path'], '--output', stream, '--export-recon', recon,
                '--qp', qp, '--pred-structure', 'low-delay-p', '--intra-select', 'adaptive',
                '--partition-select', 'adaptive', '--rd-mode', 'pruned', '--coeff-opt', 'off']
            if mode == 'candidate':
                args += ['--motion-opt', 'rd']
            seconds = command(args, work / 'encode.log', out, deadline)
            snapshot = CAMPAIGN / 'snapshot'
            command([snapshot / 'decoder.exe', '--summary', '-o', decoded, stream], work / 'decode.log', out, deadline)
            quality = check_quality(source, recon, decoded)
            metric = work / 'vmaf.json'
            relative = metric.relative_to(out).as_posix()
            filters = '[0:v]setpts=PTS-STARTPTS,format=yuv420p10le[d];[1:v]setpts=PTS-STARTPTS,format=yuv420p10le[r];'
            filters += f'[d][r]libvmaf=model=path=model.json:log_fmt=json:log_path={relative}:n_threads=1'
            command([snapshot / 'ffmpeg.exe', '-hide_banner', '-nostdin', '-v', 'error', '-filter_complex_threads', '1',
                '-f', 'rawvideo', '-pixel_format', 'yuv420p10le', '-video_size', '1920x1080', '-framerate', '24000/1001',
                '-i', recon, '-i', source['path'], '-filter_complex', filters, '-f', 'null', '-'], work / 'vmaf.log', out, deadline)
            vmaf = json.loads(metric.read_text())
            if len(vmaf['frames']) != source['frames']:
                raise ValueError('VMAF frame count differs')
            trace = [{k: int(v) for k, v in re.findall(r'(\w+)=(\d+)', line)}
                for line in (work / 'encode.log').read_text().splitlines() if line.startswith('rd frame=')]
            if len(trace) != source['frames']:
                raise ValueError('missing RD frame statistics')
            result[mode] = dict(bytes=stream.stat().st_size, stream_sha256=sha(stream), recon_sha256=sha(recon),
                seconds=seconds, quality=quality, vmaf=vmaf['pooled_metrics']['vmaf']['mean'], rd_frames=trace)
            (folder / 'progress.json').write_text(json.dumps(result, indent=2) + '\n')
            decoded.unlink()
            recon.unlink()
        result['status'] = 'PASS'
    except Exception as error:
        result.update(status='FAIL', error=str(error))
        raise
    finally:
        target.write_text(json.dumps(result, indent=2) + '\n')
    return result


def run(manifest_path, candidate, out, jobs, first_pair):
    guard()
    manifest = json.loads(manifest_path.read_text())
    for source in manifest['sources']:
        if sha(Path(source['path'])) != source['sha256']:
            raise ValueError('frozen native sequence changed')
    config = dict(manifest_sha256=sha(manifest_path), baseline=manifest['baseline'],
                  candidate=dict(path=str(candidate), sha256=sha(candidate)),
                  script_dependencies={str(p):sha(p) for p in [Path(__file__), Path(__file__).with_name('m6_r5_prepare.py'),
                                                              Path(__file__).with_name('m53_batch.py')]},
                  dependencies={str(p):sha(p) for p in [CAMPAIGN / 'snapshot' / n for n in ['decoder.exe', 'ffmpeg.exe', 'model.json']]})
    if sha(Path(config['baseline']['path'])) != config['baseline']['sha256']:
        raise ValueError('baseline changed')
    config['config_sha256'] = hashlib.sha256(json.dumps(config, sort_keys=True).encode()).hexdigest()
    out.mkdir(parents=True, exist_ok=True)
    config_path = out / 'config.json'
    if config_path.exists() and json.loads(config_path.read_text()) != config:
        raise ValueError('cannot change frozen quality configuration')
    config_path.write_text(json.dumps(config, indent=2) + '\n')
    shutil.copyfile(CAMPAIGN / 'snapshot/model.json', out / 'model.json')
    cases = [(source, qp) for source in manifest['sources'] for qp in manifest['qps']]
    if first_pair:
        cases = cases[:1]
    started, rows = time.perf_counter(), []
    report = dict(status='RUNNING', config=config, jobs=jobs, timing_scope='quality workflow includes recon export; not the independent speed experiment', cases=rows)
    try:
        with ThreadPoolExecutor(max_workers=jobs) as pool:
            futures = [pool.submit(pair, s, q, config, out, started + 6*3600) for s, q in cases]
            for future in futures:
                if time.perf_counter() - started > 6 * 3600:
                    raise TimeoutError('quality matrix exceeded six-hour budget')
                rows.append(future.result(timeout=max(1, 6*3600-(time.perf_counter()-started))))
                (out / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
        report['bd_rate'] = []
        if not first_pair:
            for source in manifest['sources']:
                for metric in ['vmaf', 'psnr_y']:
                    selected = [r for r in rows if r['scene'] == source['scene']]
                    curves = [[(r[mode]['vmaf'] if metric == 'vmaf' else r[mode]['quality']['psnr_yuv_db'][0], r[mode]['bytes'])
                              for r in selected] for mode in ['baseline', 'candidate']]
                    value = bd_rate(*curves) if all(q is not None for c in curves for q, _ in c) else dict(percent=None,reason='infinite PSNR')
                    report['bd_rate'].append(dict(scene=source['scene'], metric=metric, **value))
        report['status'] = 'PREFLIGHT_PASS' if first_pair else 'PASS'
    except Exception as error:
        report.update(status='FAIL', error=str(error))
        raise
    finally:
        (out / 'result.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--jobs', type=int, choices=[1, 2], default=1)
    parser.add_argument('--first-pair', action='store_true')
    args = parser.parse_args()
    run(args.manifest, args.candidate, args.out, args.jobs, args.first_pair)
