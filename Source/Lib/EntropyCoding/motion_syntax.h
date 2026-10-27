#ifndef SVT_AV2_MOTION_SYNTAX_H
#define SVT_AV2_MOTION_SYNTAX_H
#include "entropy_encoder.h"
#include "block_geometry.h"
/* Integer luma pixels; the AV2 ONE_PEL shell coder consumes this difference
 * after the wire's 1/8-pel scale has been removed. One entry per Y/U/V group. */
typedef struct SvtAv2TileBlockMode {
    uint32_t is_inter;
    int32_t mv_x, mv_y;
} SvtAv2TileBlockMode;
typedef struct SvtAv2MotionCell {
    int32_t mv_x, mv_y;
    uint8_t valid, is_inter, is_new, skip_txfm;
} SvtAv2MotionCell;
typedef struct SvtAv2MotionSyntax {
    SvtAv2MotionCell* cells;
    uint32_t cols, rows;
} SvtAv2MotionSyntax;
int svt_av2_motion_syntax_init(SvtAv2MotionSyntax* state, uint32_t width, uint32_t height);
void svt_av2_motion_syntax_free(SvtAv2MotionSyntax* state);
int svt_av2_motion_write_block(SvtAv2MotionSyntax* state, SvtAv2EntropyEncoder* enc,
    const SvtAv2BlockView* block, const SvtAv2TileBlockMode* mode, uint32_t skip_txfm);
int svt_av2_motion_write_difference(SvtAv2EntropyEncoder* enc, int32_t dx, int32_t dy);
#endif
