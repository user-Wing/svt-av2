"""Check fixed-decoder actual qindex/CDF initialization/nonlossless/large levels."""
import argparse
import json
import re
from pathlib import Path


def verify(text, qp, depth, frames, require_high=False):
    actual = qp + (48 if depth == 10 else 0)
    expected_ctx = 0 if actual <= 90 else 1 if actual <= 140 else 2 if actual <= 190 else 3
    first_eob16 = (1946,1839,3946,5029)
    errors, headers, cdfs, levels = [], [], [], []
    for line in text.splitlines():
        if not line.startswith(('M53FRAME ', 'M53CDF ', 'M53LEVEL ')):
            continue
        row = {k:int(v) for k,v in re.findall(r'(\w+)=(-?\d+)',line)}
        if line.startswith('M53FRAME '):
            headers.append(row)
            if (row['qindex'],row['bd'],row['lossless']) != (actual,depth,0):
                errors.append(row)
        elif line.startswith('M53CDF '):
            q = row['qindex']
            ctx = 0 if q <= 90 else 1 if q <= 140 else 2 if q <= 190 else 3
            cdfs.append(row)
            if row['context'] != ctx or row['eob16_icdf'] != 32768-first_eob16[ctx]:
                errors.append(row)
        else:
            levels.append(row)
            if (row['qindex'],row['bd']) != (actual,depth) or not 8191 < row['level'] < (1<<20):
                errors.append(row)
    if [row['frame'] for row in headers] != list(range(frames)):
        errors.append({'frame_count_or_order':len(headers)})
    if sum(r['qindex']==actual and r['context']==expected_ctx for r in cdfs) < frames:
        errors.append({'missing_actual_q_cdf':actual})
    if require_high and not levels:
        errors.append({'missing_large_level':True})
    return {'pass':not errors,'errors':errors,'qp':qp,'depth':depth,'qindex':actual,
            'expected_q_context':expected_ctx,'frame_headers':headers,
            'cdf_initializations':cdfs,'large_level_count':len(levels),
            'max_actual_level':max((r['level'] for r in levels),default=0),
            'large_level_signs':sorted({r['sign'] for r in levels})}


def self_test():
    trace = ('M53CDF qindex=91 context=1 eob16_icdf=30929\n'
             'M53FRAME frame=0 qindex=91 bd=10 lossless=0\n'
             'M53LEVEL frame=0 qindex=91 bd=10 plane=0 pos=0 level=26000 sign=1\n')
    assert verify(trace,43,10,1,True)['pass']
    for mutation in (trace.replace('context=1','context=0'),trace.replace('lossless=0','lossless=1'),
                     trace.replace('qindex=91','qindex=43'),trace.replace('26000','1048576')):
        assert not verify(mutation,43,10,1,True)['pass']
    return {'pass':True,'negative_mutations':4}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace',type=Path,nargs='?')
    parser.add_argument('--qp',type=int,default=100)
    parser.add_argument('--depth',type=int,choices=(8,10),default=8)
    parser.add_argument('--frames',type=int,default=3)
    parser.add_argument('--require-high-range',action='store_true')
    parser.add_argument('--self-test',action='store_true')
    parser.add_argument('--output',type=Path)
    args = parser.parse_args()
    if args.self_test:
        result = self_test()
    else:
        if args.trace is None or not 1 <= args.qp <= 255:
            parser.error('trace and QP1..255 required')
        result = verify(args.trace.read_text(encoding='utf-8',errors='replace'),args.qp,args.depth,args.frames,args.require_high_range)
    encoded = json.dumps(result,indent=2)
    if args.output:
        args.output.write_text(encoded+'\n',encoding='utf-8')
    print(encoded)
    raise SystemExit(0 if result['pass'] else 1)


if __name__ == '__main__':
    main()
