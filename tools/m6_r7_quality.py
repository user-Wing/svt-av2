"""RD-off adaptive entropy pairs with exact unchanged-reconstruction gates."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import shutil
import time

from m6_r5_prepare import CAMPAIGN, guard, sha
from m6_r5_quality import command, check_quality


def pair(source, qp, config, out, deadline):
    guard()
    folder = out / f"{source['scene']}-{qp}"
    target = folder / 'result.json'
    if target.exists():
        cached = json.loads(target.read_text())
        if cached['status']=='PASS' and cached['config_sha256']==config['config_sha256'] and all(
                sha(folder/mode/'stream.av2')==cached[mode]['stream_sha256'] for mode in ['baseline','candidate']):
            return cached
        raise ValueError('existing R7 case is not safely reusable')
    folder.mkdir()
    result = dict(status='RUNNING',scene=source['scene'],qp=qp,source_sha256=source['sha256'],config_sha256=config['config_sha256'])
    try:
        for mode in ['baseline','candidate']:
            program = config[mode]
            if sha(Path(program['path']))!=program['sha256']:
                raise ValueError('R7 paired program changed')
            work=folder/mode
            work.mkdir()
            stream,recon,decoded=work/'stream.av2',work/'recon.yuv',work/'decoded.y4m'
            args=[program['path'],'--input',source['path'],'--output',stream,'--export-recon',recon,
                '--qp',qp,'--pred-structure','low-delay-p','--intra-select','adaptive',
                '--partition-select','adaptive','--rd-mode','off','--coeff-opt','off','--motion-opt','off']
            if mode=='candidate':
                args+=['--entropy-adapt','on']
            seconds=command(args,work/'encode.log',out,deadline)
            command([CAMPAIGN/'snapshot/decoder.exe','--summary','-o',decoded,stream],work/'decode.log',out,deadline)
            quality=check_quality(source,recon,decoded)
            fingerprint=sha(recon)
            if mode=='candidate':
                if fingerprint!=result['baseline']['recon_sha256'] or quality!=result['baseline']['quality']:
                    raise ValueError('entropy-only experiment changed reconstructed samples')
                score=result['baseline']['vmaf']
            else:
                relative=(work/'vmaf.json').relative_to(out).as_posix()
                filters='[0:v]setpts=PTS-STARTPTS,format=yuv420p10le[d];[1:v]setpts=PTS-STARTPTS,format=yuv420p10le[r];'
                filters+=f'[d][r]libvmaf=model=path=model.json:log_fmt=json:log_path={relative}:n_threads=1'
                command([CAMPAIGN/'snapshot/ffmpeg.exe','-hide_banner','-nostdin','-v','error','-filter_complex_threads','1',
                    '-f','rawvideo','-pixel_format','yuv420p10le','-video_size','1920x1080','-framerate','24000/1001',
                    '-i',recon,'-i',source['path'],'-filter_complex',filters,'-f','null','-'],work/'vmaf.log',out,deadline)
                metric=json.loads((work/'vmaf.json').read_text())
                if len(metric['frames'])!=source['frames']:
                    raise ValueError('R7 baseline VMAF frame count differs')
                score=metric['pooled_metrics']['vmaf']['mean']
            if any(line.startswith('rd frame=') for line in (work/'encode.log').read_text().splitlines()):
                raise ValueError('RD-off experiment emitted RD decisions')
            result[mode]=dict(bytes=stream.stat().st_size,stream_sha256=sha(stream),recon_sha256=fingerprint,
                seconds=seconds,quality=quality,vmaf=score,
                quality_reused_from_baseline=(mode=='candidate'),avm_samples_match=True)
            (folder/'progress.json').write_text(json.dumps(result,indent=2)+'\n')
            recon.unlink()
            decoded.unlink()
        result['byte_change_percent']=100*(result['candidate']['bytes']/result['baseline']['bytes']-1)
        result['status']='PASS'
    except Exception as error:
        result.update(status='FAIL',error=str(error))
        raise
    finally:
        target.write_text(json.dumps(result,indent=2)+'\n')
    return result


def run(args):
    guard()
    manifest=json.loads(args.manifest.read_text())
    if manifest['qps']!=[80,100,120,140] or len(manifest['sources'])!=3:
        raise ValueError('R7 preselected source/QP matrix differs')
    for source in manifest['sources']:
        if sha(Path(source['path']))!=source['sha256']:
            raise ValueError('R7 frozen sequence changed')
    config=dict(manifest_sha256=sha(args.manifest),baseline=dict(path=str(args.baseline),sha256=sha(args.baseline)),
        candidate=dict(path=str(args.candidate),sha256=sha(args.candidate)),
        flags=dict(rd='off',coeff='off',motion='off',entropy_baseline='off',entropy_candidate='on'),
        contract=dict(path=str(args.contract),sha256=sha(args.contract)),
        script_dependencies={str(p):sha(p) for p in [Path(__file__),Path(__file__).with_name('m6_r5_quality.py'),
            Path(__file__).with_name('m6_r5_prepare.py'),Path(__file__).with_name('m53_batch.py')]},
        dependencies={str(p):sha(p) for p in [CAMPAIGN/'snapshot'/n for n in ['decoder.exe','ffmpeg.exe','model.json']]})
    config['config_sha256']=hashlib.sha256(json.dumps(config,sort_keys=True).encode()).hexdigest()
    args.out.mkdir(parents=True,exist_ok=True)
    target=args.out/'config.json'
    if target.exists() and json.loads(target.read_text())!=config:
        raise ValueError('cannot change frozen R7 quality configuration')
    target.write_text(json.dumps(config,indent=2)+'\n')
    shutil.copyfile(CAMPAIGN/'snapshot/model.json',args.out/'model.json')
    cases=[(source,qp) for source in manifest['sources'] for qp in manifest['qps']]
    if args.first_pair:
        cases=cases[:1]
    report=dict(status='RUNNING',config=config,jobs=args.jobs,cases=[])
    started=time.perf_counter()
    try:
        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            futures=[pool.submit(pair,s,q,config,args.out,started+6*3600) for s,q in cases]
            for future in futures:
                report['cases'].append(future.result(timeout=max(1,6*3600-(time.perf_counter()-started))))
                (args.out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
        report['status']='PREFLIGHT_PASS' if args.first_pair else 'PASS'
    except Exception as error:
        report.update(status='FAIL',error=str(error))
        raise
    finally:
        (args.out/'result.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__)
    for name in ['manifest','baseline','candidate','contract','out']:
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--jobs',type=int,choices=[1,2],default=1)
    parser.add_argument('--first-pair',action='store_true')
    run(parser.parse_args())
