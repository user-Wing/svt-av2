"""Extract the R7 semantic model manifest from the fixed AVM defaults.

The independent C extractor includes the original tables and prob.h. Its output
must be compared with manifest.json; it is never a production dependency.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path


def initializer(text, name):
    text = re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S)
    m = re.search(r'\b' + name + r'\b[^=;]*=\s*\{', text)
    if not m:
        raise ValueError(name)
    start = m.end() - 1
    depth = 0
    for i in range(start, len(text)):
        depth += (text[i] == '{') - (text[i] == '}')
        if depth == 0:
            return text[start:i + 1]
    raise ValueError('unterminated ' + name)


def rows(body, path=()):
    # A leaf is identified by its paired actual CDFn/PARAn, never array padding.
    m = re.fullmatch(r'\{\s*AVM_CDF(\d+)\(([^)]*)\)\s*,\s*AVM_PARA\1\(([^)]*)\)\s*\}', body, re.S)
    if m:
        n = int(m[1])
        boundaries = [int(x.strip()) for x in m[2].split(',')]
        raw = [int(x.strip()) for x in m[3].split(',')]
        assert len(boundaries) == n - 1 and len(raw) == 3
        bias = [2, 3, 4] if n < 4 else [3, 4, 5]
        state = [32768 - x for x in boundaries] + [0, 0] + [a + b for a, b in zip(raw, bias)]
        yield path, n, state
        return
    depth = 0
    index = 0
    for i, ch in enumerate(body[1:-1], 1):
        if ch == '{':
            if depth == 0:
                start = i
            depth += 1
        elif ch == '}':
            depth -= 1
            if depth == 0:
                yield from rows(body[start:i + 1], path + (index,))
                index += 1


COEFF = [
    ('EOB16', 'eob_multi16'), ('EOB32', 'eob_multi32'), ('EOB64', 'eob_multi64'),
    ('EOB128', 'eob_multi128'), ('EOB256', 'eob_multi256'),
    ('EOB512', 'eob_multi512'), ('EOB1024', 'eob_multi1024'), ('EOB_EXTRA', 'eob_extra'),
    ('BASE_EOB', 'coeff_base_eob_multi'), ('BASE_EOB_UV', 'coeff_base_eob_multi_uv'),
    ('LF_EOB', 'coeff_base_lf_eob_multi'), ('LF_EOB_UV', 'coeff_base_lf_eob_multi_uv'),
    ('BASE', 'coeff_base_multi'), ('BASE_UV', 'coeff_base_multi_uv'),
    ('LF', 'coeff_base_lf_multi'), ('LF_UV', 'coeff_base_lf_multi_uv'),
    ('BR', 'coeff_lps_multi'), ('BR_UV', 'coeff_lps_multi_uv'),
    ('BR_LF', 'coeff_lps_lf_multi'), ('TXB_SKIP', 'txb_skip'),
    ('V_SKIP', 'v_txb_skip'), ('DC_SIGN', 'dc_sign'),
]
MODE = [
    ('SPLIT', 'do_split'), ('RECT', 'rect_type'), ('Y_SET', 'y_mode_set'),
    ('Y_IDX', 'y_mode_idx'), ('Y_OFFSET', 'y_mode_idx_offset'), ('UV', 'uv_mode'),
    ('PALETTE', 'palette_y_mode'), ('INTRA_INTER', 'intra_inter'),
    ('INTER_MODE', 'inter_single_mode'), ('DRL', 'drl'), ('SKIP', 'skip_txfm'),
    ('INTER_TX', 'inter_ext_tx'),
]


def generate(avm, dest, evidence):
    headers = {k: avm / 'av2/common' / ('entropy_inits_' + k + '.h') for k in ('coeffs', 'modes', 'mv')}
    tables = {k: p.read_text() for k, p in headers.items()}
    families = []
    for key, suffix in COEFF:
        name = 'av2_default_' + suffix + '_cdfs'
        entries = list(rows(initializer(tables['coeffs'], name)))
        groups = [[(p, n, s) for p, n, s in entries if p[0] == q] for q in range(4)]
        assert all(len(g) == len(groups[0]) for g in groups)
        families.append((key, name, groups))
    for key, suffix in MODE:
        name = 'default_' + suffix + ('_cdfs' if key == 'SKIP' else '_cdf')
        entries = list(rows(initializer(tables['modes'], name)))
        if key in ('SPLIT', 'RECT', 'DRL'):
            entries = [e for e in entries if e[0][0] == 0]
        if key == 'INTER_TX':
            entries = [e for e in entries if e[0][0] == 3]
        assert entries, key
        families.append((key, name, [entries] * 4))
    nmv = dict((p, (n, s)) for p, n, s in rows(initializer(tables['mv'], 'default_nmv_context')))
    mv_paths = [('MV_SET', [(0,)]), ('MV_CLASS', [(1, 3)]),
                ('MV_LOW', [(4, i) for i in range(2)]), ('MV_CLASS2', [(5,)]),
                ('MV_BITS', [(6, 0, i) for i in range(6)]),
                ('MV_GREATER', [(7, i) for i in range(2)]), ('MV_COL', [(8, i) for i in range(4)])]
    for key, paths in mv_paths:
        entries = [(p, *nmv[p]) for p in paths]
        families.append((key, 'default_nmv_context', [entries] * 4))
    intra_entries = [e for e in rows(initializer(tables['modes'], 'default_intra_ext_tx_cdf'))
                     if e[0][0] == 2 and e[0][1] < 3]
    assert len(intra_entries) == 3 and all(e[1] == 2 for e in intra_entries)
    families.append(('INTRA_TX', 'default_intra_ext_tx_cdf', [intra_entries] * 4))
    for key, name in [('CFL', 'default_cfl_cdf'), ('CFL_INDEX', 'default_cfl_index_cdf'),
                      ('MRL', 'default_mrl_index_cdf'), ('MULTI_MRL', 'default_multi_line_mrl_cdf')]:
        entries = list(rows(initializer(tables['modes'], name)))
        families.append((key, name, [entries] * 4))
    full_entries = [e for e in rows(initializer(tables['modes'], 'default_intra_ext_tx_cdf'))
                    if e[0][0] == 1 and e[0][1] < 3]
    assert len(full_entries) == 3 and all(e[1] == 7 for e in full_entries)
    families.append(('INTRA_TX_FULL', 'default_intra_ext_tx_cdf', [full_entries] * 4))
    for key, name, selected in [
        ('LONG_TX32', 'default_tx_ext_32_cdf', (0,)),
        ('LONG_TX_SHORT', 'default_intra_ext_tx_short_side_cdf', (1, 2)),
    ]:
        entries = [e for e in rows(initializer(tables['modes'], name)) if e[0][0] in selected]
        assert len(entries) == len(selected)
        families.append((key, name, [entries] * 4))
    for key, name in [('CFL_SIGN', 'default_cfl_sign_cdf'),
                      ('CFL_ALPHA', 'default_cfl_alpha_cdf')]:
        entries = list(rows(initializer(tables['modes'], name)))
        assert len(entries) == (1 if key == 'CFL_SIGN' else 6)
        families.append((key, name, [entries] * 4))
    dest.mkdir(parents=True, exist_ok=True)
    evidence.mkdir(parents=True, exist_ok=True)
    defines, ns, allq, manifest, checks = [], [], [[] for _ in range(4)], [], []
    base = 0
    for key, name, groups in families:
        defines.append(f'#define SVT_AV2_MODEL_{key} {base}u')
        defines.append(f'#define SVT_AV2_MODEL_{key}_ROWS {len(groups[0])}u')
        for row, (_, n, _) in enumerate(groups[0]):
            ns.append(n)
            for q in range(4):
                path, nn, state = groups[q][row]
                assert nn == n and 2 <= n <= 8
                allq[q].append(state + [0] * (12 - len(state)))
                if name == 'default_nmv_context':
                    fields = ['joint_shell_set_cdf', 'joint_shell_class_cdf_0',
                              'joint_shell_class_cdf_1', 'joint_shell_last_two_classes_cdf',
                              'shell_offset_low_class_cdf', 'shell_offset_class2_cdf',
                              'shell_offset_other_class_cdf', 'col_mv_greater_flags_cdf',
                              'col_mv_index_cdf']
                    expr = name + '.' + fields[path[0]] + ''.join(f'[{i}]' for i in path[1:])
                else:
                    expr = name + ''.join(f'[{i}]' for i in path)
                checks.append(f'check({q}, {base + row}, {n}, {expr});')
                manifest.append(dict(qctx=q, id=base + row, family=key, row=row, source=expr, n=n, state=state))
        base += len(groups[0])
    h = '#ifndef SVT_AV2_ENTROPY_MODEL_DATA_H\n#define SVT_AV2_ENTROPY_MODEL_DATA_H\n#include <stdint.h>\n' + '\n'.join(defines)
    h += f'\n#define SVT_AV2_MODEL_COUNT {base}u\nextern const uint8_t svt_av2_model_nsyms[SVT_AV2_MODEL_COUNT];\nextern const uint16_t svt_av2_model_defaults[4][SVT_AV2_MODEL_COUNT][12];\n#endif\n'
    (dest / 'entropy_model_data.h').write_text(h)
    c = '/* Generated by tools/gen_entropy_models.py; fixed AVM 966a7d7 defaults. */\n#include "entropy_model_data.h"\n'
    c += 'const uint8_t svt_av2_model_nsyms[SVT_AV2_MODEL_COUNT] = {' + ','.join(map(str, ns)) + '};\n'
    c += 'const uint16_t svt_av2_model_defaults[4][SVT_AV2_MODEL_COUNT][12] = {\n'
    for bank in allq:
        c += '{\n' + ''.join('{' + ','.join(map(str, s)) + '},\n' for s in bank) + '},\n'
    c += '};\n'
    (dest / 'entropy_model_data.c').write_text(c)
    metadata = {'models': base, 'rows': manifest, 'sources': {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in list(headers.values()) + [avm / 'avm_dsp/prob.h']}}
    (evidence / 'manifest.json').write_text(json.dumps(metadata, indent=2))
    oracle = '#include <stdio.h>\n#include "av2/common/entropy_inits_coeffs.h"\n#include "av2/common/entropy_inits_modes.h"\n#include "av2/common/entropy_inits_mv.h"\n'
    oracle += 'static void check(unsigned q,unsigned id,unsigned n,const avm_cdf_prob *s){printf("%u %u %u",q,id,n);for(unsigned i=0;i<n+4;i++)printf(" %u",(unsigned)s[i]);puts("");}\nint main(void){\n'
    oracle += '\n'.join(checks) + '\nreturn 0;}\n'
    (evidence / 'extract-models.c').write_text(oracle)
    print(json.dumps({'models': base, 'qctx_rows': len(manifest), 'manifest': str(evidence / 'manifest.json')}))


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--avm', type=Path, required=True)
    p.add_argument('--dest', type=Path, required=True)
    p.add_argument('--evidence', type=Path, required=True)
    a = p.parse_args()
    generate(a.avm, a.dest, a.evidence)
