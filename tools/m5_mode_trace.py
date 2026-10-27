"""Check actual fixed-tag decoder M5MODE traces against AV2 mode-list rules.

Input comes from an instrumented diagnostic copy of the fixed decoder, never
encoder metadata. The immutable decoder separately verifies the raw pixels.
"""
import argparse
import collections
import json
import re
from pathlib import Path

DEFAULT = (17,45,3,10,24,31,38,52,15,19,43,47,1,5,8,12,
           22,26,29,33,36,40,50,54,16,18,44,46,2,4,9,11,
           23,25,30,32,37,39,51,53,14,20,42,48,0,6,7,13,
           21,27,28,34,35,41,49,55)


def mode_list(width, height, left, above):
    angular = list(dict.fromkeys(m for m in (left, above) if m >= 5))
    order = list(range(5))
    if min(width, height) >= 8:
        order.extend(angular)
        if width * height > 64:
            for distance in range(4):
                for mode in angular:
                    for delta in (-distance - 1, distance + 1):
                        candidate = 5 + (mode - 5 + delta) % 56
                        if candidate not in order:
                            order.append(candidate)
    order.extend(m + 5 for m in DEFAULT if m + 5 not in order)
    return order


def check(text):
    rows, errors, uv_rows, blocks = [], [], [], []
    grid = {}
    inter_grid = {}
    inter_neighbour_uses = 0
    for line in text.splitlines():
        if line.startswith('M5BLOCK '):
            block = {k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', line)}
            blocks.append(block)
            joint = 0 if block['inter'] else block['joint']
            if block['inter'] and block['joint'] != 0:
                errors.append(block)
            for y in range(block['y'] // 4, (block['y'] + block['h']) // 4):
                for x in range(block['x'] // 4, (block['x'] + block['w']) // 4):
                    key = (block['frame'], x, y)
                    grid[key] = joint
                    inter_grid[key] = bool(block['inter'])
            continue
        if line.startswith('M5UV '):
            uv = {k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', line)}
            uv_rows.append(uv)
            if not rows:
                errors.append(uv)
                continue
            y = rows[-1]
            context = int(y['mode'] != 0)
            if (uv['x'], uv['y'], uv['ctx'], uv['index'], uv['mode'], uv['cfl']) != (
                    y['x'], y['y'], context, context, 0, 0):
                errors.append(uv)
            continue
        if not line.startswith('M5MODE '):
            continue
        row = {k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', line)}
        rows.append(row)
        x, y, w, h = (row[k] // 4 for k in ('x', 'y', 'w', 'h'))
        frame = row.get('frame', 0)
        expected_left = grid.get((frame, x - 1, y + h - 1), 0)
        expected_above = grid.get((frame, x + w - 1, y - 1), 0)
        inter_neighbour_uses += sum(inter_grid.get(key, False) for key in (
            (frame, x - 1, y + h - 1), (frame, x + w - 1, y - 1)))
        if (row['left'], row['above']) != (expected_left, expected_above):
            errors.append({'neighbour_state': row, 'expected': [expected_left, expected_above]})
        order = mode_list(row['w'], row['h'], row['left'], row['above'])
        index = order.index(row['joint'])
        expected_set = 0 if index < 13 else 1 + (index - 13) // 16
        expected_ctx = sum(row[k] >= 5 for k in ('left', 'above'))
        expected_mode = {0: 0, 22: 1, 50: 2}.get(row['joint'])
        if (row['ctx'], row['set'], row['index'], row['mode'], row['delta']) != (
                expected_ctx, expected_set, index, expected_mode, 0):
            errors.append(row)
    return {'rows': len(rows), 'uv_rows': len(uv_rows), 'blocks': len(blocks),
            'inter_blocks': sum(b['inter'] != 0 for b in blocks),
            'inter_neighbour_uses': inter_neighbour_uses,
            'mode_counts': dict(collections.Counter(r['mode'] for r in rows)),
            'context_counts': dict(collections.Counter(r['ctx'] for r in rows)),
            'frame_mode_counts': {str(frame): dict(collections.Counter(r['mode'] for r in rows if r.get('frame') == frame))
                                  for frame in sorted({r.get('frame', 0) for r in rows})},
            'errors': errors, 'pass': bool(rows) and len(uv_rows) == len(rows) and not errors}


def self_test():
    # Fixed spec pp175-176: base list starts DC,SMOOTH,SV,SH,PAETH,V,H;
    # one H neighbour puts H first and +/-1..4 around it before default V.
    assert mode_list(16, 16, 0, 0)[:7] == [0, 1, 2, 3, 4, 22, 50]
    assert mode_list(16, 16, 50, 0)[:14] == [0,1,2,3,4,50,49,51,48,52,47,53,46,54]
    assert mode_list(16, 16, 50, 0).index(22) == 14
    assert mode_list(8, 8, 22, 50)[:7] == [0,1,2,3,4,22,50]
    for left in (0, 22, 50):
        for above in (0, 22, 50):
            order = mode_list(16, 16, left, above)
            assert sorted(order) == list(range(61))
    valid = ('M5BLOCK frame=1 x=48 y=0 w=16 h=16 inter=0 joint=50\n'
             'M5MODE frame=1 x=64 y=0 w=16 h=16 ctx=1 set=1 index=14 joint=22 mode=1 delta=0 left=50 above=0\n'
             'M5UV x=64 y=0 ctx=1 index=1 mode=0 cfl=0\n')
    assert check(valid)['pass']
    assert not check(valid.replace('index=14', 'index=6'))['pass']
    assert not check(valid.replace('ctx=1 index=1', 'ctx=0 index=0'))['pass']
    return {'pass': True, 'neighbour_pairs': 9, 'negative_mutations': 2}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace', type=Path, nargs='?')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        result = self_test()
    else:
        if args.trace is None:
            parser.error('trace or --self-test required')
        result = check(args.trace.read_text(encoding='utf-8', errors='replace'))
    encoded = json.dumps(result, indent=2)
    if args.output:
        args.output.write_text(encoded + '\n', encoding='utf-8')
    print(encoded)
    raise SystemExit(0 if result['pass'] else 1)


if __name__ == '__main__':
    main()
