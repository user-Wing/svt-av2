#include "rd_frame.h"
#include "transform_path.h"
#include "test_util.h"
#include "motion_estimation.h"
#include "inter_prediction.h"

#include <stdlib.h>
#include <string.h>

static uint64_t total_accepts;

typedef struct Audit {
    uint64_t minimum[16], seen[16], committed[16];
    uint64_t minimum_id[16], r5_id[16];
    uint8_t wide_seen[16];
    uint64_t lambda;
    uint64_t first_sb_costs[512];
    uint8_t first_sb_seen[512];
    const struct Audit* exhaustive;
    SvtAv2RdFrameOutput* output;
    const SvtAv2FrameBuffer* source;
    const SvtAv2FrameBuffer* reference;
    size_t previous_modes;
    uint32_t width, height;
} Audit;

static void observe(void* user, size_t sb, uint64_t id, uint64_t d,
    uint64_t r, uint64_t cost, int committed) {
    Audit* audit = (Audit*)user;
    SVT_TEST_CHECK(sb < 16u);
    if (sb >= 16u) return;
    /* Small fixture range: independent ordinary multiplication is safe here. */
    SVT_TEST_CHECK(r < UINT64_MAX / audit->lambda);
    SVT_TEST_CHECK_EQ_INT(cost, d + (audit->lambda * r + 524288u) / 1048576u);
    if (!committed) {
        if ((id & (1u << 29u)) && !audit->wide_seen[sb]) {
            audit->r5_id[sb] = audit->minimum_id[sb]; audit->wide_seen[sb] = 1u;
        }
        if (sb == 0u && id < 512u) {
            audit->first_sb_costs[id] = cost; audit->first_sb_seen[id] = 1u;
            if (audit->exhaustive != NULL) {
                SVT_TEST_CHECK(audit->exhaustive->first_sb_seen[id] != 0u);
                SVT_TEST_CHECK_EQ_INT(cost, audit->exhaustive->first_sb_costs[id]);
            }
        }
        if (audit->seen[sb] == 0u || cost < audit->minimum[sb] ||
            (cost == audit->minimum[sb] && id < audit->minimum_id[sb])) {
            audit->minimum[sb] = cost; audit->minimum_id[sb] = id;
        }
        ++audit->seen[sb];
    } else {
        SVT_TEST_CHECK_EQ_INT(cost, audit->minimum[sb]);
        SVT_TEST_CHECK_EQ_INT(id, audit->minimum_id[sb]);
        if (audit->reference != NULL && audit->output != NULL) {
            const size_t count = audit->output->mode_count - audit->previous_modes;
            const uint32_t cols = (audit->width + 63u) / 64u;
            const int full = audit->width - (uint32_t)(sb % cols) * 64u >= 64u &&
                audit->height - (uint32_t)(sb / cols) * 64u >= 64u;
            if (full) {
                SVT_TEST_CHECK(count <= 4u);
                for (size_t leaf = 0u; leaf < count; ++leaf) {
                    const SvtAv2TileCoeffBlock* block = &audit->output->blocks[(audit->previous_modes + leaf) * 3u];
                    SvtAv2BlockView view = {0}; view.x = block->x; view.y = block->y;
                    view.w = block->width; view.h = block->height;
                    SvtAv2MotionSearchList list;
                    SVT_TEST_CHECK_EQ_INT(svt_av2_motion_estimate_list(audit->source, audit->reference, &view, &list), SVT_AV2_ERROR_NONE);
                    uint32_t slot = (id >> (21u + 2u * (uint32_t)leaf)) & 3u;
                    if (id & (1u << 29u)) {
                        if (slot == 0u) slot = (audit->r5_id[sb] >> (21u + 2u * (uint32_t)leaf)) & 3u;
                        else {
                            SVT_TEST_CHECK_EQ_INT(svt_av2_motion_estimate_wide(audit->source, audit->reference, &view, &list), SVT_AV2_ERROR_NONE);
                            --slot;
                        }
                    }
                    const SvtAv2TileBlockMode* mode = &audit->output->modes[audit->previous_modes + leaf];
                    SVT_TEST_CHECK_EQ_INT(mode->is_inter, ((id >> (2u * (uint32_t)leaf)) & 3u) == 3u);
                    if (mode->is_inter) {
                        SVT_TEST_CHECK(slot < list.count);
                        SVT_TEST_CHECK_EQ_INT(mode->mv_x, list.mv[slot].mv_x);
                        SVT_TEST_CHECK_EQ_INT(mode->mv_y, list.mv[slot].mv_y);
                    }
                }
            }
            audit->previous_modes = audit->output->mode_count;
        }
        ++audit->committed[sb];
    }
}

static void fill(SvtAv2FrameBuffer* picture) {
    const uint32_t step = picture->bit_depth > 8u ? 2u : 1u;
    for (uint32_t p = 0u; p < 3u; ++p)
        for (uint32_t y = 0u; y < svt_av2_picture_plane_height(picture, p); ++y)
            for (uint32_t x = 0u; x < svt_av2_picture_plane_width(picture, p); ++x) {
                const uint32_t value = ((x * 11u + y * 7u + (x * y % 31u) + p * 27u) & 255u)
                    << (picture->bit_depth - 8u);
                svt_av2_sample_store(picture->planes[p] + (size_t)y * picture->strides[p] + x * step,
                    picture->bit_depth, (uint16_t)value);
            }
}

static uint64_t picture_hash(const SvtAv2FrameBuffer* picture) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (uint32_t p = 0u; p < 3u; ++p)
        for (size_t i = 0u; i < (size_t)picture->strides[p] * picture->aligned_height[p]; ++i)
            hash = (hash ^ picture->planes[p][i]) * UINT64_C(1099511628211);
    return hash;
}

static uint64_t independent_distortion(const SvtAv2FrameBuffer* source,
    const SvtAv2FrameBuffer* recon) {
    uint64_t sums[16] = {0u};
    const uint32_t cols = (source->visible_width + 63u) / 64u;
    const uint32_t step = source->bit_depth > 8u ? 2u : 1u;
    for (uint32_t p = 0u; p < 3u; ++p) {
        const uint32_t scale = p != 0u && source->chroma_format == SVT_AV2_CHROMA_420 ? 2u : 1u;
        const uint64_t weight = p == 0u || scale == 2u ? 4u : 1u;
        for (uint32_t y = 0u; y < svt_av2_picture_plane_height(source, p); ++y)
            for (uint32_t x = 0u; x < svt_av2_picture_plane_width(source, p); ++x) {
                const int a = svt_av2_sample_load(source->planes[p] + (size_t)y * source->strides[p] + x * step, source->bit_depth);
                const int b = svt_av2_sample_load(recon->planes[p] + (size_t)y * recon->strides[p] + x * step, recon->bit_depth);
                const size_t sb = (size_t)(y * scale / 64u) * cols + x * scale / 64u;
                sums[sb] += weight * (uint64_t)((a - b) * (a - b));
            }
    }
    uint64_t result = 0u;
    for (size_t i = 0u; i < 16u; ++i)
        result += source->bit_depth == 10u ? (sums[i] + 8u) / 16u : sums[i];
    return result;
}

typedef struct FaultPath { SvtAv2IntraTransformPath real; uint32_t calls, fail_at; } FaultPath;
static SvtAv2Error fault_forward(void* user, uint32_t p, uint32_t w, uint32_t h,
    const int16_t* residual, uint32_t stride, int32_t* coeff, int32_t* qcoeff,
    int32_t* dqcoeff, int* zero) {
    FaultPath* fault = (FaultPath*)user;
    const SvtAv2Error error = fault->real.forward(fault->real.user, p, w, h,
        residual, stride, coeff, qcoeff, dqcoeff, zero);
    return ++fault->calls == fault->fail_at ? SVT_AV2_ERROR_INVALID_STATE : error;
}
static SvtAv2Error fault_inverse(void* user, uint32_t p, uint32_t w, uint32_t h,
    const int32_t* dqcoeff, uint32_t stride, int16_t* residual) {
    FaultPath* fault = (FaultPath*)user;
    return fault->real.inverse(fault->real.user, p, w, h, dqcoeff, stride, residual);
}

static void frame_test(uint32_t width, uint32_t height, uint32_t bd,
    SvtAv2ChromaFormat format, int inter) {
    SvtAv2FrameBuffer source = {0}, recon = {0};
    SvtAv2ReferenceFrame reference = {0}; SvtAv2ReconScratch scratch = {0};
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&source, width, height, bd, format), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&recon, width, height, bd, format), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(&scratch, 64u, 64u), SVT_AV2_ERROR_NONE);
    fill(&source);
    if (inter) {
        const uint32_t step = bd > 8u ? 2u : 1u;
        SVT_TEST_CHECK_EQ_INT(svt_av2_reference_init(&reference, width, height, bd, format), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_reference_prepare(&reference, &source, 0u), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_reference_commit(&reference), SVT_AV2_ERROR_NONE);
        uint16_t* prediction = (uint16_t*)malloc((size_t)width * height * sizeof(uint16_t));
        const SvtAv2MotionVector mv = {-16, 16};
        for (uint32_t p = 0u; p < 3u; ++p) {
            SvtAv2BlockView view = {0};
            view.w = svt_av2_picture_plane_width(&source, p); view.h = svt_av2_picture_plane_height(&source, p);
            SVT_TEST_CHECK_EQ_INT(svt_av2_inter_predict(&reference.committed, p, &view, mv, prediction, view.w), SVT_AV2_ERROR_NONE);
            for (uint32_t y = 0u; y < view.h; ++y)
                for (uint32_t x = 0u; x < view.w; ++x)
                    svt_av2_sample_store(source.planes[p] + (size_t)y * source.strides[p] + x * step,
                        bd, prediction[(size_t)y * view.w + x]);
        }
        free(prediction);
    }
    const uint64_t source_hash = picture_hash(&source);
    const uint64_t reference_hash = inter ? picture_hash(&reference.committed) : 0u;
    const size_t count = (size_t)width * height;
    SvtAv2RdFrameOutput output = {0};
    output.block_capacity = count / 64u * 3u; output.mode_capacity = count / 64u;
    output.coeff_capacity = count * 3u; output.decision_capacity = ((width + 63u) / 64u) * ((height + 63u) / 64u);
    output.blocks = (SvtAv2TileCoeffBlock*)calloc(output.block_capacity, sizeof(*output.blocks));
    output.modes = (SvtAv2TileBlockMode*)calloc(output.mode_capacity, sizeof(*output.modes));
    output.qcoeff = (int32_t*)calloc(output.coeff_capacity, sizeof(*output.qcoeff));
    output.decisions = (uint16_t*)calloc(output.decision_capacity, sizeof(*output.decisions));
    SVT_TEST_CHECK(output.blocks != NULL && output.modes != NULL && output.qcoeff != NULL && output.decisions != NULL);
    SvtAv2RdFrameConfig config = {0};
    svt_av2_frame_header_config_default(&config.frame_header, width, height);
    config.frame_header.bit_depth = bd; config.frame_header.chroma_format = (uint32_t)format;
    config.frame_header.base_q_idx = 100u + (bd == 10u ? 48u : 0u);
    config.frame_header.reduced_tx_set = 2u; config.frame_header.multi_frame = 1u;
    config.frame_header.m5_intra_modes = 1u; config.frame_header.inter_frame = inter != 0;
    config.rd_mode = 1u; config.partition_selection = 1u;
    SvtAv2TransformPathState transform;
    SVT_TEST_CHECK_EQ_INT(svt_av2_transform_path_init_bd(&transform, &config.transform,
        config.frame_header.base_q_idx, bd), SVT_AV2_ERROR_NONE);
    Audit audit = {0}; uint32_t qstep;
    SVT_TEST_CHECK_EQ_INT(svt_av2_rd_lambda(config.frame_header.base_q_idx, bd, &audit.lambda, &qstep), SVT_AV2_ERROR_NONE);
    config.candidate_hook = observe; config.candidate_hook_user = &audit;
    SvtAv2RdFrameStats baseline = {0}, first = {0}, reversed = {0}, pruned = {0};
    config.candidate_hook = NULL; config.motion_optimization = 1u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_rd_frame_encode(&source, &recon, inter ? &reference : NULL,
        &config, &scratch, &output, &baseline), SVT_AV2_ERROR_NONE);
    const size_t baseline_modes = output.mode_count;
    config.motion_optimization = 2u;
    config.coeff_optimization = 1u;
    SvtAv2RdFrameStats rejected = {0};
    SVT_TEST_CHECK_EQ_INT(svt_av2_rd_frame_encode(&source, &recon, inter ? &reference : NULL,
        &config, &scratch, &output, &rejected), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(output.block_count, 0u);
    config.coeff_optimization = 0u;
    config.candidate_hook = observe;
    audit.output = &output; audit.source = &source; audit.reference = inter ? &reference.committed : NULL;
    audit.width = width; audit.height = height;
    SVT_TEST_CHECK_EQ_INT(svt_av2_rd_frame_encode(&source, &recon, inter ? &reference : NULL,
        &config, &scratch, &output, &first), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(first.replay_checks, output.decision_count);
    SVT_TEST_CHECK_EQ_INT(first.distortion, independent_distortion(&source, &recon));
    for (size_t i = 0u; i < output.decision_count; ++i) SVT_TEST_CHECK_EQ_INT(audit.committed[i], 1u);
    SVT_TEST_CHECK(first.motion_trials <= first.full_superblocks * 16u);
    SVT_TEST_CHECK(first.motion_accepts <= first.motion_trials);
    total_accepts += first.wide_accepts;
    SVT_TEST_CHECK(first.wide_trials <= first.full_superblocks * 8u);
    SVT_TEST_CHECK(first.wide_accepts <= first.wide_trials);
    if (width == 64u && height == 64u)
        SVT_TEST_CHECK_EQ_INT(first.candidate_count - baseline.candidate_count, first.wide_trials);
    if (width == 64u && height == 64u) SVT_TEST_CHECK(first.cost <= baseline.cost);
    if (!inter || (width < 64u && height < 64u)) {
        SVT_TEST_CHECK_EQ_INT(first.wide_trials, 0u);
        SVT_TEST_CHECK_EQ_INT(first.winner_hash, baseline.winner_hash);
    }
    config.frame_header.partition_decisions = output.decisions;
    config.frame_header.partition_decision_count = output.decision_count;
    uint64_t replay_rate = 0u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_frame(&config.frame_header, output.blocks, output.block_count,
        inter ? output.modes : NULL, inter ? output.mode_count : 0u, &replay_rate), 0);
    SVT_TEST_CHECK_EQ_INT(replay_rate, first.rate_q12);
    size_t recon_bytes = 0u;
    for (uint32_t p = 0u; p < 3u; ++p) recon_bytes += (size_t)recon.strides[p] * recon.aligned_height[p];
    uint8_t* saved_recon = (uint8_t*)malloc(recon_bytes);
    int32_t* saved_coeff = (int32_t*)malloc(output.coeff_count * sizeof(int32_t));
    const size_t saved_count = output.coeff_count;
    memcpy(saved_recon, recon.buffer, recon_bytes);
    memcpy(saved_coeff, output.qcoeff, saved_count * sizeof(int32_t));
    config.frame_header.partition_decisions = NULL; config.frame_header.partition_decision_count = 0u;
    config.reverse_candidates = 1u; memset(&audit, 0, sizeof(audit));
    svt_av2_rd_lambda(config.frame_header.base_q_idx, bd, &audit.lambda, &qstep);
    audit.output = &output; audit.source = &source; audit.reference = inter ? &reference.committed : NULL;
    audit.width = width; audit.height = height;
    SVT_TEST_CHECK_EQ_INT(svt_av2_rd_frame_encode(&source, &recon, inter ? &reference : NULL,
        &config, &scratch, &output, &reversed), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(first.winner_hash, reversed.winner_hash);
    SVT_TEST_CHECK_EQ_INT(first.cost, reversed.cost);
    SVT_TEST_CHECK_EQ_INT(first.rate_q12, reversed.rate_q12);
    SVT_TEST_CHECK_EQ_INT(output.coeff_count, saved_count);
    SVT_TEST_CHECK(memcmp(saved_coeff, output.qcoeff, saved_count * sizeof(int32_t)) == 0);
    SVT_TEST_CHECK(memcmp(saved_recon, recon.buffer, recon_bytes) == 0);
    config.reverse_candidates = 0u; config.rd_mode = 2u; config.candidate_hook = NULL;
    /* Reverse enumeration collected the complete original domain. On the first
     * SB, identical entry state permits exact per-candidate subset comparison. */
    Audit pruned_audit = {0}; pruned_audit.lambda = audit.lambda;
    pruned_audit.exhaustive = &audit;
    pruned_audit.output = &output; pruned_audit.source = &source;
    pruned_audit.reference = inter ? &reference.committed : NULL;
    pruned_audit.width = width; pruned_audit.height = height;
    config.candidate_hook = observe; config.candidate_hook_user = &pruned_audit;
    SVT_TEST_CHECK_EQ_INT(svt_av2_rd_frame_encode(&source, &recon, inter ? &reference : NULL,
        &config, &scratch, &output, &pruned), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK(pruned.candidate_count <= first.candidate_count);
    /* A single SB shares its entry state, so the pruned subset cannot beat full. */
    if (width <= 64u && height <= 64u) SVT_TEST_CHECK(pruned.cost >= first.cost);
    const size_t capacity = output.coeff_capacity; output.coeff_capacity = 0u;
    config.candidate_hook = NULL;
    SVT_TEST_CHECK(svt_av2_rd_frame_encode(&source, &recon, inter ? &reference : NULL,
        &config, &scratch, &output, &pruned) != SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(output.block_count, 0u); output.coeff_capacity = capacity;
    config.rd_mode = 1u;
    FaultPath fault = {config.transform, 0u, 5u};
    if (inter && width == 64u && height == 64u)
        fault.fail_at = (uint32_t)(baseline.forward_calls - baseline_modes * 3u + 1u);
    config.transform.user = &fault; config.transform.forward = fault_forward;
    config.transform.inverse = fault_inverse;
    SVT_TEST_CHECK(svt_av2_rd_frame_encode(&source, &recon, inter ? &reference : NULL,
        &config, &scratch, &output, &pruned) != SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(output.block_count, 0u);
    config.transform = fault.real;
    config.rd_mode = 1u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_rd_frame_encode(&source, &recon, inter ? &reference : NULL,
        &config, &scratch, &output, &reversed), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(first.winner_hash, reversed.winner_hash);
    SVT_TEST_CHECK(memcmp(saved_recon, recon.buffer, recon_bytes) == 0);
    SVT_TEST_CHECK_EQ_INT(picture_hash(&source), source_hash);
    if (inter) {
        SVT_TEST_CHECK_EQ_INT(picture_hash(&reference.committed), reference_hash);
        SVT_TEST_CHECK_EQ_INT(reference.frame_index, 0u);
    }
    free(saved_recon); free(saved_coeff); free(output.blocks); free(output.modes);
    free(output.qcoeff); free(output.decisions);
    svt_av2_reference_release(&reference); svt_av2_recon_pool_free(&scratch);
    svt_av2_picture_free(&source); svt_av2_picture_free(&recon);
}

typedef struct FullCandidate { int32_t x, y; uint64_t sad; } FullCandidate;
static int full_order(const void* av, const void* bv) {
    const FullCandidate* a = (const FullCandidate*)av;
    const FullCandidate* b = (const FullCandidate*)bv;
    if (a->sad != b->sad) return a->sad < b->sad ? -1 : 1;
    const int am = abs(a->x) + abs(a->y), bm = abs(b->x) + abs(b->y);
    if (am != bm) return am < bm ? -1 : 1;
    if (a->y != b->y) return a->y < b->y ? -1 : 1;
    return a->x < b->x ? -1 : a->x != b->x;
}
static uint64_t full_sad(const SvtAv2FrameBuffer* source, const SvtAv2FrameBuffer* reference,
    const SvtAv2BlockView* block, int32_t dx, int32_t dy) {
    uint64_t sum = 0u;
    const uint32_t step = source->bit_depth > 8u ? 2u : 1u;
    for (uint32_t y = 0u; y < block->h; ++y)
        for (uint32_t x = 0u; x < block->w; ++x) {
            int32_t rx = (int32_t)(block->x + x) + dx, ry = (int32_t)(block->y + y) + dy;
            if (rx < 0) rx = 0;
            if (ry < 0) ry = 0;
            if (rx >= (int32_t)reference->visible_width) rx = (int32_t)reference->visible_width - 1;
            if (ry >= (int32_t)reference->visible_height) ry = (int32_t)reference->visible_height - 1;
            const int a = svt_av2_sample_load(source->planes[0] + (size_t)(block->y + y) * source->strides[0] + (block->x + x) * step, source->bit_depth);
            const int b = svt_av2_sample_load(reference->planes[0] + (size_t)ry * reference->strides[0] + (size_t)rx * step, reference->bit_depth);
            sum += (uint64_t)abs(a - b);
        }
    return sum;
}
static void list_tests(void) {
    unsigned appended = 0u;
    for (unsigned format = 0u; format < 3u; ++format) {
        const uint32_t bd = format == 0u ? 8u : 10u, step = bd == 8u ? 1u : 2u;
        const SvtAv2ChromaFormat chroma = format == 2u ? SVT_AV2_CHROMA_444 : SVT_AV2_CHROMA_420;
        SvtAv2FrameBuffer source = {0}, reference = {0};
        svt_av2_picture_alloc_ex(&source, 32u, 32u, bd, chroma);
        svt_av2_picture_alloc_ex(&reference, 32u, 32u, bd, chroma);
        for (unsigned pattern = 0u; pattern < 3u; ++pattern) {
            fill(&reference);
            for (uint32_t y = 0u; y < 32u; ++y)
                for (uint32_t x = 0u; x < 32u; ++x) {
                    const uint32_t rx = x + 5u < 32u ? x + 5u : 31u, ry = y > 3u ? y - 3u : 0u;
                    uint16_t value = pattern == 0u ? 0u : pattern == 1u ?
                        (uint16_t)(((x * 19u + y * 23u) & 255u) << (bd - 8u)) :
                        svt_av2_sample_load(reference.planes[0] + (size_t)ry * reference.strides[0] + rx * step, bd);
                    svt_av2_sample_store(source.planes[0] + (size_t)y * source.strides[0] + x * step, bd, value);
                    if (pattern == 0u)
                        svt_av2_sample_store(reference.planes[0] + (size_t)y * reference.strides[0] + x * step, bd, 0u);
                }
            for (unsigned position = 0u; position < 3u; ++position) {
                SvtAv2BlockView block = {0}; block.x = position * 8u; block.y = position * 8u;
                block.w = block.h = 16u;
                FullCandidate oracle[289]; size_t n = 0u;
                for (int32_t y = -8; y <= 8; ++y)
                    for (int32_t x = -8; x <= 8; ++x) {
                        oracle[n].x = x; oracle[n].y = y;
                        oracle[n++].sad = full_sad(&source, &reference, &block, x, y);
                    }
                qsort(oracle, n, sizeof(oracle[0]), full_order);
                SvtAv2MotionSearchList list; SvtAv2MotionSearchResult legacy;
                SVT_TEST_CHECK_EQ_INT(svt_av2_motion_estimate_list(&source, &reference, &block, &list), SVT_AV2_ERROR_NONE);
                SVT_TEST_CHECK_EQ_INT(svt_av2_motion_estimate(&source, &reference, &block, &legacy), SVT_AV2_ERROR_NONE);
                SVT_TEST_CHECK_EQ_INT(list.candidates, 289u);
                SVT_TEST_CHECK_EQ_INT(list.candidates, legacy.candidates);
                SVT_TEST_CHECK_EQ_INT(list.mv[0].mv_x, legacy.mv.mv_x);
                SVT_TEST_CHECK_EQ_INT(list.mv[0].mv_y, legacy.mv.mv_y);
                SVT_TEST_CHECK_EQ_INT(list.sad[0], legacy.sad);
                int top_zero = 0;
                for (size_t i = 0u; i < 3u; ++i) {
                    SVT_TEST_CHECK_EQ_INT(list.mv[i].mv_x, oracle[i].x);
                    SVT_TEST_CHECK_EQ_INT(list.mv[i].mv_y, oracle[i].y);
                    SVT_TEST_CHECK_EQ_INT(list.sad[i], oracle[i].sad);
                    top_zero |= oracle[i].x == 0 && oracle[i].y == 0;
                }
                SVT_TEST_CHECK_EQ_INT(list.count, top_zero ? 3u : 4u);
                if (!top_zero) {
                    ++appended;
                    SVT_TEST_CHECK_EQ_INT(list.mv[3].mv_x, 0);
                    SVT_TEST_CHECK_EQ_INT(list.mv[3].mv_y, 0);
                    SVT_TEST_CHECK_EQ_INT(list.sad[3], full_sad(&source, &reference, &block, 0, 0));
                }
                for (uint32_t i = 0u; i < list.count; ++i)
                    for (uint32_t j = i + 1u; j < list.count; ++j)
                        SVT_TEST_CHECK(list.mv[i].mv_x != list.mv[j].mv_x || list.mv[i].mv_y != list.mv[j].mv_y);
                FullCandidate visited[80]; size_t visited_count = 0u;
                uint8_t seen[1089] = {0};
                for (int32_t y = -16; y <= 16; y += 4)
                    for (int32_t x = -16; x <= 16; x += 4) {
                        if (x >= -8 && x <= 8 && y >= -8 && y <= 8) continue;
                        visited[visited_count].x = x; visited[visited_count].y = y;
                        visited[visited_count++].sad = full_sad(&source, &reference, &block, x, y);
                        seen[(y + 16) * 33 + x + 16] = 1u;
                    }
                SVT_TEST_CHECK_EQ_INT(visited_count, 56u);
                qsort(visited, visited_count, sizeof(visited[0]), full_order);
                const int32_t center_x = visited[0].x, center_y = visited[0].y;
                for (int32_t y = center_y - 2; y <= center_y + 2; ++y)
                    for (int32_t x = center_x - 2; x <= center_x + 2; ++x) {
                        if (x < -16 || x > 16 || y < -16 || y > 16 ||
                            (x >= -8 && x <= 8 && y >= -8 && y <= 8)) continue;
                        const size_t coordinate = (size_t)(y + 16) * 33u + (uint32_t)(x + 16);
                        if (seen[coordinate]) continue;
                        seen[coordinate] = 1u;
                        visited[visited_count].x = x; visited[visited_count].y = y;
                        visited[visited_count++].sad = full_sad(&source, &reference, &block, x, y);
                    }
                SVT_TEST_CHECK(visited_count <= 80u);
                qsort(visited, visited_count, sizeof(visited[0]), full_order);
                SvtAv2MotionSearchList wide;
                SVT_TEST_CHECK_EQ_INT(svt_av2_motion_estimate_wide(&source, &reference, &block, &wide), SVT_AV2_ERROR_NONE);
                SVT_TEST_CHECK_EQ_INT(wide.count, 2u);
                SVT_TEST_CHECK_EQ_INT(wide.candidates, visited_count);
                for (uint32_t i = 0u; i < 2u; ++i) {
                    SVT_TEST_CHECK_EQ_INT(wide.mv[i].mv_x, visited[i].x);
                    SVT_TEST_CHECK_EQ_INT(wide.mv[i].mv_y, visited[i].y);
                    SVT_TEST_CHECK_EQ_INT(wide.sad[i], visited[i].sad);
                }
            }
        }
        svt_av2_picture_free(&source); svt_av2_picture_free(&reference);
    }
    SVT_TEST_CHECK(appended > 0u);
}

int main(void) {
    list_tests();
    frame_test(56u, 56u, 10u, SVT_AV2_CHROMA_420, 1);
    frame_test(64u, 64u, 8u, SVT_AV2_CHROMA_420, 0);
    frame_test(64u, 64u, 8u, SVT_AV2_CHROMA_420, 1);
    frame_test(128u, 128u, 8u, SVT_AV2_CHROMA_420, 0);
    frame_test(128u, 128u, 8u, SVT_AV2_CHROMA_420, 1);
    frame_test(136u, 72u, 10u, SVT_AV2_CHROMA_420, 0);
    frame_test(120u, 120u, 10u, SVT_AV2_CHROMA_444, 1);
    SVT_TEST_CHECK(total_accepts > 0u);
    SVT_TEST_REPORT("test_m6_wide");
}
