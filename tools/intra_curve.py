"""Frozen single-picture screening curves. All original points/logs are retained.

This is development evidence, not the 50-image/20-holdout I-GATE campaign.
Quality-encode wall/CPU below include process startup and reconstruction export,
and exclude decoding/scoring;
one run per point is deliberately not a formal five-repeat speed result.
"""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

import numpy as np
from m53_batch import header, dimensions, y4m_frame, bd_rate


# (Y/UV selector, partition selector, adaptive CDF, primary TX, coefficient search)
AV2_PROFILES = {
    'legacy': ('adaptive', 'adaptive', False, 'dct', 'off'),
    'basic': ('basic', 'adaptive', False, 'dct', 'off'),
    'adaptive': ('basic', 'adaptive', True, 'dct', 'off'),
    'fine': ('basic', 'fine', True, 'dct', 'off'),
    'directional': ('directional', 'fine', True, 'dct', 'off'),
    'mixed': ('directional', 'mixed', True, 'dct', 'off'),
    'primary': ('directional', 'fine', True, 'primary', 'off'),
    'rdoq': ('directional', 'fine', True, 'dct', 'rdoq'),
    'cfl': ('cfl', 'fine', True, 'dct', 'off'),
    'tools': ('cfl', 'fine', True, 'primary', 'rdoq'),
    'combined': ('cfl', 'mixed', True, 'primary', 'rdoq'),
    'angular': ('directional', 'fine', True, 'dct', 'off'),
    'tools-angle': ('cfl', 'fine', True, 'primary', 'rdoq'),
    'combined-angle': ('cfl', 'mixed', True, 'primary', 'rdoq'),
    'rect': ('cfl', 'rect', True, 'primary', 'rdoq'),
    'rect-angle': ('cfl', 'rect', True, 'primary', 'rdoq'),
    'mrl': ('cfl', 'rect', True, 'primary', 'rdoq'),
    'full-mrl': ('cfl', 'rect', True, 'full', 'rdoq'),
    'longrect': ('cfl', 'longrect', True, 'primary', 'rdoq'),
    'longrect-full': ('cfl', 'longrect', True, 'full', 'rdoq'),
    'mixedrect-full': ('cfl', 'mixedrect', True, 'full', 'rdoq'),
    'mixed8-full': ('cfl', 'mixed8', True, 'full', 'rdoq'),
    'thinrect-full': ('cfl', 'thinrect', True, 'full', 'rdoq'),
    'mixedthin-full': ('cfl', 'mixedthin', True, 'full', 'rdoq'),
    'thin64-full': ('cfl', 'thin64', True, 'full', 'rdoq'),
}

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def save(path, value):
    temp = path.with_suffix('.tmp')
    temp.write_text(json.dumps(value, indent=2, allow_nan=False), encoding='utf-8')
    temp.replace(path)


def run(command, log, cwd, timeout=3600):
    command = [str(x) for x in command]
    started = time.perf_counter()
    with log.open('wb') as out, subprocess.Popen(command, stdout=out, stderr=subprocess.STDOUT, cwd=cwd) as proc:
        try:
            proc.wait(timeout=timeout)
            elapsed = time.perf_counter() - started
            cpu = None
            if os.name == 'nt':
                values = [wintypes.FILETIME() for _ in range(4)]
                kernel = ctypes.WinDLL('kernel32', use_last_error=True)
                kernel.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)]*4
                if not kernel.GetProcessTimes(int(proc._handle), *(ctypes.byref(v) for v in values)):
                    raise ctypes.WinError(ctypes.get_last_error())
                cpu = sum((v.dwHighDateTime << 32) | v.dwLowDateTime for v in values[2:])/10000000
            if proc.returncode:
                raise RuntimeError('command failed: ' + str(log))
        except BaseException:
            if proc.poll() is None:
                proc.kill()
                proc.wait()
            raise
    return dict(command=command, wall_seconds=elapsed, cpu_seconds=cpu)


def main():
    p = argparse.ArgumentParser()
    for name in ['manifest', 'aomenc', 'aomdec', 'av2', 'avmdec', 'ffmpeg', 'model', 'out']:
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--svtav1', type=Path)
    p.add_argument('--encode-timeout-seconds', type=int, default=10800)
    p.add_argument('--jobs', type=int, choices=range(1, 17), default=1)
    p.add_argument('--profiles', default='aom,legacy,basic')
    p.add_argument('--aom-q', default='16,24,32,40,48,56')
    p.add_argument('--av2-q', default='40,70,100,130,160,190')
    p.add_argument('--rd-lambda', choices=['original', 'avm-ai'], default='original')
    p.add_argument('--tree-refine', choices=['off', 'rd', 'wide'], default='rd')
    p.add_argument('--cfl-search', choices=['off', 'rd'], default='off')
    p.add_argument('--mhccp-search', choices=['off', 'rd'], default='off')
    p.add_argument('--intra-joint', choices=['off', 'rd'], default='off')
    args = p.parse_args()
    if args.encode_timeout_seconds < 1:
        p.error('positive encode timeout required')
    args.out = args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=False)
    script_dir = args.out/'tools'
    script_dir.mkdir()
    for helper in ['intra_curve.py', 'm53_batch.py', 'verify_m4.py', 'compare_recon.py', 'verify_m3.py', 'm4_model.py', 'verify_m2.py', 'run_matrix.py']:
        shutil.copy2(Path(__file__).with_name(helper), script_dir/helper)
    programs = {}
    for name in ['aomenc', 'aomdec', 'av2', 'avmdec', 'ffmpeg', 'svtav1']:
        original = getattr(args, name)
        if original is None:
            continue
        target = args.out/(name+original.suffix)
        shutil.copy2(original, target)
        programs[name] = dict(path=str(target), sha256=sha(target), source_path=str(original.resolve()))
    shutil.copy2(args.model, args.out/'model.json')
    qs = dict(aom=[int(q) for q in args.aom_q.split(',')], av2=[int(q) for q in args.av2_q.split(',')])
    if len(set(qs['aom'])) < 6 or len(set(qs['av2'])) < 6:
        raise ValueError('at least six independent quality points are required')
    manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
    if any(sample.get('split') == 'holdout' for sample in manifest):
        raise ValueError('development screening must not score the blind holdout')
    profiles = args.profiles.split(',')
    if not profiles or any(profile not in ['aom', 'svtav1', *AV2_PROFILES] for profile in profiles):
        raise ValueError('unknown screening profile')
    if 'svtav1' in profiles and not args.svtav1:
        raise ValueError('SVT-AV1 profile requires --svtav1')
    protocol = dict(primary='VMAF v0.6.1', secondary='PSNR-Y/U/V', quality_tolerance=0.1,
        python=sys.version, numpy=np.__version__,
        helper_sha256={name: sha(Path(__file__).with_name(name)) for name in
            ['m53_batch.py','verify_m4.py','compare_recon.py','verify_m3.py','m4_model.py','verify_m2.py','run_matrix.py']},
        matching='PCHIP log bytes, common interval, no extrapolation; retain nonmonotonic points and report N/A',
        aom_usage='--allintra is official AOM_USAGE_ALL_INTRA (the contract --ai spelling is unavailable)',
        programs=programs, model_sha256=sha(args.out/'model.json'), script_sha256=sha(Path(__file__)),
        manifest_sha256=sha(args.manifest), sources=manifest, quality_points=qs, profiles=profiles, jobs=args.jobs, encode_timeout_seconds=args.encode_timeout_seconds,
        rd_lambda=args.rd_lambda,
        tree_refinement=args.tree_refine,
        cfl_search=args.cfl_search,
        mhccp_search=args.mhccp_search,
        intra_joint=args.intra_joint,
        timing='quality runs include recon export; one process per point; no formal speed gate',
        gate_status='NOT_RUN', holdout_used=False)
    save(args.out/'protocol.json', protocol)
    def one(sample, profile, q):
        source = Path(sample['path']).resolve()
        if sha(source) != sample['sha256']:
            raise ValueError('frozen input changed')
        with source.open('rb') as f:
            meta = header(f)
            payload = y4m_frame(f, sum(dimensions(meta))*(2 if meta['depth'] > 8 else 1))
            if payload is None or f.read(1):
                raise ValueError('input must contain exactly one frame')
        name = f"{sample['id']}-{profile}-q{q}"
        work = args.out/name
        work.mkdir()
        stream, decoded, recon = work/'stream.obu', work/'decoded.yuv', work/'recon.yuv'
        if profile == 'aom':
            command = [programs['aomenc']['path'], '--allintra', '--threads=1', '--cpu-used=0',
                '--end-usage=q', f'--cq-level={q}', '--limit=1', '--passes=1',
                '--tile-columns=0', '--tile-rows=0', '--obu', f"--bit-depth={meta['depth']}",
                f"--input-bit-depth={meta['depth']}", f"--profile={1 if meta['chroma'] == '444' else 0}", '-o', stream, source]
            decoder = programs['aomdec']['path']
        elif profile == 'svtav1':
            command = [programs['svtav1']['path'], '-i', source, '-b', stream, '--obu', '--pred-struct', '0',
                '--preset', '4', '--tune', '3', '--avif', '1', '--lp', '1', '--rc', '0',
                '--aq-mode', '0', '--qp', q, '-n', '1']
            decoder = programs['aomdec']['path']
        else:
            intra, partition, adaptive, transform, coeff = AV2_PROFILES[profile]
            command = [programs['av2']['path'], '--input', source, '--output', stream,
                '--still-picture', '--threads', '1', '--qp', q, '--intra-select', intra,
                '--partition-select', partition, '--rd-mode', 'pruned', '--export-recon', recon]
            if args.rd_lambda != 'original':
                command += ['--rd-lambda', args.rd_lambda]
            if args.cfl_search != 'off':
                command += ['--cfl-search', args.cfl_search]
            if args.mhccp_search != 'off':
                command += ['--mhccp-search', args.mhccp_search]
            if args.intra_joint != 'off':
                command += ['--intra-joint', args.intra_joint]
            if adaptive:
                command += ['--entropy-adapt', 'on']
            if transform != 'dct':
                command += ['--transform-select', transform]
            if coeff != 'off':
                command += ['--coeff-opt', coeff]
            if profile in ['angular', 'tools-angle', 'combined-angle', 'rect-angle', 'mrl', 'full-mrl', 'longrect', 'longrect-full', 'mixedrect-full', 'mixed8-full', 'thinrect-full', 'mixedthin-full', 'thin64-full']:
                command += ['--angle-search', 'rd']
            if profile in ['mrl', 'full-mrl', 'longrect', 'longrect-full', 'mixedrect-full', 'mixed8-full', 'thinrect-full', 'mixedthin-full', 'thin64-full']:
                command += ['--mrl-search', 'rd']
            if profile in ['longrect', 'longrect-full', 'mixedrect-full', 'mixed8-full', 'thinrect-full', 'mixedthin-full', 'thin64-full']:
                command += ['--tree-refine', args.tree_refine]
            decoder = programs['avmdec']['path']
        encode = run(command, work/'encode.log', args.out, timeout=args.encode_timeout_seconds)
        before = sha(recon) if profile in AV2_PROFILES else None
        decode = run([decoder, '--rawvideo', f"--output-bit-depth={meta['depth']}",
                      '-o', decoded, stream], work/'decode.log', args.out)
        if before is not None and (sha(decoded) != before or sha(recon) != before):
            raise ValueError('fixed AVM reconstruction mismatch: ' + name)
        raw = decoded.read_bytes()
        if len(raw) != len(payload):
            raise ValueError('decoded pixel format, dimensions or frame count changed')
        dtype = np.uint8 if meta['depth'] == 8 else np.dtype('<u2')
        src, rec = (np.frombuffer(x, dtype=dtype).astype(np.int64) for x in [payload, raw])
        pos, sse, psnr = 0, [], []
        for count in dimensions(meta):
            delta = src[pos:pos+count]-rec[pos:pos+count]
            error = int(np.sum(delta*delta))
            sse.append(error)
            psnr.append(10*math.log10(((1 << meta['depth'])-1)**2*count/error) if error else None)
            pos += count
        pixel = 'yuv'+meta['chroma']+('p10le' if meta['depth'] == 10 else 'p')
        filters = '[0:v]setpts=PTS-STARTPTS[d];[1:v]setpts=PTS-STARTPTS[r];'
        filters += f'[d][r]libvmaf=model=path=model.json:log_fmt=json:log_path={name}/vmaf.json:n_threads=1'
        score = run([programs['ffmpeg']['path'], '-hide_banner', '-nostdin', '-v', 'error',
            '-threads', '1', '-filter_complex_threads', '1', '-f', 'rawvideo', '-pixel_format', pixel,
            '-video_size', f"{meta['width']}x{meta['height']}", '-framerate', '25', '-i', decoded,
            '-i', source, '-filter_complex', filters, '-f', 'null', '-'], work/'vmaf.log', args.out)
        metrics = json.loads((work/'vmaf.json').read_text())
        if len(metrics['frames']) != 1:
            raise ValueError('VMAF frame count differs')
        row = dict(sample=sample['id'], category=sample['category'], profile=profile, q=q,
            bytes=stream.stat().st_size, stream_sha256=sha(stream), decoded_sha256=sha(decoded),
            source_sha256=sha(source), meta=meta, sse_yuv=sse, psnr_yuv=psnr,
            vmaf=metrics['pooled_metrics']['vmaf']['mean'], encode=encode, decode=decode, score=score)
        save(work/'result.json', row)
        print(name, row['bytes'], round(row['vmaf'], 4), round(encode['wall_seconds'], 3), flush=True)
        return row
    from concurrent.futures import ThreadPoolExecutor, as_completed
    rows = []
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        jobs = [pool.submit(one, sample, profile, q) for sample in manifest
                for profile in profiles for q in qs['aom' if profile in ['aom', 'svtav1'] else 'av2']]
        for job in as_completed(jobs):
            rows.append(job.result())
            save(args.out/'results.json', dict(status='RUNNING', rows=rows))
    comparisons = []
    for sample in manifest:
        for reference, test in [('aom', 'legacy'), ('aom', 'basic'), ('legacy', 'basic'),
                                ('aom', 'adaptive'), ('basic', 'adaptive'), ('svtav1', 'adaptive'),
                                ('aom', 'fine'), ('adaptive', 'fine'),
                                ('aom', 'directional'), ('fine', 'directional'),
                                ('aom', 'mixed'), ('directional', 'mixed'),
                                ('aom', 'primary'), ('directional', 'primary'),
                                ('aom', 'rdoq'), ('directional', 'rdoq'),
                                ('aom', 'cfl'), ('directional', 'cfl'),
                                ('aom', 'tools'), ('directional', 'tools'),
                                ('aom', 'combined'), ('directional', 'combined'), ('tools', 'combined'),
                                ('aom', 'angular'), ('directional', 'angular'),
                                ('aom', 'tools-angle'), ('tools', 'tools-angle'),
                                ('aom', 'combined-angle'), ('combined', 'combined-angle')]:
            if reference not in profiles or test not in profiles:
                continue
            curves = [[(r['vmaf'], r['bytes']) for r in rows if r['sample'] == sample['id'] and r['profile'] == profile]
                      for profile in [reference, test]]
            comparisons.append(dict(sample=sample['id'], reference=reference, test=test, vmaf_bd_rate=bd_rate(*curves)))
    save(args.out/'results.json', dict(status='PASS', gate_status='NOT_RUN', rows=rows, comparisons=comparisons))


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        if '--out' in sys.argv:
            target = Path(sys.argv[sys.argv.index('--out')+1])/'results.json'
            if target.parent.exists():
                value = json.loads(target.read_text()) if target.exists() else {}
                value.update(status='FAIL', error=str(error), gate_status='NOT_RUN')
                save(target, value)
        raise
