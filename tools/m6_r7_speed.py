"""Fresh RD-off entropy-off/on paired timings; distinct from motion timings."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import statistics
import time

from m6_r5_prepare import guard, sha
from m6_r5_speed import measure


def run(args):
    guard()
    quality=json.loads(args.quality_result.read_text())
    manifest=json.loads(args.manifest.read_text())
    if quality['status']!='PASS' or len(quality['cases'])!=12 or quality['config']['manifest_sha256']!=sha(args.manifest):
        raise ValueError('complete matching R7 quality PASS required')
    programs={variant:dict(quality['config'][key]) for variant,key in [('A','baseline'),('B','candidate')]}
    for program in programs.values():
        if sha(Path(program['path']))!=program['sha256']:
            raise ValueError('R7 program changed after quality validation')
    expected_streams={(p['scene'],variant):p[key]['stream_sha256'] for p in quality['cases'] if p['qp']==100
                      for variant,key in [('A','baseline'),('B','candidate')]}
    if any((source['scene'],variant) not in expected_streams for source in manifest['sources'] for variant in ['A','B']):
        raise ValueError('R7 QP100 quality coverage missing')
    args.out.mkdir(parents=True,exist_ok=False)
    report=dict(status='RUNNING',manifest_sha256=sha(args.manifest),quality_result_sha256=sha(args.quality_result),
        programs=programs,contract=quality['config']['contract'],order='ABBAAB',logical_cpus=os.cpu_count(),
        script_dependencies={str(p):sha(p) for p in [Path(__file__),Path(__file__).with_name('m6_r5_speed.py'),
            Path(__file__).with_name('m6_r5_prepare.py')]},
        flags=dict(rd='off',coeff='off',motion='off'),whole_machine_isolated=False,
        timing_scope='RD-off CLI input and bitstream only; no recon/decoder/VMAF/build; not divided by RD-on motion timings',
        warmups=[],runs=[])
    started=time.perf_counter()
    try:
        for source in manifest['sources']:
            if sha(Path(source['path']))!=source['sha256']:
                raise ValueError('R7 frozen source changed')
            fingerprints={}
            for warmup,order in [(True,'AB'),(False,'ABBAAB')]:
                for index,variant in enumerate(order):
                    program=programs[variant]
                    if sha(Path(program['path']))!=program['sha256']:
                        raise ValueError('R7 program changed during speed window')
                    name=f"{source['scene']}-{'warmup-' if warmup else ''}{index}-{variant}"
                    stream=args.out/(name+'.av2')
                    command=[program['path'],'--input',source['path'],'--output',str(stream),'--qp','100',
                        '--pred-structure','low-delay-p','--intra-select','adaptive','--partition-select','adaptive',
                        '--rd-mode','off','--coeff-opt','off','--motion-opt','off']
                    if variant=='B':
                        command+=['--entropy-adapt','on']
                    if warmup:
                        command+=['--frames','2']
                    item=measure(command,args.out/(name+'.log'),1800,started+6*3600)
                    fingerprint=sha(stream)
                    item.update(scene=source['scene'],order_index=index,variant=variant,command=command,
                        command_sha256=hashlib.sha256(json.dumps(command).encode()).hexdigest(),
                        stream_sha256=fingerprint,bytes=stream.stat().st_size,
                        fps=(2 if warmup else source['frames'])/item['wall_seconds'])
                    if warmup:
                        item['excluded_from_paired_timing']=True
                        report['warmups'].append(item)
                    else:
                        if fingerprint!=expected_streams[(source['scene'],variant)]:
                            raise ValueError('R7 timed stream differs from verified QP100 quality stream')
                        if variant in fingerprints and fingerprints[variant]!=fingerprint:
                            raise ValueError('R7 repeated variant stream differs')
                        fingerprints[variant]=fingerprint
                        report['runs'].append(item)
                    (args.out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
        report['paired']=[]
        for scene in ['2D','3D','Real']:
            runs=[r for r in report['runs'] if r['scene']==scene]
            pairs=[]
            for index in [0,2,4]:
                a,b=sorted(runs[index:index+2],key=lambda r:r['variant'])
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
        (args.out/'result.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__)
    for name in ['manifest','quality-result','out']:
        parser.add_argument('--'+name,type=Path,required=True)
    run(parser.parse_args())
