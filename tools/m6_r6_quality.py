"""R6 wide-motion candidates against verified R5 RD streams; no baseline encode."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import re
import shutil
import time

from m53_batch import bd_rate
from m6_r5_prepare import CAMPAIGN, guard, sha
from m6_r5_quality import check_quality, command


def candidate_case(source, old, config, out, deadline):
    guard()
    if time.perf_counter() > deadline:
        raise TimeoutError('R6 quality six-hour budget exceeded')
    if sha(Path(config['candidate']['path'])) != config['candidate']['sha256']:
        raise ValueError('R6 candidate changed')
    folder = out / f"{source['scene']}-{old['qp']}"
    folder.mkdir()
    result = dict(status='RUNNING', scene=source['scene'], qp=old['qp'], source_sha256=source['sha256'],
                  config_sha256=config['config_sha256'], baseline=old['candidate'])
    try:
        stream, recon, decoded = folder / 'stream.av2', folder / 'recon.yuv', folder / 'decoded.y4m'
        seconds = command([config['candidate']['path'], '--input', source['path'], '--output', stream,
            '--export-recon', recon, '--qp', old['qp'], '--pred-structure', 'low-delay-p',
            '--intra-select', 'adaptive', '--partition-select', 'adaptive', '--rd-mode', 'pruned',
            '--coeff-opt', 'off', '--motion-opt', 'wide'], folder / 'encode.log', out, deadline)
        snapshot = CAMPAIGN / 'snapshot'
        command([snapshot / 'decoder.exe', '--summary', '-o', decoded, stream], folder / 'decode.log', out, deadline)
        quality = check_quality(source, recon, decoded)
        relative = (folder / 'vmaf.json').relative_to(out).as_posix()
        filters = '[0:v]setpts=PTS-STARTPTS,format=yuv420p10le[d];[1:v]setpts=PTS-STARTPTS,format=yuv420p10le[r];'
        filters += f'[d][r]libvmaf=model=path=model.json:log_fmt=json:log_path={relative}:n_threads=1'
        command([snapshot / 'ffmpeg.exe', '-hide_banner', '-nostdin', '-v', 'error', '-filter_complex_threads', '1',
            '-f', 'rawvideo', '-pixel_format', 'yuv420p10le', '-video_size', '1920x1080', '-framerate', '24000/1001',
            '-i', recon, '-i', source['path'], '-filter_complex', filters, '-f', 'null', '-'], folder / 'vmaf.log', out, deadline)
        metric = json.loads((folder / 'vmaf.json').read_text())
        if len(metric['frames']) != source['frames']:
            raise ValueError('R6 VMAF frame count differs')
        trace = [{k:int(v) for k,v in re.findall(r'(\w+)=(\d+)', line)}
                 for line in (folder / 'encode.log').read_text().splitlines() if line.startswith('rd frame=')]
        if len(trace) != source['frames']:
            raise ValueError('R6 RD frame count differs')
        result['candidate'] = dict(bytes=stream.stat().st_size, stream_sha256=sha(stream), recon_sha256=sha(recon),
            seconds=seconds, quality=quality, vmaf=metric['pooled_metrics']['vmaf']['mean'], rd_frames=trace)
        result['status'] = 'PASS'
        decoded.unlink()
        recon.unlink()
    except Exception as error:
        result.update(status='FAIL', error=str(error))
        raise
    finally:
        (folder / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


def run(manifest_path, r5_result_path, candidate, out):
    guard()
    old = json.loads(r5_result_path.read_text())
    manifest = json.loads(manifest_path.read_text())
    if old['status'] != 'PASS' or len(old['cases']) != 12 or old['config']['manifest_sha256'] != sha(manifest_path):
        raise ValueError('complete frozen R5 PASS with matching manifest is required')
    expected = {(s['scene'], q) for s in manifest['sources'] for q in manifest['qps']}
    if {(p['scene'], p['qp']) for p in old['cases']} != expected:
        raise ValueError('R5 matrix coverage differs')
    source_map = {s['scene']:s for s in manifest['sources']}
    for source in manifest['sources']:
        if sha(Path(source['path'])) != source['sha256']:
            raise ValueError('frozen sequence changed')
    retained = []
    for pair in old['cases']:
        case = r5_result_path.parent / f"{pair['scene']}-{pair['qp']}"
        path = case / 'candidate/stream.av2'
        if sha(path) != pair['candidate']['stream_sha256']:
            raise ValueError('R5 retained candidate stream changed')
        verified = json.loads((case / 'result.json').read_text())
        if verified['status'] != 'PASS' or verified['candidate'] != pair['candidate']:
            raise ValueError('R5 case quality/trace differs from complete report')
        retained.append(dict(case_result=str(case / 'result.json'), case_sha256=sha(case / 'result.json'),
                             stream=str(path), stream_sha256=sha(path)))
    config = dict(manifest_sha256=sha(manifest_path), r5_result_sha256=sha(r5_result_path),
        baseline=dict(**old['config']['candidate'], args=['--motion-opt','rd']),
        candidate=dict(path=str(candidate), sha256=sha(candidate), args=['--motion-opt','wide']),
        reused_baseline_streams=retained, dependencies=old['config']['dependencies'],
        script_dependencies={str(p):sha(p) for p in [Path(__file__), Path(__file__).with_name('m6_r5_quality.py'),
            Path(__file__).with_name('m6_r5_prepare.py'), Path(__file__).with_name('m53_batch.py')]})
    for name, expected_hash in config['dependencies'].items():
        if sha(Path(name)) != expected_hash:
            raise ValueError('R5 scoring dependency changed')
    config['config_sha256'] = hashlib.sha256(json.dumps(config,sort_keys=True).encode()).hexdigest()
    out.mkdir(parents=True,exist_ok=False)
    (out/'config.json').write_text(json.dumps(config,indent=2)+'\n')
    shutil.copyfile(r5_result_path,out/'frozen-r5-result.json')
    shutil.copyfile(manifest_path,out/'manifest.json')
    shutil.copyfile(CAMPAIGN/'snapshot/model.json',out/'model.json')
    report = dict(status='RUNNING',config=config,jobs=2,baseline_reused=True,cases=[])
    started=time.perf_counter()
    try:
        with ThreadPoolExecutor(max_workers=2) as pool:
            futures=[pool.submit(candidate_case,source_map[p['scene']],p,config,out,started+6*3600) for p in old['cases']]
            for future in futures:
                report['cases'].append(future.result(timeout=max(1,6*3600-(time.perf_counter()-started))))
                (out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
        report['bd_rate']=[]
        for scene in source_map:
            rows=[p for p in report['cases'] if p['scene']==scene]
            for metric in ['vmaf','psnr_y']:
                curves=[[(p[mode]['vmaf'] if metric=='vmaf' else p[mode]['quality']['psnr_yuv_db'][0],p[mode]['bytes'])
                         for p in rows] for mode in ['baseline','candidate']]
                value=bd_rate(*curves) if all(q is not None for c in curves for q,_ in c) else dict(percent=None,reason='infinite PSNR')
                report['bd_rate'].append(dict(scene=scene,metric=metric,**value))
        report['status']='PASS'
    except Exception as error:
        report.update(status='FAIL',error=str(error))
        raise
    finally:
        (out/'result.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--manifest',type=Path,required=True)
    parser.add_argument('--r5-result',type=Path,required=True)
    parser.add_argument('--candidate',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    run(args.manifest,args.r5_result,args.candidate,args.out)
