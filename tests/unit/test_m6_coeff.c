#include "rd_frame.h"
#include "coeff_syntax.h"
#include "transform_path.h"
#include "test_util.h"

#include <stdlib.h>
#include <string.h>

static uint64_t total_accepts;
static uint32_t test_qp = 100u;
static int low_energy;

typedef struct TracePath {
    SvtAv2IntraTransformPath real;
    int32_t original_q[12][1024];
    uint64_t calls;
} TracePath;

static SvtAv2Error trace_forward(void* user, uint32_t plane, uint32_t w, uint32_t h,
    const int16_t* residual, uint32_t stride, int32_t* coeff, int32_t* qcoeff,
    int32_t* dqcoeff, int* zero) {
    TracePath* trace = (TracePath*)user;
    const SvtAv2Error error = trace->real.forward(trace->real.user, plane, w, h,
        residual, stride, coeff, qcoeff, dqcoeff, zero);
    if (error == SVT_AV2_ERROR_NONE) {
        memcpy(trace->original_q[trace->calls % 12u], qcoeff,
            (size_t)SVT_AV2_MIN(w, 32u) * SVT_AV2_MIN(h, 32u) * sizeof(int32_t));
        ++trace->calls;
    }
    return error;
}
static SvtAv2Error trace_inverse(void* user, uint32_t plane, uint32_t w, uint32_t h,
    const int32_t* dqcoeff, uint32_t stride, int16_t* residual) {
    TracePath* trace = (TracePath*)user;
    return trace->real.inverse(trace->real.user, plane, w, h, dqcoeff, stride, residual);
}

typedef struct Audit {
    uint64_t minimum[16], seen[16], committed[16];
    uint64_t minimum_id[16], winner_id[16];
    uint64_t lambda;
    uint64_t first_sb_costs[512];
    uint8_t first_sb_seen[512];
    const struct Audit* exhaustive;
    TracePath* trace;
    SvtAv2RdFrameOutput* output;
    size_t previous_blocks;
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
        if (id >= 512u) {
            SVT_TEST_CHECK((id >> 9u) <= 4095u);
        }
        ++audit->seen[sb];
    } else {
        SVT_TEST_CHECK_EQ_INT(cost, audit->minimum[sb]);
        SVT_TEST_CHECK_EQ_INT(id, audit->minimum_id[sb]);
        audit->winner_id[sb] = id;
        if (audit->trace != NULL && audit->output != NULL) {
            const uint32_t cols = (audit->width + 63u) / 64u;
            const int full = audit->width - (uint32_t)(sb % cols) * 64u >= 64u &&
                audit->height - (uint32_t)(sb / cols) * 64u >= 64u;
            const size_t blocks = audit->output->block_count - audit->previous_blocks;
            if (full) {
                SVT_TEST_CHECK(blocks <= 12u);
                for (size_t b = 0u; b < blocks; ++b) {
                    const SvtAv2TileCoeffBlock* block = &audit->output->blocks[audit->previous_blocks + b];
                    const int32_t* original = audit->trace->original_q[(audit->trace->calls - blocks + b) % 12u];
                    uint16_t scan[1024];
                    uint32_t tx = 0u;
                    for (uint32_t side = 4u; side < block->width; side *= 2u) ++tx;
                    const size_t count = svt_av2_coeff_scan_dct_dct((SvtAv2TxSize)tx, scan);
                    const uint32_t eob = svt_av2_coeff_find_eob(original, scan, count);
                    const uint32_t selected = (id >> (9u + (uint32_t)b)) & 1u;
                    const size_t target = selected && eob > 1u ? scan[eob - 1u] : count;
                    for (size_t c = 0u; c < count; ++c) {
                        int32_t expected = original[c];
                        if (c == target) expected += expected > 0 ? -1 : 1;
                        SVT_TEST_CHECK_EQ_INT(block->qcoeff[c], expected);
                    }
                    SVT_TEST_CHECK_EQ_INT(block->qcoeff[0], original[0]);
                }
            }
            audit->previous_blocks = audit->output->block_count;
        }
        ++audit->committed[sb];
    }
}

static void fill(SvtAv2FrameBuffer* picture) {
    const uint32_t step = picture->bit_depth > 8u ? 2u : 1u;
    for (uint32_t p = 0u; p < 3u; ++p)
        for (uint32_t y = 0u; y < svt_av2_picture_plane_height(picture, p); ++y)
            for (uint32_t x = 0u; x < svt_av2_picture_plane_width(picture, p); ++x) {
                const uint32_t value = (low_energy ?
                    126u + ((x * 13u + y * 7u + p) % 5u) :
                    ((x * 11u + y * 7u + (x * y % 31u) + p * 27u) & 255u))
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
        SVT_TEST_CHECK_EQ_INT(svt_av2_reference_init(&reference, width, height, bd, format), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_reference_prepare(&reference, &source, 0u), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_reference_commit(&reference), SVT_AV2_ERROR_NONE);
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
    config.frame_header.base_q_idx = test_qp + (bd == 10u ? 48u : 0u);
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
    config.candidate_hook = NULL;
    SVT_TEST_CHECK_EQ_INT(svt_av2_rd_frame_encode(&source, &recon, inter ? &reference : NULL,
        &config, &scratch, &output, &baseline), SVT_AV2_ERROR_NONE);
    const size_t baseline_modes = output.mode_count;
    TracePath trace = {0}; trace.real = config.transform;
    config.transform.user = &trace; config.transform.forward = trace_forward;
    config.transform.inverse = trace_inverse;
    audit.trace = &trace; audit.output = &output; audit.width = width; audit.height = height;
    config.coeff_optimization = 1u;
    config.candidate_hook = observe;
    SVT_TEST_CHECK_EQ_INT(svt_av2_rd_frame_encode(&source, &recon, inter ? &reference : NULL,
        &config, &scratch, &output, &first), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(first.replay_checks, output.decision_count);
    SVT_TEST_CHECK_EQ_INT(first.distortion, independent_distortion(&source, &recon));
    for (size_t i = 0u; i < output.decision_count; ++i) SVT_TEST_CHECK_EQ_INT(audit.committed[i], 1u);
    SVT_TEST_CHECK(first.coeff_trials <= first.full_superblocks * 12u);
    SVT_TEST_CHECK(first.coeff_accepts <= first.coeff_trials);
    total_accepts += first.coeff_accepts;
    SVT_TEST_CHECK_EQ_INT(first.candidate_count - baseline.candidate_count, first.coeff_trials);
    if (width == 64u && height == 64u) SVT_TEST_CHECK(first.cost <= baseline.cost);
    if (width < 64u && height < 64u) {
        SVT_TEST_CHECK_EQ_INT(first.coeff_trials, 0u);
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
    audit.trace = &trace; audit.output = &output; audit.width = width; audit.height = height;
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
    pruned_audit.trace = &trace; pruned_audit.output = &output;
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
    /* On a single full SB, fail specifically on the first coefficient trial,
     * after all ordinary paths and before the original winner's replay. */
    if (width == 64u && height == 64u)
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

int main(void) {
    frame_test(56u, 56u, 10u, SVT_AV2_CHROMA_420, 0);
    frame_test(64u, 64u, 8u, SVT_AV2_CHROMA_420, 0);
    frame_test(64u, 64u, 8u, SVT_AV2_CHROMA_420, 1);
    frame_test(128u, 128u, 8u, SVT_AV2_CHROMA_420, 0);
    frame_test(128u, 128u, 8u, SVT_AV2_CHROMA_420, 1);
    frame_test(136u, 72u, 10u, SVT_AV2_CHROMA_420, 0);
    frame_test(120u, 120u, 10u, SVT_AV2_CHROMA_444, 1);
    low_energy = 1;
    test_qp = 80u; frame_test(64u, 64u, 8u, SVT_AV2_CHROMA_420, 0);
    test_qp = 140u; frame_test(64u, 64u, 10u, SVT_AV2_CHROMA_420, 0);
    test_qp = 220u; frame_test(64u, 64u, 10u, SVT_AV2_CHROMA_444, 0);
    SVT_TEST_CHECK(total_accepts > 0u);
    SVT_TEST_REPORT("test_m6_coeff");
}
