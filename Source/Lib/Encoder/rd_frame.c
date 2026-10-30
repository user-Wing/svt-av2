#ifndef M6_SATD_ANGLES
#define M6_SATD_ANGLES 0
#endif
#ifndef M6_LAMBDA_NUMERATOR
#define M6_LAMBDA_NUMERATOR 1u
#endif
#ifndef M6_LAMBDA_DENOMINATOR
#define M6_LAMBDA_DENOMINATOR 1u
#endif
#if M6_LAMBDA_NUMERATOR <= 0 || M6_LAMBDA_DENOMINATOR <= 0
#error Lambda scale must be positive
#endif
#ifndef M6_FINE16
#define M6_FINE16 0
#endif
#ifndef M6_FINE16_PAIR
#define M6_FINE16_PAIR 0
#endif
#if M6_FINE16_PAIR && !M6_FINE16
#error M6_FINE16_PAIR_requires_M6_FINE16
#endif
#ifndef M6_FINE16_RETAIN_OLD
#define M6_FINE16_RETAIN_OLD 0
#endif
#ifndef M6_FINE16_REVERSE_CHOICES
#define M6_FINE16_REVERSE_CHOICES 0
#endif
#ifndef M6_TREE_SHARED
#define M6_TREE_SHARED 0
#endif
#ifndef M6_TOOL_REPEAT
#define M6_TOOL_REPEAT 0
#endif
#ifndef M6_REPEAT_RETAIN_OLD
#define M6_REPEAT_RETAIN_OLD 0
#endif
#ifndef M6_TREE_RETAIN_OLD
#define M6_TREE_RETAIN_OLD 0
#endif
#ifndef M6_TREE_RDOQ
#define M6_TREE_RDOQ 0
#endif
#ifndef M6_RDOQ_CONTEXT
#define M6_RDOQ_CONTEXT 0
#endif

#ifndef M6_ROOT_ASYM
#define M6_ROOT_ASYM 0
#endif
#if M6_ROOT_ASYM
#include <stdio.h>
#endif

#include "rd_frame.h"
#include "transform_path.h"
#include "coeff_syntax.h"
#include "inter_prediction.h"
#include "intra_prediction.h"
#include "motion_estimation.h"
#include "quantization.h"

#include <stdlib.h>
#include <string.h>

#define RD_MAX_LEAVES 64u
#define RD_MAX_COEFFS (3u * 64u * 64u)
#define RD_HASH_OFFSET UINT64_C(14695981039346656037)
#define RD_HASH_PRIME UINT64_C(1099511628211)

SvtAv2Error svt_av2_rd_lambda(uint32_t qindex, uint32_t bit_depth,
    uint64_t* lambda_q8, uint32_t* qstep_qtx) {
    SvtAv2QuantParams params;
    if (lambda_q8 == NULL || qstep_qtx == NULL ||
        svt_av2_quant_params_init_tx_bd(&params, qindex, 16u, 16u, bit_depth) != SVT_AV2_QUANT_OK)
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    const uint64_t q = (uint32_t)params.plane[0].ac;
    const unsigned shift = 2u + 2u * (bit_depth - 8u);
    *qstep_qtx = (uint32_t)q;
    *lambda_q8 = (q * q + (UINT64_C(1) << (shift - 1u))) >> shift;
    return SVT_AV2_ERROR_NONE;
}

static SvtAv2Error rd_lambda_ai_scaled(uint32_t qindex, uint32_t bit_depth,
    uint32_t numerator, uint32_t denominator, uint64_t* lambda_q8, uint32_t* qstep_qtx) {
    SvtAv2Error error = svt_av2_rd_lambda(qindex, bit_depth, lambda_q8, qstep_qtx);
    if (error != SVT_AV2_ERROR_NONE) return error;
    const uint64_t q = *qstep_qtx;
    uint64_t rdmult = (q * q * 80u / 32u + 32u) >> 6u;
    if (bit_depth == 10u) rdmult = (rdmult + 8u) >> 4u;
    if (rdmult == 0u) rdmult = 1u;
    *lambda_q8 = (rdmult + 1u) >> 1u;
    if (numerator != denominator) {
        *lambda_q8 = (*lambda_q8 * numerator + denominator / 2u) / denominator;
        if (*lambda_q8 == 0u) *lambda_q8 = 1u;
    }
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_rd_lambda_ai(uint32_t qindex, uint32_t bit_depth,
    uint64_t* lambda_q8, uint32_t* qstep_qtx) {
    return rd_lambda_ai_scaled(qindex, bit_depth, M6_LAMBDA_NUMERATOR,
        M6_LAMBDA_DENOMINATOR, lambda_q8, qstep_qtx);
}

SvtAv2Error svt_av2_rd_lambda_ai150(uint32_t qindex, uint32_t bit_depth,
    uint64_t* lambda_q8, uint32_t* qstep_qtx) {
    return rd_lambda_ai_scaled(qindex, bit_depth, 6u, 4u, lambda_q8, qstep_qtx);
}

SvtAv2Error svt_av2_rd_cost(uint64_t distortion, uint64_t lambda_q8,
    uint64_t rate_q12, uint64_t* cost) {
    /* Exact rounded product / 2^20 without requiring nonstandard uint128.
     * Every term is nonnegative, so overflow can be rejected before multiply. */
    const uint64_t mask = (UINT64_C(1) << 20u) - 1u;
    const uint64_t a = lambda_q8 >> 20u, b = lambda_q8 & mask;
    const uint64_t c = rate_q12 >> 20u, d = rate_q12 & mask;
    if (cost == NULL || (a != 0u && rate_q12 > UINT64_MAX / a) ||
        (b != 0u && c > UINT64_MAX / b)) return SVT_AV2_ERROR_INVALID_PARAMETER;
    const uint64_t high = a * rate_q12, middle = b * c;
    const uint64_t low = (b * d + (UINT64_C(1) << 19u)) >> 20u;
    if (high > UINT64_MAX - middle || high + middle > UINT64_MAX - low ||
        high + middle + low > UINT64_MAX - distortion) return SVT_AV2_ERROR_INVALID_PARAMETER;
    *cost = distortion + high + middle + low;
    return SVT_AV2_ERROR_NONE;
}

uint64_t svt_av2_rd_distortion(const uint64_t sse[3], uint32_t bit_depth,
    uint32_t chroma_format) {
    const uint64_t weight = chroma_format == SVT_AV2_CHROMA_420 ? 4u : 1u;
    const unsigned shift = 2u * (bit_depth - 8u);
    const uint64_t value = 4u * sse[0] + weight * (sse[1] + sse[2]);
    return shift == 0u ? value : (value + (UINT64_C(1) << (shift - 1u))) >> shift;
}

#ifndef M6_Y_BOUND
#define M6_Y_BOUND 0
#endif
#ifndef M6_Y_BOUND_VERIFY
#define M6_Y_BOUND_VERIFY 0
#endif
#ifndef M6_TX_CACHE
#define M6_TX_CACHE 0
#endif
#ifndef M6_TX_CACHE_SLOTS
#define M6_TX_CACHE_SLOTS 1024u
#endif
#ifndef M6_TX_CACHE_WAYS
#define M6_TX_CACHE_WAYS 1u
#endif
#if M6_TX_CACHE
#include <stdio.h>
#if M6_TX_CACHE_WAYS == 0 || (M6_TX_CACHE_WAYS & (M6_TX_CACHE_WAYS-1u)) != 0 || M6_TX_CACHE_WAYS > M6_TX_CACHE_SLOTS
#error M6_TX_CACHE_WAYS_must_be_a_power_of_two_not_greater_than_slots
#endif
#if M6_TX_CACHE_SLOTS == 0 || (M6_TX_CACHE_SLOTS & (M6_TX_CACHE_SLOTS - 1u)) != 0
#error M6_TX_CACHE_SLOTS_must_be_a_power_of_two
#endif
#include "rd_transform_memo.inc"
#endif

typedef struct RdWork {
    const SvtAv2FrameBuffer* source;
    SvtAv2FrameBuffer* recon;
    const SvtAv2FrameBuffer* reference;
    const SvtAv2RdFrameConfig* config;
    SvtAv2ReconScratch* scratch;
    SvtAv2RdFrameOutput* output;
    SvtAv2RdFrameStats stats;
    SvtAv2FrameGeometry geometry;
    SvtAv2TileRateSession* rate;
    SvtAv2TileRatePrefix* basic_rate_prefix;
    SvtAv2TileRatePrefix* rdoq_rate_prefix; /* One frame owner; snapshots borrow. */
    SvtAv2TileCoeffBlock rdoq_rate_blocks[3];
    int32_t rdoq_rate_coeffs[3u*1024u];
    uint32_t rdoq_rate_active,rdoq_rate_ready,rdoq_rate_leaf,rdoq_use_context;
    uint32_t basic_rate_active, defer_rdoq, tree_rdoq, tool_rounds;
    struct RdBasicMemo* basic_memo;
#if M6_TX_CACHE
    RdTransformCache* transform_cache; /* Shared by finalist snapshots; never rewound. */
#endif
    uint32_t memo_prefix, memo_active;
    SvtAv2BlockView leaves[RD_MAX_LEAVES];
    SvtAv2MotionVector mvs[RD_MAX_LEAVES];
    SvtAv2MotionSearchList mv_lists[4];
    uint32_t wide_stage;
    uint32_t motion_policy; /* two bits per full-SB leaf select a candidate slot */
    uint32_t leaf_count;
    SvtAv2TileCoeffBlock blocks[RD_MAX_LEAVES * 3u];
    SvtAv2TileBlockMode modes[RD_MAX_LEAVES];
    int32_t coeffs[RD_MAX_COEFFS];
    size_t coeff_count;
    uint32_t coeff_policy; /* three bits per leaf: one tail-level decrement flag per plane */
    int32_t rdoq_coeff[RD_MAX_COEFFS];
    uint8_t rdoq_saved[RD_MAX_LEAVES * 3u];
    uint8_t rdoq_tx_types[RD_MAX_LEAVES * 3u];
    uint32_t rdoq_target; /* block to propose, UINT32_MAX for replay only */
    uint32_t joint_y_tx; /* zero on the retained original tool routes */
    uint32_t rdoq_phase; /* private bounded proposal strength: /16, /4, 1x */
    uint16_t prediction[3][64u * 64u];
    uint16_t backup[3][64u * 64u];
    uint16_t prefix_recon[3][64u * 64u];
    uint64_t prefix_sse[3];
    size_t prefix_coeff_count;
    uint32_t prefix_leaf, prefix_active;
    uint32_t uv_y_cache, uv_y_size; /* refine_basic UV: 0=off, 1=capture, 2=replay */
    uint16_t uv_y_recon[64u * 64u];
    int32_t uv_y_coeff[64u * 64u];
    uint32_t uv_y_offset[RD_MAX_LEAVES];
    uint64_t uv_y_sse[RD_MAX_LEAVES];
    SvtAv2TileCoeffBlock uv_y_blocks[RD_MAX_LEAVES];
    SvtAv2BlockView region[3];
    uint8_t ids[RD_MAX_LEAVES], best_ids[RD_MAX_LEAVES];
    uint8_t uv_ids[RD_MAX_LEAVES];
    uint8_t tx_types[RD_MAX_LEAVES];
    uint8_t cfl_explicit[RD_MAX_LEAVES], cfl_alpha_idx[RD_MAX_LEAVES], cfl_alpha_signs[RD_MAX_LEAVES];
    uint8_t mrl[RD_MAX_LEAVES];
    uint8_t mhccp[RD_MAX_LEAVES];
    uint32_t skip_mhccp_refinement;
} RdWork;

/* Node identity is an exact chosen-prefix extension; kind scores are separate. */
typedef struct RdBasicMemoNode {
    uint32_t parent,key[38],valid;
    uint64_t sse[2][3],rate[2];
} RdBasicMemoNode;
typedef struct RdBasicMemo {
    uint32_t count;
    uint16_t slots[16384];
    RdBasicMemoNode nodes[8192];
} RdBasicMemo;

static uint64_t hash_word(uint64_t hash, uint32_t value) {
    for (unsigned i = 0u; i < 4u; ++i) {
        hash = (hash ^ (value & 255u)) * RD_HASH_PRIME;
        value >>= 8u;
    }
    return hash;
}

static int collect_leaf(void* user, const SvtAv2BlockView* block) {
    RdWork* work = (RdWork*)user;
    if (work->leaf_count >= RD_MAX_LEAVES) return -1;
    work->leaves[work->leaf_count++] = *block;
    return 0;
}

static SvtAv2Error geometry_for_sb(RdWork* work, size_t sb, uint16_t decision) {
    work->output->decisions[sb] = decision;
    work->leaf_count = 0u;
    SvtAv2Error error = svt_av2_partition_traverse_sb_plan(&work->geometry, sb,
        work->output->decisions, work->output->decision_count, collect_leaf, work);
    if (error != SVT_AV2_ERROR_NONE) return error;
    if (work->reference != NULL) {
        for (uint32_t i = 0u; i < work->leaf_count; ++i) {
            if (work->config->motion_optimization && work->region[0].w == 64u && work->region[0].h == 64u) {
                if (i >= 4u) return SVT_AV2_ERROR_INVALID_STATE;
                error = svt_av2_motion_estimate_list(work->source, work->reference, &work->leaves[i], &work->mv_lists[i]);
                if (error != SVT_AV2_ERROR_NONE) return error;
                work->mvs[i] = work->mv_lists[i].mv[0];
                work->stats.motion_candidates += work->mv_lists[i].candidates;
                continue;
            }
            SvtAv2MotionSearchResult result;
            error = svt_av2_motion_estimate(work->source, work->reference, &work->leaves[i], &result);
            if (error != SVT_AV2_ERROR_NONE) return error;
            work->mvs[i] = result.mv;
            work->stats.motion_candidates += result.candidates;
        }
    }
    return SVT_AV2_ERROR_NONE;
}

static void region_copy_buffer(RdWork* work, uint16_t buffer[3][64u * 64u], int restore) {
    const uint32_t step = work->recon->bit_depth > 8u ? 2u : 1u;
    for (uint32_t plane = 0u; plane < 3u; ++plane) {
        const SvtAv2BlockView* block = &work->region[plane];
        for (uint32_t y = 0u; y < block->h; ++y) {
            uint8_t* row = work->recon->planes[plane] +
                (size_t)(block->y + y) * work->recon->strides[plane] + (size_t)block->x * step;
            uint8_t* saved = (uint8_t*)buffer[plane] + (size_t)y * block->w * step;
            if (restore) memcpy(row, saved, (size_t)block->w * step);
            else memcpy(saved, row, (size_t)block->w * step);
        }
    }
}

static void region_copy(RdWork* work, int restore) {
    region_copy_buffer(work, work->backup, restore);
}

static SvtAv2Error plane_blocks(RdWork* work, uint32_t leaf, SvtAv2BlockView blocks[3]) {
    blocks[0] = work->leaves[leaf];
    const SvtAv2Error error = svt_av2_chroma_view(&work->geometry, &blocks[0], &blocks[1]);
    blocks[2] = blocks[1];
    return error;
}

/* mode IDs are stable: DC=0,V=1,H=2,ME=3. They also define tie breaks. */
static void basic_memo_key(const RdWork* work,uint32_t leaf,uint32_t mode,
    const SvtAv2BlockView blocks[3],uint32_t key[38]) {
    for (uint32_t p=0u;p<3u;++p) {
        key[4u*p]=blocks[p].x;key[4u*p+1u]=blocks[p].y;
        key[4u*p+2u]=blocks[p].w;key[4u*p+3u]=blocks[p].h;
    }
    const uint32_t tuple[26]={mode,work->uv_ids[leaf],work->mrl[leaf],work->tx_types[leaf],
        work->cfl_explicit[leaf],work->cfl_alpha_idx[leaf],work->cfl_alpha_signs[leaf],work->mhccp[leaf],
        work->coeff_policy,work->rdoq_target,work->rdoq_saved[leaf*3u],work->rdoq_saved[leaf*3u+1u],work->rdoq_saved[leaf*3u+2u],
        work->rdoq_tx_types[leaf*3u],work->rdoq_tx_types[leaf*3u+1u],work->rdoq_tx_types[leaf*3u+2u],
        work->wide_stage,work->motion_policy,(uint32_t)work->mvs[leaf].mv_x,(uint32_t)work->mvs[leaf].mv_y,
        0u,mode,0u,work->uv_ids[leaf],0u,work->uv_ids[leaf]};
    memcpy(key+12u,tuple,sizeof(tuple));
}

static uint32_t basic_memo_node(RdBasicMemo* memo,uint32_t parent,const uint32_t key[38]) {
    uint32_t hash=parent;
    for (uint32_t i=0u;i<38u;++i) hash=(hash^key[i])*16777619u;
    for (uint32_t slot=hash&16383u;;slot=(slot+1u)&16383u) {
        const uint32_t id=memo->slots[slot];
        if (id) {
            const RdBasicMemoNode* node=&memo->nodes[id-1u];
            if (node->parent==parent && memcmp(node->key,key,sizeof(node->key))==0) return id;
        } else {
            if (memo->count==8192u) return UINT32_MAX;
            RdBasicMemoNode* node=&memo->nodes[memo->count];
            node->parent=parent;memcpy(node->key,key,sizeof(node->key));node->valid=0u;
            memo->slots[slot]=(uint16_t)++memo->count;return memo->count;
        }
    }
}

static int basic_memo_allowed(const RdWork* work,int partial) {
    if (!work->basic_memo || partial || work->reference || !work->basic_rate_active ||
        work->config->rd_mode!=3u || work->config->frame_header.m5_intra_modes<3u ||
        work->coeff_policy || work->wide_stage || work->motion_policy || work->rdoq_target!=UINT32_MAX) return 0;
    for (uint32_t i=0u;i<RD_MAX_LEAVES;++i)
        if (work->mrl[i] || work->tx_types[i] || work->cfl_explicit[i] || work->cfl_alpha_idx[i] ||
            work->cfl_alpha_signs[i] || work->mhccp[i] || work->rdoq_saved[i*3u] ||
            work->rdoq_saved[i*3u+1u] || work->rdoq_saved[i*3u+2u]) return 0;
    return 1;
}

static RdBasicMemoNode* basic_memo_trial(RdWork* work,uint32_t leaf,uint32_t mode,
    const SvtAv2BlockView blocks[3]) {
    if (!work->memo_active) return NULL;
    uint32_t key[38];basic_memo_key(work,leaf,mode,blocks,key);
    const uint32_t id=basic_memo_node(work->basic_memo,work->memo_prefix,key);
    return id==UINT32_MAX?NULL:&work->basic_memo->nodes[id-1u];
}

static SvtAv2Error basic_memo_cost(const RdWork* work,const RdBasicMemoNode* node,
    uint32_t kind,uint64_t* cost) {
    uint64_t sse[3];
    for (uint32_t p=0u;p<3u;++p) sse[p]=work->prefix_sse[p]+node->sse[kind][p];
    return svt_av2_rd_cost(svt_av2_rd_distortion(sse,work->source->bit_depth,work->source->chroma_format),
        work->stats.lambda_q8,svt_av2_tile_rate_prefix_current_rate(work->basic_rate_prefix)+node->rate[kind],cost);
}

static void basic_memo_save(const RdWork* work,RdBasicMemoNode* node,uint32_t kind,
    const uint64_t sse[3],uint64_t rate) {
    for (uint32_t p=0u;p<3u;++p) node->sse[kind][p]=sse[p]-work->prefix_sse[p];
    node->rate[kind]=rate-svt_av2_tile_rate_prefix_current_rate(work->basic_rate_prefix);
    node->valid|=1u<<kind;
}

static int legal_mode(const RdWork* work, uint32_t leaf, uint32_t mode) {
    if (mode == 0u) return 1;
    if (mode == 3u) return work->reference != NULL;
    const uint32_t w = work->leaves[leaf].w, h = work->leaves[leaf].h;
    if (work->config->rd_mode==3u && mode>=8u && w!=h &&
        !((w==8u && h==16u) || (w==16u && h==8u)) &&
        !(work->config->angle_search && w>=16u && h>=16u && w<=4u*h && h<=4u*w)) return 0;
    if (mode >= 8u && w != h && !((w == 8u && h == 16u) || (w == 16u && h == 8u)) &&
        !(work->config->angle_search && w >= 8u && h >= 8u &&
          ((w <= 4u * h && h <= 4u * w) ||
           (w == 8u && h == 64u) || (w == 64u && h == 8u)))) return 0;
    if (mode >= 14u) return mode <= 61u && work->config->angle_search && work->config->frame_header.m5_intra_modes >= 3u;
    if (mode >= 8u) return mode <= 13u && work->config->frame_header.m5_intra_modes >= 3u;
    if (mode >= 4u) return mode <= 7u && work->config->frame_header.m5_intra_modes >= 2u;
    if (!work->config->frame_header.m5_intra_modes) return 0;
    return mode == 1u ? work->leaves[leaf].y != 0u : work->leaves[leaf].x != 0u;
}

/* Availability of the extra smooth endpoint in the current raster SB tree.
 * Earlier SB rows and the already decoded left SB are committed; inside this
 * SB only preceding coding leaves may supply a sample. Coordinates are luma. */
static int reference_decoded(const RdWork* work, uint32_t leaf, int32_t x, int32_t y) {
    if (x < 0 || y < 0) return 0;
    const uint32_t px = (uint32_t)x, py = (uint32_t)y;
    const SvtAv2BlockView* sb = &work->region[0];
    if (py < sb->y) return 1;
    if (py >= sb->y + sb->h) return 0;
    if (px < sb->x) return 1;
    for (uint32_t i = 0u; i < leaf; ++i) {
        const SvtAv2BlockView* b = &work->leaves[i];
        if (px >= b->x && px < b->x + b->w && py >= b->y && py < b->y + b->h) return 1;
    }
    return 0;
}

static SvtAv2Error predict(RdWork* work, uint32_t leaf, uint32_t mode,
    const SvtAv2BlockView blocks[3], uint64_t sad[3], int plane_selection) {
    uint8_t bytes[64u * 64u * 2u];
    const uint16_t one = 1u;
    const int direct = work->config->rd_mode == 3u && work->source->bit_depth == 10u &&
        ((const unsigned char*)&one)[0] == 1u;
    const uint32_t step = work->source->bit_depth > 8u ? 2u : 1u;
    const uint32_t first=plane_selection==2 || (work->uv_y_cache==2u && plane_selection==0)?1u:0u;
    for (uint32_t plane=first;plane<(plane_selection==1?1u:3u);++plane) {
        const SvtAv2BlockView* block = &blocks[plane];
        SvtAv2Error error;
        if (mode == 3u) {
            error = svt_av2_inter_predict(work->reference, plane, block, work->mvs[leaf],
                work->prediction[plane], block->w);
        } else {
            uint8_t* const predicted = direct ? (uint8_t*)work->prediction[plane] : bytes;
            const SvtAv2IntraBlock intra = {block->x, block->y, block->w, block->h, plane};
            SvtAv2IntraReference reference;
            if (work->config->frame_header.m5_intra_modes < 2u) {
                error = svt_av2_intra_reference_build(work->recon, &intra, &reference);
                if (error != SVT_AV2_ERROR_NONE) return error;
            }
            if (work->config->frame_header.m5_intra_modes >= 2u) {
                const int32_t scale = plane && work->source->chroma_format == SVT_AV2_CHROMA_420 ? 2 : 1;
                const int tr = reference_decoded(work, leaf, (int32_t)(block->x + block->w) * scale,
                    ((int32_t)block->y - 1) * scale);
                const int bl = reference_decoded(work, leaf, ((int32_t)block->x - 1) * scale,
                    (int32_t)(block->y + block->h) * scale);
                const int inherit = plane != 0u && mode >= 14u &&
                    svt_av2_intra_mode_base(mode) == work->uv_ids[leaf];
                if ((plane == 0u && (mode >= 8u || work->mrl[leaf])) || inherit) {
                    uint32_t tr_count = 0u, bl_count = 0u;
                    const uint32_t tr_limit = plane == 0u && work->mrl[leaf] ? block->w : block->h;
                    const uint32_t bl_limit = plane == 0u && work->mrl[leaf] ? block->h : block->w;
                    while (tr_count < tr_limit && reference_decoded(work, leaf,
                        (int32_t)(block->x + block->w + tr_count) * scale, ((int32_t)block->y - 1) * scale)) ++tr_count;
                    while (bl_count < bl_limit && reference_decoded(work, leaf,
                        ((int32_t)block->x - 1) * scale, (int32_t)(block->y + block->h + bl_count) * scale)) ++bl_count;
                    error = plane == 0u && work->mrl[leaf] ?
                        svt_av2_intra_mrls_predict(work->recon,&intra,mode,work->mrl[leaf],
                            tr_count,bl_count,predicted,block->w*step) :
                        svt_av2_intra_directional_predict(work->recon, &intra,
                            mode, tr_count, bl_count, predicted, block->w * step);
                } else error = svt_av2_intra_basic_predict(work->recon, &intra,
                    plane == 0u ? mode : work->uv_ids[leaf] == 14u ? 0u : work->uv_ids[leaf], tr, bl, predicted, block->w * step);
            } else if (plane == 0u && mode != 0u)
                error = svt_av2_intra_axis_predict(&reference, &intra,
                    mode == 1u ? SVT_AV2_INTRA_AXIS_VERTICAL : SVT_AV2_INTRA_AXIS_HORIZONTAL,
                    predicted, block->w * step);
            else error = svt_av2_intra_dc_predict(&reference, &intra, predicted, block->w * step);
            if (error == SVT_AV2_ERROR_NONE && !direct)
                for (uint32_t i = 0u; i < block->w * block->h; ++i)
                    work->prediction[plane][i] = svt_av2_sample_load(bytes + i * step, work->source->bit_depth);
        }
        if (error != SVT_AV2_ERROR_NONE) return error;
        if (sad == NULL) continue;
        sad[plane] = 0u;
        for (uint32_t y = 0u; y < block->h; ++y) {
            const uint8_t* row = work->source->planes[plane] +
                (size_t)(block->y + y) * work->source->strides[plane] + (size_t)block->x * step;
            for (uint32_t x = 0u; x < block->w; ++x) {
                const int difference = (int)svt_av2_sample_load(row + x * step, work->source->bit_depth) -
                    work->prediction[plane][(size_t)y * block->w + x];
                sad[plane] += (uint64_t)(difference < 0 ? -difference : difference);
            }
        }
    }
    return SVT_AV2_ERROR_NONE;
}

#if M6_SATD_ANGLES
/* Hadamard cost only screens additional modes; exact R+D still decides. */
static uint64_t angle_satd(const RdWork* work,const SvtAv2BlockView* b) {
    uint64_t cost=0u;
    const uint32_t step=work->source->bit_depth>8u?2u:1u;
    for (uint32_t y=0u;y<b->h;y+=4u) for (uint32_t x=0u;x<b->w;x+=4u) {
        int32_t tmp[4][4];
        for (uint32_t j=0u;j<4u;++j) {
            int32_t d[4];
            const uint8_t* row=work->source->planes[0]+(size_t)(b->y+y+j)*work->source->strides[0]+(size_t)(b->x+x)*step;
            for (uint32_t i=0u;i<4u;++i) d[i]=(int32_t)svt_av2_sample_load(row+i*step,work->source->bit_depth)-work->prediction[0][(size_t)(y+j)*b->w+x+i];
            const int32_t a=d[0]+d[3],b0=d[1]+d[2],c=d[1]-d[2],e=d[0]-d[3];
            tmp[j][0]=a+b0;tmp[j][1]=e+c;tmp[j][2]=a-b0;tmp[j][3]=e-c;
        }
        for (uint32_t i=0u;i<4u;++i) {
            const int32_t a=tmp[0][i]+tmp[3][i],b0=tmp[1][i]+tmp[2][i],c=tmp[1][i]-tmp[2][i],e=tmp[0][i]-tmp[3][i];
            const int32_t d[4]={a+b0,e+c,a-b0,e-c};
            for (uint32_t j=0u;j<4u;++j) cost+=(uint64_t)(d[j]<0?-d[j]:d[j]);
        }
    }
    return (cost+1u)>>1u;
}
static void angle_satd_rank(const RdWork* work,const SvtAv2BlockView* b,uint32_t mode,uint8_t ranked[4],uint64_t scores[4]) {
    if (!work->config->satd_angles) return;
    const uint64_t score=angle_satd(work,b);
    for (uint32_t slot=0u;slot<4u;++slot) if (score<scores[slot] || (score==scores[slot] && mode<ranked[slot])) {
        for (uint32_t j=3u;j>slot;--j) {scores[j]=scores[j-1u];ranked[j]=ranked[j-1u];}
        scores[slot]=score;ranked[slot]=(uint8_t)mode;break;
    }
}
static void angle_satd_append(const RdWork* work,uint8_t candidates[14],uint32_t* count,const uint8_t ranked[4]) {
    if (!work->config->satd_angles) return;
    for (uint32_t slot=0u;slot<4u;++slot) {
        uint32_t i=0u;while(i<*count && candidates[i]!=ranked[slot]) ++i;
        if (i==*count) candidates[(*count)++]=ranked[slot];
    }
}
#endif

static SvtAv2Error rank_modes(RdWork* work, uint32_t leaf,
    const SvtAv2BlockView blocks[3], uint8_t ranked[4], uint32_t* count, int partial) {
    uint64_t costs[4] = {UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX};
    uint64_t y_costs[4] = {UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX};
    *count = 0u;
    for (uint32_t mode = 0u; mode < 4u; ++mode) {
        if (!legal_mode(work, leaf, mode)) continue;
        uint64_t sad[3];
        const SvtAv2Error error = predict(work, leaf, mode, blocks, sad, 0);
        if (error != SVT_AV2_ERROR_NONE) return error;
        costs[mode] = sad[0] + sad[1] + sad[2];
        y_costs[mode] = sad[0];
        ranked[(*count)++] = (uint8_t)mode;
    }
    if (partial) {
        /* Reproduce M5: first choose intra on Y, then intra/inter on all planes. */
        uint32_t intra = 0u;
        for (uint32_t mode = 1u; mode < 3u; ++mode)
            if (y_costs[mode] < y_costs[intra]) intra = mode;
        ranked[0] = (uint8_t)(costs[3] < costs[intra] ? 3u : intra);
        *count = 1u;
        return SVT_AV2_ERROR_NONE;
    }
    for (uint32_t i = 1u; i < *count; ++i) {
        const uint8_t mode = ranked[i];
        uint32_t j = i;
        while (j != 0u && (costs[mode] < costs[ranked[j - 1u]] ||
            (costs[mode] == costs[ranked[j - 1u]] && mode < ranked[j - 1u]))) {
            ranked[j] = ranked[j - 1u]; --j;
        }
        ranked[j] = mode;
    }
    return SVT_AV2_ERROR_NONE;
}

/* Proposal cost uses the real coefficient symbol writer with the default CDF
 * and zero neighbours. It deliberately does not mutate the tile CDF session.
 * The outer block-coordinate pass accepts only a complete SB improvement with
 * actual neighbours, adaptive CDFs and inverse/clip pixel distortion. */
static SvtAv2Error rdoq_rate(const RdWork* work, uint32_t plane,
    const SvtAv2BlockView* block, const int32_t* qcoeff, uint32_t tx_type, uint32_t mode, uint64_t* rate) {
#if M6_RDOQ_CONTEXT
    if (work->rdoq_rate_active) {
        if (plane>=3u || work->rdoq_target/3u!=work->rdoq_rate_leaf ||
            work->rdoq_target%3u!=plane) return SVT_AV2_ERROR_INVALID_STATE;
        SvtAv2TileCoeffBlock trial[3];
        memcpy(trial,work->rdoq_rate_blocks,sizeof(trial));
        /* Rebind borrowed views after every RdWork copy; snapshots contain
         * no self-referential anchor pointers. */
        for (uint32_t p=0u;p<3u;++p) trial[p].qcoeff=work->rdoq_rate_coeffs+p*1024u;
#if M6_TX_CACHE
        if (work->transform_cache) ++work->transform_cache->rdoq_live_rate_calls;
#endif
        if (trial[plane].width!=block->w || trial[plane].height!=block->h)
            return SVT_AV2_ERROR_INVALID_STATE;
        trial[plane].qcoeff=qcoeff;trial[plane].tx_type=(uint8_t)tx_type;
        uint64_t total=0u;
        const uint64_t prefix=svt_av2_tile_rate_prefix_current_rate(work->rdoq_rate_prefix);
        if (svt_av2_tile_rate_prefix_trial(work->rdoq_rate_prefix,work->rdoq_rate_leaf,
            trial,0,&total)!=0 || total<prefix) return SVT_AV2_ERROR_INVALID_STATE;
        *rate=total-prefix;return SVT_AV2_ERROR_NONE;
    }
#endif
    SvtAv2TxSize tx;
    if (svt_av2_coeff_tx_size_from_dims(block->w, block->h, &tx) != SVT_AV2_COEFF_OK)
        return SVT_AV2_ERROR_INVALID_STATE;
    const uint32_t scale = plane && work->source->chroma_format == SVT_AV2_CHROMA_420 ? 2u : 1u;
    const SvtAv2CoeffPlaneInfo pi = {plane, tx,
        block->x / 4u, block->y / 4u,
        work->source->visible_width / (4u * scale),
        work->source->visible_height / (4u * scale), block->w, block->h};
    SvtAv2CoeffNeighbours neighbours;
    svt_av2_coeff_neighbours_reset(&neighbours);
    SvtAv2EntropyEncoder enc;
    svt_av2_entropy_rate_init(&enc);
    if (svt_av2_coeff_write_block_transform_intra(&enc, &pi, 0u, qcoeff, &neighbours,
        svt_av2_coeff_q_context(work->config->frame_header.base_q_idx), 0u,
        work->config->frame_header.reduced_tx_set, tx_type, mode) != 0)
        return SVT_AV2_ERROR_INVALID_STATE;
    *rate = enc.rate_q12;
    return SVT_AV2_ERROR_NONE;
}

static uint64_t rdoq_distortion(const RdWork* work, uint32_t plane,
    const SvtAv2QuantParams* params, uint64_t error) {
    /* DCT coefficient gain is 8,4,2 for tx_scale 0,1,2. For 64x64
     * the packed 32x32 inverse is upsampled, giving the same energy scale. */
    const unsigned shift = (unsigned)(6 - 2 * params->tx_scale) +
        2u * (work->source->bit_depth - 8u);
    const uint64_t weight = plane == 0u || work->source->chroma_format == SVT_AV2_CHROMA_420 ? 4u : 1u;
    return (weight * error + (UINT64_C(1) << (shift - 1u))) >> shift;
}

static uint64_t rdoq_error(int32_t coeff, int32_t dequant) {
    const int64_t delta = (int64_t)coeff - dequant;
    return (uint64_t)(delta * delta);
}

static int32_t rdoq_dequant(const SvtAv2QuantParams* params, int32_t level,
    uint32_t plane, uint32_t position) {
    const int32_t value = svt_av2_dequant_coeff(params, level, (int)plane, position == 0u);
    const int32_t minimum = svt_av2_dequant_clip_min(params->bit_depth);
    const int32_t maximum = svt_av2_dequant_clip_max(params->bit_depth);
    return value < minimum ? minimum : value > maximum ? maximum : value;
}

static SvtAv2Error rdoq_propose(RdWork* work, uint32_t plane,
    const SvtAv2BlockView* block, size_t count, uint32_t tx_type, uint32_t mode) {
    SvtAv2QuantParams params;
    if (svt_av2_quant_params_init_tx_bd(&params, work->config->frame_header.base_q_idx,
        block->w, block->h, work->source->bit_depth) != SVT_AV2_QUANT_OK)
        return SVT_AV2_ERROR_INVALID_STATE;
    /* Restrict the scalar proposal to nearby conservative local changes;
     * the full-SB acceptance still uses the unchanged global lambda. */
    const uint64_t proposal_lambda = work->rdoq_phase == 0u ?
        (work->stats.lambda_q8 + 8u) >> 4u : work->rdoq_phase == 1u ?
        (work->stats.lambda_q8 + 2u) >> 2u : work->stats.lambda_q8;
    int32_t* q = work->scratch->qcoeff;
    int32_t* dq = work->scratch->dqcoeff;
    const int32_t* coeff = work->scratch->coeff;
    uint16_t scan[1024];
    SvtAv2TxSize tx;
    if (svt_av2_coeff_tx_size_from_dims(block->w, block->h, &tx) != SVT_AV2_COEFF_OK)
        return SVT_AV2_ERROR_INVALID_STATE;
    if (svt_av2_coeff_scan_dct_dct(tx, scan) != count)
        return SVT_AV2_ERROR_INVALID_STATE;
    uint64_t error = 0u, rate, best_cost;
    for (size_t c = 0u; c < count; ++c) error += rdoq_error(coeff[c], dq[c]);
    SvtAv2Error status = rdoq_rate(work, plane, block, q, tx_type, mode, &rate);
    if (status != SVT_AV2_ERROR_NONE ||
        svt_av2_rd_cost(rdoq_distortion(work, plane, &params, error), proposal_lambda,
            rate, &best_cost) != SVT_AV2_ERROR_NONE) return SVT_AV2_ERROR_INVALID_STATE;
    for (size_t index = count; index != 0u; --index) {
        const uint32_t c = scan[index - 1u];
        const int32_t entering = q[c];
        const int32_t sign = coeff[c] < 0 ? -1 : 1;
        const int32_t magnitude = entering < 0 ? -entering : entering;
        int32_t chosen = entering, chosen_dq = dq[c];
        uint64_t chosen_error = error;
        const uint64_t old_error = rdoq_error(coeff[c], dq[c]);
        const int32_t levels[2] = {magnitude == 0 ? sign : (magnitude - 1) * sign,
            (magnitude + 1) * sign};
        for (uint32_t i = 0u; i < (magnitude == 0 ? 1u : 2u); ++i) {
            const int32_t candidate = levels[i];
            if (candidate > svt_av2_coeff_max_level() || candidate < -svt_av2_coeff_max_level()) continue;
            const int32_t candidate_dq = rdoq_dequant(&params, candidate, plane, c);
            const uint64_t next_error = error - old_error + rdoq_error(coeff[c], candidate_dq);
            /* A new nonzero whose squared error cannot improve is not useful
             * as a scalar proposal; context changes are left to exact SB RD. */
            if (magnitude == 0 && next_error >= error) continue;
            q[c] = candidate;
            /* EOB<=1 infers DCT. Keep ADST coefficients in its signalled
             * domain; the separate DCT candidate retains these EOB choices. */
            if (tx_type != 0u && svt_av2_coeff_find_eob(q, scan, count) <= 1u) continue;
            /* Every syntax cost is nonnegative. A strict improvement is
             * impossible when distortion alone reaches the incumbent J. */
            const uint64_t distortion = rdoq_distortion(work, plane, &params, next_error);
            if (distortion >= best_cost) continue;
            uint64_t cost;
            status = rdoq_rate(work, plane, block, q, tx_type, mode, &rate);
            if (status != SVT_AV2_ERROR_NONE ||
                svt_av2_rd_cost(distortion, proposal_lambda, rate, &cost) != SVT_AV2_ERROR_NONE)
                return SVT_AV2_ERROR_INVALID_STATE;
            if (cost < best_cost) {
                best_cost = cost; chosen = candidate; chosen_dq = candidate_dq; chosen_error = next_error;
            }
        }
        q[c] = chosen; dq[c] = chosen_dq; error = chosen_error;
    }
    /* Test every nonzero EOB cut, including DC-only and all-zero, rather than
     * assuming that a single last-level decrement can find a useful suffix. */
    int32_t best_q[1024];
    memcpy(best_q, q, count * sizeof(*q));
    const uint32_t eob = svt_av2_coeff_find_eob(q, scan, count);
    for (uint32_t index = eob; index != 0u; --index) {
        const uint32_t c = scan[index - 1u];
        if (q[c] == 0) continue;
        error -= rdoq_error(coeff[c], dq[c]);
        error += rdoq_error(coeff[c], 0); q[c] = 0;
        if (tx_type != 0u && svt_av2_coeff_find_eob(q, scan, count) <= 1u) break;
        /* Keep cutting even when this cut cannot win: its successor may
         * lower the error. Only the provably losing rate evaluation is skipped. */
        const uint64_t distortion = rdoq_distortion(work, plane, &params, error);
        if (distortion >= best_cost) continue;
        uint64_t cost;
        status = rdoq_rate(work, plane, block, q, tx_type, mode, &rate);
        if (status != SVT_AV2_ERROR_NONE ||
            svt_av2_rd_cost(distortion, proposal_lambda, rate, &cost) != SVT_AV2_ERROR_NONE)
            return SVT_AV2_ERROR_INVALID_STATE;
        if (cost < best_cost) { best_cost = cost; memcpy(best_q, q, count * sizeof(*q)); }
    }
    memcpy(q, best_q, count * sizeof(*q));
    return svt_av2_dequantize_block(&params, q, dq, count, (int)plane) == SVT_AV2_QUANT_OK ?
        SVT_AV2_ERROR_NONE : SVT_AV2_ERROR_INVALID_STATE;
}

/* Production transform_path reads only valid residual samples, geometry,
 * plane, and immutable per-frame QP/bitdepth/quantizer state. Cache the initial
 * forward result before any RDOQ mutation. Hashing only selects a slot: all
 * scalar keys and every valid residual sample must match before reuse. */
static SvtAv2Error forward_trial(RdWork* work, uint32_t plane,
    const SvtAv2BlockView* block, uint32_t tx_type, int* all_zero) {
#if M6_TX_CACHE
    RdTransformCache* const cache = work->transform_cache;
    const uint32_t key[6] = {plane, block->w, block->h, tx_type,
        work->config->frame_header.base_q_idx, (uint32_t)work->source->bit_depth};
    uint64_t hash = RD_HASH_OFFSET;
    if (cache != NULL) {
        uint32_t area = block->w * block->h, group = 0u;
        while (area > 16u && group < 8u) { area >>= 1u; ++group; }
        ++cache->requests_by_area[group];
        for (uint32_t i = 0u; i < 6u; ++i) hash = hash_word(hash, key[i]);
        for (uint32_t y = 0u; y < block->h; ++y)
            for (uint32_t x = 0u; x < block->w; ++x)
                hash = (hash ^ (uint16_t)work->scratch->residual[
                    (size_t)y * work->scratch->stride + x]) * RD_HASH_PRIME;
        const uint32_t id = rd_tx_memo_find(cache, hash, key,
            work->scratch->residual, work->scratch->stride);
        if (id) {
            if (!rd_tx_memo_restore(cache, id, work->scratch->coeff,
                    work->scratch->qcoeff, work->scratch->dqcoeff, all_zero))
                return SVT_AV2_ERROR_INVALID_STATE;
            return SVT_AV2_ERROR_NONE;
        }
        ++cache->misses;
        ++cache->misses_by_area[group];
    }
#endif
    /* The only work counter increment is an actual callback dispatch. */
    ++work->stats.forward_calls;
    const SvtAv2Error error = tx_type ?
        work->config->transform.forward_type(work->config->transform.user,
            plane, block->w, block->h, work->scratch->residual,
            work->scratch->stride, work->scratch->coeff, work->scratch->qcoeff,
            work->scratch->dqcoeff, all_zero, tx_type) :
        work->config->transform.forward(work->config->transform.user,
            plane, block->w, block->h, work->scratch->residual,
            work->scratch->stride, work->scratch->coeff, work->scratch->qcoeff,
            work->scratch->dqcoeff, all_zero);
#if M6_TX_CACHE
    if (cache != NULL && error == SVT_AV2_ERROR_NONE)
        (void)rd_tx_memo_store(cache, hash, key, work->scratch->residual,
            work->scratch->stride, work->scratch->coeff,
            work->scratch->qcoeff, *all_zero);
#endif
    return error;
}

static SvtAv2Error reconstruct_leaf_planes(RdWork* work, uint32_t leaf, uint32_t mode,
    const SvtAv2BlockView blocks[3], uint64_t sse[3], uint32_t planes) {
    SvtAv2Error error = predict(work, leaf, mode, blocks, NULL, planes==1u);
    if (error != SVT_AV2_ERROR_NONE) return error;
    const uint32_t step = work->source->bit_depth > 8u ? 2u : 1u;
    work->modes[leaf].is_inter = mode == 3u;
    work->modes[leaf].mv_x = mode == 3u ? work->mvs[leaf].mv_x : 0;
    work->modes[leaf].mv_y = mode == 3u ? work->mvs[leaf].mv_y : 0;
    for (uint32_t plane = 0u; plane < planes; ++plane) {
        const SvtAv2BlockView* block = &blocks[plane];
        const size_t count = (size_t)SVT_AV2_MIN(block->w, 32u) * SVT_AV2_MIN(block->h, 32u);
        const uint64_t old_sse = sse[plane];
        uint8_t* destination = work->recon->planes[plane] +
            (size_t)block->y * work->recon->strides[plane] + (size_t)block->x * step;
        if (plane == 0u && work->uv_y_cache == 2u) {
            SvtAv2TileCoeffBlock* entry = &work->blocks[leaf * 3u];
            *entry = work->uv_y_blocks[leaf];
            entry->qcoeff = work->coeffs + work->coeff_count;
            memcpy(work->coeffs + work->coeff_count, work->uv_y_coeff + work->uv_y_offset[leaf], count * sizeof(int32_t));
            work->coeff_count += count;
            for (uint32_t y = 0u; y < block->h; ++y)
                memcpy(destination + (size_t)y * work->recon->strides[0],
                    (const uint8_t*)work->uv_y_recon +
                    ((size_t)(block->y - work->region[0].y + y) * 64u + block->x - work->region[0].x) * step,
                    (size_t)block->w * step);
            sse[0] += work->uv_y_sse[leaf];
            continue;
        }
        if (plane && work->uv_ids[leaf] == 14u) {
            /* CfL reads the candidate Y after inverse transform and clip.
             * The earlier all-plane prediction supplies only a DC placeholder. */
            const SvtAv2IntraBlock intra = {block->x, block->y, block->w, block->h, plane};
            uint8_t cfl[64u * 64u * 2u];
            if (work->mhccp[leaf]) {
                const int32_t scale = work->source->chroma_format == SVT_AV2_CHROMA_420 ? 2 : 1;
                uint32_t tr = 0u, bl = 0u;
                while (tr < block->w && reference_decoded(work,leaf,
                    (int32_t)(block->x+block->w+tr)*scale,((int32_t)block->y-1)*scale)) ++tr;
                while (bl < block->h && reference_decoded(work,leaf,
                    ((int32_t)block->x-1)*scale,(int32_t)(block->y+block->h+bl)*scale)) ++bl;
                error=svt_av2_intra_mhccp_predict(work->recon,&intra,work->mhccp[leaf]-1u,
                    tr,bl,cfl,block->w*step,NULL);
            } else if (work->cfl_explicit[leaf]) {
                const uint32_t joint=work->cfl_alpha_signs[leaf];
                const uint32_t sign=plane==1u?(joint+1u)/3u:(joint+1u)%3u;
                const uint32_t idx=plane==1u?work->cfl_alpha_idx[leaf]>>3u:work->cfl_alpha_idx[leaf]&7u;
                const int32_t alpha=sign==0u?0:sign==1u?-(int32_t)(idx+1u):(int32_t)(idx+1u);
                error=svt_av2_intra_cfl_explicit_predict(work->recon,&intra,64u,cfl,block->w*step,alpha);
            } else error = svt_av2_intra_cfl_implicit_predict(work->recon, &intra, 64u,
                cfl, block->w * step, NULL);
            if (error != SVT_AV2_ERROR_NONE) return error;
            for (uint32_t i = 0u; i < block->w * block->h; ++i)
                work->prediction[plane][i] = svt_av2_sample_load(cfl + i * step, work->source->bit_depth);
        }
        for (uint32_t y = 0u; y < block->h; ++y) {
            const uint8_t* source = work->source->planes[plane] +
                (size_t)(block->y + y) * work->source->strides[plane] + (size_t)block->x * step;
            for (uint32_t x = 0u; x < block->w; ++x) {
                const uint16_t prediction = work->prediction[plane][(size_t)y * block->w + x];
                work->scratch->residual[(size_t)y * work->scratch->stride + x] =
                    (int16_t)((int)svt_av2_sample_load(source + x * step, work->source->bit_depth) - prediction);
                svt_av2_sample_store(destination + (size_t)y * work->recon->strides[plane] + x * step,
                    work->recon->bit_depth, prediction);
            }
        }
        uint32_t tx_type = plane == 0u ? work->tx_types[leaf] : 0u;
        int all_zero = 0;
        error = forward_trial(work,plane,block,tx_type,&all_zero);
        if (error != SVT_AV2_ERROR_NONE) return error;
        if (tx_type != 0u) {
            uint16_t scan[256];
            SvtAv2TxSize size;
            if (svt_av2_coeff_tx_size_from_dims(block->w, block->h, &size) != SVT_AV2_COEFF_OK ||
                svt_av2_coeff_scan_dct_dct(size, scan) != count)
                return SVT_AV2_ERROR_INVALID_STATE;
            /* EOB<=1 never signals an intra TX type. Requantize DCT instead. */
            if (svt_av2_coeff_find_eob(work->scratch->qcoeff, scan, count) <= 1u) {
                tx_type = 0u;
                error = forward_trial(work,plane,block,0u,&all_zero);
                if (error != SVT_AV2_ERROR_NONE) return error;
            }
        }
        const uint32_t policy = leaf < 4u ? (work->coeff_policy >> (3u * leaf + plane)) & 1u : 0u;
        if (policy != 0u) {
            uint16_t scan[1024];
            uint32_t tx_size = 0u;
            for (uint32_t side = 4u; side < block->w; side *= 2u) ++tx_size;
            if (svt_av2_coeff_scan_dct_dct((SvtAv2TxSize)tx_size, scan) != count)
                return SVT_AV2_ERROR_INVALID_STATE;
            const uint32_t eob = svt_av2_coeff_find_eob(work->scratch->qcoeff, scan, count);
            if (eob > 1u) {
                int32_t* last = &work->scratch->qcoeff[scan[eob - 1u]];
                *last += *last > 0 ? -1 : 1;
                SvtAv2QuantParams params;
                if (svt_av2_quant_params_init_tx_bd(&params, work->config->frame_header.base_q_idx,
                    block->w, block->h, work->source->bit_depth) != SVT_AV2_QUANT_OK ||
                    svt_av2_dequantize_block(&params, work->scratch->qcoeff,
                        work->scratch->dqcoeff, count, (int)plane) != SVT_AV2_QUANT_OK)
                    return SVT_AV2_ERROR_INVALID_STATE;
            }
        }
        const uint32_t rdoq_block = leaf * 3u + plane;
        if (work->config->coeff_optimization == 2u && work->rdoq_target == rdoq_block) {
#if M6_FLIP_TX
            const uint32_t tx_mode=plane==0u?svt_av2_intra_tx_mode(mode,block->w,block->h,work->mrl[leaf]):mode;
#else
            const uint32_t tx_mode=mode;
#endif
#if M6_RDOQ_CONTEXT
            const uint64_t prefix_before=work->rdoq_rate_ready?
                svt_av2_tile_rate_prefix_current_rate(work->rdoq_rate_prefix):0u;
            work->rdoq_rate_active=work->rdoq_rate_ready;
#endif
            error = rdoq_propose(work, plane, block, count, tx_type, tx_mode);
#if M6_RDOQ_CONTEXT
            work->rdoq_rate_active=0u;
            if (error==SVT_AV2_ERROR_NONE && work->rdoq_rate_ready &&
                prefix_before!=svt_av2_tile_rate_prefix_current_rate(work->rdoq_rate_prefix))
                error=SVT_AV2_ERROR_INVALID_STATE;
#endif
            if (error != SVT_AV2_ERROR_NONE) return error;
            work->rdoq_tx_types[rdoq_block] = (uint8_t)tx_type;
            memcpy(work->rdoq_coeff + work->coeff_count, work->scratch->qcoeff, count * sizeof(int32_t));
        } else if (work->rdoq_saved[rdoq_block]) {
            SvtAv2QuantParams params;
            tx_type = work->rdoq_tx_types[rdoq_block];
            memcpy(work->scratch->qcoeff, work->rdoq_coeff + work->coeff_count, count * sizeof(int32_t));
            if (svt_av2_quant_params_init_tx_bd(&params, work->config->frame_header.base_q_idx,
                block->w, block->h, work->source->bit_depth) != SVT_AV2_QUANT_OK ||
                svt_av2_dequantize_block(&params, work->scratch->qcoeff,
                    work->scratch->dqcoeff, count, (int)plane) != SVT_AV2_QUANT_OK)
                return SVT_AV2_ERROR_INVALID_STATE;
        }
        error = tx_type ? work->config->transform.inverse_type(work->config->transform.user, plane, block->w, block->h,
            work->scratch->dqcoeff, work->scratch->stride, work->scratch->inverse_residual, tx_type) :
            work->config->transform.inverse(work->config->transform.user, plane, block->w, block->h,
            work->scratch->dqcoeff, work->scratch->stride, work->scratch->inverse_residual);
        if (error != SVT_AV2_ERROR_NONE) return error;
        error = svt_av2_recon_write_block(work->recon, plane, block, destination,
            work->recon->strides[plane], work->scratch->inverse_residual, work->scratch->stride);
        if (error != SVT_AV2_ERROR_NONE) return error;
        if (count > RD_MAX_COEFFS - work->coeff_count) return SVT_AV2_ERROR_INVALID_STATE;
        SvtAv2TileCoeffBlock* entry = &work->blocks[leaf * 3u + plane];
        entry->plane = plane; entry->x = block->x; entry->y = block->y;
        entry->width = block->w; entry->height = block->h;
        entry->intra_mode = (uint8_t)(plane == 0u && mode != 3u ? mode :
            work->config->frame_header.m5_intra_modes >= 2u ? work->uv_ids[leaf] : 0u);
        entry->tx_type = (uint8_t)tx_type;
        entry->mrl = (uint8_t)(plane == 0u ? work->mrl[leaf] : 0u);
        entry->mhccp = (uint8_t)(plane == 0u ? work->mhccp[leaf] : 0u);
        entry->cfl_explicit = (uint8_t)(plane == 0u ? work->cfl_explicit[leaf] : 0u);
        entry->cfl_alpha_idx = (uint8_t)(plane == 0u ? work->cfl_alpha_idx[leaf] : 0u);
        entry->cfl_alpha_signs = (uint8_t)(plane == 0u ? work->cfl_alpha_signs[leaf] : 0u);
        entry->qcoeff = work->coeffs + work->coeff_count;
        memcpy(work->coeffs + work->coeff_count, work->scratch->qcoeff, count * sizeof(int32_t));
        work->coeff_count += count;
        for (uint32_t y = 0u; y < block->h; ++y) {
            const uint8_t* src = work->source->planes[plane] +
                (size_t)(block->y + y) * work->source->strides[plane] + (size_t)block->x * step;
            const uint8_t* rec = destination + (size_t)y * work->recon->strides[plane];
            for (uint32_t x = 0u; x < block->w; ++x) {
                const int difference = (int)svt_av2_sample_load(src + x * step, work->source->bit_depth) -
                    (int)svt_av2_sample_load(rec + x * step, work->recon->bit_depth);
                sse[plane] += (uint64_t)(difference * difference);
            }
        }
        if (plane == 0u && work->uv_y_cache == 1u) {
            work->uv_y_blocks[leaf] = *entry;
            work->uv_y_offset[leaf] = work->uv_y_size;
            memcpy(work->uv_y_coeff + work->uv_y_size, entry->qcoeff, count * sizeof(int32_t));
            work->uv_y_size += (uint32_t)count;
            work->uv_y_sse[leaf] = sse[0] - old_sse;
            for (uint32_t y = 0u; y < block->h; ++y)
                memcpy((uint8_t*)work->uv_y_recon +
                    ((size_t)(block->y - work->region[0].y + y) * 64u + block->x - work->region[0].x) * step,
                    destination + (size_t)y * work->recon->strides[0], (size_t)block->w * step);
        }
    }
    return SVT_AV2_ERROR_NONE;
}

static SvtAv2Error reconstruct_leaf(RdWork* work,uint32_t leaf,uint32_t mode,
    const SvtAv2BlockView blocks[3],uint64_t sse[3]) {
    return reconstruct_leaf_planes(work,leaf,mode,blocks,sse,3u);
}

static uint64_t path_hash(const RdWork* work) {
    uint64_t hash = RD_HASH_OFFSET;
    for (uint32_t i = 0u; i < work->leaf_count; ++i) {
        hash = hash_word(hash, work->ids[i]);
        if (work->config->frame_header.enable_mrls) hash = hash_word(hash, work->mrl[i]);
        if (work->config->transform_selection) hash = hash_word(hash, work->blocks[i * 3u].tx_type);
        if (work->config->frame_header.m5_intra_modes >= 2u)
            hash = hash_word(hash, work->uv_ids[i]);
        if (work->config->mhccp_search) hash = hash_word(hash, work->mhccp[i]);
        if (work->cfl_explicit[i] && work->uv_ids[i] == 14u) {
            hash = hash_word(hash, 1u);
            hash = hash_word(hash, work->cfl_alpha_idx[i]);
            hash = hash_word(hash, work->cfl_alpha_signs[i]);
        }
        hash = hash_word(hash, (uint32_t)work->modes[i].mv_x);
        hash = hash_word(hash, (uint32_t)work->modes[i].mv_y);
    }
    for (size_t i = 0u; i < work->coeff_count; ++i) hash = hash_word(hash, (uint32_t)work->coeffs[i]);
    const uint32_t step = work->recon->bit_depth > 8u ? 2u : 1u;
    for (uint32_t plane = 0u; plane < 3u; ++plane) {
        const SvtAv2BlockView* block = &work->region[plane];
        for (uint32_t y = 0u; y < block->h; ++y) {
            const uint8_t* row = work->recon->planes[plane] +
                (size_t)(block->y + y) * work->recon->strides[plane] + (size_t)block->x * step;
            for (uint32_t x = 0u; x < block->w; ++x)
                hash = hash_word(hash, svt_av2_sample_load(row + x * step, work->recon->bit_depth));
        }
    }
    if (work->coeff_policy != 0u) hash = hash_word(hash, work->coeff_policy);
    if (work->motion_policy != 0u) hash = hash_word(hash, work->motion_policy);
    if (work->wide_stage != 0u) hash = hash_word(hash, work->wide_stage);
    return hash;
}

/* ordinal digits choose mode IDs in exhaustive, ranked SAD slots in pruned.
 * saved IDs replay exactly; a candidate never reconstructs a different winner. */
static SvtAv2Error build_path(RdWork* work, uint32_t ordinal, int pruned,
    int partial, const uint8_t* saved_ids, int* legal, uint64_t* distortion, uint32_t* path_id) {
    uint64_t sse[3] = {0u, 0u, 0u};
    const uint32_t first_leaf = work->prefix_active ? work->prefix_leaf : 0u;
    if (work->prefix_active) memcpy(sse, work->prefix_sse, sizeof(sse));
    work->coeff_count = work->prefix_active ? work->prefix_coeff_count : 0u;
    *path_id = 0u; *legal = 1;
    if (!partial && work->leaf_count <= 4u)
        for (uint32_t leaf = 0u; leaf < first_leaf; ++leaf)
            *path_id |= work->ids[leaf] << ((work->config->frame_header.m5_intra_modes >= 3u ? 4u :
                work->config->frame_header.m5_intra_modes == 2u ? 3u : 2u) * leaf);
    if (!partial && !pruned && saved_ids == NULL) {
        uint32_t modes = ordinal;
        for (uint32_t leaf = first_leaf; leaf < work->leaf_count; ++leaf) {
            if (!legal_mode(work, leaf, modes % 4u)) { *legal = 0; return SVT_AV2_ERROR_NONE; }
            modes /= 4u;
        }
    }
    for (uint32_t leaf = first_leaf; leaf < work->leaf_count; ++leaf) {
        SvtAv2BlockView blocks[3];
        SvtAv2Error error = plane_blocks(work, leaf, blocks);
        if (error != SVT_AV2_ERROR_NONE) return error;
        uint32_t mode;
        if (saved_ids != NULL) mode = saved_ids[leaf];
        else if (partial || pruned) {
            uint8_t ranked[4]; uint32_t count;
            error = rank_modes(work, leaf, blocks, ranked, &count, partial);
            if (error != SVT_AV2_ERROR_NONE) return error;
            const uint32_t digit = partial ? 0u : ordinal % 2u;
            ordinal /= 2u;
            if (digit >= count) { *legal = 0; return SVT_AV2_ERROR_NONE; }
            mode = ranked[digit];
        } else { mode = ordinal % 4u; ordinal /= 4u; }
        if (!legal_mode(work, leaf, mode)) { *legal = 0; return SVT_AV2_ERROR_NONE; }
        work->ids[leaf] = (uint8_t)mode;
        /* Only coarse full trees fit the packed ID; fine/partial use the hash. */
        if (!partial && work->leaf_count <= 4u) *path_id |= mode << ((work->config->frame_header.m5_intra_modes >= 3u ? 4u :
            work->config->frame_header.m5_intra_modes == 2u ? 3u : 2u) * leaf);
        error = reconstruct_leaf(work, leaf, mode, blocks, sse);
        if (error != SVT_AV2_ERROR_NONE) return error;
    }
    *distortion = svt_av2_rd_distortion(sse, work->source->bit_depth, work->source->chroma_format);
    return SVT_AV2_ERROR_NONE;
}

static SvtAv2Error append_winner(RdWork* work) {
    SvtAv2RdFrameOutput* output = work->output;
    const size_t block_count = work->leaf_count * 3u;
    if (block_count > output->block_capacity - output->block_count ||
        work->leaf_count > output->mode_capacity - output->mode_count ||
        work->coeff_count > output->coeff_capacity - output->coeff_count)
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    for (size_t i = 0u; i < block_count; ++i) {
        SvtAv2TileCoeffBlock entry = work->blocks[i];
        const size_t offset = (size_t)(entry.qcoeff - work->coeffs);
        entry.qcoeff = output->qcoeff + output->coeff_count + offset;
        output->blocks[output->block_count + i] = entry;
    }
    memcpy(output->modes + output->mode_count, work->modes, work->leaf_count * sizeof(*work->modes));
    memcpy(output->qcoeff + output->coeff_count, work->coeffs, work->coeff_count * sizeof(*work->coeffs));
    output->block_count += block_count; output->mode_count += work->leaf_count;
    output->coeff_count += work->coeff_count;
    return SVT_AV2_ERROR_NONE;
}

/* Extended plans keep stage payloads in their legacy low word. Existing
 * plans retain their exact ID, tie order and audit hash. */
static uint64_t plan_candidate_id(uint16_t decision, uint32_t legacy, uint32_t payload) {
    return decision < 255u ? legacy : ((uint64_t)decision << 32u) | payload;
}

typedef struct RdBasicChoice {
    uint64_t cost, distortion, rate, hash;
    uint64_t id;
} RdBasicChoice;

/* A coordinate trial cannot change earlier coding leaves. Reconstruct that
 * fixed prefix once, retaining its coefficient storage, modes and exact SSE.
 * Each trial still rebuilds the entire affected suffix and rates the full SB. */
static SvtAv2Error prepare_prefix(RdWork* work, uint32_t first_leaf) {
    region_copy(work, 1);
    work->coeff_count = 0u;
    memset(work->prefix_sse, 0, sizeof(work->prefix_sse));
    for (uint32_t leaf = 0u; leaf < first_leaf; ++leaf) {
        SvtAv2BlockView blocks[3];
        SvtAv2Error error = plane_blocks(work, leaf, blocks);
        if (error != SVT_AV2_ERROR_NONE) return error;
        work->ids[leaf] = work->best_ids[leaf];
        error = reconstruct_leaf(work, leaf, work->best_ids[leaf], blocks, work->prefix_sse);
        if (error != SVT_AV2_ERROR_NONE) return error;
    }
    work->prefix_coeff_count = work->coeff_count;
    work->prefix_leaf = first_leaf;
    region_copy_buffer(work, work->prefix_recon, 0);
    return SVT_AV2_ERROR_NONE;
}

/* Later coordinate passes fix all leaves before the next target. Extend
 * that prefix only once; trials still rebuild the suffix and rate the full SB. */
static SvtAv2Error extend_prefix(RdWork* work, uint32_t first_leaf) {
    region_copy_buffer(work, work->prefix_recon, 1);
    work->coeff_count = work->prefix_coeff_count;
    for (uint32_t leaf = work->prefix_leaf; leaf < first_leaf; ++leaf) {
        SvtAv2BlockView blocks[3];
        SvtAv2Error error = plane_blocks(work, leaf, blocks);
        if (error != SVT_AV2_ERROR_NONE) return error;
        work->ids[leaf] = work->best_ids[leaf];
        error = reconstruct_leaf(work, leaf, work->best_ids[leaf], blocks, work->prefix_sse);
        if (error != SVT_AV2_ERROR_NONE) return error;
    }
    work->prefix_coeff_count = work->coeff_count;
    work->prefix_leaf = first_leaf;
    region_copy_buffer(work, work->prefix_recon, 0);
    return SVT_AV2_ERROR_NONE;
}

static SvtAv2Error refine_basic_original(RdWork* work, size_t sb, int partial,
    uint16_t decision, RdBasicChoice* best) {
    SvtAv2Error error;
    /* One bounded raster coordinate pass, seeded by the original DC/V/H
     * winner. Each Y or shared U/V trial restores its fixed coding prefix,
     * rebuilds the affected suffix and trials the complete actual tile syntax.
     * Later TXs therefore see the candidate's normative earlier samples. */
    error = prepare_prefix(work, 0u);
    if (error != SVT_AV2_ERROR_NONE) { region_copy(work, 1); return error; }
    work->prefix_active = 1u;
    for (uint32_t leaf = 0u; leaf < work->leaf_count; ++leaf) {
        error = extend_prefix(work, leaf);
        if (error != SVT_AV2_ERROR_NONE) {
            work->prefix_active = 0u; region_copy(work, 1); return error;
        }
        for (uint32_t component = 0u; component < 2u; ++component) {
            uint32_t choice = component ? work->uv_ids[leaf] : work->best_ids[leaf];
            const uint32_t entering_uv = work->uv_ids[leaf];
            const uint32_t mode_count = !component && work->config->frame_header.m5_intra_modes >= 3u ? 14u :
                component && work->config->frame_header.m5_intra_modes == 4u ? 9u : 8u;
            for (uint32_t index = 0u; index < mode_count; ++index) {
                const uint32_t slot = work->config->reverse_candidates ? mode_count - 1u - index : index;
                const uint32_t mode = component && slot == 8u ? 14u : slot;
                if (mode == 3u || (mode != 14u && !legal_mode(work, leaf, mode))) continue;
                uint8_t ids[RD_MAX_LEAVES];
                memcpy(ids, work->best_ids, work->leaf_count);
                if (component) work->uv_ids[leaf] = (uint8_t)mode;
                else ids[leaf] = (uint8_t)mode;
                region_copy_buffer(work, work->prefix_recon, 1);
                uint64_t d = 0u, r = 0u, cost = 0u; uint32_t path; int valid;
                error = build_path(work, 0u, 0, partial, ids, &valid, &d, &path);
                if (error == SVT_AV2_ERROR_NONE && (!valid ||
                    svt_av2_tile_rate_trial(work->rate, sb, decision, work->blocks,
                        work->leaf_count * 3u, NULL, 0u, 0, &r) != 0))
                    error = SVT_AV2_ERROR_INVALID_STATE;
                if (error == SVT_AV2_ERROR_NONE)
                    error = svt_av2_rd_cost(d, work->stats.lambda_q8, r, &cost);
                if (error != SVT_AV2_ERROR_NONE) {
                    work->prefix_active = 0u; region_copy(work, 1); return error;
                }
                ++work->stats.candidate_count;
                const uint32_t payload = (1u << 28u) |
                    (work->config->frame_header.m5_intra_modes >= 3u ?
                        (leaf << 5u) | (component << 4u) : (leaf << 4u) | (component << 3u)) | mode;
                const uint64_t id = plan_candidate_id(decision, payload |
                    (work->config->partition_selection >= 2u ? (uint32_t)decision <<
                    (work->config->frame_header.m5_intra_modes >= 3u ? 16u : 12u) : 0u), payload);
                if (work->config->candidate_hook != NULL)
                    work->config->candidate_hook(work->config->candidate_hook_user, sb, id, d, r, cost, 0);
                if (cost < best->cost || (cost == best->cost && mode < choice)) {
                    best->cost = cost; best->distortion = d; best->rate = r; best->id = id;
                    best->hash = path_hash(work); choice = mode;
                    if (!component) memcpy(work->best_ids, ids, work->leaf_count);
                }
                if (component) work->uv_ids[leaf] = (uint8_t)entering_uv;
            }
            if (component) work->uv_ids[leaf] = (uint8_t)choice;
        }
    }
    work->prefix_active = 0u;
    region_copy(work, 1);
    return SVT_AV2_ERROR_NONE;
}

/* The clipped candidate Y is already reconstructed. Classic CfL reads only
 * that Y and causal UV DC references; this score never selects an RD winner. */
static SvtAv2Error local_basic_cost(RdWork* work, uint32_t leaf, uint32_t mode, uint64_t* cost) {
    SvtAv2BlockView blocks[3];
    SvtAv2Error error=plane_blocks(work,leaf,blocks);
    if (error!=SVT_AV2_ERROR_NONE) return error;
    uint64_t sse[3]={0u,0u,0u},rate=0u;
    work->coeff_count=work->prefix_coeff_count;work->ids[leaf]=(uint8_t)mode;
    RdBasicMemoNode* memo=basic_memo_trial(work,leaf,mode,blocks);
    if (memo && (memo->valid&(1u<<0u))) return basic_memo_cost(work,memo,0u,cost);
    error=reconstruct_leaf(work,leaf,mode,blocks,sse);
    if (error!=SVT_AV2_ERROR_NONE) return error;
    if (work->basic_rate_active) {
        for (uint32_t plane=0u;plane<3u;++plane) sse[plane]+=work->prefix_sse[plane];
        if (svt_av2_tile_rate_prefix_trial(work->basic_rate_prefix,leaf,
            work->blocks+leaf*3u,0,&rate)!=0) return SVT_AV2_ERROR_INVALID_STATE;
    } else for (uint32_t plane=0u;plane<3u;++plane) {
        const SvtAv2TileCoeffBlock* entry=&work->blocks[leaf*3u+plane];
        uint64_t part=0u;
#if M6_FLIP_TX
            const uint32_t tx_mode=plane==0u?svt_av2_intra_tx_mode(entry->intra_mode,entry->width,entry->height,entry->mrl):entry->intra_mode;
#else
            const uint32_t tx_mode=entry->intra_mode;
#endif
            error=rdoq_rate(work,plane,blocks+plane,entry->qcoeff,entry->tx_type,tx_mode,&part);
        if (error!=SVT_AV2_ERROR_NONE) return error;
        rate+=part;
    }
    error=svt_av2_rd_cost(svt_av2_rd_distortion(sse,work->source->bit_depth,
        work->source->chroma_format),work->stats.lambda_q8,rate,cost);
    if (error==SVT_AV2_ERROR_NONE && memo) basic_memo_save(work,memo,0u,sse,rate);
    return error;
}

static SvtAv2Error local_basic_y_cost(RdWork* work,uint32_t leaf,uint32_t mode,uint64_t* cost) {
    SvtAv2BlockView blocks[3];
    SvtAv2Error error=plane_blocks(work,leaf,blocks);
    if (error!=SVT_AV2_ERROR_NONE) return error;
    uint64_t sse[3],rate=0u;memcpy(sse,work->prefix_sse,sizeof(sse));
    work->coeff_count=work->prefix_coeff_count;work->ids[leaf]=(uint8_t)mode;
    RdBasicMemoNode* memo=basic_memo_trial(work,leaf,mode,blocks);
    if (memo && (memo->valid&(1u<<1u))) return basic_memo_cost(work,memo,1u,cost);
    error=reconstruct_leaf_planes(work,leaf,mode,blocks,sse,1u);
    if (error!=SVT_AV2_ERROR_NONE) return error;
    if (svt_av2_tile_rate_prefix_trial_y(work->basic_rate_prefix,leaf,
        work->blocks+leaf*3u,&rate)!=0) return SVT_AV2_ERROR_INVALID_STATE;
    error=svt_av2_rd_cost(svt_av2_rd_distortion(sse,work->source->bit_depth,
        work->source->chroma_format),work->stats.lambda_q8,rate,cost);
    if (error==SVT_AV2_ERROR_NONE && memo) basic_memo_save(work,memo,1u,sse,rate);
    return error;
}

static SvtAv2Error refine_basic_fast(RdWork* work, size_t sb, int partial,
    uint16_t decision, RdBasicChoice* best) {
    work->uv_y_cache=0u;work->memo_active=0u;work->memo_prefix=0u;
    if (work->config->rd_mode!=3u)
        return refine_basic_original(work,sb,partial,decision,best);
    uint8_t entering_y[RD_MAX_LEAVES],entering_uv[RD_MAX_LEAVES];
    memcpy(entering_y,work->best_ids,sizeof(entering_y));
    memcpy(entering_uv,work->uv_ids,sizeof(entering_uv));
    if (work->basic_rate_prefix!=NULL) {
        size_t leaves=0u;
        if (svt_av2_tile_rate_prefix_begin(work->basic_rate_prefix,work->rate,sb,
            decision,&leaves)!=0 || leaves!=work->leaf_count) return SVT_AV2_ERROR_INVALID_STATE;
        work->basic_rate_active=1u;
    }
    SvtAv2Error error=prepare_prefix(work,0u);
    if (error!=SVT_AV2_ERROR_NONE) {work->basic_rate_active=0u;region_copy(work,1);return error;}
    work->prefix_active=1u;
    work->memo_active=(uint32_t)basic_memo_allowed(work,partial);
    for (uint32_t leaf=0u;leaf<work->leaf_count && error==SVT_AV2_ERROR_NONE;++leaf) {
        error=extend_prefix(work,leaf);
        if (error!=SVT_AV2_ERROR_NONE) break;
        if (leaf && work->basic_rate_active) {
            uint64_t previous_rate=0u;
            if (svt_av2_tile_rate_prefix_trial(work->basic_rate_prefix,leaf-1u,
                work->blocks+(leaf-1u)*3u,1,&previous_rate)!=0) {
                error=SVT_AV2_ERROR_INVALID_STATE;break;
            }
        }
        if (leaf && work->memo_active) {
            SvtAv2BlockView blocks[3];uint32_t key[38];
            error=plane_blocks(work,leaf-1u,blocks);if (error!=SVT_AV2_ERROR_NONE) break;
            basic_memo_key(work,leaf-1u,work->best_ids[leaf-1u],blocks,key);
            work->memo_prefix=basic_memo_node(work->basic_memo,work->memo_prefix,key);
            if (work->memo_prefix==UINT32_MAX) work->memo_active=0u;
        }
        uint64_t y_mask=UINT64_MAX;
        if (!partial && work->basic_rate_active && work->config->frame_header.m5_intra_modes>=3u) {
            SvtAv2BlockView blocks[3];
            error=plane_blocks(work,leaf,blocks);
            if (error!=SVT_AV2_ERROR_NONE) break;
            uint8_t ranked[4]={255u,255u,255u,255u};
            uint64_t scores[4]={UINT64_MAX,UINT64_MAX,UINT64_MAX,UINT64_MAX};
            for (uint32_t mode=0u;mode<14u;++mode) {
                if (mode==3u || !legal_mode(work,leaf,mode)) continue;
                uint64_t sad[3]={0u,0u,0u};
                error=predict(work,leaf,mode,blocks,sad,1);
                if (error!=SVT_AV2_ERROR_NONE) break;
                for (uint32_t slot=0u;slot<4u;++slot) if (sad[0]<scores[slot] ||
                    (sad[0]==scores[slot] && mode<ranked[slot])) {
                    for (uint32_t j=3u;j>slot;--j) {scores[j]=scores[j-1u];ranked[j]=ranked[j-1u];}
                    scores[slot]=sad[0];ranked[slot]=(uint8_t)mode;break;
                }
            }
            if (error!=SVT_AV2_ERROR_NONE) break;
            y_mask=UINT64_C(1)<<work->best_ids[leaf];
            for (uint32_t slot=0u;slot<4u;++slot)
                if (ranked[slot]!=255u) y_mask|=UINT64_C(1)<<ranked[slot];
        }
        for (uint32_t component=0u;component<2u && error==SVT_AV2_ERROR_NONE;++component) {
            work->uv_y_size=0u;
            if (component && work->basic_rate_active) work->uv_y_cache=1u;
            const uint32_t original_uv=work->uv_ids[leaf];
            const uint32_t incumbent=component?original_uv:work->best_ids[leaf];
            uint64_t uv_mask=UINT64_MAX;
            if (component && !partial && work->basic_rate_active && work->config->frame_header.m5_intra_modes==4u) {
                SvtAv2BlockView blocks[3];
                error=plane_blocks(work,leaf,blocks);
                uint8_t ranked[2]={255u,255u};uint64_t scores[2]={UINT64_MAX,UINT64_MAX};
                for (uint32_t mode=0u;mode<8u && error==SVT_AV2_ERROR_NONE;++mode) {
                    if (mode==3u || !legal_mode(work,leaf,mode)) continue;
                    work->uv_ids[leaf]=(uint8_t)mode;
                    uint64_t sad[3]={0u,0u,0u};
                    error=predict(work,leaf,work->best_ids[leaf],blocks,sad,2);
                    if (error!=SVT_AV2_ERROR_NONE) break;
                    const uint64_t score=sad[1]+sad[2];
                    for (uint32_t slot=0u;slot<2u;++slot) if (score<scores[slot] ||
                        (score==scores[slot] && mode<ranked[slot])) {
                        if (!slot) {scores[1]=scores[0];ranked[1]=ranked[0];}
                        scores[slot]=score;ranked[slot]=(uint8_t)mode;break;
                    }
                }
                work->uv_ids[leaf]=(uint8_t)original_uv;
                if (error!=SVT_AV2_ERROR_NONE) {work->uv_y_cache=0u;break;}
                uv_mask=(UINT64_C(1)<<incumbent)|(UINT64_C(1)<<14u);
                for (uint32_t slot=0u;slot<2u;++slot)
                    if (ranked[slot]!=255u) uv_mask|=UINT64_C(1)<<ranked[slot];
            }
            uint32_t choice=incumbent;
            uint64_t selected=UINT64_MAX;
            const uint32_t mode_count=!component && work->config->frame_header.m5_intra_modes>=3u?14u:
                component && work->config->frame_header.m5_intra_modes==4u?9u:8u;
            for (uint32_t scan=0u;scan<mode_count;++scan) {
                const uint32_t mode=component && scan==8u?14u:scan;
                if (mode==3u || (mode!=14u && !legal_mode(work,leaf,mode))) continue;
                if (!component && !(y_mask&(UINT64_C(1)<<mode))) continue;
                if (component && !(uv_mask&(UINT64_C(1)<<mode))) continue;
                work->uv_ids[leaf]=(uint8_t)(component?mode:original_uv);
                uint64_t score=0u;
                error=!component && !partial && work->basic_rate_active &&
                    work->config->frame_header.m5_intra_modes>=3u ?
                    local_basic_y_cost(work,leaf,mode,&score) :
                    local_basic_cost(work,leaf,component?work->best_ids[leaf]:mode,&score);
                if (error!=SVT_AV2_ERROR_NONE) break;
                if (component && work->basic_rate_active && work->uv_y_size) work->uv_y_cache=2u;
                if (score<selected || (score==selected &&
                    (mode==incumbent || (choice!=incumbent && mode<choice)))) {
                    selected=score;choice=mode;
                }
            }
            work->uv_y_cache=0u;work->uv_y_size=0u;
            work->ids[leaf]=work->best_ids[leaf];
            work->uv_ids[leaf]=(uint8_t)(component?choice:original_uv);
            if (!component) work->best_ids[leaf]=(uint8_t)choice;
            work->coeff_count=work->prefix_coeff_count;
            region_copy_buffer(work,work->prefix_recon,1);
            if (selected==UINT64_MAX && error==SVT_AV2_ERROR_NONE) error=SVT_AV2_ERROR_INVALID_STATE;
        }
    }
    if (error==SVT_AV2_ERROR_NONE) error=extend_prefix(work,work->leaf_count);
    uint64_t prefix_rate=0u;
    if (error==SVT_AV2_ERROR_NONE && work->basic_rate_active &&
        (svt_av2_tile_rate_prefix_trial(work->basic_rate_prefix,work->leaf_count-1u,
            work->blocks+(work->leaf_count-1u)*3u,1,&prefix_rate)!=0 ||
         svt_av2_tile_rate_prefix_finish(work->basic_rate_prefix,&prefix_rate)!=0))
        error=SVT_AV2_ERROR_INVALID_STATE;
    uint64_t d=0u,r=0u,cost=0u;
    if (error==SVT_AV2_ERROR_NONE) {
        d=svt_av2_rd_distortion(work->prefix_sse,work->source->bit_depth,work->source->chroma_format);
        if (svt_av2_tile_rate_trial(work->rate,sb,decision,work->blocks,
            work->leaf_count*3u,NULL,0u,0,&r)!=0 ||
            (work->basic_rate_active && prefix_rate!=r)) error=SVT_AV2_ERROR_INVALID_STATE;
    }
    if (error==SVT_AV2_ERROR_NONE) error=svt_av2_rd_cost(d,work->stats.lambda_q8,r,&cost);
    if (error==SVT_AV2_ERROR_NONE) {
        const uint32_t payload=(1u<<28u)|((work->leaf_count-1u)<<5u)|(1u<<4u)|work->uv_ids[work->leaf_count-1u];
        const uint64_t id=plan_candidate_id(decision,payload|((uint32_t)decision<<16u),payload);
        ++work->stats.candidate_count;
        if (work->config->candidate_hook!=NULL)
            work->config->candidate_hook(work->config->candidate_hook_user,sb,id,d,r,cost,0);
        if (cost<best->cost) {
            best->cost=cost;best->distortion=d;best->rate=r;best->id=id;best->hash=path_hash(work);
        } else {
            memcpy(work->best_ids,entering_y,sizeof(entering_y));
            memcpy(work->uv_ids,entering_uv,sizeof(entering_uv));
        }
    }
    work->memo_active=0u;work->basic_rate_active=0u;work->prefix_active=0u;region_copy(work,1);
    return error;
}

static SvtAv2Error refine_basic(RdWork* work,size_t sb,int partial,
    uint16_t decision,RdBasicChoice* best) {
    return work->config->rd_mode==3u?refine_basic_fast(work,sb,partial,decision,best):
        refine_basic_original(work,sb,partial,decision,best);
}

static SvtAv2Error rank_cfl_alphas(RdWork* work, const SvtAv2BlockView* block,
    uint32_t plane, int8_t ranked[2]) {
    uint8_t predicted[64u*64u*2u];
    uint64_t scores[2]={UINT64_MAX,UINT64_MAX};
    const uint32_t step=work->source->bit_depth>8u?2u:1u;
    const SvtAv2IntraBlock intra={block->x,block->y,block->w,block->h,plane};
    for (int32_t alpha=-8;alpha<=8;++alpha) {
        const SvtAv2Error error=svt_av2_intra_cfl_explicit_predict(work->recon,&intra,64u,
            predicted,block->w*step,alpha);
        if (error!=SVT_AV2_ERROR_NONE) return error;
        uint64_t score=0u;
        for (uint32_t y=0u;y<block->h;++y) for (uint32_t x=0u;x<block->w;++x) {
            const uint8_t* sample=work->source->planes[plane]+(size_t)(block->y+y)*work->source->strides[plane]+(block->x+x)*step;
            const int difference=(int)svt_av2_sample_load(sample,work->source->bit_depth)-
                (int)svt_av2_sample_load(predicted+((size_t)y*block->w+x)*step,work->source->bit_depth);
            score+=(uint64_t)(difference*difference);
        }
        for (uint32_t slot=0u;slot<2u;++slot) if (score<scores[slot]) {
            if (!slot) {scores[1]=scores[0];ranked[1]=ranked[0];}
            scores[slot]=score;ranked[slot]=(int8_t)alpha;break;
        }
    }
    return SVT_AV2_ERROR_NONE;
}

typedef struct RdJointChoice {
    RdBasicChoice choice;
    uint32_t mode;
} RdJointChoice;

/* A finite proposal domain, with exact suffix reconstruction and full tile R.
 * DCT-J only ranks two basic Y modes; it never replaces the entering tuple.
 * The complete original tool route is separately retained by the wrapper. */
static SvtAv2Error refine_joint_y_tx(RdWork* work, size_t sb, int partial,
    uint16_t decision, RdBasicChoice* best) {
    SvtAv2Error error=prepare_prefix(work,0u);
    if (error!=SVT_AV2_ERROR_NONE) {region_copy(work,1);return error;}
    work->prefix_active=1u;
    for (uint32_t leaf=0u;leaf<work->leaf_count;++leaf) {
        const uint32_t w=work->leaves[leaf].w,h=work->leaves[leaf].h;
        if (!((w==h && w<=16u) || (w==8u && h==16u) || (w==16u && h==8u))) continue;
        error=extend_prefix(work,leaf);
        if (error!=SVT_AV2_ERROR_NONE) {work->prefix_active=0u;region_copy(work,1);return error;}
        const uint8_t old_mode=work->best_ids[leaf],old_mrl=work->mrl[leaf],old_tx=work->tx_types[leaf];
        RdJointChoice ranked[2]={{{UINT64_MAX,0u,0u,0u,UINT64_MAX},255u},
            {{UINT64_MAX,0u,0u,0u,UINT64_MAX},255u}};
        for (uint32_t index=0u;index<14u;++index) {
            const uint32_t mode=work->config->reverse_candidates?13u-index:index;
            if (mode==3u || !legal_mode(work,leaf,mode)) continue;
            RdBasicChoice trial=*best;
            if (mode!=old_mode || old_mrl || old_tx) {
                work->best_ids[leaf]=(uint8_t)mode;work->mrl[leaf]=0u;work->tx_types[leaf]=0u;
                region_copy_buffer(work,work->prefix_recon,1);
                uint32_t path;int valid;
                error=build_path(work,0u,0,partial,work->best_ids,&valid,&trial.distortion,&path);
                if (error==SVT_AV2_ERROR_NONE && (!valid || svt_av2_tile_rate_trial(work->rate,sb,decision,
                    work->blocks,work->leaf_count*3u,NULL,0u,0,&trial.rate)!=0)) error=SVT_AV2_ERROR_INVALID_STATE;
                if (error==SVT_AV2_ERROR_NONE) error=svt_av2_rd_cost(trial.distortion,work->stats.lambda_q8,trial.rate,&trial.cost);
                if (error!=SVT_AV2_ERROR_NONE) {work->prefix_active=0u;region_copy(work,1);return error;}
                trial.hash=path_hash(work);
                const uint32_t payload=((1u<<31u)|(1u<<23u))|(leaf<<8u)|mode;
                trial.id=plan_candidate_id(decision,payload,payload);
                ++work->stats.candidate_count;
                if (work->config->candidate_hook!=NULL) work->config->candidate_hook(work->config->candidate_hook_user,
                    sb,trial.id,trial.distortion,trial.rate,trial.cost,0);
            }
            for (uint32_t slot=0u;slot<2u;++slot) if (trial.cost<ranked[slot].choice.cost ||
                (trial.cost==ranked[slot].choice.cost && mode<ranked[slot].mode)) {
                if (!slot) ranked[1]=ranked[0];
                ranked[slot].choice=trial;ranked[slot].mode=mode;break;
            }
            work->best_ids[leaf]=old_mode;work->mrl[leaf]=old_mrl;work->tx_types[leaf]=old_tx;
        }
        RdBasicChoice chosen=*best,coordinate=*best;
        uint32_t chosen_code=UINT32_MAX;
        uint8_t chosen_mode=old_mode,chosen_mrl=old_mrl,chosen_tx=old_tx;
        for (uint32_t index=0u;index<8u;++index) {
            const uint32_t pair=work->config->reverse_candidates?7u-index:index;
            const uint32_t rank=pair/4u,type=pair%4u,mode=ranked[rank].mode;
            if (mode==255u || (type==1u && w==4u && mode==7u)) continue;
            work->best_ids[leaf]=(uint8_t)mode;work->mrl[leaf]=0u;work->tx_types[leaf]=(uint8_t)type;
            RdBasicChoice trial=ranked[rank].choice;
            if (type) {
                region_copy_buffer(work,work->prefix_recon,1);
                uint32_t path;int valid;
                error=build_path(work,0u,0,partial,work->best_ids,&valid,&trial.distortion,&path);
                if (error==SVT_AV2_ERROR_NONE && (!valid || svt_av2_tile_rate_trial(work->rate,sb,decision,
                    work->blocks,work->leaf_count*3u,NULL,0u,0,&trial.rate)!=0)) error=SVT_AV2_ERROR_INVALID_STATE;
                if (error==SVT_AV2_ERROR_NONE) error=svt_av2_rd_cost(trial.distortion,work->stats.lambda_q8,trial.rate,&trial.cost);
                if (error!=SVT_AV2_ERROR_NONE) {work->prefix_active=0u;region_copy(work,1);return error;}
                trial.hash=path_hash(work);++work->stats.candidate_count;
            }
            const uint32_t code=mode*4u+type,payload=((1u<<31u)|(1u<<23u))|(1u<<20u)|(leaf<<8u)|code;
            trial.id=plan_candidate_id(decision,payload,payload);
            if (work->config->candidate_hook!=NULL) work->config->candidate_hook(work->config->candidate_hook_user,
                sb,trial.id,trial.distortion,trial.rate,trial.cost,0);
            if (!rank && trial.cost<coordinate.cost) coordinate=trial;
            if (trial.cost<chosen.cost || (trial.cost==chosen.cost && chosen_code!=UINT32_MAX && code<chosen_code)) {
                chosen=trial;chosen_code=code;chosen_mode=(uint8_t)mode;chosen_mrl=0u;chosen_tx=(uint8_t)type;
            }
        }
        if (work->config->candidate_hook!=NULL) {
            const uint32_t payload=((1u<<31u)|(1u<<23u))|(2u<<20u)|(leaf<<8u);
            work->config->candidate_hook(work->config->candidate_hook_user,sb,plan_candidate_id(decision,payload,payload),
                coordinate.distortion,coordinate.rate,coordinate.cost,0);
            work->config->candidate_hook(work->config->candidate_hook_user,sb,plan_candidate_id(decision,payload|1u,payload|1u),
                chosen.distortion,chosen.rate,chosen.cost,0);
        }
        work->best_ids[leaf]=chosen_mode;work->mrl[leaf]=chosen_mrl;work->tx_types[leaf]=chosen_tx;
        *best=chosen;
    }
    work->prefix_active=0u;region_copy(work,1);
    return SVT_AV2_ERROR_NONE;
}

#if M6_RDOQ_CONTEXT
static SvtAv2Error prepare_rdoq_rate_context(RdWork* work,size_t sb,uint16_t decision,
    uint32_t leaf) {
    work->rdoq_rate_active=0u;work->rdoq_rate_ready=0u;
    if (!work->rdoq_use_context || work->config->frame_header.m5_intra_modes<3u)
        return SVT_AV2_ERROR_NONE;
    if (!work->rdoq_rate_prefix || work->rdoq_target!=UINT32_MAX)
        return SVT_AV2_ERROR_INVALID_STATE;
    /* extend_prefix has rebuilt every prior leaf from the selected coefficients.
     * Rebuild this leaf without a new proposal; stale rejected suffixes are not
     * valid anchors. Other planes are fixed only for proposing levels. */
    region_copy_buffer(work,work->prefix_recon,1);
    SvtAv2BlockView blocks[3];
    SvtAv2Error error=plane_blocks(work,leaf,blocks);
    if (error!=SVT_AV2_ERROR_NONE) return error;
    work->coeff_count=work->prefix_coeff_count;work->ids[leaf]=work->best_ids[leaf];
    work->uv_y_cache=0u;
    uint64_t sse[3]={0u,0u,0u};
    error=reconstruct_leaf(work,leaf,work->best_ids[leaf],blocks,sse);
    if (error!=SVT_AV2_ERROR_NONE) return error;
    for (uint32_t p=0u;p<3u;++p) {
        work->rdoq_rate_blocks[p]=work->blocks[leaf*3u+p];
        const size_t count=(size_t)SVT_AV2_MIN(blocks[p].w,32u)*SVT_AV2_MIN(blocks[p].h,32u);
        memcpy(work->rdoq_rate_coeffs+p*1024u,work->blocks[leaf*3u+p].qcoeff,count*sizeof(int32_t));
        work->rdoq_rate_blocks[p].qcoeff=NULL;
    }
    size_t leaves=0u;
    if (svt_av2_tile_rate_prefix_begin(work->rdoq_rate_prefix,work->rate,sb,decision,&leaves)!=0 ||
        leaves!=work->leaf_count) return SVT_AV2_ERROR_INVALID_STATE;
    for (uint32_t i=0u;i<leaf;++i) {
        uint64_t rate=0u;
        if (svt_av2_tile_rate_prefix_trial(work->rdoq_rate_prefix,i,
            work->blocks+i*3u,1,&rate)!=0) return SVT_AV2_ERROR_INVALID_STATE;
    }
    work->rdoq_rate_leaf=leaf;work->rdoq_rate_ready=1u;
    region_copy_buffer(work,work->prefix_recon,1);
    return SVT_AV2_ERROR_NONE;
}
#endif

#ifndef M6_RDOQ_WIDE
#define M6_RDOQ_WIDE 0
#endif
static SvtAv2Error refine_rdoq_single(RdWork* work, size_t sb, int partial,
    uint16_t best_decision, RdBasicChoice* best) {
    SvtAv2Error error;
    work->rdoq_phase=0u;
    uint64_t best_cost=best->cost, best_d=best->distortion, best_r=best->rate, best_hash=best->hash;
    uint64_t best_id=best->id;
    if (work->config->coeff_optimization == 2u) {
        error = prepare_prefix(work, 0u);
        if (error != SVT_AV2_ERROR_NONE) { region_copy(work, 1); return error; }
        work->prefix_active = 1u;
        /* One proposal per leaf/plane. Previously accepted coefficient arrays
         * are replayed; later leaves see the candidate's real reconstruction.
         * No approximate coefficient cost can directly become a winner. */
        for (uint32_t block = 0u; block < work->leaf_count * 3u; ++block) {
            const SvtAv2BlockView* leaf = &work->leaves[block / 3u];
            /* Keep the new large rectangles scalar until their transform-domain
             * normalization has been independently verified. */
            if (leaf->w != leaf->h && (leaf->w > 16u || leaf->h > 16u)) continue;
            work->rdoq_target = UINT32_MAX;
            error = extend_prefix(work, block / 3u);
            if (error != SVT_AV2_ERROR_NONE) {
                work->prefix_active = 0u; region_copy(work, 1); return error;
            }
            work->rdoq_target = block;
            region_copy_buffer(work, work->prefix_recon, 1);
            uint64_t d = 0u, r = 0u, cost = 0u; uint32_t path; int valid;
            error = build_path(work, 0u, 0, partial, work->best_ids, &valid, &d, &path);
            if (error == SVT_AV2_ERROR_NONE && (!valid ||
                svt_av2_tile_rate_trial(work->rate, sb, best_decision, work->blocks,
                    work->leaf_count * 3u, NULL, 0u, 0, &r) != 0)) error = SVT_AV2_ERROR_INVALID_STATE;
            if (error == SVT_AV2_ERROR_NONE) error = svt_av2_rd_cost(d, work->stats.lambda_q8, r, &cost);
            if (error != SVT_AV2_ERROR_NONE) { work->prefix_active = 0u; region_copy(work, 1); return error; }
            const uint32_t payload = (1u << 26u) | block;
            const uint64_t id = plan_candidate_id(best_decision,payload,payload);
            ++work->stats.candidate_count; ++work->stats.coeff_trials;
            if (work->config->candidate_hook != NULL)
                work->config->candidate_hook(work->config->candidate_hook_user, sb, id, d, r, cost, 0);
            if (cost < best_cost) {
                best_cost = cost; best_d = d; best_r = r; best_id = id;
                best_hash = path_hash(work); work->rdoq_saved[block] = 1u;
                ++work->stats.coeff_accepts;
            }
        }
        work->rdoq_target = UINT32_MAX;
        work->prefix_active = 0u; region_copy(work, 1);
    }
    best->cost=best_cost; best->distortion=best_d; best->rate=best_r;
    best->hash=best_hash; best->id=best_id;
    return SVT_AV2_ERROR_NONE;
}

#if M6_RDOQ_WIDE
static SvtAv2Error refine_rdoq_multi(RdWork* work, size_t sb, int partial,
    uint16_t best_decision, RdBasicChoice* best) {
    SvtAv2Error error;
    work->rdoq_phase=0u;
    uint64_t best_cost=best->cost, best_d=best->distortion, best_r=best->rate, best_hash=best->hash;
    uint64_t best_id=best->id;
    if (work->config->coeff_optimization == 2u) {
        error = prepare_prefix(work, 0u);
        if (error != SVT_AV2_ERROR_NONE) { region_copy(work, 1); return error; }
        work->prefix_active = 1u;
        /* Bounded strengths start from the same scalar coefficients. Every
         * proposal rebuilds the suffix and rates the complete actual SB.
         * No approximate coefficient cost can directly become a winner. */
        for (uint32_t block = 0u; block < work->leaf_count * 3u; ++block) {
            const SvtAv2BlockView* leaf=&work->leaves[block/3u];
            if (leaf->w!=leaf->h && (leaf->w>16u || leaf->h>16u)) continue;
            work->rdoq_target = UINT32_MAX;
            error = extend_prefix(work, block / 3u);
            if (error != SVT_AV2_ERROR_NONE) {
                work->prefix_active = 0u; region_copy(work, 1); return error;
            }
#if M6_RDOQ_CONTEXT
            error=prepare_rdoq_rate_context(work,sb,best_decision,block/3u);
            if (error!=SVT_AV2_ERROR_NONE) {
                work->prefix_active=0u;region_copy(work,1);return error;
            }
#endif
            int32_t chosen_coeff[1024];
            size_t chosen_offset=0u,chosen_count=0u;
            uint32_t chosen_phase=3u;uint8_t chosen_type=0u;
            for (uint32_t index=0u;index<3u;++index) {
                const uint32_t phase=work->config->reverse_candidates?2u-index:index;
                work->rdoq_phase=phase;
                work->rdoq_target = block;
                region_copy_buffer(work, work->prefix_recon, 1);
                uint64_t d = 0u, r = 0u, cost = 0u; uint32_t path; int valid;
                error = build_path(work, 0u, 0, partial, work->best_ids, &valid, &d, &path);
                if (error == SVT_AV2_ERROR_NONE && (!valid ||
                    svt_av2_tile_rate_trial(work->rate, sb, best_decision, work->blocks,
                        work->leaf_count * 3u, NULL, 0u, 0, &r) != 0)) error = SVT_AV2_ERROR_INVALID_STATE;
                if (error == SVT_AV2_ERROR_NONE) error = svt_av2_rd_cost(d, work->stats.lambda_q8, r, &cost);
                if (error != SVT_AV2_ERROR_NONE) { work->prefix_active = 0u; region_copy(work, 1); return error; }
                const uint32_t payload = (1u << (phase ? 29u : 26u)) | (phase << 8u) | block;
                const uint64_t id = plan_candidate_id(best_decision,payload,payload);
                ++work->stats.candidate_count; ++work->stats.coeff_trials;
                if (work->config->candidate_hook != NULL)
                    work->config->candidate_hook(work->config->candidate_hook_user, sb, id, d, r, cost, 0);
                if (cost < best_cost || (cost == best_cost && chosen_phase < 3u && phase < chosen_phase)) {
                    best_cost = cost; best_d = d; best_r = r; best_id = id;
                    best_hash = path_hash(work); work->rdoq_saved[block] = 1u;
                    const SvtAv2TileCoeffBlock* entry=work->blocks+block;
                    chosen_offset=(size_t)(entry->qcoeff-work->coeffs);
                    chosen_count=(size_t)SVT_AV2_MIN(entry->width,32u)*SVT_AV2_MIN(entry->height,32u);
                    memcpy(chosen_coeff,entry->qcoeff,chosen_count*sizeof(*chosen_coeff));
                    chosen_type=work->rdoq_tx_types[block];chosen_phase=phase;
                    ++work->stats.coeff_accepts;
                }
            }
            if (chosen_phase < 3u) {
                memcpy(work->rdoq_coeff+chosen_offset,chosen_coeff,chosen_count*sizeof(*chosen_coeff));
                work->rdoq_tx_types[block]=chosen_type;
            }
        }
        work->rdoq_rate_active=0u;
        work->rdoq_target = UINT32_MAX;
        work->rdoq_phase = 0u;
        work->prefix_active = 0u; region_copy(work, 1);
    }
    best->cost=best_cost; best->distortion=best_d; best->rate=best_r;
    best->hash=best_hash; best->id=best_id;
    return SVT_AV2_ERROR_NONE;
}

#endif
static SvtAv2Error refine_rdoq_fast(RdWork* work,size_t sb,int partial,
    uint16_t best_decision,RdBasicChoice* best) {
#if M6_RDOQ_WIDE
    if (work->config->coeff_optimization!=2u)
        return refine_rdoq_single(work,sb,partial,best_decision,best);
    RdWork* states=(RdWork*)malloc(2u*sizeof(*states));
    if (states==NULL) return SVT_AV2_ERROR_OUT_OF_MEMORY;
    states[0]=*work;
    const RdBasicChoice entry=*best;
    RdBasicChoice winner={UINT64_MAX,0u,0u,0u,UINT64_MAX};
    uint32_t winner_route=0u;
    SvtAv2RdFrameStats aggregate=work->stats;
    SvtAv2Error error=SVT_AV2_ERROR_NONE;
    const uint32_t routes=M6_RDOQ_CONTEXT?3u:2u;
    for (uint32_t route=0u;route<routes;++route) {
        *work=states[0];work->stats=aggregate;
        work->rdoq_use_context=route==2u;work->rdoq_rate_active=0u;work->rdoq_rate_ready=0u;
        region_copy(work,1);
        RdBasicChoice trial=entry;
#if M6_TX_CACHE
        if (work->transform_cache) ++work->transform_cache->rdoq_route_trials[route];
#endif
        error=route?refine_rdoq_multi(work,sb,partial,best_decision,&trial):
            refine_rdoq_single(work,sb,partial,best_decision,&trial);
        aggregate=work->stats;
        if (error!=SVT_AV2_ERROR_NONE) break;
        if (trial.cost<winner.cost) {winner=trial;winner_route=route;states[1]=*work;}
    }
    *work=states[error==SVT_AV2_ERROR_NONE?1u:0u];work->stats=aggregate;
    work->rdoq_rate_active=0u;work->rdoq_rate_ready=0u;work->rdoq_use_context=0u;
#if M6_TX_CACHE
    if (error==SVT_AV2_ERROR_NONE && work->transform_cache)
        ++work->transform_cache->rdoq_route_wins[winner_route];
#else
    (void)winner_route;
#endif
    region_copy(work,1);free(states);
    if (error==SVT_AV2_ERROR_NONE) *best=winner;
    return error;
#else
    return refine_rdoq_single(work,sb,partial,best_decision,best);
#endif
}

typedef struct RdLeafTools {
    uint8_t y,uv,mrl,tx,explicit_cfl,signs,idx,mh,effective_tx;
} RdLeafTools;

static RdLeafTools leaf_tools_get(const RdWork* work,uint32_t leaf) {
    const RdLeafTools tuple={work->best_ids[leaf],work->uv_ids[leaf],work->mrl[leaf],
        work->tx_types[leaf],work->cfl_explicit[leaf],work->cfl_alpha_signs[leaf],
        work->cfl_alpha_idx[leaf],work->mhccp[leaf],work->blocks[leaf*3u].tx_type};
    return tuple;
}
static void leaf_tools_set(RdWork* work,uint32_t leaf,const RdLeafTools* tuple) {
    work->best_ids[leaf]=tuple->y;work->uv_ids[leaf]=tuple->uv;
    work->mrl[leaf]=tuple->mrl;work->tx_types[leaf]=tuple->tx;
    work->cfl_explicit[leaf]=tuple->explicit_cfl;work->cfl_alpha_signs[leaf]=tuple->signs;
    work->cfl_alpha_idx[leaf]=tuple->idx;work->mhccp[leaf]=tuple->mh;
    work->blocks[leaf*3u].tx_type=tuple->effective_tx;
}
static uint64_t leaf_tools_key(const RdLeafTools* t) {
    return ((uint64_t)t->y<<56u)|((uint64_t)t->uv<<48u)|((uint64_t)t->mrl<<40u)|
        ((uint64_t)t->tx<<32u)|((uint64_t)t->explicit_cfl<<24u)|
        ((uint64_t)t->signs<<16u)|((uint64_t)t->idx<<8u)|t->mh;
}
/* Partial-prefix scores never masquerade as complete-SB candidate hooks. */
static SvtAv2Error linear_leaf_trial(RdWork* work,uint32_t leaf,RdLeafTools proposal,
    RdLeafTools* best,uint64_t* cost,uint64_t entering_cost,int* accepted) {
    leaf_tools_set(work,leaf,&proposal);
    region_copy_buffer(work,work->prefix_recon,1);
    uint64_t score=0u;
#if M6_Y_BOUND && M6_TX_CACHE
    uint64_t lower=0u,y_rate=0u;
    int bound_active=0;
#if M6_Y_BOUND_VERIFY
    uint8_t y_probe[64u*64u*2u];
#endif
    /* This stage precedes RDOQ. The exact production Y reconstruction is the
     * same in the one-plane and full path. Omit only nonnegative UV error/rate.
     * Strict > preserves all possible cost ties and the existing tuple order. */
    if (work->transform_cache && work->basic_rate_active && !work->memo_active &&
        !work->uv_y_cache && work->rdoq_target==UINT32_MAX &&
        !work->rdoq_saved[leaf*3u]) {
        SvtAv2BlockView blocks[3];
        SvtAv2Error pre=plane_blocks(work,leaf,blocks);
        uint64_t y_sse[3]={0u,0u,0u};
        work->coeff_count=work->prefix_coeff_count;work->ids[leaf]=proposal.y;
        if (pre==SVT_AV2_ERROR_NONE)
            pre=reconstruct_leaf_planes(work,leaf,proposal.y,blocks,y_sse,1u);
        if (pre!=SVT_AV2_ERROR_NONE) { *accepted=0;leaf_tools_set(work,leaf,best);return pre; }
        uint64_t partial_sse[3]={work->prefix_sse[0]+y_sse[0],work->prefix_sse[1],work->prefix_sse[2]};
        if (svt_av2_tile_rate_prefix_trial_y(work->basic_rate_prefix,leaf,
            work->blocks+leaf*3u,&y_rate)!=0)
            return SVT_AV2_ERROR_INVALID_STATE;
        pre=svt_av2_rd_cost(svt_av2_rd_distortion(partial_sse,work->source->bit_depth,
            work->source->chroma_format),work->stats.lambda_q8,y_rate,&lower);
        if (pre!=SVT_AV2_ERROR_NONE) return pre;
        ++work->transform_cache->bound_trials;bound_active=1;
#if M6_Y_BOUND_VERIFY
        const SvtAv2BlockView* yb=&blocks[0];
        const uint32_t bytes=work->recon->bit_depth>8u?2u:1u;
        for (uint32_t y=0u;y<yb->h;++y)
            memcpy(y_probe+(size_t)y*yb->w*bytes,work->recon->planes[0]+
                (size_t)(yb->y+y)*work->recon->strides[0]+yb->x*bytes,(size_t)yb->w*bytes);
#else
        if (lower>*cost) {
            ++work->transform_cache->bound_pruned;
            *accepted=0;leaf_tools_set(work,leaf,best);
            region_copy_buffer(work,work->prefix_recon,1);
            work->coeff_count=work->prefix_coeff_count;
            return SVT_AV2_ERROR_NONE;
        }
#endif
        region_copy_buffer(work,work->prefix_recon,1);
    }
#endif
    const SvtAv2Error error=local_basic_cost(work,leaf,proposal.y,&score);
#if M6_Y_BOUND && M6_TX_CACHE
    if (error==SVT_AV2_ERROR_NONE && bound_active && lower>score)
        return SVT_AV2_ERROR_INVALID_STATE;
#if M6_Y_BOUND_VERIFY
    if (error==SVT_AV2_ERROR_NONE && bound_active) {
        uint64_t full_rate=0u;
        if (svt_av2_tile_rate_prefix_trial(work->basic_rate_prefix,leaf,
            work->blocks+leaf*3u,0,&full_rate)!=0 || y_rate>full_rate)
            return SVT_AV2_ERROR_INVALID_STATE;
        const SvtAv2BlockView* yb=&work->leaves[leaf];
        const uint32_t bytes=work->recon->bit_depth>8u?2u:1u;
        for (uint32_t y=0u;y<yb->h;++y)
            if (memcmp(y_probe+(size_t)y*yb->w*bytes,work->recon->planes[0]+
                (size_t)(yb->y+y)*work->recon->strides[0]+yb->x*bytes,(size_t)yb->w*bytes)!=0)
                return SVT_AV2_ERROR_INVALID_STATE;
    }
#endif
#endif
    *accepted=0;
    if (error==SVT_AV2_ERROR_NONE && (score<*cost ||
        (score==*cost && score<entering_cost && leaf_tools_key(&proposal)<leaf_tools_key(best)))) {
        *best=leaf_tools_get(work,leaf);*cost=score;*accepted=1;
    }
    leaf_tools_set(work,leaf,best);
    return error;
}

#if M6_TOOL_REPEAT
/* A changed direction or MRL may not support the inherited primary TX.
 * Keep every legal inherited pair and independently offer the default TX.
 * Reject known illegal syntax before touching candidate state. */
static SvtAv2Error repeat_pair_trial(RdWork* work,uint32_t leaf,RdLeafTools proposal,
    RdLeafTools* best,uint64_t* cost,uint64_t anchor,int mrl_trial) {
    const uint32_t w=work->leaves[leaf].w,h=work->leaves[leaf].h;
    const uint8_t inherited=proposal.tx;
    for (uint32_t arm=0u;arm<2u;++arm) {
        if (arm && !inherited) continue;
        proposal.tx=arm?0u:inherited;proposal.effective_tx=0u;
        if (proposal.tx && svt_av2_intra_tx_symbol(
            svt_av2_intra_tx_mode(proposal.y,w,h,proposal.mrl),w,h,proposal.tx)>=7u) {
#if M6_TX_CACHE
            if (work->transform_cache) ++work->transform_cache->repeat_pair_illegal;
#endif
            continue;
        }
        int accepted=0;
        const SvtAv2Error error=linear_leaf_trial(work,leaf,proposal,best,cost,anchor,&accepted);
        if (error!=SVT_AV2_ERROR_NONE) return error;
        if (mrl_trial) {++work->stats.mrl_trials;work->stats.mrl_accepts+=(uint64_t)accepted;}
        else {++work->stats.angle_trials;work->stats.angle_accepts+=(uint64_t)accepted;}
#if M6_TX_CACHE
        if (work->transform_cache) {
            ++work->transform_cache->repeat_pair_trials;
            work->transform_cache->repeat_pair_default+=(uint64_t)arm;
        }
#endif
    }
    return SVT_AV2_ERROR_NONE;
}
#endif

static SvtAv2Error refine_tools_linear(RdWork* work,size_t sb,int partial,
    uint16_t decision,RdBasicChoice* best) {
    RdLeafTools entering[RD_MAX_LEAVES];
    for (uint32_t leaf=0u;leaf<work->leaf_count;++leaf) entering[leaf]=leaf_tools_get(work,leaf);
    size_t leaves=0u;
    SvtAv2Error error=SVT_AV2_ERROR_NONE;
    if (svt_av2_tile_rate_prefix_begin(work->basic_rate_prefix,work->rate,sb,decision,&leaves)!=0 ||
        leaves!=work->leaf_count) return SVT_AV2_ERROR_INVALID_STATE;
    work->basic_rate_active=1u;
    error=prepare_prefix(work,0u);
    if (error!=SVT_AV2_ERROR_NONE) goto cleanup;
    work->prefix_active=1u;
    for (uint32_t leaf=0u;leaf<work->leaf_count;++leaf) {
        error=extend_prefix(work,leaf);
        if (error!=SVT_AV2_ERROR_NONE) goto cleanup;
        uint64_t previous=0u;
        if (leaf && svt_av2_tile_rate_prefix_trial(work->basic_rate_prefix,leaf-1u,
            work->blocks+(leaf-1u)*3u,1,&previous)!=0) {error=SVT_AV2_ERROR_INVALID_STATE;goto cleanup;}
        RdLeafTools selected=leaf_tools_get(work,leaf);
        region_copy_buffer(work,work->prefix_recon,1);
        uint64_t cost=0u;
        error=local_basic_cost(work,leaf,selected.y,&cost);
        if (error!=SVT_AV2_ERROR_NONE) goto cleanup;
        selected=leaf_tools_get(work,leaf);
        const uint32_t rounds=work->tool_rounds?work->tool_rounds:1u;
#if M6_TOOL_REPEAT && M6_TX_CACHE
        int first_joint_changed=0;
#endif
        for (uint32_t tool_round=0u;tool_round<rounds;++tool_round) {
            if (tool_round) {
                work->uv_y_cache=0u;work->uv_y_size=0u;
                leaf_tools_set(work,leaf,&selected);
                region_copy_buffer(work,work->prefix_recon,1);
                error=local_basic_cost(work,leaf,selected.y,&cost);
                if (error!=SVT_AV2_ERROR_NONE) goto cleanup;
                selected=leaf_tools_get(work,leaf);
            }
#if M6_TOOL_REPEAT && M6_TX_CACHE
            const uint64_t round_start=leaf_tools_key(&selected);
#endif
        if (work->config->angle_search && legal_mode(work,leaf,14u)) {
            SvtAv2BlockView blocks[3];
            error=plane_blocks(work,leaf,blocks);
            if (error!=SVT_AV2_ERROR_NONE) goto cleanup;
            uint8_t ranked[4]={255u,255u,255u,255u};
            uint64_t scores[4]={UINT64_MAX,UINT64_MAX,UINT64_MAX,UINT64_MAX};
#if M6_SATD_ANGLES
            uint8_t satd_ranked[4]={255u,255u,255u,255u};
            uint64_t satd_scores[4]={UINT64_MAX,UINT64_MAX,UINT64_MAX,UINT64_MAX};
#endif
            for (uint32_t mode=14u;mode<=61u;++mode) {
                uint64_t sad[3]={0u,0u,0u};
                error=predict(work,leaf,mode,blocks,sad,1);
                if (error!=SVT_AV2_ERROR_NONE) goto cleanup;
#if M6_SATD_ANGLES
                angle_satd_rank(work,&blocks[0],mode,satd_ranked,satd_scores);
#endif
                for (uint32_t slot=0u;slot<4u;++slot) if (sad[0]<scores[slot] ||
                    (sad[0]==scores[slot] && mode<ranked[slot])) {
                    for (uint32_t j=3u;j>slot;--j) {scores[j]=scores[j-1u];ranked[j]=ranked[j-1u];}
                    scores[slot]=sad[0];ranked[slot]=(uint8_t)mode;break;
                }
            }
            uint8_t candidates[14];uint32_t count=4u;
            memcpy(candidates,ranked,sizeof(ranked));
            const uint32_t joint=svt_av2_intra_mode_joint(svt_av2_intra_mode_base(selected.y));
            if (joint>=5u) for (uint32_t slot=0u;slot<6u;++slot) {
                const uint8_t mode=(uint8_t)(14u+((joint-5u)/7u)*6u+slot);
                uint32_t i=0u;while (i<count && candidates[i]!=mode) ++i;
                if (i==count) candidates[count++]=mode;
            }
#if M6_SATD_ANGLES
            angle_satd_append(work,candidates,&count,satd_ranked);
#endif
            const uint64_t anchor=cost;
            for (uint32_t index=0u;index<count;++index) {
                RdLeafTools proposal=selected;
                proposal.y=candidates[work->config->reverse_candidates?count-1u-index:index];
#if M6_TOOL_REPEAT
                if (tool_round) {
                    error=repeat_pair_trial(work,leaf,proposal,&selected,&cost,anchor,0);
                    if (error!=SVT_AV2_ERROR_NONE) goto cleanup;
                    continue;
                }
#endif
                int accepted=0;
                error=linear_leaf_trial(work,leaf,proposal,&selected,&cost,anchor,&accepted);
                if (error!=SVT_AV2_ERROR_NONE) goto cleanup;
                ++work->stats.angle_trials;work->stats.angle_accepts+=(uint64_t)accepted;
            }
        }
        const uint32_t w=work->leaves[leaf].w,h=work->leaves[leaf].h;
        if (work->config->frame_header.enable_mrls && w>=8u && h>=8u &&
            (w==h || w==2u*h || h==2u*w || (w==16u && h==64u) || (w==64u && h==16u)) &&
            svt_av2_intra_mode_joint(selected.y)>=5u) {
            static const uint8_t candidates[6]={1u,2u,3u,5u,6u,7u};
            const uint64_t anchor=cost;
            for (uint32_t index=0u;index<6u;++index) {
                RdLeafTools proposal=selected;
                proposal.mrl=candidates[work->config->reverse_candidates?5u-index:index];
#if M6_TOOL_REPEAT
                if (tool_round) {
                    error=repeat_pair_trial(work,leaf,proposal,&selected,&cost,anchor,1);
                    if (error!=SVT_AV2_ERROR_NONE) goto cleanup;
                    continue;
                }
#endif
                int accepted=0;
                error=linear_leaf_trial(work,leaf,proposal,&selected,&cost,anchor,&accepted);
                if (error!=SVT_AV2_ERROR_NONE) goto cleanup;
                ++work->stats.mrl_trials;work->stats.mrl_accepts+=(uint64_t)accepted;
            }
        }
        if (work->config->transform_selection && ((w==h && w<=16u) ||
            (work->config->transform_selection==2u && ((w==8u && h==16u) || (w==16u && h==8u))))) {
            const uint64_t anchor=cost;
            const uint32_t count=work->config->transform_selection==2u?(M6_FLIP_TX?8u:3u):1u;
            for (uint32_t index=0u;index<count;++index) {
                RdLeafTools proposal=selected;
                proposal.tx=(uint8_t)(count==1u?3u:work->config->reverse_candidates?count-index:1u+index);
                if (proposal.tx==1u && w==4u && selected.y==7u) continue;
#if M6_FLIP_TX
                if (work->config->transform_selection==2u &&
                    svt_av2_intra_tx_symbol(svt_av2_intra_tx_mode(proposal.y,w,h,proposal.mrl),w,h,proposal.tx)>=7u) continue;
#endif
                int accepted=0;
                error=linear_leaf_trial(work,leaf,proposal,&selected,&cost,anchor,&accepted);
                if (error!=SVT_AV2_ERROR_NONE) goto cleanup;
                ++work->stats.transform_trials;work->stats.transform_accepts+=(uint64_t)accepted;
            }
        }

#if M6_TOOL_REPEAT && M6_TX_CACHE
        const uint8_t before_joint_y=selected.y;
#endif
#if M6_JOINT_Y_TX && M6_FLIP_TX
        /* The DCT-selected prediction need not win after primary TX search.
         * Test every legal basic prediction/TX pair from one fixed entering
         * tuple and the same causal prefix; retain the original selected tuple.
         * Subsequent UV search and final complete-SB J gate are unchanged. */
        if (work->config->transform_selection==2u &&
            ((w==h && w<=16u) || (w==8u && h==16u) || (w==16u && h==8u))) {
            static const uint8_t modes[13]={0u,1u,2u,4u,5u,6u,7u,8u,9u,10u,11u,12u,13u};
            const RdLeafTools anchor_tuple=selected;
            const uint64_t anchor_cost=cost;
            for (uint32_t index=0u;index<13u*9u;++index) {
                const uint32_t pair=work->config->reverse_candidates?13u*9u-1u-index:index;
                RdLeafTools proposal=anchor_tuple;
                proposal.y=modes[pair/9u];proposal.tx=(uint8_t)(pair%9u);
                proposal.mrl=0u;proposal.effective_tx=0u;
                if (!legal_mode(work,leaf,proposal.y) ||
                    svt_av2_intra_tx_symbol(svt_av2_intra_tx_mode(proposal.y,w,h,proposal.mrl),w,h,proposal.tx)>=7u ||
                    leaf_tools_key(&proposal)==leaf_tools_key(&anchor_tuple)) continue;
                int accepted=0;
                error=linear_leaf_trial(work,leaf,proposal,&selected,&cost,anchor_cost,&accepted);
                if (error!=SVT_AV2_ERROR_NONE) goto cleanup;
                ++work->stats.transform_trials;work->stats.transform_accepts+=(uint64_t)accepted;
            }
        }
#endif
#if M6_TOOL_REPEAT && M6_TX_CACHE
        if (!tool_round && selected.y!=before_joint_y) first_joint_changed=1;
#endif
        if (work->config->cfl_search) {
            leaf_tools_set(work,leaf,&selected);
            region_copy_buffer(work,work->prefix_recon,1);
            error=local_basic_cost(work,leaf,selected.y,&cost);
            if (error!=SVT_AV2_ERROR_NONE) goto cleanup;
            SvtAv2BlockView blocks[3];
            error=plane_blocks(work,leaf,blocks);
            int8_t alphas[2][2]={{0,0},{0,0}};
            if (error==SVT_AV2_ERROR_NONE) error=rank_cfl_alphas(work,blocks+1u,1u,alphas[0]);
            if (error==SVT_AV2_ERROR_NONE) error=rank_cfl_alphas(work,blocks+2u,2u,alphas[1]);
            if (error!=SVT_AV2_ERROR_NONE) goto cleanup;
            const uint64_t anchor=cost;
            for (uint32_t index=0u;index<4u;++index) {
                const uint32_t pair=work->config->reverse_candidates?3u-index:index;
                const int32_t u=alphas[0][pair/2u],v=alphas[1][pair%2u];
                if (!u && !v) continue;
                const uint32_t usign=u<0?1u:u>0?2u:0u,vsign=v<0?1u:v>0?2u:0u;
                const uint32_t ui=u?(uint32_t)(u<0?-u:u)-1u:0u,vi=v?(uint32_t)(v<0?-v:v)-1u:0u;
                RdLeafTools proposal=selected;
                proposal.uv=14u;proposal.explicit_cfl=1u;proposal.mh=0u;
                proposal.signs=(uint8_t)(usign*3u+vsign-1u);proposal.idx=(uint8_t)((ui<<3u)|vi);
                int accepted=0;
                error=linear_leaf_trial(work,leaf,proposal,&selected,&cost,anchor,&accepted);
                if (error!=SVT_AV2_ERROR_NONE) goto cleanup;
                ++work->stats.cfl_trials;work->stats.cfl_accepts+=(uint64_t)accepted;
            }
        }
        if (work->config->mhccp_search &&
            svt_av2_intra_mhccp_allowed(w,h,(uint32_t)work->source->chroma_format)) {
            const uint64_t anchor=cost;
            for (uint32_t index=0u;index<3u;++index) {
                RdLeafTools proposal=selected;
                proposal.uv=14u;proposal.explicit_cfl=0u;proposal.idx=proposal.signs=0u;
                proposal.mh=(uint8_t)(work->config->reverse_candidates?3u-index:1u+index);
                int accepted=0;
                error=linear_leaf_trial(work,leaf,proposal,&selected,&cost,anchor,&accepted);
                if (error!=SVT_AV2_ERROR_NONE) goto cleanup;
                ++work->stats.mhccp_trials;work->stats.mhccp_accepts+=(uint64_t)accepted;
            }
        }

            leaf_tools_set(work,leaf,&selected);
#if M6_TOOL_REPEAT && M6_TX_CACHE
            if (tool_round && work->transform_cache) {
                ++work->transform_cache->repeat_rounds;
                work->transform_cache->repeat_changes+=(uint64_t)(leaf_tools_key(&selected)!=round_start);
                work->transform_cache->repeat_after_joint+=(uint64_t)first_joint_changed;
            }
#endif
            if (rounds>1u) {work->uv_y_cache=0u;work->uv_y_size=0u;}
        }
        leaf_tools_set(work,leaf,&selected);
    }
    error=extend_prefix(work,work->leaf_count);
    if (error==SVT_AV2_ERROR_NONE) {
        uint64_t rate=0u,whole=0u,cost=0u;
        if (svt_av2_tile_rate_prefix_trial(work->basic_rate_prefix,work->leaf_count-1u,
            work->blocks+(work->leaf_count-1u)*3u,1,&rate)!=0 ||
            svt_av2_tile_rate_prefix_finish(work->basic_rate_prefix,&rate)!=0 ||
            svt_av2_tile_rate_trial(work->rate,sb,decision,work->blocks,
                work->leaf_count*3u,NULL,0u,0,&whole)!=0 || rate!=whole)
            error=SVT_AV2_ERROR_INVALID_STATE;
        const uint64_t d=svt_av2_rd_distortion(work->prefix_sse,work->source->bit_depth,work->source->chroma_format);
        if (error==SVT_AV2_ERROR_NONE) error=svt_av2_rd_cost(d,work->stats.lambda_q8,rate,&cost);
        if (error==SVT_AV2_ERROR_NONE) {
            const uint64_t id=plan_candidate_id(decision,(1u<<29u)|decision,1u<<29u);
            ++work->stats.candidate_count;
            if (work->config->candidate_hook!=NULL)
                work->config->candidate_hook(work->config->candidate_hook_user,sb,id,d,rate,cost,0);
            if (cost<best->cost) {
                best->cost=cost;best->distortion=d;best->rate=rate;best->hash=path_hash(work);best->id=id;
            } else for (uint32_t leaf=0u;leaf<work->leaf_count;++leaf) leaf_tools_set(work,leaf,entering+leaf);
        }
    }
cleanup:
    if (error!=SVT_AV2_ERROR_NONE)
        for (uint32_t leaf=0u;leaf<work->leaf_count;++leaf) leaf_tools_set(work,leaf,entering+leaf);
    work->basic_rate_active=0u;work->prefix_active=0u;region_copy(work,1);
    return error==SVT_AV2_ERROR_NONE && !work->defer_rdoq?refine_rdoq_fast(work,sb,partial,decision,best):error;
}

static SvtAv2Error refine_tools_once(RdWork* work, size_t sb, int partial,
    uint16_t best_decision, RdBasicChoice* best) {
    SvtAv2Error error;
    uint64_t best_cost=best->cost, best_d=best->distortion, best_r=best->rate, best_hash=best->hash;
    uint64_t best_id=best->id;
    if (work->config->angle_search) {
        error = prepare_prefix(work, 0u);
        if (error != SVT_AV2_ERROR_NONE) { region_copy(work, 1); return error; }
        work->prefix_active = 1u;
        /* Retain the selected tree and all old candidates. Screen all 48 new
         * angles by causal Y SAD, then exact-test the best four plus up to six
         * offsets around the selected directional base. UV inherits a V/H
         * angle where required; only the final R+D trials compute UV. */
        for (uint32_t leaf = 0u; leaf < work->leaf_count; ++leaf) {
            if (!legal_mode(work, leaf, 14u)) continue;
            error = extend_prefix(work, leaf);
            if (error != SVT_AV2_ERROR_NONE) {
                work->prefix_active = 0u; region_copy(work, 1); return error;
            }
            uint64_t seed_d; uint32_t seed_path; int seed_valid;
            error = build_path(work, 0u, 0, partial, work->best_ids, &seed_valid, &seed_d, &seed_path);
            if (error != SVT_AV2_ERROR_NONE || !seed_valid) {
                work->prefix_active = 0u; region_copy(work, 1); return SVT_AV2_ERROR_INVALID_STATE;
            }
            SvtAv2BlockView blocks[3];
            error = plane_blocks(work, leaf, blocks);
            if (error != SVT_AV2_ERROR_NONE) { work->prefix_active = 0u; region_copy(work, 1); return error; }
            uint8_t ranked[4] = {255u,255u,255u,255u};
            uint64_t scores[4] = {UINT64_MAX,UINT64_MAX,UINT64_MAX,UINT64_MAX};
#if M6_SATD_ANGLES
            uint8_t satd_ranked[4]={255u,255u,255u,255u};
            uint64_t satd_scores[4]={UINT64_MAX,UINT64_MAX,UINT64_MAX,UINT64_MAX};
#endif
            for (uint32_t mode = 14u; mode <= 61u; ++mode) {
                uint64_t sad[3] = {0u,0u,0u};
                error = predict(work, leaf, mode, blocks, sad, 1);
                if (error != SVT_AV2_ERROR_NONE) { work->prefix_active = 0u; region_copy(work, 1); return error; }
#if M6_SATD_ANGLES
                angle_satd_rank(work,&blocks[0],mode,satd_ranked,satd_scores);
#endif
                for (uint32_t slot = 0u; slot < 4u; ++slot) {
                    if (sad[0] < scores[slot] || (sad[0] == scores[slot] && mode < ranked[slot])) {
                        for (uint32_t j = 3u; j > slot; --j) { scores[j] = scores[j - 1u]; ranked[j] = ranked[j - 1u]; }
                        scores[slot] = sad[0]; ranked[slot] = (uint8_t)mode; break;
                    }
                }
            }
            uint8_t candidates[14]; uint32_t count = 4u;
            memcpy(candidates, ranked, sizeof(ranked));
            const uint32_t base = svt_av2_intra_mode_base(work->best_ids[leaf]);
            const uint32_t joint = svt_av2_intra_mode_joint(base);
            if (joint >= 5u) {
                for (uint32_t slot = 0u; slot < 6u; ++slot) {
                    const uint8_t mode = (uint8_t)(14u + ((joint - 5u) / 7u) * 6u + slot);
                    uint32_t i = 0u;
                    while (i < count && candidates[i] != mode) ++i;
                    if (i == count) candidates[count++] = mode;
                }
            }
#if M6_SATD_ANGLES
            angle_satd_append(work,candidates,&count,satd_ranked);
#endif
            uint32_t choice = work->best_ids[leaf];
            for (uint32_t index = 0u; index < count; ++index) {
                const uint32_t mode = candidates[work->config->reverse_candidates ? count - 1u - index : index];
                uint8_t ids[RD_MAX_LEAVES];
                memcpy(ids, work->best_ids, work->leaf_count); ids[leaf] = (uint8_t)mode;
                region_copy_buffer(work, work->prefix_recon, 1);
                uint64_t d = 0u, r = 0u, cost = 0u;
                uint32_t path; int valid;
                error = build_path(work, 0u, 0, partial, ids, &valid, &d, &path);
                if (error == SVT_AV2_ERROR_NONE && (!valid ||
                    svt_av2_tile_rate_trial(work->rate, sb, best_decision, work->blocks,
                        work->leaf_count * 3u, NULL, 0u, 0, &r) != 0)) error = SVT_AV2_ERROR_INVALID_STATE;
                if (error == SVT_AV2_ERROR_NONE) error = svt_av2_rd_cost(d, work->stats.lambda_q8, r, &cost);
                if (error != SVT_AV2_ERROR_NONE) { work->prefix_active = 0u; region_copy(work, 1); return error; }
                ++work->stats.candidate_count; ++work->stats.angle_trials;
                const uint32_t payload = (1u << 26u) | (leaf << 6u) | mode;
                const uint64_t id = plan_candidate_id(best_decision, payload, payload);
                if (work->config->candidate_hook != NULL)
                    work->config->candidate_hook(work->config->candidate_hook_user, sb, id, d, r, cost, 0);
                if (cost < best_cost || (cost == best_cost && mode < choice)) {
                    best_cost = cost; best_d = d; best_r = r; best_hash = path_hash(work); best_id = id;
                    memcpy(work->best_ids, ids, work->leaf_count); choice = mode;
                    ++work->stats.angle_accepts;
                }
            }
        }
        work->prefix_active = 0u; region_copy(work, 1);
    }
    if (work->config->frame_header.enable_mrls) {
        /* Refine the selected direction before TX/RDOQ. Every candidate
         * restores and rebuilds its complete causal SB, including UV CfL. */
        static const uint8_t candidates[6] = {1u,2u,3u,5u,6u,7u};
        error=prepare_prefix(work,0u);
        if (error!=SVT_AV2_ERROR_NONE) {region_copy(work,1);return error;}
        work->prefix_active=1u;
        for (uint32_t leaf=0u;leaf<work->leaf_count;++leaf) {
            const uint32_t w=work->leaves[leaf].w,h=work->leaves[leaf].h;
            if ((w!=h && w!=2u*h && h!=2u*w &&
                !((w==16u && h==64u) || (w==64u && h==16u))) || w<8u || h<8u ||
                svt_av2_intra_mode_joint(work->best_ids[leaf])<5u) continue;
            error=extend_prefix(work,leaf);
            if (error!=SVT_AV2_ERROR_NONE) {work->prefix_active=0u;region_copy(work,1);return error;}
            uint32_t choice=0u;
            for (uint32_t index=0u;index<6u;++index) {
                const uint32_t selection=candidates[work->config->reverse_candidates?5u-index:index];
                work->mrl[leaf]=(uint8_t)selection;
                region_copy_buffer(work,work->prefix_recon,1);
                uint64_t d=0u,r=0u,cost=0u; uint32_t path; int valid;
                error=build_path(work,0u,0,partial,work->best_ids,&valid,&d,&path);
                if (error==SVT_AV2_ERROR_NONE && (!valid ||
                    svt_av2_tile_rate_trial(work->rate,sb,best_decision,work->blocks,
                        work->leaf_count*3u,NULL,0u,0,&r)!=0)) error=SVT_AV2_ERROR_INVALID_STATE;
                if (error==SVT_AV2_ERROR_NONE) error=svt_av2_rd_cost(d,work->stats.lambda_q8,r,&cost);
                if (error!=SVT_AV2_ERROR_NONE) {work->prefix_active=0u;region_copy(work,1);return error;}
                ++work->stats.candidate_count; ++work->stats.mrl_trials;
                const uint32_t payload=(1u<<24u)|(leaf<<3u)|selection;
                const uint64_t id=plan_candidate_id(best_decision,payload,payload);
                if (work->config->candidate_hook!=NULL)
                    work->config->candidate_hook(work->config->candidate_hook_user,sb,id,d,r,cost,0);
                if (cost<best_cost || (cost==best_cost && selection<choice)) {
                    best_cost=cost;best_d=d;best_r=r;best_hash=path_hash(work);best_id=id;choice=selection;
                    ++work->stats.mrl_accepts;
                }
            }
            work->mrl[leaf]=(uint8_t)choice;
        }
        work->prefix_active=0u;region_copy(work,1);
    }
    if (work->config->transform_selection) {
        error = prepare_prefix(work, 0u);
        if (error != SVT_AV2_ERROR_NONE) { work->prefix_active = 0u; region_copy(work, 1); return error; }
        work->prefix_active = 1u;
        /* Keep the entering DCT candidate; each primary candidate rebuilds the
         * full SB from the same reconstruction and committed CDF prefix. */
        for (uint32_t leaf = 0u; leaf < work->leaf_count; ++leaf) {
            const uint32_t w = work->leaves[leaf].w, h = work->leaves[leaf].h;
            if (!((w == h && w <= 16u) || (work->config->transform_selection == 2u &&
                ((w == 8u && h == 16u) || (w == 16u && h == 8u))))) continue;
            error = extend_prefix(work, leaf);
            if (error != SVT_AV2_ERROR_NONE) {
                work->prefix_active = 0u; region_copy(work, 1); return error;
            }
            uint32_t choice = work->tx_types[leaf];
            const uint32_t count = work->config->transform_selection == 2u ? 3u : 1u;
            for (uint32_t index = 0u; index < count; ++index) {
                const uint32_t type = count == 1u ? 3u :
                    work->config->reverse_candidates ? 3u - index : 1u + index;
                /* av2_md_idx2type[size_class=0][Paeth] excludes ADST_DCT. */
                if (type == 1u && work->leaves[leaf].w == 4u && work->best_ids[leaf] == 7u) continue;
                work->tx_types[leaf] = (uint8_t)type;
                region_copy_buffer(work, work->prefix_recon, 1);
                uint64_t d = 0u, r = 0u, cost = 0u;
                uint32_t path; int valid;
                error = build_path(work, 0u, 0, partial, work->best_ids, &valid, &d, &path);
                if (error == SVT_AV2_ERROR_NONE && (!valid ||
                    svt_av2_tile_rate_trial(work->rate, sb, best_decision, work->blocks,
                        work->leaf_count * 3u, NULL, 0u, 0, &r) != 0)) error = SVT_AV2_ERROR_INVALID_STATE;
                if (error == SVT_AV2_ERROR_NONE) error = svt_av2_rd_cost(d, work->stats.lambda_q8, r, &cost);
                if (error != SVT_AV2_ERROR_NONE) { work->prefix_active = 0u; region_copy(work, 1); return error; }
                ++work->stats.candidate_count; ++work->stats.transform_trials;
                const uint32_t payload = (1u << 27u) | (leaf << 2u) | type;
                const uint64_t id = plan_candidate_id(best_decision,payload,payload);
                if (work->config->candidate_hook != NULL)
                    work->config->candidate_hook(work->config->candidate_hook_user, sb, id, d, r, cost, 0);
                if (cost < best_cost || (count > 1u && cost == best_cost && type < choice)) {
                    best_cost = cost; best_d = d; best_r = r; best_hash = path_hash(work); best_id = id;
                    choice = type; ++work->stats.transform_accepts;
                }
                work->tx_types[leaf] = (uint8_t)choice;
            }
        }
        work->prefix_active = 0u; region_copy(work, 1);
    }
    if (work->joint_y_tx) {
        RdBasicChoice joint={best_cost,best_d,best_r,best_hash,best_id};
        error=refine_joint_y_tx(work,sb,partial,best_decision,&joint);
        if (error!=SVT_AV2_ERROR_NONE) return error;
        best_cost=joint.cost;best_d=joint.distortion;best_r=joint.rate;best_hash=joint.hash;best_id=joint.id;
    }
    if (work->config->cfl_search) {
        error=prepare_prefix(work,0u);
        if (error!=SVT_AV2_ERROR_NONE) {region_copy(work,1);return error;}
        work->prefix_active=1u;
        for (uint32_t leaf=0u;leaf<work->leaf_count;++leaf) {
            error=extend_prefix(work,leaf);
            if (error!=SVT_AV2_ERROR_NONE) {work->prefix_active=0u;region_copy(work,1);return error;}
            uint64_t seed_d;uint32_t seed_path;int valid;
            error=build_path(work,0u,0,partial,work->best_ids,&valid,&seed_d,&seed_path);
            if (error!=SVT_AV2_ERROR_NONE || !valid) {work->prefix_active=0u;region_copy(work,1);return SVT_AV2_ERROR_INVALID_STATE;}
            SvtAv2BlockView blocks[3];
            error=plane_blocks(work,leaf,blocks);
            int8_t alphas[2][2]={{0,0},{0,0}};
            if (error==SVT_AV2_ERROR_NONE) error=rank_cfl_alphas(work,blocks+1u,1u,alphas[0]);
            if (error==SVT_AV2_ERROR_NONE) error=rank_cfl_alphas(work,blocks+2u,2u,alphas[1]);
            if (error!=SVT_AV2_ERROR_NONE) {work->prefix_active=0u;region_copy(work,1);return error;}
            uint8_t chosen_uv=work->uv_ids[leaf], chosen_explicit=work->cfl_explicit[leaf];
            uint8_t chosen_idx=work->cfl_alpha_idx[leaf], chosen_signs=work->cfl_alpha_signs[leaf];
            for (uint32_t index=0u;index<4u;++index) {
                const uint32_t pair=work->config->reverse_candidates?3u-index:index;
                const int32_t u=alphas[0][pair/2u],v=alphas[1][pair%2u];
                if (!u && !v) continue;
                const uint32_t usign=u<0?1u:u>0?2u:0u, vsign=v<0?1u:v>0?2u:0u;
                const uint32_t signs=usign*3u+vsign-1u;
                const uint32_t ui=u?(uint32_t)(u<0?-u:u)-1u:0u, vi=v?(uint32_t)(v<0?-v:v)-1u:0u;
                const uint32_t idx=(ui<<3u)|vi, code=(signs<<6u)|idx;
                work->uv_ids[leaf]=14u;work->cfl_explicit[leaf]=1u;
                work->cfl_alpha_idx[leaf]=(uint8_t)idx;work->cfl_alpha_signs[leaf]=(uint8_t)signs;
                region_copy_buffer(work,work->prefix_recon,1);
                uint64_t d=0u,r=0u,cost=0u;uint32_t path;
                error=build_path(work,0u,0,partial,work->best_ids,&valid,&d,&path);
                if (error==SVT_AV2_ERROR_NONE && (!valid || svt_av2_tile_rate_trial(work->rate,sb,best_decision,
                    work->blocks,work->leaf_count*3u,NULL,0u,0,&r)!=0)) error=SVT_AV2_ERROR_INVALID_STATE;
                if (error==SVT_AV2_ERROR_NONE) error=svt_av2_rd_cost(d,work->stats.lambda_q8,r,&cost);
                if (error!=SVT_AV2_ERROR_NONE) {work->prefix_active=0u;region_copy(work,1);return error;}
                ++work->stats.candidate_count;++work->stats.cfl_trials;
                const uint32_t payload=(1u<<25u)|(leaf<<9u)|code;
                const uint64_t id=plan_candidate_id(best_decision,payload|((uint32_t)best_decision<<16u),payload);
                if (work->config->candidate_hook!=NULL)
                    work->config->candidate_hook(work->config->candidate_hook_user,sb,id,d,r,cost,0);
                if (cost<best_cost || (cost==best_cost && chosen_explicit && code<((uint32_t)chosen_signs<<6u|chosen_idx))) {
                    best_cost=cost;best_d=d;best_r=r;best_hash=path_hash(work);best_id=id;
                    chosen_uv=14u;chosen_explicit=1u;chosen_idx=(uint8_t)idx;chosen_signs=(uint8_t)signs;
                    ++work->stats.cfl_accepts;
                }
                work->uv_ids[leaf]=chosen_uv;work->cfl_explicit[leaf]=chosen_explicit;
                work->cfl_alpha_idx[leaf]=chosen_idx;work->cfl_alpha_signs[leaf]=chosen_signs;
            }
        }
        work->prefix_active=0u;region_copy(work,1);
    }
    if (work->config->mhccp_search && !work->skip_mhccp_refinement) {
        error=prepare_prefix(work,0u);
        if (error!=SVT_AV2_ERROR_NONE) {region_copy(work,1);return error;}
        work->prefix_active=1u;
        for (uint32_t leaf=0u;leaf<work->leaf_count;++leaf) {
            if (!svt_av2_intra_mhccp_allowed(work->leaves[leaf].w,work->leaves[leaf].h,
                (uint32_t)work->source->chroma_format)) continue;
            error=extend_prefix(work,leaf);
            if (error!=SVT_AV2_ERROR_NONE) {work->prefix_active=0u;region_copy(work,1);return error;}
            const uint8_t old_uv=work->uv_ids[leaf],old_explicit=work->cfl_explicit[leaf];
            uint32_t choice=0u;
            for (uint32_t index=0u;index<3u;++index) {
                const uint32_t selection=work->config->reverse_candidates?3u-index:1u+index;
                work->uv_ids[leaf]=14u;work->cfl_explicit[leaf]=0u;work->mhccp[leaf]=(uint8_t)selection;
                region_copy_buffer(work,work->prefix_recon,1);
                uint64_t d=0u,r=0u,cost=0u;uint32_t path;int valid;
                error=build_path(work,0u,0,partial,work->best_ids,&valid,&d,&path);
                if (error==SVT_AV2_ERROR_NONE && (!valid || svt_av2_tile_rate_trial(work->rate,sb,best_decision,
                    work->blocks,work->leaf_count*3u,NULL,0u,0,&r)!=0)) error=SVT_AV2_ERROR_INVALID_STATE;
                if (error==SVT_AV2_ERROR_NONE) error=svt_av2_rd_cost(d,work->stats.lambda_q8,r,&cost);
                if (error!=SVT_AV2_ERROR_NONE) {work->prefix_active=0u;region_copy(work,1);return error;}
                ++work->stats.candidate_count;++work->stats.mhccp_trials;
                const uint32_t payload=(UINT32_C(1)<<30u)|(leaf<<2u)|selection;
                const uint64_t id=plan_candidate_id(best_decision,payload|((uint32_t)best_decision<<16u),payload);
                if (work->config->candidate_hook!=NULL)
                    work->config->candidate_hook(work->config->candidate_hook_user,sb,id,d,r,cost,0);
                if (cost<best_cost || (cost==best_cost && choice && selection<choice)) {
                    best_cost=cost;best_d=d;best_r=r;best_hash=path_hash(work);best_id=id;choice=selection;
                    ++work->stats.mhccp_accepts;
                }
                work->mhccp[leaf]=(uint8_t)choice;
                work->uv_ids[leaf]=choice?14u:old_uv;
                work->cfl_explicit[leaf]=choice?0u:old_explicit;
            }
        }
        work->prefix_active=0u;region_copy(work,1);
    }
    if (!partial && work->config->coeff_optimization == 1u) {
        const uint64_t mode_id = best_id;
        uint32_t best_policy = 0u;
        /* One raster coordinate pass over the original winning tree/modes.
         * Every trial rebuilds all leaves from the identical SB entry state. */
        for (uint32_t leaf = 0u; leaf < work->leaf_count; ++leaf) {
            for (uint32_t plane = 0u; plane < 3u; ++plane) {
                work->coeff_policy = best_policy | (1u << (3u * leaf + plane));
                region_copy(work, 1);
                uint64_t trial_d = 0u, trial_r = 0u, trial_cost = 0u;
                uint32_t trial_path; int trial_legal;
                error = build_path(work, 0u, 0, 0, work->best_ids, &trial_legal, &trial_d, &trial_path);
                if (error == SVT_AV2_ERROR_NONE && (!trial_legal ||
                    svt_av2_tile_rate_trial(work->rate, sb, best_decision, work->blocks,
                        work->leaf_count * 3u, work->reference == NULL ? NULL : work->modes,
                        work->reference == NULL ? 0u : work->leaf_count, 0, &trial_r) != 0))
                    error = SVT_AV2_ERROR_INVALID_STATE;
                if (error == SVT_AV2_ERROR_NONE)
                    error = svt_av2_rd_cost(trial_d, work->stats.lambda_q8, trial_r, &trial_cost);
                if (error != SVT_AV2_ERROR_NONE) { region_copy(work, 1); return error; }
                ++work->stats.candidate_count; ++work->stats.coeff_trials;
                const uint64_t trial_id = mode_id | (work->coeff_policy << 9u);
                if (work->config->candidate_hook != NULL)
                    work->config->candidate_hook(work->config->candidate_hook_user, sb, trial_id,
                        trial_d, trial_r, trial_cost, 0);
                if (trial_cost < best_cost || (trial_cost == best_cost && trial_id < best_id)) {
                    best_cost = trial_cost; best_d = trial_d; best_r = trial_r; best_id = trial_id;
                    best_policy = work->coeff_policy; best_hash = path_hash(work);
                    ++work->stats.coeff_accepts;
                }
            }
        }
        work->coeff_policy = best_policy;
        region_copy(work, 1);
    }
    if (work->config->coeff_optimization == 2u) {
        error = prepare_prefix(work, 0u);
        if (error != SVT_AV2_ERROR_NONE) { region_copy(work, 1); return error; }
        work->prefix_active = 1u;
        /* Bounded strengths start from the same scalar coefficients. Every
         * proposal rebuilds the suffix and rates the complete actual SB.
         * No approximate coefficient cost can directly become a winner. */
        for (uint32_t block = 0u; block < work->leaf_count * 3u; ++block) {
            work->rdoq_target = UINT32_MAX;
            error = extend_prefix(work, block / 3u);
            if (error != SVT_AV2_ERROR_NONE) {
                work->prefix_active = 0u; region_copy(work, 1); return error;
            }
            int32_t chosen_coeff[1024];
            size_t chosen_offset=0u,chosen_count=0u;
            uint32_t chosen_phase=3u;uint8_t chosen_type=0u;
            for (uint32_t index=0u;index<3u;++index) {
                const uint32_t phase=work->config->reverse_candidates?2u-index:index;
                work->rdoq_phase=phase;
                work->rdoq_target = block;
                region_copy_buffer(work, work->prefix_recon, 1);
                uint64_t d = 0u, r = 0u, cost = 0u; uint32_t path; int valid;
                error = build_path(work, 0u, 0, partial, work->best_ids, &valid, &d, &path);
                if (error == SVT_AV2_ERROR_NONE && (!valid ||
                    svt_av2_tile_rate_trial(work->rate, sb, best_decision, work->blocks,
                        work->leaf_count * 3u, NULL, 0u, 0, &r) != 0)) error = SVT_AV2_ERROR_INVALID_STATE;
                if (error == SVT_AV2_ERROR_NONE) error = svt_av2_rd_cost(d, work->stats.lambda_q8, r, &cost);
                if (error != SVT_AV2_ERROR_NONE) { work->prefix_active = 0u; region_copy(work, 1); return error; }
                const uint32_t payload = (1u << (phase ? 29u : 26u)) | (phase << 8u) | block |
                    (work->rdoq_use_context?(1u<<27u):0u);
                const uint64_t id = plan_candidate_id(best_decision,payload,payload);
                ++work->stats.candidate_count; ++work->stats.coeff_trials;
                if (work->config->candidate_hook != NULL)
                    work->config->candidate_hook(work->config->candidate_hook_user, sb, id, d, r, cost, 0);
                if (cost < best_cost || (cost == best_cost && chosen_phase < 3u && phase < chosen_phase)) {
                    best_cost = cost; best_d = d; best_r = r; best_id = id;
                    best_hash = path_hash(work); work->rdoq_saved[block] = 1u;
                    const SvtAv2TileCoeffBlock* entry=work->blocks+block;
                    chosen_offset=(size_t)(entry->qcoeff-work->coeffs);
                    chosen_count=(size_t)SVT_AV2_MIN(entry->width,32u)*SVT_AV2_MIN(entry->height,32u);
                    memcpy(chosen_coeff,entry->qcoeff,chosen_count*sizeof(*chosen_coeff));
                    chosen_type=work->rdoq_tx_types[block];chosen_phase=phase;
                    ++work->stats.coeff_accepts;
                }
            }
            if (chosen_phase < 3u) {
                memcpy(work->rdoq_coeff+chosen_offset,chosen_coeff,chosen_count*sizeof(*chosen_coeff));
                work->rdoq_tx_types[block]=chosen_type;
            }
        }
        work->rdoq_target = UINT32_MAX;
        work->rdoq_phase = 0u;
        work->prefix_active = 0u; region_copy(work, 1);
    }
    best->cost=best_cost; best->distortion=best_d; best->rate=best_r;
    best->hash=best_hash; best->id=best_id;
    return SVT_AV2_ERROR_NONE;
}

/* Retain the complete original routes before comparing joint Y/TX routes.
 * Every branch starts at the same basic seed with identical syntax/CDFs.
 * With joint search off, the existing MH two-route order and tie break remain. */
static SvtAv2Error refine_tools(RdWork* work, size_t sb, int partial,
    uint16_t decision, RdBasicChoice* best) {
    if (work->config->rd_mode==3u)
        return refine_tools_linear(work,sb,partial,decision,best);
    const uint32_t mh_count=work->config->mhccp_search &&
        work->config->coeff_optimization==2u?2u:1u;
    const uint32_t joint_count=work->config->intra_joint?2u:1u;
    const uint32_t count=mh_count*joint_count;
    if (count==1u) return refine_tools_once(work,sb,partial,decision,best);
    RdWork* states=(RdWork*)malloc(2u*sizeof(*states));
    if (states==NULL) return SVT_AV2_ERROR_OUT_OF_MEMORY;
    states[0]=*work;
    SvtAv2RdFrameStats aggregate=work->stats;
    RdBasicChoice winner={UINT64_MAX,0u,0u,0u,UINT64_MAX};
    RdBasicChoice groups[2]={{UINT64_MAX,0u,0u,0u,UINT64_MAX},
        {UINT64_MAX,0u,0u,0u,UINT64_MAX}};
    uint32_t winner_group=UINT32_MAX;
    SvtAv2Error error=SVT_AV2_ERROR_NONE;
    for (uint32_t i=0u;i<count;++i) {
        const uint32_t branch=work->config->reverse_candidates?count-1u-i:i;
        const uint32_t joint=branch/mh_count;
        *work=states[0];work->stats=aggregate;
        work->joint_y_tx=joint;
        work->skip_mhccp_refinement=mh_count==2u && branch%mh_count==0u;
        region_copy(work,1);
        RdBasicChoice trial=*best;
        error=refine_tools_once(work,sb,partial,decision,&trial);
        aggregate=work->stats;
        if (error!=SVT_AV2_ERROR_NONE) break;
        if (trial.cost<groups[joint].cost || (trial.cost==groups[joint].cost &&
            (trial.hash<groups[joint].hash || (trial.hash==groups[joint].hash && trial.id<groups[joint].id))))
            groups[joint]=trial;
        if (trial.cost<winner.cost || (trial.cost==winner.cost &&
            (joint<winner_group || (joint==winner_group &&
            (trial.hash<winner.hash || (trial.hash==winner.hash && trial.id<winner.id)))))) {
            winner=trial;winner_group=joint;states[1]=*work;
        }
    }
    *work=states[error==SVT_AV2_ERROR_NONE?1u:0u];
    work->stats=aggregate;work->skip_mhccp_refinement=0u;work->joint_y_tx=0u;
    if (error==SVT_AV2_ERROR_NONE) {
        *best=winner;
        if (joint_count==2u && work->config->candidate_hook!=NULL) {
            const uint32_t payload=(UINT32_C(1)<<31u)|(1u<<23u)|(3u<<20u);
            for (uint32_t group=0u;group<2u;++group)
                work->config->candidate_hook(work->config->candidate_hook_user,sb,
                    plan_candidate_id(decision,payload|group,payload|group),
                    groups[group].distortion,groups[group].rate,groups[group].cost,0);
        }
    }
    region_copy(work,1);
    free(states);
    return error;
}

typedef struct RdTreeSeed {
    RdBasicChoice choice;
    uint16_t decision;
    uint32_t leaf_count;
    uint8_t ids[RD_MAX_LEAVES], uv[RD_MAX_LEAVES];
} RdTreeSeed;

static void offer_tree(const RdWork* work, uint16_t decision, const RdBasicChoice* choice,
    RdTreeSeed seeds[4], uint32_t* count) {
    const uint32_t limit=work->config->tree_finalists>=8u?4u:2u;
    uint32_t slot=0u;
    while (slot<*count && seeds[slot].decision!=decision) ++slot;
    if (slot<*count) {
        if (choice->cost>=seeds[slot].choice.cost) return;
        for (uint32_t j=slot+1u;j<*count;++j) seeds[j-1u]=seeds[j];
        --*count;
    }
    slot=0u;
    while (slot<*count && (seeds[slot].choice.cost<choice->cost ||
        (seeds[slot].choice.cost==choice->cost && seeds[slot].decision<decision))) ++slot;
    if (slot>=limit) return;
    if (*count<limit) ++*count;
    for (uint32_t j=*count-1u;j>slot;--j) seeds[j]=seeds[j-1u];
    seeds[slot].choice=*choice;seeds[slot].decision=decision;seeds[slot].leaf_count=work->leaf_count;
    memcpy(seeds[slot].ids,work->best_ids,work->leaf_count);
    memcpy(seeds[slot].uv,work->uv_ids,work->leaf_count);
}

/* Each complete tree starts at the same SB entry. Only the final replay is
 * appended and committed; the rate session keeps its original committed CDF. */
static SvtAv2Error refine_tree_finalists_once(RdWork* work, size_t sb, int partial,
    const RdTreeSeed seeds[10], uint32_t count, uint16_t* decision, RdBasicChoice* best) {
    if (!count || count>10u) return SVT_AV2_ERROR_INVALID_PARAMETER;
    RdWork* saved=(RdWork*)malloc(sizeof(*saved));
    if (saved==NULL) return SVT_AV2_ERROR_OUT_OF_MEMORY;
    *saved=*work;
    const uint16_t incumbent=*decision;
    const uint32_t defer=!work->tree_rdoq && work->config->rd_mode==3u && count>1u && !partial &&
        work->config->coeff_optimization==2u;
    uint16_t selected=incumbent;
    RdBasicChoice winner={UINT64_MAX,0u,0u,0u,UINT64_MAX};
    SvtAv2Error error=SVT_AV2_ERROR_NONE;
    for (uint32_t i=0u; i<count; ++i) {
        const RdTreeSeed* seed=&seeds[work->config->reverse_candidates ? count-1u-i : i];
        region_copy(work,1);
        work->prefix_active=0u; work->prefix_leaf=0u; work->prefix_coeff_count=0u;
        work->coeff_policy=work->motion_policy=work->wide_stage=0u;
        memset(work->mrl,0,sizeof(work->mrl));
        memset(work->mhccp,0,sizeof(work->mhccp));
        memset(work->tx_types,0,sizeof(work->tx_types));
        memset(work->cfl_explicit,0,sizeof(work->cfl_explicit));
        memset(work->cfl_alpha_idx,0,sizeof(work->cfl_alpha_idx));
        memset(work->cfl_alpha_signs,0,sizeof(work->cfl_alpha_signs));
        memset(work->rdoq_saved,0,sizeof(work->rdoq_saved));
        work->rdoq_target=UINT32_MAX;
        error=geometry_for_sb(work,sb,seed->decision);
        if (error!=SVT_AV2_ERROR_NONE || work->leaf_count!=seed->leaf_count) {
            error=SVT_AV2_ERROR_INVALID_STATE; break;
        }
        memcpy(work->best_ids,seed->ids,work->leaf_count);
        memcpy(work->uv_ids,seed->uv,work->leaf_count);
        RdBasicChoice trial=seed->choice;
        uint64_t d=0u,r=0u; uint32_t path; int valid;
        error=build_path(work,0u,0,partial,work->best_ids,&valid,&d,&path);
        if (error!=SVT_AV2_ERROR_NONE || !valid || d!=trial.distortion || path_hash(work)!=trial.hash ||
            svt_av2_tile_rate_trial(work->rate,sb,seed->decision,work->blocks,
                work->leaf_count*3u,NULL,0u,0,&r)!=0 || r!=trial.rate) {
            error=SVT_AV2_ERROR_INVALID_STATE; break;
        }
        region_copy(work,1);
        work->defer_rdoq=defer;
        error=refine_tools(work,sb,partial,seed->decision,&trial);
#if M6_TREE_RDOQ && M6_TX_CACHE
        if (work->tree_rdoq && work->transform_cache) ++work->transform_cache->treeq_finalists;
#endif
        work->defer_rdoq=0u;
        if (error!=SVT_AV2_ERROR_NONE) break;
        error=build_path(work,0u,0,partial,work->best_ids,&valid,&d,&path);
        if (error!=SVT_AV2_ERROR_NONE || !valid || d!=trial.distortion || path_hash(work)!=trial.hash ||
            svt_av2_tile_rate_trial(work->rate,sb,seed->decision,work->blocks,
                work->leaf_count*3u,NULL,0u,0,&r)!=0 || r!=trial.rate) {
            error=SVT_AV2_ERROR_INVALID_STATE; break;
        }
        ++work->stats.tree_finalist_trials;
        if (work->config->candidate_hook!=NULL)
            work->config->candidate_hook(work->config->candidate_hook_user,sb,
                plan_candidate_id(seed->decision,(UINT32_C(1)<<31u)|seed->decision,UINT32_C(1)<<31u),
                trial.distortion,trial.rate,trial.cost,0);
        const int tie=trial.cost==winner.cost &&
            ((seed->decision==incumbent && selected!=incumbent) ||
             (seed->decision!=incumbent && selected!=incumbent && seed->decision<selected) ||
             (seed->decision==selected && trial.hash<winner.hash));
        if (trial.cost<winner.cost || tie) {
            winner=trial; selected=seed->decision; *saved=*work;
        }
    }
    if (error==SVT_AV2_ERROR_NONE) {
        const SvtAv2RdFrameStats aggregate=work->stats;
        *work=*saved; work->stats=aggregate;
        if (selected!=incumbent) ++work->stats.tree_finalist_accepts;
        *decision=selected; *best=winner;
        error=geometry_for_sb(work,sb,selected);
        if (error==SVT_AV2_ERROR_NONE && defer)
            error=refine_rdoq_fast(work,sb,partial,selected,best);
    }
    work->defer_rdoq=0u;
    region_copy(work,1);
    free(saved);
    return error;
}

#if M6_TREE_RDOQ
static SvtAv2Error treeq_verify(RdWork* work,size_t sb,int partial,
    uint16_t decision,const RdBasicChoice* expected) {
    region_copy(work,1);
    uint64_t d=0u,r=0u,j=0u;uint32_t path=0u;int valid=0;
    SvtAv2Error error=build_path(work,0u,0,partial,work->best_ids,&valid,&d,&path);
    if (error==SVT_AV2_ERROR_NONE && (!valid || d!=expected->distortion ||
        path_hash(work)!=expected->hash || svt_av2_tile_rate_trial(work->rate,sb,decision,
        work->blocks,work->leaf_count*3u,NULL,0u,0,&r)!=0 || r!=expected->rate))
        error=SVT_AV2_ERROR_INVALID_STATE;
    if (error==SVT_AV2_ERROR_NONE) error=svt_av2_rd_cost(d,work->stats.lambda_q8,r,&j);
    if (error==SVT_AV2_ERROR_NONE && j!=expected->cost) error=SVT_AV2_ERROR_INVALID_STATE;
    region_copy(work,1);return error;
}
#endif
#if M6_TREE_RDOQ && M6_TREE_SHARED
/* F is the unchanged linear tool search with final RDOQ deferred. G is the
 * entire original RDOQ wrapper. The old winner is selected by F, the new one
 * by G; both preserve the original incumbent/plan/hash tie ordering. */
static int shared_tree_better(const RdBasicChoice* trial,uint16_t plan,
    const RdBasicChoice* winner,uint16_t selected,uint16_t incumbent) {
    return trial->cost<winner->cost || (trial->cost==winner->cost &&
        ((plan==incumbent && selected!=incumbent) ||
         (plan!=incumbent && selected!=incumbent && plan<selected) ||
         (plan==selected && trial->hash<winner->hash)));
}
static SvtAv2Error refine_tree_finalists_shared(RdWork* work,size_t sb,int partial,
    const RdTreeSeed seeds[10],uint32_t count,uint16_t* decision,RdBasicChoice* best) {
    if (!count || count>10u) return SVT_AV2_ERROR_INVALID_PARAMETER;
    RdWork* states=(RdWork*)malloc(3u*sizeof(*states));
    if (!states) return SVT_AV2_ERROR_OUT_OF_MEMORY;
    states[0]=*work;
    const uint16_t incumbent=*decision;
    uint16_t old_plan=incumbent,new_plan=incumbent;
    RdBasicChoice old_scalar={UINT64_MAX,0u,0u,0u,UINT64_MAX};
    RdBasicChoice old_final=old_scalar,new_final=old_scalar;
    SvtAv2Error error=SVT_AV2_ERROR_NONE;
    for (uint32_t i=0u;i<count;++i) {
        const SvtAv2RdFrameStats aggregate=work->stats;
        *work=states[0];work->stats=aggregate;
        work->tree_rdoq=0u;region_copy(work,1);
        const RdTreeSeed* seed=&seeds[work->config->reverse_candidates?count-1u-i:i];
        work->prefix_active=0u; work->prefix_leaf=0u; work->prefix_coeff_count=0u;
        work->coeff_policy=work->motion_policy=work->wide_stage=0u;
        memset(work->mrl,0,sizeof(work->mrl));
        memset(work->mhccp,0,sizeof(work->mhccp));
        memset(work->tx_types,0,sizeof(work->tx_types));
        memset(work->cfl_explicit,0,sizeof(work->cfl_explicit));
        memset(work->cfl_alpha_idx,0,sizeof(work->cfl_alpha_idx));
        memset(work->cfl_alpha_signs,0,sizeof(work->cfl_alpha_signs));
        memset(work->rdoq_saved,0,sizeof(work->rdoq_saved));
        work->rdoq_target=UINT32_MAX;
        error=geometry_for_sb(work,sb,seed->decision);
        if (error!=SVT_AV2_ERROR_NONE || work->leaf_count!=seed->leaf_count) {
            error=SVT_AV2_ERROR_INVALID_STATE; break;
        }
        memcpy(work->best_ids,seed->ids,work->leaf_count);
        memcpy(work->uv_ids,seed->uv,work->leaf_count);
        RdBasicChoice trial=seed->choice;
        uint64_t d=0u,r=0u; uint32_t path; int valid;
        error=build_path(work,0u,0,partial,work->best_ids,&valid,&d,&path);
        if (error!=SVT_AV2_ERROR_NONE || !valid || d!=trial.distortion || path_hash(work)!=trial.hash ||
            svt_av2_tile_rate_trial(work->rate,sb,seed->decision,work->blocks,
                work->leaf_count*3u,NULL,0u,0,&r)!=0 || r!=trial.rate) {
            error=SVT_AV2_ERROR_INVALID_STATE; break;
        }
        region_copy(work,1);work->defer_rdoq=1u;
#if M6_TX_CACHE
        if (work->transform_cache) ++work->transform_cache->shared_f;
#endif
        error=refine_tools(work,sb,partial,seed->decision,&trial);
        work->defer_rdoq=0u;
        if (error!=SVT_AV2_ERROR_NONE) break;
        const RdBasicChoice scalar=trial; /* F hash/cost, never overwritten by G. */
        error=treeq_verify(work,sb,partial,seed->decision,&scalar);
        if (error!=SVT_AV2_ERROR_NONE) break;
        const int select_old=shared_tree_better(&scalar,seed->decision,
            &old_scalar,old_plan,incumbent);
#if M6_TX_CACHE
        if (work->transform_cache) {
            ++work->transform_cache->shared_g;
            ++work->transform_cache->treeq_finalists;
        }
#endif
        error=refine_rdoq_fast(work,sb,partial,seed->decision,&trial);
        if (error==SVT_AV2_ERROR_NONE) error=treeq_verify(work,sb,partial,seed->decision,&trial);
        if (error!=SVT_AV2_ERROR_NONE) break;
        ++work->stats.tree_finalist_trials;
        if (work->config->candidate_hook!=NULL)
            work->config->candidate_hook(work->config->candidate_hook_user,sb,
                plan_candidate_id(seed->decision,(UINT32_C(1)<<31u)|seed->decision,UINT32_C(1)<<31u),
                trial.distortion,trial.rate,trial.cost,0);
        if (select_old) {
            old_scalar=scalar;old_final=trial;old_plan=seed->decision;states[1]=*work;
        }
        if (shared_tree_better(&trial,seed->decision,&new_final,new_plan,incumbent)) {
            new_final=trial;new_plan=seed->decision;states[2]=*work;
        }
    }
    const SvtAv2RdFrameStats aggregate=work->stats;
    RdBasicChoice winner=*best;uint16_t winner_plan=incumbent;
    if (error==SVT_AV2_ERROR_NONE) {
        const int new_wins=new_final.cost<old_final.cost;
#if M6_TX_CACHE
        if (work->transform_cache) {
            ++work->transform_cache->treeq_trials;
            work->transform_cache->treeq_wins+=(uint64_t)new_wins;
            work->transform_cache->treeq_changes+=(uint64_t)(new_wins && new_plan!=old_plan);
        }
#endif
        const int take_new=new_wins && !M6_TREE_RETAIN_OLD;
        winner=take_new?new_final:old_final;winner_plan=take_new?new_plan:old_plan;
        *work=states[take_new?2u:1u];
    } else *work=states[0];
    work->stats=aggregate;work->tree_rdoq=0u;work->defer_rdoq=0u;
    if (error==SVT_AV2_ERROR_NONE) {
        if (winner_plan!=incumbent) ++work->stats.tree_finalist_accepts;
        const uint32_t selected_leaves=work->leaf_count;
#if M6_TX_CACHE
        if (work->transform_cache) {
            ++work->transform_cache->treeq_plan_commits;
            work->transform_cache->treeq_plan_repairs+=
                (uint64_t)(work->output->decisions[sb]!=winner_plan);
        }
#endif
        error=geometry_for_sb(work,sb,winner_plan);
        if (error==SVT_AV2_ERROR_NONE && work->leaf_count!=selected_leaves)
            error=SVT_AV2_ERROR_INVALID_STATE;
        if (error==SVT_AV2_ERROR_NONE) error=treeq_verify(work,sb,partial,winner_plan,&winner);
    }
    region_copy(work,1);free(states);
    if (error==SVT_AV2_ERROR_NONE) {*best=winner;*decision=winner_plan;}
    return error;
}
#endif

static SvtAv2Error refine_tree_finalists(RdWork* work,size_t sb,int partial,
    const RdTreeSeed seeds[10],uint32_t count,uint16_t* decision,RdBasicChoice* best) {
#if M6_TREE_RDOQ
    if (work->config->rd_mode!=3u || count<2u || partial || work->config->coeff_optimization!=2u)
        return refine_tree_finalists_once(work,sb,partial,seeds,count,decision,best);
    RdWork* states=(RdWork*)malloc(2u*sizeof(*states));
    if (!states) return SVT_AV2_ERROR_OUT_OF_MEMORY;
    states[0]=*work;const RdBasicChoice entry=*best;const uint16_t entry_decision=*decision;
    work->tree_rdoq=0u;
    RdBasicChoice winner=entry;uint16_t winner_decision=entry_decision;
#if M6_TREE_SHARED
    SvtAv2Error error=refine_tree_finalists_shared(work,sb,partial,seeds,count,&winner_decision,&winner);
    SvtAv2RdFrameStats aggregate=work->stats;
#else
    SvtAv2Error error=refine_tree_finalists_once(work,sb,partial,seeds,count,&winner_decision,&winner);
    if (error==SVT_AV2_ERROR_NONE) error=treeq_verify(work,sb,partial,winner_decision,&winner);
    SvtAv2RdFrameStats aggregate=work->stats;
    if (error==SVT_AV2_ERROR_NONE) {
        states[1]=*work;
        *work=states[0];work->stats=aggregate;work->tree_rdoq=1u;region_copy(work,1);
        RdBasicChoice trial=entry;uint16_t trial_decision=entry_decision;
        error=refine_tree_finalists_once(work,sb,partial,seeds,count,&trial_decision,&trial);
        if (error==SVT_AV2_ERROR_NONE) error=treeq_verify(work,sb,partial,trial_decision,&trial);
        aggregate=work->stats;
#if M6_TX_CACHE
        if (work->transform_cache) {
            ++work->transform_cache->treeq_trials;
            work->transform_cache->treeq_wins+=(uint64_t)(error==SVT_AV2_ERROR_NONE && trial.cost<winner.cost);
            work->transform_cache->treeq_changes+=(uint64_t)(error==SVT_AV2_ERROR_NONE && trial.cost<winner.cost && trial_decision!=winner_decision);
        }
#endif
#if M6_TREE_RETAIN_OLD
        *work=states[1];
#else
        if (error==SVT_AV2_ERROR_NONE && trial.cost<winner.cost) {
            winner=trial;winner_decision=trial_decision;
        } else *work=states[1];
#endif
    }
#endif
#if M6_TOOL_REPEAT
    if (error==SVT_AV2_ERROR_NONE) {
        states[1]=*work;
        *work=states[0];work->stats=aggregate;work->tree_rdoq=1u;work->tool_rounds=2u;
        region_copy(work,1);
        RdBasicChoice trial=entry;uint16_t trial_decision=entry_decision;
        error=refine_tree_finalists_once(work,sb,partial,seeds,count,&trial_decision,&trial);
        if (error==SVT_AV2_ERROR_NONE) error=treeq_verify(work,sb,partial,trial_decision,&trial);
        aggregate=work->stats;
#if M6_TX_CACHE
        if (work->transform_cache) {
            ++work->transform_cache->repeat_trials;
            work->transform_cache->repeat_wins+=(uint64_t)(error==SVT_AV2_ERROR_NONE && trial.cost<winner.cost);
        }
#endif
#if M6_REPEAT_RETAIN_OLD
        *work=states[1];
#else
        if (error==SVT_AV2_ERROR_NONE && trial.cost<winner.cost) {
            winner=trial;winner_decision=trial_decision;
        } else *work=states[1];
#endif
    }
#endif
    if (error!=SVT_AV2_ERROR_NONE) *work=states[0];
    work->stats=aggregate;work->tree_rdoq=0u;work->defer_rdoq=0u;work->tool_rounds=0u;
    /* RdWork snapshots borrow output->decisions. Rebind that shared plan to
     * the selected state before final replay and the eventual packet writer. */
    if (error==SVT_AV2_ERROR_NONE) {
        const uint32_t selected_leaves=work->leaf_count;
#if M6_TX_CACHE
        if (work->transform_cache) {
            ++work->transform_cache->treeq_plan_commits;
            work->transform_cache->treeq_plan_repairs+=
                (uint64_t)(work->output->decisions[sb]!=winner_decision);
        }
#endif
        error=geometry_for_sb(work,sb,winner_decision);
        if (error==SVT_AV2_ERROR_NONE && work->leaf_count!=selected_leaves)
            error=SVT_AV2_ERROR_INVALID_STATE;
    }
    region_copy(work,1);free(states);
    if (error==SVT_AV2_ERROR_NONE) {*best=winner;*decision=winner_decision;}
    return error;
#else
    return refine_tree_finalists_once(work,sb,partial,seeds,count,decision,best);
#endif
}

 #include "fine16_partition_search.inc"
 #include "fine16_pair_search.inc"

/* Keep equivalent old geometries in their established plan namespace. The
 * coordinate search retains its independent base5 code even after this map. */
static uint16_t mixed8_plan(uint32_t code) {
    static const uint16_t uniform[5]={2u,3u,85u,86u,1u};
    uint32_t digits=code, square_code=4u, power=1u, first=code%5u;
    int squares=1, same=1;
    for (uint32_t q=0u;q<4u;++q) {
        const uint32_t shape=digits%5u;digits/=5u;
        const uint32_t depth=shape==4u?1u:shape==0u?2u:3u;
        if (shape==2u || shape==3u) squares=0;
        if (shape!=first) same=0;
        square_code+=(depth-1u)*power;power*=3u;
    }
    return same?uniform[first]:(uint16_t)(squares?square_code:255u+code);
}

static SvtAv2Error encode_sb(RdWork* work, size_t sb) {
/* Transform input is position-independent: preserve exact cache across SBs.
     * A new frame owns a fresh zero-initialized cache with immutable transform state. */
    if (work->basic_memo) {memset(work->basic_memo->slots,0,sizeof(work->basic_memo->slots));work->basic_memo->count=0u;}
    work->coeff_policy = 0u; work->motion_policy = 0u; work->wide_stage = 0u;
    memset(work->mrl,0,sizeof(work->mrl));
    memset(work->mhccp,0,sizeof(work->mhccp));
    memset(work->rdoq_saved, 0, sizeof(work->rdoq_saved));
    work->rdoq_target = UINT32_MAX;
    memset(work->uv_ids, 0, sizeof(work->uv_ids));
    memset(work->tx_types, 0, sizeof(work->tx_types));
    memset(work->cfl_explicit,0,sizeof(work->cfl_explicit));
    memset(work->cfl_alpha_idx,0,sizeof(work->cfl_alpha_idx));
    memset(work->cfl_alpha_signs,0,sizeof(work->cfl_alpha_signs));
    SvtAv2Error error = svt_av2_block_from_sb(&work->geometry,
        (uint32_t)(sb % work->geometry.sb_cols), (uint32_t)(sb / work->geometry.sb_cols), &work->region[0]);
    if (error != SVT_AV2_ERROR_NONE) return error;
    /* This is a visible backup rectangle, not a coded TX shape. Partial SB
     * dimensions need direct subsampling rather than the coded-shape table. */
    work->region[1] = work->region[0];
    if (work->source->chroma_format == SVT_AV2_CHROMA_420) {
        work->region[1].x /= 2u; work->region[1].y /= 2u;
        work->region[1].w /= 2u; work->region[1].h /= 2u;
    }
    work->region[2] = work->region[1];
    const int partial = work->region[0].w != 64u || work->region[0].h != 64u;
    if (partial) ++work->stats.partial_superblocks; else ++work->stats.full_superblocks;
    region_copy(work, 0);
    uint64_t best_cost = UINT64_MAX, best_d = 0u, best_r = 0u, best_hash = 0u;
    uint64_t best_id = UINT64_MAX; uint16_t best_decision = 0u;
    RdTreeSeed finalists[10], reserved[2], before_thin[2], thin64[2];
    uint32_t finalist_count=0u, reserved_count=0u, before_thin_count=0u, thin64_count=0u;
#if M6_ROOT_ASYM
    RdTreeSeed before_asym[2];
    uint32_t before_asym_count=0u;
#endif
    const uint32_t tree_count = !partial && work->config->partition_selection ? 2u : 1u;
    for (uint32_t decision = 0u; decision < tree_count; ++decision) {
        error = geometry_for_sb(work, sb, (uint16_t)decision);
        if (error != SVT_AV2_ERROR_NONE) break;
        const int pruned = decision != 0u && work->config->rd_mode >= 2u;
        const uint32_t paths = partial ? 1u : 1u << ((pruned ? 1u : 2u) * work->leaf_count);
        for (uint32_t index = 0u; index < paths; ++index) {
            const uint32_t ordinal = work->config->reverse_candidates ? paths - 1u - index : index;
            region_copy(work, 1);
            uint64_t distortion = 0u, rate = 0u, cost; uint32_t path_id; int legal;
            error = build_path(work, ordinal, pruned, partial, NULL, &legal, &distortion, &path_id);
            if (error != SVT_AV2_ERROR_NONE) break;
            if (!legal) continue;
            if (svt_av2_tile_rate_trial(work->rate, sb, (uint16_t)decision, work->blocks,
                work->leaf_count * 3u, work->reference == NULL ? NULL : work->modes, work->reference == NULL ? 0u : work->leaf_count,
                0, &rate) != 0) { error = SVT_AV2_ERROR_INVALID_STATE; break; }
            error = svt_av2_rd_cost(distortion, work->stats.lambda_q8, rate, &cost);
            if (error != SVT_AV2_ERROR_NONE) break;
            ++work->stats.candidate_count;
            const uint64_t candidate_id = decision *
                (work->config->frame_header.m5_intra_modes >= 3u ? 65536u :
                 work->config->frame_header.m5_intra_modes == 2u ? 4096u : 256u) + path_id;
            if (work->config->candidate_hook != NULL)
                work->config->candidate_hook(work->config->candidate_hook_user, sb, candidate_id,
                    distortion, rate, cost, 0);
            if (cost < best_cost || (cost == best_cost && candidate_id < best_id)) {
                best_cost = cost; best_d = distortion; best_r = rate; best_id = candidate_id;
                best_decision = (uint16_t)decision; best_hash = path_hash(work);
                memcpy(work->best_ids, work->ids, work->leaf_count);
            }
        }
        if (error != SVT_AV2_ERROR_NONE) break;
    }
    region_copy(work, 1);
    if (error != SVT_AV2_ERROR_NONE || best_id == UINT64_MAX)
        return error != SVT_AV2_ERROR_NONE ? error : SVT_AV2_ERROR_INVALID_STATE;
    error = geometry_for_sb(work, sb, best_decision);
    if (error != SVT_AV2_ERROR_NONE) return error;
    if (!partial && work->reference != NULL && work->config->motion_optimization) {
        uint32_t best_policy = 0u;
        for (uint32_t leaf = 0u; leaf < work->leaf_count; ++leaf) {
            const uint8_t entering_mode = work->best_ids[leaf];
            const uint32_t entering_slot = (best_policy >> (2u * leaf)) & 3u;
            for (uint32_t index = 0u; index < work->mv_lists[leaf].count; ++index) {
                const uint32_t slot = work->config->reverse_candidates ? work->mv_lists[leaf].count - 1u - index : index;
                if (entering_mode == 3u && entering_slot == slot) continue;
                work->motion_policy = (best_policy & ~(3u << (2u * leaf))) | (slot << (2u * leaf));
                for (uint32_t i = 0u; i < work->leaf_count; ++i)
                    work->mvs[i] = work->mv_lists[i].mv[(work->motion_policy >> (2u * i)) & 3u];
                uint8_t trial_ids[4]; memcpy(trial_ids, work->best_ids, work->leaf_count);
                trial_ids[leaf] = 3u;
                region_copy(work, 1);
                uint64_t trial_d = 0u, trial_r = 0u, trial_cost = 0u;
                uint32_t trial_path; int trial_legal;
                error = build_path(work, 0u, 0, 0, trial_ids, &trial_legal, &trial_d, &trial_path);
                if (error == SVT_AV2_ERROR_NONE && (!trial_legal ||
                    svt_av2_tile_rate_trial(work->rate, sb, best_decision, work->blocks,
                        work->leaf_count * 3u, work->modes, work->leaf_count, 0, &trial_r) != 0))
                    error = SVT_AV2_ERROR_INVALID_STATE;
                if (error == SVT_AV2_ERROR_NONE)
                    error = svt_av2_rd_cost(trial_d, work->stats.lambda_q8, trial_r, &trial_cost);
                if (error != SVT_AV2_ERROR_NONE) { region_copy(work, 1); return error; }
                ++work->stats.candidate_count; ++work->stats.motion_trials;
                const uint64_t trial_id = (uint32_t)best_decision * 256u + trial_path + (work->motion_policy << 21u);
                if (work->config->candidate_hook != NULL)
                    work->config->candidate_hook(work->config->candidate_hook_user, sb, trial_id,
                        trial_d, trial_r, trial_cost, 0);
                if (trial_cost < best_cost || (trial_cost == best_cost && trial_id < best_id)) {
                    best_cost = trial_cost; best_d = trial_d; best_r = trial_r; best_id = trial_id;
                    best_policy = work->motion_policy; best_hash = path_hash(work);
                    memcpy(work->best_ids, trial_ids, work->leaf_count);
                    ++work->stats.motion_accepts;
                }
            }
        }
        work->motion_policy = best_policy;
        for (uint32_t i = 0u; i < work->leaf_count; ++i)
            work->mvs[i] = work->mv_lists[i].mv[(best_policy >> (2u * i)) & 3u];
        region_copy(work, 1);
    }
    if (!partial && work->reference != NULL && work->config->motion_optimization == 2u) {
        SvtAv2MotionSearchList wide[4];
        SvtAv2MotionVector incumbent[4];
        memcpy(incumbent, work->mvs, work->leaf_count * sizeof(*incumbent));
        const uint32_t incumbent_policy = work->motion_policy;
        uint32_t best_policy = 0u; int accepted = 0;
        for (uint32_t i = 0u; i < work->leaf_count; ++i) {
            error = svt_av2_motion_estimate_wide(work->source, work->reference, &work->leaves[i], &wide[i]);
            if (error != SVT_AV2_ERROR_NONE) { region_copy(work, 1); return error; }
            work->stats.motion_candidates += wide[i].candidates;
        }
        for (uint32_t leaf = 0u; leaf < work->leaf_count; ++leaf) {
            for (uint32_t index = 0u; index < wide[leaf].count; ++index) {
                const uint32_t slot = 1u + (work->config->reverse_candidates ? wide[leaf].count - 1u - index : index);
                work->motion_policy = (best_policy & ~(3u << (2u * leaf))) | (slot << (2u * leaf));
                work->wide_stage = 1u;
                for (uint32_t i = 0u; i < work->leaf_count; ++i) {
                    const uint32_t choice = (work->motion_policy >> (2u * i)) & 3u;
                    work->mvs[i] = choice == 0u ? incumbent[i] : wide[i].mv[choice - 1u];
                }
                uint8_t trial_ids[4]; memcpy(trial_ids, work->best_ids, work->leaf_count);
                trial_ids[leaf] = 3u;
                region_copy(work, 1);
                uint64_t trial_d = 0u, trial_r = 0u, trial_cost = 0u;
                uint32_t trial_path; int trial_legal;
                error = build_path(work, 0u, 0, 0, trial_ids, &trial_legal, &trial_d, &trial_path);
                if (error == SVT_AV2_ERROR_NONE && (!trial_legal ||
                    svt_av2_tile_rate_trial(work->rate, sb, best_decision, work->blocks,
                        work->leaf_count * 3u, work->modes, work->leaf_count, 0, &trial_r) != 0))
                    error = SVT_AV2_ERROR_INVALID_STATE;
                if (error == SVT_AV2_ERROR_NONE)
                    error = svt_av2_rd_cost(trial_d, work->stats.lambda_q8, trial_r, &trial_cost);
                if (error != SVT_AV2_ERROR_NONE) { region_copy(work, 1); return error; }
                ++work->stats.candidate_count; ++work->stats.wide_trials;
                const uint64_t trial_id = (1u << 29u) | ((uint32_t)best_decision * 256u + trial_path) | (work->motion_policy << 21u);
                if (work->config->candidate_hook != NULL)
                    work->config->candidate_hook(work->config->candidate_hook_user, sb, trial_id,
                        trial_d, trial_r, trial_cost, 0);
                if (trial_cost < best_cost || (trial_cost == best_cost && trial_id < best_id)) {
                    best_cost = trial_cost; best_d = trial_d; best_r = trial_r; best_id = trial_id;
                    best_policy = work->motion_policy; best_hash = path_hash(work); accepted = 1;
                    memcpy(work->best_ids, trial_ids, work->leaf_count); ++work->stats.wide_accepts;
                }
            }
        }
        work->wide_stage = accepted ? 1u : 0u;
        work->motion_policy = accepted ? best_policy : incumbent_policy;
        for (uint32_t i = 0u; i < work->leaf_count; ++i) {
            const uint32_t choice = (best_policy >> (2u * i)) & 3u;
            work->mvs[i] = choice == 0u ? incumbent[i] : wide[i].mv[choice - 1u];
        }
        region_copy(work, 1);
    }
    if (work->config->frame_header.m5_intra_modes >= 2u) {
        RdBasicChoice best = {best_cost, best_d, best_r, best_hash, best_id};
        error = refine_basic(work, sb, partial, best_decision, &best);
        if (error != SVT_AV2_ERROR_NONE) return error;
        best_cost = best.cost; best_d = best.distortion; best_r = best.rate;
        best_hash = best.hash; best_id = best.id;
        if (work->config->tree_finalists && !partial)
            offer_tree(work,best_decision,&best,finalists,&finalist_count);
    }
    if (!partial && work->config->partition_selection >= 2u) {
        uint8_t saved_ids[RD_MAX_LEAVES], saved_uv[RD_MAX_LEAVES];
        const uint16_t initial_decision = best_decision;
        const uint64_t initial_cost = best_cost;
        memcpy(saved_ids, work->best_ids, sizeof(saved_ids));
        memcpy(saved_uv, work->uv_ids, sizeof(saved_uv));
        uint32_t mixed_codes[4] = {0u, 0u, 0u, 0u};
        uint64_t mixed_costs[4] = {UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX};
        const uint32_t tree_trials = work->config->partition_selection == 10u ? 68u :
            work->config->partition_selection == 9u ? 66u :
            work->config->partition_selection == 8u ? 58u :
            work->config->partition_selection == 7u ? (M6_ROOT_ASYM ? 60u : 56u) :
            work->config->partition_selection == 6u ? 40u :
            work->config->partition_selection == 5u ? (M6_ROOT_ASYM ? 28u : 24u) :
            work->config->partition_selection == 4u ? 18u :
            work->config->partition_selection == 3u ? 16u : 3u;
        for (uint32_t index = 0u; index < tree_trials; ++index) {
#if M6_ROOT_ASYM
            if ((work->config->partition_selection==5u || work->config->partition_selection==7u) &&
                index==(work->config->partition_selection==7u?56u:24u)) {
                before_asym_count=SVT_AV2_MIN(finalist_count,2u);
                memcpy(before_asym,finalists,before_asym_count*sizeof(*before_asym));
            }
#endif
            if (index==18u && work->config->tree_finalists>=4u) {
                reserved_count=SVT_AV2_MIN(finalist_count,2u);
                memcpy(reserved,finalists,reserved_count*sizeof(*reserved));
            }
            if (index==56u && work->config->tree_finalists>=8u) {
                before_thin_count=SVT_AV2_MIN(finalist_count,2u);
                memcpy(before_thin,finalists,before_thin_count*sizeof(*before_thin));
            }
            if (index==56u) {
                /* Seed the thin-shape pass from the completed old mixed8 domain. */
                uint32_t digits=mixed_codes[2],power=1u;
                for (uint32_t q=0u;q<4u;++q) {
                    mixed_codes[3]+=(digits%5u)*power;digits/=5u;power*=7u;
                }
                mixed_costs[3]=mixed_costs[2];
            }
            uint16_t decision = (uint16_t)index;
            uint32_t family = 0u, mixed_code = 0u;
            if (M6_ROOT_ASYM && (work->config->partition_selection==5u || work->config->partition_selection==7u) &&
                index>=(work->config->partition_selection==7u?56u:24u)) {
                decision=(uint16_t)(3285u+index-(work->config->partition_selection==7u?56u:24u));
            } else if (index >= 66u) {
                decision=(uint16_t)(3283u+index-66u);
            } else if (index >= 58u) {
                static const uint32_t powers[4] = {1u,7u,49u,343u};
                family=3u;
                const uint32_t quadrant=(index-58u)/2u,shape=(index-58u)%2u+5u;
                const uint32_t power=powers[quadrant];
                mixed_code=mixed_codes[family]-mixed_codes[family]/power%7u*power+shape*power;
                decision=svt_av2_partition_mixed7_plan(mixed_code);
                if (mixed_code==mixed_codes[family]) continue;
            } else if (index >= 56u) {
                family=3u;mixed_code=(index-56u+5u)*400u;
                decision=(uint16_t)(880u+index-56u);
            } else if (index >= 40u) {
                static const uint32_t powers[4] = {1u,5u,25u,125u};
                family=2u;
                const uint32_t quadrant=(index-40u)/4u, shape=(index-40u)%4u+1u;
                const uint32_t power=powers[quadrant];
                mixed_code=mixed_codes[family]-mixed_codes[family]/power%5u*power+shape*power;
                decision=mixed8_plan(mixed_code);
                if (mixed_code==mixed_codes[family]) continue;
            } else if (index >= 24u) {
                static const uint32_t powers[4] = {1u, 3u, 9u, 27u};
                family = (index - 24u) / 8u;
                const uint32_t quadrant = (index - 24u) % 8u / 2u;
                const uint32_t shape = (index - 24u) % 2u + 1u;
                const uint32_t power = powers[quadrant];
                mixed_code = mixed_codes[family] - mixed_codes[family] / power % 3u * power + shape * power;
                decision = (uint16_t)((family ? 174u : 93u) + mixed_code);
                if (mixed_code == mixed_codes[family]) continue;
            } else if (index >= 16u) decision = (uint16_t)(85u + index - 16u);
            else if (index >= 4u) {
                const uint32_t quadrant = (index - 4u) / 3u;
                const uint32_t depth = (index - 4u) % 3u + 1u;
                uint32_t code = 4u, power = 1u, uniform = 0u;
                for (uint32_t q = 0u; q < 4u; ++q) {
                    const uint32_t d = q == quadrant ? depth :
                        (best_decision ? svt_av2_partition_plan_depth(best_decision, q) : 1u);
                    code += (d - 1u) * power; power *= 3u;
                    if (q == 0u) uniform = d;
                    else if (d != uniform) uniform = 0u;
                }
                decision = (uint16_t)(uniform ? uniform : code);
            }
            if (decision == initial_decision || decision == best_decision) {
                const uint64_t known=decision == initial_decision ? initial_cost : best_cost;
                if (decision == 1u || decision == 2u) mixed_costs[decision - 1u] = known;
                if (decision == 2u && index<40u) mixed_costs[2]=known;
                if (index>=40u && index<56u && known<mixed_costs[2]) {mixed_costs[2]=known;mixed_codes[2]=mixed_code;}
                if (index>=56u && index<66u && known<mixed_costs[3]) {mixed_costs[3]=known;mixed_codes[3]=mixed_code;}
                continue;
            }
            error = geometry_for_sb(work, sb, decision);
            if (error != SVT_AV2_ERROR_NONE) return error;
            memset(work->uv_ids, 0, sizeof(work->uv_ids));
            RdBasicChoice trial = {UINT64_MAX, 0u, 0u, 0u, UINT64_MAX};
            /* Coarse seeds retain the original candidate domain. The fine
             * tree starts from one causal DC/V/H SAD path, then runs the same
             * true-rate seven-mode Y/UV coordinate pass as each coarse tree. */
            const int greedy = decision >= 2u;
            const int pruned = decision != 0u && work->config->rd_mode >= 2u;
            const uint32_t paths = greedy ? 1u : 1u << ((pruned ? 1u : 2u) * work->leaf_count);
            for (uint32_t seed_index = 0u; seed_index < paths; ++seed_index) {
                const uint32_t ordinal = work->config->reverse_candidates ? paths - 1u - seed_index : seed_index;
                region_copy(work, 1);
                uint64_t d, r, cost; uint32_t path; int valid;
                error = build_path(work, ordinal, pruned, greedy, NULL, &valid, &d, &path);
                if (error != SVT_AV2_ERROR_NONE) return error;
                if (!valid) continue;
                if (svt_av2_tile_rate_trial(work->rate, sb, decision, work->blocks,
                    work->leaf_count * 3u, NULL, 0u, 0, &r) != 0)
                    return SVT_AV2_ERROR_INVALID_STATE;
                error = svt_av2_rd_cost(d, work->stats.lambda_q8, r, &cost);
                if (error != SVT_AV2_ERROR_NONE) return error;
                const uint64_t id = plan_candidate_id(decision, (uint32_t)decision *
                    (work->config->frame_header.m5_intra_modes >= 3u ? 65536u : 4096u) + path, path);
                ++work->stats.candidate_count;
                if (work->config->candidate_hook != NULL)
                    work->config->candidate_hook(work->config->candidate_hook_user, sb, id, d, r, cost, 0);
                if (cost < trial.cost || (cost == trial.cost && id < trial.id)) {
                    trial.cost = cost; trial.distortion = d; trial.rate = r;
                    trial.hash = path_hash(work); trial.id = id;
                    memcpy(work->best_ids, work->ids, work->leaf_count);
                }
            }
            if (trial.id == UINT64_MAX) return SVT_AV2_ERROR_INVALID_STATE;
            error = refine_basic(work, sb, 0, decision, &trial);
            if (error != SVT_AV2_ERROR_NONE) return error;
            if (decision == 1u || decision == 2u) mixed_costs[decision - 1u] = trial.cost;
            if (decision == 2u && index<40u) mixed_costs[2]=trial.cost;
            if (index >= 24u && index < 66u && trial.cost < mixed_costs[family]) {
                mixed_costs[family] = trial.cost;
                mixed_codes[family] = mixed_code;
            }
            if (work->config->tree_finalists==10u && index>=66u) {
                /* Keep both new shapes without evicting any of the old eight seeds. */
                RdTreeSeed* seed=&thin64[thin64_count++];
                seed->choice=trial;seed->decision=decision;seed->leaf_count=work->leaf_count;
                memcpy(seed->ids,work->best_ids,work->leaf_count);
                memcpy(seed->uv,work->uv_ids,work->leaf_count);
            } else if (work->config->tree_finalists)
                offer_tree(work,decision,&trial,finalists,&finalist_count);
            if (trial.cost < best_cost || (trial.cost == best_cost && decision < best_decision)) {
                best_cost = trial.cost; best_d = trial.distortion; best_r = trial.rate;
                best_hash = trial.hash; best_id = trial.id; best_decision = decision;
                memcpy(saved_ids, work->best_ids, sizeof(saved_ids));
                memcpy(saved_uv, work->uv_ids, sizeof(saved_uv));
            }
        }
        memcpy(work->best_ids, saved_ids, sizeof(saved_ids));
        memcpy(work->uv_ids, saved_uv, sizeof(saved_uv));
        error = geometry_for_sb(work, sb, best_decision);
        if (error != SVT_AV2_ERROR_NONE) return error;
        region_copy(work, 1);
    }
    /* Preserve both seeds from the original square/small-rectangle domain.
     * Newly added trees otherwise may evict them before full tool refinement. */
#if M6_ROOT_ASYM
    for (uint32_t i=0u;i<before_asym_count;++i) {
        uint32_t j=0u;
        while (j<finalist_count && (finalists[j].decision!=before_asym[i].decision ||
            finalists[j].choice.hash!=before_asym[i].choice.hash)) ++j;
        if (j==finalist_count) finalists[finalist_count++]=before_asym[i];
    }
#endif
    for (uint32_t i=0u; i<reserved_count; ++i) {
        uint32_t j=0u;
        while (j<finalist_count && (finalists[j].decision!=reserved[i].decision ||
            finalists[j].choice.hash!=reserved[i].choice.hash)) ++j;
        if (j==finalist_count) finalists[finalist_count++]=reserved[i];
    }
    for (uint32_t i=0u;i<before_thin_count;++i) {
        uint32_t j=0u;
        while (j<finalist_count && (finalists[j].decision!=before_thin[i].decision ||
            finalists[j].choice.hash!=before_thin[i].choice.hash)) ++j;
        if (j==finalist_count) finalists[finalist_count++]=before_thin[i];
    }
    for (uint32_t i=0u;i<thin64_count;++i) finalists[finalist_count++]=thin64[i];
    RdBasicChoice tool_best={best_cost,best_d,best_r,best_hash,best_id};
    if (work->config->tree_finalists && !partial && finalist_count>1u)
        error=refine_tree_finalists(work,sb,partial,finalists,finalist_count,&best_decision,&tool_best);
    else error=refine_tools(work,sb,partial,best_decision,&tool_best);
    if (error!=SVT_AV2_ERROR_NONE) { region_copy(work,1); return error; }
#if M6_FINE16
    if (!partial && work->config->rd_mode==3u && work->config->partition_selection>=2u &&
        work->config->tree_finalists) {
        RdTreeSeed fine16_seeds[4];uint32_t fine16_count=0u;
        error=fine16_prepare(work,sb,best_decision,fine16_seeds,&fine16_count);
        if (error==SVT_AV2_ERROR_NONE && fine16_count)
            error=fine16_refine(work,sb,fine16_seeds,fine16_count,&best_decision,&tool_best);
#if M6_FINE16_PAIR
        if (error==SVT_AV2_ERROR_NONE && fine16_count) {
            RdTreeSeed pair_seeds[4];uint32_t pair_count=0u;
            error=fine16_pair_prepare(work,sb,best_decision,fine16_seeds[0].decision,pair_seeds,&pair_count);
            if (error==SVT_AV2_ERROR_NONE && pair_count)
                error=fine16_pair_refine(work,sb,pair_seeds,pair_count,&best_decision,&tool_best);
        }
#endif
        if (error!=SVT_AV2_ERROR_NONE) {region_copy(work,1);return error;}
    }
#endif
    if (work->output->decisions[sb]!=best_decision) {
        region_copy(work,1);return SVT_AV2_ERROR_INVALID_STATE;
    }
    best_cost=tool_best.cost; best_d=tool_best.distortion; best_r=tool_best.rate;
    best_hash=tool_best.hash; best_id=tool_best.id;
    uint64_t distortion = 0u, rate = 0u; uint32_t path_id; int legal;
    error = build_path(work, 0u, 0, partial, work->best_ids, &legal, &distortion, &path_id);
    if (error != SVT_AV2_ERROR_NONE) { region_copy(work, 1); return error; }
    if (!legal || distortion != best_d || path_hash(work) != best_hash ||
        svt_av2_tile_rate_trial(work->rate, sb, best_decision, work->blocks, work->leaf_count * 3u,
            work->reference == NULL ? NULL : work->modes, work->reference == NULL ? 0u : work->leaf_count, 0, &rate) != 0 || rate != best_r) {
        region_copy(work, 1); return SVT_AV2_ERROR_INVALID_STATE;
    }
    ++work->stats.replay_checks;
    error = append_winner(work);
    if (error != SVT_AV2_ERROR_NONE) { region_copy(work, 1); return error; }
    if (svt_av2_tile_rate_trial(work->rate, sb, best_decision, work->blocks, work->leaf_count * 3u,
        work->reference == NULL ? NULL : work->modes, work->reference == NULL ? 0u : work->leaf_count, 1, &rate) != 0 || rate != best_r) {
        region_copy(work, 1); return SVT_AV2_ERROR_INVALID_STATE;
    }
    work->stats.distortion += best_d; work->stats.rate_q12 += best_r; work->stats.cost += best_cost;
    if (work->config->candidate_hook != NULL)
        work->config->candidate_hook(work->config->candidate_hook_user, sb, best_id,
            best_d, best_r, best_cost, 1);
    if (best_id >> 32u) work->stats.winner_hash = hash_word(work->stats.winner_hash, (uint32_t)(best_id >> 32u));
    work->stats.winner_hash = hash_word(hash_word(work->stats.winner_hash, (uint32_t)best_id), (uint32_t)best_hash);
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_rd_frame_encode(const SvtAv2FrameBuffer* source,
    SvtAv2FrameBuffer* recon, const SvtAv2ReferenceFrame* reference,
    const SvtAv2RdFrameConfig* config, SvtAv2ReconScratch* scratch,
    SvtAv2RdFrameOutput* output, SvtAv2RdFrameStats* stats) {
    if (stats != NULL) memset(stats, 0, sizeof(*stats));
    if (output != NULL) output->block_count = output->mode_count = output->coeff_count = output->decision_count = 0u;
    if (source == NULL || recon == NULL || config == NULL || scratch == NULL || output == NULL ||
        source->buffer == NULL || recon->buffer == NULL || source->buffer == recon->buffer ||
        source->visible_width != recon->visible_width || source->visible_height != recon->visible_height ||
        source->bit_depth != recon->bit_depth || source->chroma_format != recon->chroma_format ||
        config->rd_mode < 1u || config->rd_mode > 3u || config->partition_selection > 10u ||
        (config->rd_mode==3u && (reference!=NULL || config->frame_header.multi_frame ||
            config->frame_header.m5_intra_modes<3u || config->partition_selection>7u ||
            config->intra_joint || config->coeff_optimization==1u || config->motion_optimization ||
            config->tree_finalists>4u)) ||
        config->rd_lambda_model > 2u || (config->rd_lambda_model && (reference != NULL ||
            config->frame_header.multi_frame)) ||
        (config->partition_selection >= 2u && config->frame_header.m5_intra_modes < 2u) ||
        config->frame_header.enable_mrls > 1u || (config->frame_header.enable_mrls &&
            (reference != NULL || config->frame_header.multi_frame || config->frame_header.m5_intra_modes<3u)) ||
        (config->tree_finalists!=0u && config->tree_finalists!=2u && config->tree_finalists!=4u &&
         config->tree_finalists!=8u && config->tree_finalists!=10u) ||
        (config->tree_finalists && (reference!=NULL || config->frame_header.multi_frame ||
            config->partition_selection<2u || config->frame_header.m5_intra_modes<2u)) ||
        config->intra_joint > 1u || (config->intra_joint && (reference != NULL ||
            config->frame_header.multi_frame || config->frame_header.m5_intra_modes < 3u ||
            config->transform_selection != 2u)) ||
        config->mhccp_search > 1u || config->frame_header.enable_mhccp != config->mhccp_search ||
        (config->mhccp_search && (reference != NULL || config->frame_header.multi_frame ||
            config->frame_header.m5_intra_modes != 4u)) ||
        config->cfl_search > 1u || (config->cfl_search && (reference != NULL ||
            config->frame_header.multi_frame || config->frame_header.m5_intra_modes != 4u)) ||
        config->angle_search > 1u || (config->angle_search && (reference != NULL ||
            config->frame_header.multi_frame || config->frame_header.m5_intra_modes < 3u)) ||
        config->transform_selection > 2u ||
        (config->transform_selection && (reference != NULL || config->frame_header.multi_frame ||
            config->frame_header.reduced_tx_set != (config->transform_selection == 2u ? 0u : 1u) ||
            config->frame_header.m5_intra_modes < 2u ||
            config->transform.forward_type == NULL || config->transform.inverse_type == NULL)) ||
        (!config->transform_selection && config->frame_header.reduced_tx_set == 1u) ||
        config->coeff_optimization > 2u || config->motion_optimization > 2u ||
        (config->coeff_optimization == 2u && config->frame_header.m5_intra_modes < 2u) ||
        (config->coeff_optimization && config->motion_optimization) ||
        config->frame_header.m5_intra_modes > 4u ||
        (config->frame_header.m5_intra_modes >= 2u && (reference != NULL ||
            config->frame_header.multi_frame || config->coeff_optimization == 1u || config->motion_optimization)) ||
        config->transform.forward == NULL || config->transform.inverse == NULL ||
        output->blocks == NULL || output->modes == NULL || output->qcoeff == NULL || output->decisions == NULL ||
        scratch->residual == NULL || scratch->inverse_residual == NULL || scratch->coeff == NULL ||
        scratch->qcoeff == NULL || scratch->dqcoeff == NULL || scratch->stride < 64u ||
        scratch->capacity < 4096u || scratch->coeff_capacity < 1024u)
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    const SvtAv2FrameHeaderConfig* header = &config->frame_header;
    if (header->frame_width != source->visible_width || header->frame_height != source->visible_height ||
        header->bit_depth != (uint32_t)source->bit_depth || header->chroma_format != (uint32_t)source->chroma_format ||
        (header->inter_frame != 0u ? 1u : 0u) != (reference != NULL ? 1u : 0u)) return SVT_AV2_ERROR_INVALID_PARAMETER;
    const SvtAv2FrameBuffer* ref = reference == NULL ? NULL : svt_av2_reference_view(reference);
    if (reference != NULL && (ref == NULL || ref->buffer == source->buffer || ref->buffer == recon->buffer ||
        ref->visible_width != source->visible_width || ref->visible_height != source->visible_height ||
        ref->bit_depth != source->bit_depth || ref->chroma_format != source->chroma_format))
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    RdWork* work = (RdWork*)calloc(1u, sizeof(*work));
    if (work == NULL) return SVT_AV2_ERROR_OUT_OF_MEMORY;
    work->source = source; work->recon = recon; work->reference = ref;
    work->config = config; work->scratch = scratch; work->output = output;
#if M6_TX_CACHE
    /* Allocation failure safely falls back to the original forward path. */
    if (config->rd_mode==3u && svt_av2_transform_path_cacheable(&config->transform,
        header->base_q_idx,header->bit_depth))
        work->transform_cache=rd_tx_memo_create();
#endif
    work->stats.winner_hash = RD_HASH_OFFSET;
    uint32_t qstep;
    SvtAv2Error error = config->rd_lambda_model == 2u ?
        svt_av2_rd_lambda_ai150(header->base_q_idx, source->bit_depth, &work->stats.lambda_q8, &qstep) :
        config->rd_lambda_model ?
        svt_av2_rd_lambda_ai(header->base_q_idx, source->bit_depth, &work->stats.lambda_q8, &qstep) :
        svt_av2_rd_lambda(header->base_q_idx, source->bit_depth, &work->stats.lambda_q8, &qstep);
    if (error == SVT_AV2_ERROR_NONE)
        error = svt_av2_frame_geometry_init_ex(&work->geometry, source->visible_width,
            source->visible_height, 64u, source->chroma_format);
    if (error == SVT_AV2_ERROR_NONE) {
        const size_t count = (size_t)work->geometry.sb_cols * work->geometry.sb_rows;
        if (output->decision_capacity < count) error = SVT_AV2_ERROR_INVALID_PARAMETER;
        else { output->decision_count = count; memset(output->decisions, 0, count * sizeof(*output->decisions)); }
    }
    if (error == SVT_AV2_ERROR_NONE && svt_av2_tile_rate_create(header, &work->rate) != 0)
        error = SVT_AV2_ERROR_INVALID_STATE;
    if (error==SVT_AV2_ERROR_NONE && config->rd_mode==3u &&
        svt_av2_tile_rate_prefix_create(&work->basic_rate_prefix)!=0)
        error=SVT_AV2_ERROR_OUT_OF_MEMORY;
#if M6_RDOQ_CONTEXT
    if (error==SVT_AV2_ERROR_NONE && config->rd_mode==3u && config->coeff_optimization==2u &&
        svt_av2_tile_rate_prefix_create(&work->rdoq_rate_prefix)!=0)
        error=SVT_AV2_ERROR_OUT_OF_MEMORY;
#endif
    if (error==SVT_AV2_ERROR_NONE && work->basic_rate_prefix)
        work->basic_memo=(RdBasicMemo*)calloc(1u,sizeof(*work->basic_memo));
    if (error == SVT_AV2_ERROR_NONE)
        error = svt_av2_recon_reset_frame(recon, (uint16_t)(1u << (source->bit_depth - 1u)));
    if (error == SVT_AV2_ERROR_NONE)
        for (size_t sb = 0u; sb < output->decision_count; ++sb) {
            error = encode_sb(work, sb);
            if (error != SVT_AV2_ERROR_NONE) break;
        }
    if (error == SVT_AV2_ERROR_NONE && stats != NULL) *stats = work->stats;
#if M6_ROOT_ASYM
    if (error == SVT_AV2_ERROR_NONE) {
        uint32_t counts[4]={0u,0u,0u,0u};
        for (size_t sb=0u;sb<output->decision_count;++sb)
            if (output->decisions[sb]>=3285u && output->decisions[sb]<=3288u)
                ++counts[output->decisions[sb]-3285u];
        fprintf(stderr,"root_asym winners=%u,%u,%u,%u\n",counts[0],counts[1],counts[2],counts[3]);
    }
#endif
    if (error != SVT_AV2_ERROR_NONE)
        output->block_count = output->mode_count = output->coeff_count = output->decision_count = 0u;
    svt_av2_tile_rate_prefix_free(work->rdoq_rate_prefix);
    svt_av2_tile_rate_prefix_free(work->basic_rate_prefix);
    free(work->basic_memo);
#if M6_TX_CACHE
    if (work->transform_cache != NULL)
        rd_tx_memo_report(work->transform_cache);
#if M6_Y_BOUND
    if (work->transform_cache) fprintf(stderr,"ybound trials=%llu pruned=%llu verify=%u\n",
        (unsigned long long)work->transform_cache->bound_trials,
        (unsigned long long)work->transform_cache->bound_pruned,(unsigned)M6_Y_BOUND_VERIFY);
#endif
#if M6_TREE_RDOQ
    if (work->transform_cache) fprintf(stderr,"treeq trials=%llu wins=%llu changes=%llu full_finalists=%llu retained_old=%u\n",
        (unsigned long long)work->transform_cache->treeq_trials,
        (unsigned long long)work->transform_cache->treeq_wins,
        (unsigned long long)work->transform_cache->treeq_changes,
        (unsigned long long)work->transform_cache->treeq_finalists,(unsigned)M6_TREE_RETAIN_OLD);
#endif
#if M6_TOOL_REPEAT
    if (work->transform_cache) fprintf(stderr,"repeat trials=%llu wins=%llu rounds=%llu changes=%llu after_joint=%llu retained_old=%u\n",
        (unsigned long long)work->transform_cache->repeat_trials,
        (unsigned long long)work->transform_cache->repeat_wins,
        (unsigned long long)work->transform_cache->repeat_rounds,
        (unsigned long long)work->transform_cache->repeat_changes,
        (unsigned long long)work->transform_cache->repeat_after_joint,(unsigned)M6_REPEAT_RETAIN_OLD);
#endif
#if M6_TOOL_REPEAT
    if (work->transform_cache) fprintf(stderr,"repeat_pair trials=%llu illegal=%llu default=%llu\n",
        (unsigned long long)work->transform_cache->repeat_pair_trials,
        (unsigned long long)work->transform_cache->repeat_pair_illegal,
        (unsigned long long)work->transform_cache->repeat_pair_default);
#endif
#if M6_TREE_RDOQ
    if (work->transform_cache) fprintf(stderr,"treeq_plan commits=%llu repairs=%llu\n",
        (unsigned long long)work->transform_cache->treeq_plan_commits,
        (unsigned long long)work->transform_cache->treeq_plan_repairs);
#endif
#if M6_TREE_RDOQ && M6_TREE_SHARED
    if (work->transform_cache) fprintf(stderr,"shared_tree F=%llu G=%llu\n",
        (unsigned long long)work->transform_cache->shared_f,
        (unsigned long long)work->transform_cache->shared_g);
#endif
#if M6_RDOQ_CONTEXT
    if (work->transform_cache) fprintf(stderr,"rdoqcontext trials=%llu,%llu,%llu wins=%llu,%llu,%llu live_rate_calls=%llu\n",
        (unsigned long long)work->transform_cache->rdoq_route_trials[0],
        (unsigned long long)work->transform_cache->rdoq_route_trials[1],
        (unsigned long long)work->transform_cache->rdoq_route_trials[2],
        (unsigned long long)work->transform_cache->rdoq_route_wins[0],
        (unsigned long long)work->transform_cache->rdoq_route_wins[1],
        (unsigned long long)work->transform_cache->rdoq_route_wins[2],
        (unsigned long long)work->transform_cache->rdoq_live_rate_calls);
#endif
    free(work->transform_cache);
#endif
    svt_av2_tile_rate_free(work->rate);
    free(work);
    return error;
}
