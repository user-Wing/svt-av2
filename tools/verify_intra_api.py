"""Single-picture API/CLI round trips and frozen multi-frame compatibility."""
import argparse
import json
from pathlib import Path

from intra_curve import run, sha, save
from verify_m4 import generate_frames, write_frames


def main():
    p = argparse.ArgumentParser()
    for name in ['app', 'baseline', 'decoder', 'out']:
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--intra-modes', default='basic,directional')
    p.add_argument('--partitions', default='adaptive,fine')
    p.add_argument('--transforms', default='dct')
    p.add_argument('--coeff-opt', choices=['off','rdoq'], default='off')
    p.add_argument('--angle-search', choices=['off','rd'], default='off')
    args = p.parse_args()
    if any(x not in ['adaptive','fine','mixed','rect'] for x in args.partitions.split(',')):
        raise ValueError('unknown partition profile')
    if any(x not in ['dct','primary'] for x in args.transforms.split(',')):
        raise ValueError('unknown transform profile')
    args.out = args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=False)
    rows = []
    result = dict(status='RUNNING', app_sha256=sha(args.app), baseline_sha256=sha(args.baseline),
                  decoder_sha256=sha(args.decoder), rows=rows)
    for depth, chroma in [(8,'420'), (10,'420'), (10,'444')]:
        for width, height in [(8,8), (24,24), (128,128), (136,72)]:
            case = args.out/f'{depth}-{chroma}-{width}x{height}'
            case.mkdir()
            source = case/'input.y4m'
            write_frames(source,width,height,depth,chroma,generate_frames(width,height,3,depth,chroma,'mixed'))
            for intra_mode in args.intra_modes.split(','):
                for partition in args.partitions.split(','):
                    for transform in args.transforms.split(','):
                        for qp in [1,100,255]:
                            for adaptive in ['off','on']:
                                tag=f'still-q{qp}-{adaptive}'+('-'+partition if partition!='adaptive' else '')+('-'+intra_mode if intra_mode!='basic' else '')+('-primary' if transform=='primary' else '')+('-rdoq' if args.coeff_opt=='rdoq' else '')+('-angles' if args.angle_search=='rd' else '')
                                stream, recon, decoded = (case/(tag+ext) for ext in ['.av2','.recon.yuv','.decoded.yuv'])
                                command = [args.app.resolve(),'--input',source,'--output',stream,'--export-recon',recon,
                                    '--still-picture','--intra-select',intra_mode,'--partition-select',partition,
                                    '--rd-mode','pruned','--entropy-adapt',adaptive,'--qp',qp,'--transform-select',transform,'--coeff-opt',args.coeff_opt,'--angle-search',args.angle_search]
                                encoding=run(command,case/(tag+'.encode.log'),args.out)
                                expected=sha(recon)
                                decoding=run([args.decoder.resolve(),'--rawvideo',f'--output-bit-depth={depth}',
                                    '-o',decoded,stream],case/(tag+'.decode.log'),args.out)
                                size=width*height*(3 if chroma=='444' else 1.5)*(2 if depth==10 else 1)
                                if sha(decoded)!=expected or recon.stat().st_size!=size or sha(recon)!=expected:
                                    raise ValueError('single-picture mismatch: '+str(stream))
                                data=stream.read_bytes()
                                if data[0:2]!=bytes([1,8]) or not (data[4]&2):
                                    raise ValueError('single-picture sequence flag not set')
                                rows.append(dict(case=str(stream.relative_to(args.out)),encode=encoding,decode=decoding,
                                                 source_sha256=sha(source),stream_sha256=sha(stream),recon_sha256=expected))
            if width>=128:
                for profile, extra in [('dc',[]),('legacy-rd',['--intra-select','adaptive','--partition-select','adaptive','--rd-mode','pruned']),
                                       ('legacy-cdf',['--intra-select','adaptive','--partition-select','adaptive','--entropy-adapt','on'])]:
                    outputs=[]
                    for label,app in [('baseline',args.baseline),('candidate',args.app)]:
                        stream=case/(profile+'-'+label+'.av2'); recon=stream.with_suffix('.yuv')
                        run([app.resolve(),'--input',source,'--output',stream,'--export-recon',recon]+extra,
                            stream.with_suffix('.log'),args.out)
                        outputs.append((sha(stream),sha(recon)))
                    if outputs[0]!=outputs[1]:
                        raise ValueError('frozen compatibility mismatch: '+str(case)+'/'+profile)
                    rows.append(dict(case=str(case.relative_to(args.out))+'/'+profile,compatibility='PASS',
                                     stream_sha256=outputs[0][0],recon_sha256=outputs[0][1]))
            save(args.out/'result.json',result)
    result['status']='PASS'
    save(args.out/'result.json',result)
    print('PASS',len(rows),'single-picture/compatibility cases')


if __name__ == '__main__':
    main()
