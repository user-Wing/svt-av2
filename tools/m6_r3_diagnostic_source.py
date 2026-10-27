"""Create an external instrumented source copy; never edit the source argument."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError('instrumentation anchor is not unique: ' + old[:70])
    return text.replace(old, new, 1)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    for name in ['Source', 'include', 'app', 'cmake']:
        shutil.copytree(args.source / name, args.out / name)
    shutil.copyfile(args.source / 'CMakeLists.txt', args.out / 'CMakeLists.txt')
    path = args.out / 'Source/Lib/Bitstream/tile_writer.c'
    text = path.read_text()
    text = replace_once(text, '#include <stdlib.h>', '#include <stdlib.h>\n#include <stdio.h>')
    text = replace_once(text, '    SvtAv2CoeffNeighbours neighbours[3];',
                        '    uint64_t diag_partition, diag_motion, diag_intra, diag_coeff[3];\n'
                        '    SvtAv2CoeffNeighbours neighbours[3];')
    text = replace_once(text, '    const uint32_t lw = log2_dim(b->w), lh = log2_dim(b->h);',
                        '    const uint64_t diag_start = tree->enc->rate_q12;\n'
                        '    const uint32_t lw = log2_dim(b->w), lh = log2_dim(b->h);')
    text = replace_once(text, '    if (part != SVT_AV2_PARTITION_NONE) {\n        SvtAv2BlockView child',
                        '    tree->diag_partition += tree->enc->rate_q12 - diag_start;\n'
                        '    if (part != SVT_AV2_PARTITION_NONE) {\n        SvtAv2BlockView child')
    text = replace_once(text, '    uint32_t is_inter = 0, skip_txfm = 0;',
                        '    uint64_t diag_phase = tree->enc->rate_q12;\n'
                        '    uint32_t is_inter = 0, skip_txfm = 0;')
    text = replace_once(text, '    uint32_t m5_mode = 0u;',
                        '    tree->diag_motion += tree->enc->rate_q12 - diag_phase;\n'
                        '    diag_phase = tree->enc->rate_q12;\n    uint32_t m5_mode = 0u;')
    text = replace_once(text, '    uint8_t u_nonzero = 0u;',
                        '    tree->diag_intra += tree->enc->rate_q12 - diag_phase;\n'
                        '    uint8_t u_nonzero = 0u;')
    text = replace_once(text, '        const SvtAv2TileCoeffBlock* input = tree->blocks + tree->next++;',
                        '        const uint64_t diag_plane = tree->enc->rate_q12;\n'
                        '        const SvtAv2TileCoeffBlock* input = tree->blocks + tree->next++;')
    text = replace_once(text, '        if (st != SVT_AV2_COEFF_OK) {',
                        '        tree->diag_coeff[p] += tree->enc->rate_q12 - diag_plane;\n'
                        '        if (st != SVT_AV2_COEFF_OK) {')
    text = replace_once(text, '        ++s->cursor;', '''        ++s->cursor;
        if (s->cursor == s->sb_count)
            fprintf(stderr, "DIAG_RATE partition=%llu motion=%llu intra=%llu Y=%llu U=%llu V=%llu\\n",
                (unsigned long long)s->tree.diag_partition, (unsigned long long)s->tree.diag_motion,
                (unsigned long long)s->tree.diag_intra, (unsigned long long)s->tree.diag_coeff[0],
                (unsigned long long)s->tree.diag_coeff[1], (unsigned long long)s->tree.diag_coeff[2]);''')
    path.write_text(text)
    path = args.out / 'Source/Lib/Codec/encoder.c'
    text = replace_once(path.read_text(), '#include <stdlib.h>', '#include <stdlib.h>\n#include <stdio.h>')
    text = replace_once(text, '        context->coded_block_count = output.block_count;', '''        uint64_t intra = 0u, inter = 0u, zero = 0u, edge = 0u, nz[3] = {0u,0u,0u};
        for (size_t leaf = 0u; leaf < output.mode_count; ++leaf) {
            const SvtAv2TileBlockMode* mode = output.modes + leaf;
            inter += mode->is_inter != 0u;
            intra += mode->is_inter == 0u;
            edge += mode->is_inter && (abs(mode->mv_x) == 8 || abs(mode->mv_y) == 8);
            uint64_t nonzero = 0u;
            for (uint32_t plane = 0u; plane < 3u; ++plane) {
                const SvtAv2TileCoeffBlock* block = output.blocks + leaf * 3u + plane;
                const size_t count = (size_t)(block->width < 32u ? block->width : 32u) *
                    (block->height < 32u ? block->height : 32u);
                for (size_t i = 0u; i < count; ++i) {
                    const uint64_t present = block->qcoeff[i] != 0;
                    nz[plane] += present; nonzero += present;
                }
            }
            zero += nonzero == 0u;
        }
        fprintf(stderr, "DIAG_FRAME frame=%llu intra=%llu inter=%llu zero=%llu mv_edge=%llu nzY=%llu nzU=%llu nzV=%llu\\n",
            (unsigned long long)context->pictures_encoded, (unsigned long long)intra,
            (unsigned long long)inter, (unsigned long long)zero, (unsigned long long)edge,
            (unsigned long long)nz[0], (unsigned long long)nz[1], (unsigned long long)nz[2]);
        context->coded_block_count = output.block_count;''')
    path.write_text(text)
    manifest = {str(p.relative_to(args.out)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in args.out.rglob('*') if p.is_file()}
    (args.out / 'diagnostic-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('Prepared diagnostic copy: ' + str(args.out))


if __name__ == '__main__':
    main()
