#include "motion_syntax.h"
#include "entropy_models.h"
#include <stdlib.h>
#include <string.h>

/* Fixed AVM entropy_inits_modes.h and entropy_inits_mv.h forward boundaries.
 * disable_cdf_update and PRIMARY_REF_NONE keep these per-frame defaults. */
static const uint16_t mode_cdf[5][2] = {
    {10043,11100},{21561,21758},{25411,25714},{14117,14341},{18288,18577}};
static const uint16_t inter_cdf[4] = {1522,14381,10455,27796};
static const uint16_t drl_cdf[5] = {15721,21115,19567,17602,13319};
static int symbol(SvtAv2EntropyEncoder* e, uint16_t cdf, uint32_t model, uint32_t v) {
    return svt_av2_entropy_encode_model(e, model, &cdf, 2u, v);
}
static int literal(SvtAv2EntropyEncoder* e, uint32_t value, uint32_t bits) {
    return svt_av2_entropy_encode_literal(e, value, (int)bits);
}
static int quniform(SvtAv2EntropyEncoder* e, uint32_t n, uint32_t value) {
    uint32_t l = 0;
    while ((1u << l) < n) ++l;
    if (l == 0) return 0;
    const uint32_t m = (1u << l) - n;
    if (value < m) return literal(e, value, l - 1u);
    int st = literal(e, m + ((value - m) >> 1), l - 1u);
    return st != 0 ? st : literal(e, (value - m) & 1u, 1u);
}
int svt_av2_motion_write_difference(SvtAv2EntropyEncoder* e, int32_t dx, int32_t dy) {
    if (dx < -32 || dx > 32 || dy < -32 || dy > 32) return SVT_AV2_ENTROPY_ERROR_RANGE;
    const uint32_t x = (uint32_t)abs(dx), y = (uint32_t)abs(dy), shell = x + y;
    uint32_t cls = 0;
    while ((2u << cls) <= shell) ++cls;
    const uint32_t offset = shell - (cls == 0u ? 0u : (1u << cls));
    static const uint16_t class_cdf[6] = {8680,13723,18208,22686,26722,30020};
    static const uint16_t low[2] = {14587,20966};
    static const uint16_t bits[6] = {17943,18934,18928,18696,19044,20362};
    int st = symbol(e, 31579, SVT_AV2_MODEL_MV_SET, 0u);
    if (st == 0) st = svt_av2_entropy_encode_model(e, SVT_AV2_MODEL_MV_CLASS, class_cdf, 7u, cls);
    if (cls < 2u) {
        if (st == 0) st = symbol(e, low[cls], SVT_AV2_MODEL_MV_LOW + cls, offset);
    } else if (cls == 2u) {
        for (uint32_t i = 0; i < 3u && st == 0; ++i) {
            st = i == 0u ? symbol(e, 13189, SVT_AV2_MODEL_MV_CLASS2, offset != i) : literal(e, offset != i, 1u);
            if (offset == i) break;
        }
    } else {
        for (uint32_t i = 0; i < cls && st == 0; ++i)
            st = symbol(e, bits[i], SVT_AV2_MODEL_MV_BITS + i, (offset >> i) & 1u);
    }
    const uint32_t maximum = shell >> 1, pair = x < shell - x ? x : shell - x;
    static const uint16_t greater[2] = {5663,4856};
    for (uint32_t i = 0; i < maximum && i < 2u && st == 0; ++i) {
        st = symbol(e, greater[i], SVT_AV2_MODEL_MV_GREATER + i, pair != i);
        if (pair == i) break;
    }
    if (st == 0 && maximum > 2u && pair >= 2u) st = quniform(e, maximum - 1u, pair - 2u);
    if (st == 0 && shell > 0u && !(pair == maximum && (shell & 1u) == 0u)) {
        static const uint16_t col[4] = {13445,13541,14045,12888};
        st = symbol(e, col[cls < 3u ? cls : 3u], SVT_AV2_MODEL_MV_COL + (cls < 3u ? cls : 3u), x > maximum);
    }
    if (st == 0 && y != 0u) st = literal(e, dy < 0, 1u);
    if (st == 0 && x != 0u) st = literal(e, dx < 0, 1u);
    return st;
}
int svt_av2_motion_syntax_init(SvtAv2MotionSyntax* s, uint32_t w, uint32_t h) {
    memset(s, 0, sizeof(*s));
    s->cols = w / 4u; s->rows = h / 4u;
    s->cells = (SvtAv2MotionCell*)calloc((size_t)s->cols * s->rows, sizeof(*s->cells));
    return s->cells == NULL ? SVT_AV2_ENTROPY_ERROR_NO_SPACE : 0;
}
void svt_av2_motion_syntax_free(SvtAv2MotionSyntax* s) {
    free(s->cells); memset(s, 0, sizeof(*s));
}
static const SvtAv2MotionCell* cell(const SvtAv2MotionSyntax* s, int32_t x, int32_t y) {
    if (x < 0 || y < 0 || (uint32_t)x >= s->cols || (uint32_t)y >= s->rows) return NULL;
    const SvtAv2MotionCell* c = s->cells + (size_t)y * s->cols + (uint32_t)x;
    return c->valid ? c : NULL;
}
int svt_av2_motion_write_block(SvtAv2MotionSyntax* s, SvtAv2EntropyEncoder* e,
    const SvtAv2BlockView* b, const SvtAv2TileBlockMode* m, uint32_t skip_txfm) {
    if (m->is_inter > 1u || m->mv_x < -16 || m->mv_x > 16 || m->mv_y < -16 || m->mv_y > 16)
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    const int32_t x = (int32_t)(b->x / 4u), y = (int32_t)(b->y / 4u);
    const int32_t w = (int32_t)(b->w / 4u), h = (int32_t)(b->h / 4u);
    /* av2_get_intra_inter_context uses the first two available spatial entries. */
    const SvtAv2MotionCell* neigh[4] = {cell(s,x-1,y+h-1),cell(s,x+w-1,y-1),cell(s,x-1,y),cell(s,x,y-1)};
    uint32_t count = 0, intra = 0;
    for (uint32_t i = 0; i < 4u && count < 2u; ++i)
        if (neigh[i]) { ++count; intra += !neigh[i]->is_inter; }
    const uint32_t ictx = count == 2u ? (intra == 2u ? 3u : intra) : (intra ? 2u : 0u);
    int st = symbol(e, inter_cdf[ictx], SVT_AV2_MODEL_INTRA_INTER + ictx, m->is_inter);
    const SvtAv2MotionCell* spatial[4] = {cell(s,x-1,y+h-1),cell(s,x+w-1,y-1),cell(s,x-1,y),cell(s,x,y-1)};
    uint32_t left = 0, above = 0, any_new = 0;
    for (uint32_t i = 0; i < 4u; ++i) if (spatial[i] && spatial[i]->is_inter) {
        if (i == 0u || i == 2u) left = 1u; else above = 1u;
        any_new |= spatial[i]->is_new;
    }
    const uint32_t ctx = left + above + 2u * any_new;
    /* DRL reordering and temporal/ref banks are disabled. */
    const SvtAv2MotionCell* candidates[9] = {spatial[0],spatial[1],spatial[2],spatial[3],
        cell(s,x-1,y+h),cell(s,x+w,y-1),cell(s,x-1,y-1),cell(s,x-3,y+h-1),cell(s,x-3,y)};
    int32_t px = 0, py = 0;
    for (uint32_t i = 0; i < 9u; ++i) if (candidates[i] && candidates[i]->is_inter) {
        px = candidates[i]->mv_x; py = candidates[i]->mv_y; break;
    }
    const uint32_t is_new = m->is_inter && (m->mv_x != px || m->mv_y != py);
    uint32_t skip_ctx = 0;
    count = 0;
    for (uint32_t i = 0; i < 4u && count < 2u; ++i)
        if (neigh[i]) { ++count; skip_ctx += neigh[i]->skip_txfm; }
    static const uint16_t skip_cdf[3] = {25865,14316,4598};
    if (st == 0 && m->is_inter) st = symbol(e, skip_cdf[skip_ctx], SVT_AV2_MODEL_SKIP + skip_ctx, skip_txfm);
    if (st == 0 && m->is_inter) st = svt_av2_entropy_encode_model(e, SVT_AV2_MODEL_INTER_MODE + ctx, mode_cdf[ctx], 3u, is_new ? 2u : 0u);
    if (st == 0 && m->is_inter) st = symbol(e, drl_cdf[ctx], SVT_AV2_MODEL_DRL + ctx, 0u);
    if (st == 0 && is_new)
        st = svt_av2_motion_write_difference(e, m->mv_x-px, m->mv_y-py);
    if (st == 0) {
        SvtAv2MotionCell value = {m->mv_x,m->mv_y,1u,(uint8_t)m->is_inter,(uint8_t)is_new,(uint8_t)skip_txfm};
        for (int32_t yy = y; yy < y+h; ++yy)
            for (int32_t xx = x; xx < x+w; ++xx)
                s->cells[(size_t)yy * s->cols + (uint32_t)xx] = value;
    }
    return st;
}
