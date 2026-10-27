"""Fresh R5-rd versus R6-wide paired timings; both flags are explicit."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import statistics
import time

from m6_r5_prepare import guard, sha
from m6_r5_speed import measure


def run(manifest_path, baseline, candidate, out):
    guard()
    manifest = json.loads(manifest_path.read_text())
    out.mkdir(parents=True, exist_ok=False)
    report = dict(status='RUNNING', manifest_sha256=sha(manifest_path), order='ABBAAB',
        baseline=dict(path=str(baseline), sha256=sha(baseline), motion_opt='rd'),
        candidate=dict(path=str(candidate), sha256=sha(candidate), motion_opt='wide'),
        script_dependencies={str(p):sha(p) for p in [Path(__file__),Path(__file__).with_name('m6_r5_speed.py')]},
        logical_cpus=os.cpu_count(), whole_machine_isolated=False,
        timing_scope='CLI input and bitstream output only; no recon/decoder/VMAF/new builds; original campaign remains active',
        warmups=[], runs=[])
    started = time.perf_counter()
    try:
        for source in manifest['sources']:
            if sha(Path(source['path'])) != source['sha256']:
                raise ValueError('frozen sequence changed')
            first_hash = {}
            for warmup, order in [(True,'AB'),(False,'ABBAAB')]:
                for index, variant in enumerate(order):
                    program = report['baseline'] if variant=='A' else report['candidate']
                    if sha(Path(program['path'])) != program['sha256']:
                        raise ValueError('paired speed program changed')
                    name = f"{source['scene']}-{'warmup-' if warmup else ''}{index}-{variant}"
                    stream = out / (name+'.av2')
                    command = [program['path'],'--input',source['path'],'--output',str(stream),'--qp','100',
                        '--pred-structure','low-delay-p','--intra-select','adaptive','--partition-select','adaptive',
                        '--rd-mode','pruned','--coeff-opt','off','--motion-opt',program['motion_opt']]
                    if warmup:
                        command += ['--frames','2']
                    item = measure(command,out/(name+'.log'),1800,started+6*3600)
                    fingerprint = sha(stream)
                    item.update(scene=source['scene'],order_index=index,variant=variant,command=command,
                        command_sha256=hashlib.sha256(json.dumps(command).encode()).hexdigest(),
                        stream_sha256=fingerprint,bytes=stream.stat().st_size,
                        fps=(2 if warmup else source['frames'])/item['wall_seconds'])
                    if warmup:
                        item['excluded_from_paired_timing']=True
                        report['warmups'].append(item)
                    else:
                        if variant in first_hash and first_hash[variant]!=fingerprint:
                            raise ValueError('paired timing variant is not deterministic')
                        first_hash[variant]=fingerprint
                        report['runs'].append(item)
                    (out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
        report['paired']=[]
        for scene in ['2D','3D','Real']:
            rows=[r for r in report['runs'] if r['scene']==scene]
            pairs=[]
            for index in [0,2,4]:
                a,b=sorted(rows[index:index+2],key=lambda r:r['variant'])
                pairs.append(dict(wall_ratio_B_A=b['wall_seconds']/a['wall_seconds'],
                                  cpu_ratio_B_A=b['process_cpu_seconds']/a['process_cpu_seconds']))
            report['paired'].append(dict(scene=scene,pairs=pairs,
                median_wall_ratio_B_A=statistics.median(p['wall_ratio_B_A'] for p in pairs),
                wall_ratio_min=min(p['wall_ratio_B_A'] for p in pairs),wall_ratio_max=max(p['wall_ratio_B_A'] for p in pairs),
                median_cpu_ratio_B_A=statistics.median(p['cpu_ratio_B_A'] for p in pairs)))
        report['status']='PASS'
    except Exception as error:
        report.update(status='FAIL',error=str(error))
        raise
    finally:
        (out/'result.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__)
    for name in ['manifest','baseline','candidate','out']:
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    run(args.manifest,args.baseline,args.candidate,args.out)
