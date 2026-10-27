#include "test_util.h"
#include "inter_frame.h"
#include "transform_path.h"
#include "inter_prediction.h"
#include <string.h>

typedef struct HookState {
    uint32_t count;
    int fail;
    SvtAv2InterBlockResult result[3];
    int32_t qcoeff[3][1024];
} HookState;
static int collect(void* user, const SvtAv2InterBlockResult* result, const SvtAv2ReconScratch* scratch) {
    HookState* state = (HookState*)user;
    if (state->fail) return 1;
    if (state->count >= 3) return 1;
    state->result[state->count] = *result;
    const uint32_t count = (result->block.w > 32 ? 32 : result->block.w) *
        (result->block.h > 32 ? 32 : result->block.h);
    memcpy(state->qcoeff[state->count], scratch->qcoeff, count * sizeof(int32_t));
    ++state->count;
    return 0;
}
static SvtAv2Error fail_forward(void* user, uint32_t plane, uint32_t w, uint32_t h,
    const int16_t* residual, uint32_t stride, int32_t* coeff, int32_t* qcoeff,
    int32_t* dqcoeff, int* zero) {
    (void)user; (void)plane; (void)w; (void)h; (void)residual; (void)stride;
    (void)coeff; (void)qcoeff; (void)dqcoeff; (void)zero;
    return SVT_AV2_ERROR_UNSUPPORTED;
}
typedef struct MixedHook { uint32_t count; SvtAv2BlockPredictionMode mode; SvtAv2MotionVector mv; } MixedHook;
static int mixed_collect(void* user, const SvtAv2InterBlockResult* result, const SvtAv2ReconScratch* scratch) {
    MixedHook* hook = (MixedHook*)user;
    SVT_TEST_CHECK_EQ_INT(result->plane, hook->count % 3);
    SVT_TEST_CHECK_EQ_INT(result->reference_frame_index, 0);
    SVT_TEST_CHECK(scratch->qcoeff != NULL);
    if (result->plane == 0) { hook->mode = result->mode; hook->mv = result->mv; }
    else {
        SVT_TEST_CHECK_EQ_INT(result->mode, hook->mode);
        SVT_TEST_CHECK_EQ_INT(result->mv.mv_x, hook->mv.mv_x);
        SVT_TEST_CHECK_EQ_INT(result->mv.mv_y, hook->mv.mv_y);
    }
    ++hook->count;
    return 0;
}
static void test_mixed_edge_blocks(void) {
    uint32_t seed = 0xA5A51234u;
    for (uint32_t f = 0; f < 3; ++f) {
        const uint32_t bd = f == 0 ? 8 : 10, step = bd == 8 ? 1u : 2u;
        const SvtAv2ChromaFormat format = f == 2 ? SVT_AV2_CHROMA_444 : SVT_AV2_CHROMA_420;
        SvtAv2FrameBuffer src, recon; SvtAv2ReferenceFrame reference;
        svt_av2_picture_alloc_ex(&src, 128, 72, bd, format);
        svt_av2_picture_alloc_ex(&recon, 128, 72, bd, format);
        svt_av2_reference_init(&reference, 128, 72, bd, format);
        for (uint32_t p = 0; p < 3; ++p)
            for (uint32_t y = 0; y < svt_av2_picture_plane_height(&src, p); ++y)
                for (uint32_t x = 0; x < svt_av2_picture_plane_width(&src, p); ++x) {
                    seed = seed * 1664525u + 1013904223u;
                    svt_av2_sample_store(src.planes[p] + y * src.strides[p] + x * step, bd, (uint16_t)((seed >> 12) & ((1u << bd) - 1u)));
                }
        svt_av2_reference_prepare(&reference, &src, 0); svt_av2_reference_commit(&reference);
        for (uint32_t p = 0; p < 3; ++p)
            for (uint32_t y = 0; y < svt_av2_picture_plane_height(&src, p); ++y)
                for (uint32_t x = svt_av2_picture_plane_width(&src, p) / 2; x < svt_av2_picture_plane_width(&src, p); ++x)
                    svt_av2_sample_store(src.planes[p] + y * src.strides[p] + x * step, bd, (uint16_t)(1u << (bd - 1)));
        SvtAv2ReconScratch scratch; svt_av2_recon_pool_init(&scratch, 64, 64);
        SvtAv2TransformPathState path; SvtAv2InterFrameConfig config; memset(&config, 0, sizeof(config));
        svt_av2_transform_path_init_bd(&path, &config.transform, bd == 8 ? 100 : 148, bd);
        MixedHook hook; memset(&hook, 0, sizeof(hook)); config.block_hook = mixed_collect; config.block_hook_user = &hook;
        SvtAv2InterFrameStats stats;
        SVT_TEST_CHECK_EQ_INT(svt_av2_inter_frame_encode(&src, &recon, &reference, &config, &scratch, &stats), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK(stats.inter_blocks > 0); SVT_TEST_CHECK(stats.intra_blocks > 0);
        SVT_TEST_CHECK_EQ_INT(hook.count, 3 * (stats.intra_blocks + stats.inter_blocks));
        SVT_TEST_CHECK_EQ_INT(reference.frame_index, 0);
        config.transform.forward = fail_forward;
        SVT_TEST_CHECK_EQ_INT(svt_av2_inter_frame_encode(&src, &recon, &reference, &config, &scratch, &stats), SVT_AV2_ERROR_UNSUPPORTED);
        SVT_TEST_CHECK_EQ_INT(reference.frame_index, 0); SVT_TEST_CHECK(reference.valid);
        svt_av2_recon_pool_free(&scratch); svt_av2_reference_release(&reference);
        svt_av2_picture_free(&src); svt_av2_picture_free(&recon);
    }
}

typedef struct AdaptiveHook {
    uint32_t count, vertical, horizontal, inter;
    uint32_t intra_mode;
    SvtAv2BlockPredictionMode mode;
    uint64_t digest;
} AdaptiveHook;
static int adaptive_collect(void* user, const SvtAv2InterBlockResult* result,
    const SvtAv2ReconScratch* scratch) {
    AdaptiveHook* hook = (AdaptiveHook*)user;
    SVT_TEST_CHECK_EQ_INT(result->plane, hook->count % 3);
    if (result->plane == 0) {
        hook->mode = result->mode; hook->intra_mode = result->intra_mode;
        if (result->mode == SVT_AV2_BLOCK_SINGLE_INTER) {
            ++hook->inter;
            SVT_TEST_CHECK_EQ_INT(result->intra_mode, 0);
        } else {
            if (result->intra_mode == 1) ++hook->vertical;
            if (result->intra_mode == 2) ++hook->horizontal;
            SVT_TEST_CHECK(result->intra_mode <= 2);
            SVT_TEST_CHECK(result->intra_mode != 1 || result->luma_block.y > 0);
            SVT_TEST_CHECK(result->intra_mode != 2 || result->luma_block.x > 0);
        }
    } else {
        SVT_TEST_CHECK_EQ_INT(result->mode, hook->mode);
        SVT_TEST_CHECK_EQ_INT(result->intra_mode, hook->intra_mode);
    }
    hook->digest = (hook->digest ^ result->mode) * 1099511628211ULL;
    hook->digest = (hook->digest ^ result->intra_mode) * 1099511628211ULL;
    hook->digest = (hook->digest ^ (uint32_t)result->mv.mv_x) * 1099511628211ULL;
    hook->digest = (hook->digest ^ (uint32_t)result->mv.mv_y) * 1099511628211ULL;
    const uint32_t count = (result->block.w > 32 ? 32 : result->block.w) *
        (result->block.h > 32 ? 32 : result->block.h);
    for (uint32_t i = 0; i < count; ++i)
        hook->digest = (hook->digest ^ (uint32_t)scratch->qcoeff[i]) * 1099511628211ULL;
    ++hook->count;
    return 0;
}
static uint64_t visible_digest(const SvtAv2FrameBuffer* frame) {
    uint64_t hash = 14695981039346656037ULL;
    const uint32_t step = frame->bit_depth > 8 ? 2u : 1u;
    for (uint32_t p = 0; p < 3; ++p)
        for (uint32_t y = 0; y < svt_av2_picture_plane_height(frame, p); ++y)
            for (uint32_t x = 0; x < svt_av2_picture_plane_width(frame, p); ++x)
                hash = (hash ^ svt_av2_sample_load(frame->planes[p] +
                    (size_t)y * frame->strides[p] + x * step, frame->bit_depth)) * 1099511628211ULL;
    return hash;
}
static void test_adaptive_p_modes(void) {
    for (uint32_t f = 0; f < 3; ++f) {
        const uint32_t bd = f == 0 ? 8 : 10, step = bd == 8 ? 1u : 2u;
        const SvtAv2ChromaFormat format = f == 2 ? SVT_AV2_CHROMA_444 : SVT_AV2_CHROMA_420;
        SvtAv2FrameBuffer src, recon; SvtAv2ReferenceFrame reference;
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&src, 128, 128, bd, format), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&recon, 128, 128, bd, format), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_reference_init(&reference, 128, 128, bd, format), SVT_AV2_ERROR_NONE);
        svt_av2_recon_reset_frame(&src, (uint16_t)(1u << (bd - 1)));
        uint32_t seed = 0x31415926u;
        for (uint32_t y = 0; y < 128; ++y) for (uint32_t x = 0; x < 128; ++x) {
            seed = seed * 1664525u + 1013904223u;
            svt_av2_sample_store(src.planes[0] + y * src.strides[0] + x * step, bd,
                (uint16_t)((seed >> 12) & ((1u << bd) - 1u)));
        }
        svt_av2_reference_prepare(&reference, &src, 0); svt_av2_reference_commit(&reference);
        const uint64_t reference_hash = visible_digest(&reference.committed);
        /* Top-left and bottom-right remain exact reference blocks (inter).
         * Top-right copies the reconstructed left edge (H); bottom-left copies
         * the reconstructed above edge (V). UV stays midlevel/DC in all cases. */
        for (uint32_t y = 0; y < 64; ++y) for (uint32_t x = 64; x < 128; ++x)
            svt_av2_sample_store(src.planes[0] + y * src.strides[0] + x * step, bd,
                svt_av2_sample_load(src.planes[0] + y * src.strides[0] + 63 * step, bd));
        for (uint32_t y = 64; y < 128; ++y) for (uint32_t x = 0; x < 64; ++x)
            svt_av2_sample_store(src.planes[0] + y * src.strides[0] + x * step, bd,
                svt_av2_sample_load(src.planes[0] + 63 * src.strides[0] + x * step, bd));
        SvtAv2ReconScratch scratch; svt_av2_recon_pool_init(&scratch, 64, 64);
        SvtAv2TransformPathState path; SvtAv2InterFrameConfig config; memset(&config, 0, sizeof(config));
        svt_av2_transform_path_init_bd(&path, &config.transform, bd == 8 ? 100 : 148, bd);
        AdaptiveHook hook; memset(&hook, 0, sizeof(hook));
        config.block_hook = adaptive_collect; config.block_hook_user = &hook;
        SvtAv2InterFrameStats stats;
        SVT_TEST_CHECK_EQ_INT(svt_av2_inter_frame_encode(&src, &recon, &reference, &config, &scratch, &stats), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(hook.vertical + hook.horizontal, 0);
        const uint64_t off_hash = visible_digest(&recon), off_coeff = hook.digest;
        memset(&hook, 0, sizeof(hook)); config.m5_select_intra = 0;
        SVT_TEST_CHECK_EQ_INT(svt_av2_inter_frame_encode(&src, &recon, &reference, &config, &scratch, &stats), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK(visible_digest(&recon) == off_hash); SVT_TEST_CHECK(hook.digest == off_coeff);
        memset(&hook, 0, sizeof(hook)); config.m5_select_intra = 1;
        SVT_TEST_CHECK_EQ_INT(svt_av2_inter_frame_encode(&src, &recon, &reference, &config, &scratch, &stats), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(hook.count, 12); SVT_TEST_CHECK_EQ_INT(hook.vertical, 1);
        SVT_TEST_CHECK_EQ_INT(hook.horizontal, 1); SVT_TEST_CHECK_EQ_INT(hook.inter, 2);
        SVT_TEST_CHECK_EQ_INT(stats.selected_sad, 0); SVT_TEST_CHECK_EQ_INT(stats.zero_planes, 12);
        SVT_TEST_CHECK(visible_digest(&src) == visible_digest(&recon));
        SVT_TEST_CHECK(reference_hash == visible_digest(&reference.committed));
        /* Genuine directional residual must follow the same selected modes. */
        const uint32_t changed_x[2] = { 70, 10 }, changed_y[2] = { 10, 70 };
        for (uint32_t i = 0; i < 2; ++i)
            for (uint32_t y = 0; y < 8; ++y) for (uint32_t x = 0; x < 8; ++x) {
                uint8_t* sample = src.planes[0] + (changed_y[i] + y) * src.strides[0] + (changed_x[i] + x) * step;
                const uint16_t value = svt_av2_sample_load(sample, bd);
                svt_av2_sample_store(sample, bd, (uint16_t)(value ^ ((1u << bd) - 1u)));
            }
        memset(&hook, 0, sizeof(hook));
        SVT_TEST_CHECK_EQ_INT(svt_av2_inter_frame_encode(&src, &recon, &reference, &config, &scratch, &stats), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(hook.vertical, 1); SVT_TEST_CHECK_EQ_INT(hook.horizontal, 1);
        SVT_TEST_CHECK_EQ_INT(hook.inter, 2); SVT_TEST_CHECK(stats.selected_sad > 0);
        SVT_TEST_CHECK(stats.nonzero_planes > 0);
        SVT_TEST_CHECK(reference_hash == visible_digest(&reference.committed));
        /* With all candidate costs tied, DC wins even where V/H are available. */
        svt_av2_recon_reset_frame(&src, (uint16_t)(1u << (bd - 1)));
        memset(&hook, 0, sizeof(hook));
        SVT_TEST_CHECK_EQ_INT(svt_av2_inter_frame_encode(&src, &recon, &reference, &config, &scratch, &stats), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(stats.intra_blocks, 4); SVT_TEST_CHECK_EQ_INT(hook.vertical + hook.horizontal, 0);
        SVT_TEST_CHECK_EQ_INT(hook.inter, 0); SVT_TEST_CHECK_EQ_INT(stats.selected_sad, 0);
        svt_av2_recon_pool_free(&scratch); svt_av2_reference_release(&reference);
        svt_av2_picture_free(&src); svt_av2_picture_free(&recon);
    }
}

static void test_partition_plan_inter(void) {
    for (uint32_t f = 0; f < 3u; ++f) {
        const uint32_t bd = f ? 10u : 8u, step = f ? 2u : 1u, scale = f ? 4u : 1u;
        const SvtAv2ChromaFormat format = f == 2u ? SVT_AV2_CHROMA_444 : SVT_AV2_CHROMA_420;
        SvtAv2FrameBuffer source, recon; SvtAv2ReferenceFrame reference;
        svt_av2_picture_alloc_ex(&source, 128, 64, bd, format);
        svt_av2_picture_alloc_ex(&recon, 128, 64, bd, format);
        svt_av2_reference_init(&reference, 128, 64, bd, format);
        svt_av2_recon_reset_frame(&source, (uint16_t)(128u * scale));
        for (uint32_t y = 0; y < 64u; ++y) for (uint32_t x = 0; x < 64u; ++x)
            svt_av2_sample_store(source.planes[0] + y * source.strides[0] + x * step, bd,
                (uint16_t)((1u + y / 32u * 2u + x / 32u) * 40u * scale));
        svt_av2_reference_prepare(&reference, &source, 0); svt_av2_reference_commit(&reference);
        const uint64_t reference_hash = visible_digest(&reference.committed);
        SvtAv2FrameGeometry geometry; svt_av2_frame_geometry_init_ex(&geometry, 128, 64, 64, format);
        uint16_t plan[2];
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_select_quads(&source, &geometry, plan, 2), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(plan[0], 1); SVT_TEST_CHECK_EQ_INT(plan[1], 0);
        SvtAv2ReconScratch scratch; svt_av2_recon_pool_init(&scratch, 64, 64);
        SvtAv2TransformPathState path; SvtAv2InterFrameConfig config; memset(&config, 0, sizeof(config));
        svt_av2_transform_path_init_bd(&path, &config.transform, f ? 148u : 100u, bd);
        config.partition_decisions = plan; config.partition_decision_count = 2u;
        config.block_hook = adaptive_collect; AdaptiveHook hook; config.block_hook_user = &hook;
        SvtAv2InterFrameStats stats;
        for (uint32_t adaptive = 0; adaptive < 2u; ++adaptive) {
            memset(&hook, 0, sizeof(hook)); config.m5_select_intra = adaptive;
            SVT_TEST_CHECK_EQ_INT(svt_av2_inter_frame_encode(&source, &recon, &reference, &config, &scratch, &stats), SVT_AV2_ERROR_NONE);
            SVT_TEST_CHECK_EQ_INT(hook.count, 15); SVT_TEST_CHECK_EQ_INT(stats.intra_blocks + stats.inter_blocks, 5);
            SVT_TEST_CHECK_EQ_INT(stats.selected_sad, 0); SVT_TEST_CHECK_EQ_INT(stats.zero_planes, 15);
            SVT_TEST_CHECK(visible_digest(&source) == visible_digest(&recon));
            SVT_TEST_CHECK_EQ_INT(plan[0], 1); SVT_TEST_CHECK_EQ_INT(plan[1], 0);
        }
        SVT_TEST_CHECK(reference_hash == visible_digest(&reference.committed));
        plan[1] = 882u; hook.count = 0;
        SVT_TEST_CHECK_EQ_INT(svt_av2_inter_frame_encode(&source, &recon, &reference, &config, &scratch, &stats), SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(hook.count, 0); SVT_TEST_CHECK(reference_hash == visible_digest(&reference.committed));
        svt_av2_recon_pool_free(&scratch); svt_av2_reference_release(&reference);
        svt_av2_picture_free(&source); svt_av2_picture_free(&recon);
    }
}

int main(void) {
    test_partition_plan_inter();
    test_adaptive_p_modes();
    test_mixed_edge_blocks();
    uint32_t seed = 0x27182818u;
    for (uint32_t f = 0; f < 3; ++f) {
        const uint32_t bd = f == 0 ? 8 : 10, step = bd == 8 ? 1u : 2u;
        const SvtAv2ChromaFormat format = f == 2 ? SVT_AV2_CHROMA_444 : SVT_AV2_CHROMA_420;
        SvtAv2FrameBuffer source, recon; SvtAv2ReferenceFrame reference;
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&source, 64, 64, bd, format), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&recon, 64, 64, bd, format), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_reference_init(&reference, 64, 64, bd, format), SVT_AV2_ERROR_NONE);
        for (uint32_t p = 0; p < 3; ++p)
            for (uint32_t y = 0; y < svt_av2_picture_plane_height(&source, p); ++y)
                for (uint32_t x = 0; x < svt_av2_picture_plane_width(&source, p); ++x) {
                    seed = seed * 1664525u + 1013904223u;
                    svt_av2_sample_store(source.planes[p] + y * source.strides[p] + x * step, bd, (uint16_t)((seed >> 12) & ((1u << bd) - 1u)));
                }
        SVT_TEST_CHECK_EQ_INT(svt_av2_reference_prepare(&reference, &source, 0), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_reference_commit(&reference), SVT_AV2_ERROR_NONE);
        SvtAv2ReconScratch scratch;
        SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(&scratch, 64, 64), SVT_AV2_ERROR_NONE);
        SvtAv2TransformPathState transform; SvtAv2InterFrameConfig config;
        memset(&config, 0, sizeof(config));
        SVT_TEST_CHECK_EQ_INT(svt_av2_transform_path_init_bd(&transform, &config.transform, bd == 8 ? 100 : 148, bd), SVT_AV2_ERROR_NONE);
        HookState hook; memset(&hook, 0, sizeof(hook));
        config.block_hook = collect; config.block_hook_user = &hook;
        SvtAv2InterFrameStats stats;
        SVT_TEST_CHECK_EQ_INT(svt_av2_inter_frame_encode(&source, &recon, &reference, &config, &scratch, &stats), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(stats.inter_blocks, 1); SVT_TEST_CHECK_EQ_INT(stats.intra_blocks, 0);
        SVT_TEST_CHECK_EQ_INT(stats.zero_planes, 3); SVT_TEST_CHECK_EQ_INT(hook.count, 3);
        for (uint32_t p = 0; p < 3; ++p) {
            SVT_TEST_CHECK_EQ_INT(hook.result[p].mode, SVT_AV2_BLOCK_SINGLE_INTER);
            SVT_TEST_CHECK_EQ_INT(hook.result[p].mv.mv_x, 0); SVT_TEST_CHECK_EQ_INT(hook.result[p].mv.mv_y, 0);
            SVT_TEST_CHECK_EQ_INT(hook.result[p].reference_frame_index, 0);
        }
        /* Odd, negative luma displacement: use actual committed samples as source. */
        const SvtAv2MotionVector mv = { -1, 3 };
        uint16_t prediction[4096];
        for (uint32_t p = 0; p < 3; ++p) {
            const SvtAv2BlockView block = { 0, 0, svt_av2_picture_plane_width(&source, p), svt_av2_picture_plane_height(&source, p), NULL };
            SVT_TEST_CHECK_EQ_INT(svt_av2_inter_predict(&reference.committed, p, &block, mv, prediction, block.w), SVT_AV2_ERROR_NONE);
            for (uint32_t y = 0; y < block.h; ++y) for (uint32_t x = 0; x < block.w; ++x)
                svt_av2_sample_store(source.planes[p] + y * source.strides[p] + x * step, bd, prediction[y * block.w + x]);
        }
        memset(&hook, 0, sizeof(hook));
        SVT_TEST_CHECK_EQ_INT(svt_av2_inter_frame_encode(&source, &recon, &reference, &config, &scratch, &stats), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(stats.inter_blocks, 1); SVT_TEST_CHECK_EQ_INT(stats.selected_sad, 0);
        SVT_TEST_CHECK_EQ_INT(hook.result[0].mv.mv_x, -1); SVT_TEST_CHECK_EQ_INT(hook.result[0].mv.mv_y, 3);
        memset(scratch.qcoeff, 0xA5, scratch.coeff_capacity * sizeof(int32_t));
        SVT_TEST_CHECK_EQ_INT(hook.qcoeff[0][0], 0);
        SVT_TEST_CHECK_EQ_INT(reference.frame_index, 0);
        /* Sparse source changes preserve inter selection but produce genuine coded residual. */
        for (uint32_t y = 0; y < 16; ++y) for (uint32_t x = 0; x < 16; ++x)
            svt_av2_sample_store(source.planes[0] + y * source.strides[0] + x * step, bd, 0);
        memset(&hook, 0, sizeof(hook));
        SVT_TEST_CHECK_EQ_INT(svt_av2_inter_frame_encode(&source, &recon, &reference, &config, &scratch, &stats), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(stats.inter_blocks, 1); SVT_TEST_CHECK(stats.nonzero_planes > 0);
        int nonzero = 0;
        for (uint32_t i = 0; i < 1024; ++i) if (hook.qcoeff[0][i] != 0) nonzero = 1;
        SVT_TEST_CHECK(nonzero);
        /* Flat midlevel has zero intra cost, so ties select intra without ME state pollution. */
        svt_av2_recon_reset_frame(&source, (uint16_t)(1u << (bd - 1)));
        memset(&hook, 0, sizeof(hook));
        SVT_TEST_CHECK_EQ_INT(svt_av2_inter_frame_encode(&source, &recon, &reference, &config, &scratch, &stats), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(stats.intra_blocks, 1);
        hook.fail = 1;
        SVT_TEST_CHECK_EQ_INT(svt_av2_inter_frame_encode(&source, &recon, &reference, &config, &scratch, &stats), SVT_AV2_ERROR_INVALID_STATE);
        SVT_TEST_CHECK_EQ_INT(reference.frame_index, 0); SVT_TEST_CHECK(reference.valid);
        svt_av2_recon_pool_free(&scratch); svt_av2_reference_release(&reference);
        svt_av2_picture_free(&source); svt_av2_picture_free(&recon);
    }
    SVT_TEST_REPORT("test_inter_frame");
}
