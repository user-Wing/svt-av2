"""Independently check fixed-decoder partition nodes, CDF contexts and leaf order.

Only the M5.2 subset is accepted: full SB64 NONE or HORZ/2xVERT/four32;
partial SBs retain the fixed legal square-leaf boundary tree.
"""
import argparse
import collections
import json
import math
import re
from pathlib import Path


def boundary_part(x, y, w, h, fw, fh):
    rows, cols = y + h // 2 < fh, x + w // 2 < fw
    if not rows or not cols:
        if w == h:
            return 2 if rows else 1
        if h > w and (not rows or (w >= 16 and x + w // 4 >= fw)):
            return 1
        if w > h and (not cols or (h >= 16 and y + h // 4 >= fh)):
            return 2
    if w > h:
        return 2
    if h > w:
        return 1
    return 1 if x + w > fw or y + h > fh else 0


def verify(text, fixed=False, expected_plans=None):
    frames, errors = {}, []
    for line in text.splitlines():
        if not line.startswith('M52PART '):
            continue
        n = {k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', line)}
        frame = n['frame']
        fw, fh = n['fw'], n['fh']
        if frame not in frames:
            frames[frame] = {'queue': [(x,y,64,64) for y in range(0,fh,64) for x in range(0,fw,64)],
                             'fw':fw,'fh':fh,'above': {}, 'left': {}, 'nodes': 0, 'leaves': [], 'full_roots': [], 'contexts': []}
        f = frames[frame]
        x,y,w,h,p = (n[k] for k in ('x','y','w','h','part'))
        if not f['queue'] or f['queue'].pop(0) != (x,y,w,h):
            errors.append({'order': n})
        full_root = w == h == 64 and x % 64 == y % 64 == 0 and x + 64 <= fw and y + 64 <= fh
        if expected_plans is not None and w == h == 64 and not full_root:
            sb_index = (y//64)*((fw+63)//64) + x//64
            if frame >= len(expected_plans) or expected_plans[frame][sb_index] != 0:
                errors.append({'partial_plan_must_be_zero': n})
        if full_root:
            f['full_roots'].append({'x':x,'y':y,'partition':p})
            if p not in ((0,) if fixed else (0,1)):
                errors.append({'root_partition': n})
            if expected_plans is not None:
                sb_index = (y//64)*((fw+63)//64) + x//64
                if frame >= len(expected_plans) or p != expected_plans[frame][sb_index]:
                    errors.append({'independent_source_plan': n})
        elif p != boundary_part(x,y,w,h,fw,fh):
            errors.append({'boundary_or_child_partition': n})
        lw, lh = int(math.log2(w)), int(math.log2(h))
        raw = 2*((f['left'].get(y//4,0) >> (lh-3)) & 1) + ((f['above'].get(x//4,0) >> (lw-3)) & 1)
        splitctx = 4*(max(lw,lh)-3) + raw
        if w == h:
            rectmap = {8:0,16:0,32:3,64:6}[w]
        elif h > w:
            rectmap = 4 if w == 32 else 1
        else:
            rectmap = 5 if h == 32 else 2
        rectctx = 4*rectmap + raw
        if n['tree'] != 0 or (n['splitctx'],n['rectctx']) != (splitctx,rectctx):
            errors.append({'context': n, 'expected': [splitctx,rectctx]})
        f['contexts'].append([splitctx,rectctx])
        f['nodes'] += 1
        if p == 0:
            if w != h:
                errors.append({'rect_leaf': n})
            f['leaves'].append([x,y,w,h])
            for col in range(x//4,(x+w)//4):
                f['above'][col] = 64 - w//4
            for row in range(y//4,(y+h)//4):
                f['left'][row] = 64 - h//4
        elif p in (1,2):
            children = [(x,y,w,h//2),(x,y+h//2,w,h//2)] if p == 1 else [(x,y,w//2,h),(x+w//2,y,w//2,h)]
            f['queue'][0:0] = [c for c in children if c[0] < fw and c[1] < fh]
        else:
            errors.append({'unsupported_partition': n})
    result = []
    for frame, f in frames.items():
        if f['queue']:
            errors.append({'incomplete_frame': frame, 'remaining': f['queue']})
        if sum(w*h for x,y,w,h in f['leaves']) != f['fw']*f['fh']:
            errors.append({'coverage': frame})
        result.append({'frame':frame,'nodes':f['nodes'],'leaf_count':len(f['leaves']),
                       'leaf_sizes':dict(collections.Counter(str(leaf[2]) for leaf in f['leaves'])),
                       'full_roots':f['full_roots'],'leaf_order':f['leaves'],
                       'contexts':sorted({tuple(c) for c in f['contexts']})})
    return {'pass':bool(frames) and not errors,'errors':errors,'frames':result}


def self_test():
    # Root H, upper V, TL/TR NONE, lower V, BL/BR NONE. Contexts from
    # fixed AVM partition_context_lookup[32x32]=56, rect maps64=6/64x32=5.
    nodes = [(0,0,64,64,1,12,24),(0,0,64,32,2,12,20),
             (0,0,32,32,0,8,12),(32,0,32,32,0,8,12),
             (0,32,64,32,2,13,21),(0,32,32,32,0,8,12),
             (32,32,32,32,0,8,12)]
    lines = ['M52PART frame=0 fw=64 fh=64 x=%d y=%d w=%d h=%d part=%d splitctx=%d rectctx=%d tree=0'%n for n in nodes]
    trace = '\n'.join(lines)
    assert verify(trace)['pass'], verify(trace)
    assert not verify(trace,fixed=True)['pass']
    assert not verify(trace.replace('splitctx=13','splitctx=12'))['pass']
    assert not verify('\n'.join(lines[:-1]))['pass']
    return {'pass':True,'valid_nodes':7,'negative_mutations':3}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace',type=Path,nargs='?')
    parser.add_argument('--fixed',action='store_true')
    parser.add_argument('--self-test',action='store_true')
    parser.add_argument('--output',type=Path)
    parser.add_argument('--expected-plans',type=Path,
                        help='Independent source-only reference plan JSON, not encoder metadata')
    args = parser.parse_args()
    if args.self_test:
        result = self_test()
    else:
        if args.trace is None:
            parser.error('trace or --self-test required')
        plans = json.loads(args.expected_plans.read_text(encoding='utf-8')) if args.expected_plans else None
        result = verify(args.trace.read_text(encoding='utf-8',errors='replace'),args.fixed,plans)
    encoded = json.dumps(result,indent=2)
    if args.output:
        args.output.write_text(encoded+'\n',encoding='utf-8')
    print(encoded)
    raise SystemExit(0 if result['pass'] else 1)


if __name__ == '__main__':
    main()
