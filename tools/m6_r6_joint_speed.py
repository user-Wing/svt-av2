"""Fresh three-version Latin timing window; no quality workflow overlaps."""
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
    manifest = json.loads(args.manifest.read_text())
    protocol = json.loads(args.protocol.read_text())
    if protocol['order'] != ['ABC','BCA','CAB'] or protocol['manifest_sha256'] != sha(args.manifest):
        raise ValueError('joint timing protocol/order differs')
    if protocol['contract']['sha256'] != sha(args.contract):
        raise ValueError('joint timing contract differs')
    r5, r6 = [json.loads(p.read_text()) for p in [args.r5_result, args.r6_result]]
    if r5['status'] != 'PASS' or r6['status'] != 'PASS':
        raise ValueError('complete R5 and R6 quality PASS required before timing')
    if sha(args.a) != manifest['baseline']['sha256'] or sha(args.b) != r5['config']['candidate']['sha256']:
        raise ValueError('A/R4 or B/R5 program differs from frozen quality experiment')
    if sha(args.c) != r6['config']['candidate']['sha256'] or sha(args.b) != r6['config']['baseline']['sha256']:
        raise ValueError('C/R6 or shared B/R5 program differs from quality experiment')
    if r5['config']['manifest_sha256'] != sha(args.manifest) or r6['config']['manifest_sha256'] != sha(args.manifest):
        raise ValueError('Latin timing sequences differ from quality experiment')
    args.out.mkdir(parents=True, exist_ok=False)
    programs = {variant:dict(path=str(path), sha256=sha(path), extra_args=extra)
                for variant,path,extra in [('A',args.a,[]),('B',args.b,['--motion-opt','rd']),
                                           ('C',args.c,['--motion-opt','wide'])]}
    if any(programs[key]['sha256'] != protocol['programs'][key]['sha256'] for key in programs):
        raise ValueError('joint timing program differs from premeasurement protocol')
    report = dict(status='RUNNING', manifest_sha256=sha(args.manifest),
        protocol_sha256=sha(args.protocol),
        quality_reports={str(p):sha(p) for p in [args.r5_result,args.r6_result]},
        contract=dict(path=str(args.contract),sha256=sha(args.contract)),
        script_dependencies={str(p):sha(p) for p in [Path(__file__),Path(__file__).with_name('m6_r5_speed.py')]},
        programs=programs, logical_cpus=os.cpu_count(), whole_machine_isolated=False,
        order=['ABC','BCA','CAB'], warmups=[], runs=[],
        protocol='premeasurement revision: one fresh Latin window supersedes two ABBAAB windows; no old timing reused',
        timing_scope='CLI input and bitstream only; no recon/decoder/VMAF/new builds; frozen campaign still running')
    started = time.perf_counter()
    try:
        for source in manifest['sources']:
            if sha(Path(source['path'])) != source['sha256']:
                raise ValueError('frozen native sequence changed')
            fingerprints = {}
            for round_index,order in [(-1,'ABC')]+list(enumerate(report['order'])):
                warmup = round_index == -1
                for position,variant in enumerate(order):
                    program = programs[variant]
                    if sha(Path(program['path'])) != program['sha256']:
                        raise ValueError('program changed in Latin timing window')
                    name=f"{source['scene']}-{'warmup' if warmup else round_index}-{position}-{variant}"
                    stream=args.out/(name+'.av2')
                    command=[program['path'],'--input',source['path'],'--output',str(stream),'--qp','100',
                        '--pred-structure','low-delay-p','--intra-select','adaptive','--partition-select','adaptive',
                        '--rd-mode','pruned','--coeff-opt','off']+program['extra_args']
                    if warmup:
                        command+=['--frames','2']
                    item=measure(command,args.out/(name+'.log'),1800,started+6*3600)
                    fingerprint=sha(stream)
                    item.update(scene=source['scene'],round=round_index,position=position,variant=variant,
                        command=command,command_sha256=hashlib.sha256(json.dumps(command).encode()).hexdigest(),
                        stream_sha256=fingerprint,bytes=stream.stat().st_size,
                        fps=(2 if warmup else source['frames'])/item['wall_seconds'])
                    if warmup:
                        item['excluded_from_paired_timing']=True
                        report['warmups'].append(item)
                    else:
                        if variant in fingerprints and fingerprints[variant]!=fingerprint:
                            raise ValueError('Latin variant bitstream is not repeatable')
                        fingerprints[variant]=fingerprint
                        report['runs'].append(item)
                    (args.out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
        report['paired']=[]
        for scene in ['2D','3D','Real']:
            rounds=[]
            for round_index in range(3):
                data={r['variant']:r for r in report['runs'] if r['scene']==scene and r['round']==round_index}
                ratios={}
                for metric,key in [('wall_seconds','wall'),('process_cpu_seconds','cpu')]:
                    ratios[key]={pair:data[pair[0]][metric]/data[pair[1]][metric] for pair in ['BA','CB','CA']}
                rounds.append(dict(round=round_index,**ratios))
            summary=dict(scene=scene,rounds=rounds)
            for metric in ['wall','cpu']:
                summary[metric]={pair:dict(median=statistics.median(r[metric][pair] for r in rounds),
                    min=min(r[metric][pair] for r in rounds),max=max(r[metric][pair] for r in rounds))
                    for pair in ['BA','CB','CA']}
            report['paired'].append(summary)
        report['status']='PASS'
    except Exception as error:
        report.update(status='FAIL',error=str(error))
        raise
    finally:
        (args.out/'result.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__)
    for name in ['manifest','protocol','r5-result','r6-result','a','b','c','contract','out']:
        parser.add_argument('--'+name,type=Path,required=True)
    run(parser.parse_args())
