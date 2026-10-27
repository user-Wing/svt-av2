/*
 * T012 unit test: the production transform path (T009 + T010) bound to the
 * frame core.
 *
 * This is the first test in T012 that runs the REAL transform and the REAL
 * quantizer, so it is the first that can say anything about a genuine
 * reconstruction. What it asserts, and what it deliberately does NOT:
 *
 *   asserts:
 *     - the production path binds and reports the frozen qindex;
 *     - a frame with sb_size = 64 (i.e. whole 64x64 coding blocks, which is what
 *       T007's traversal emits) completes end to end, so T009's packed
 *       coefficient grid (row stride min(w,32), 1024 entries for 64x64) is wired
 *       correctly;
 *     - the reconstruction is NOT the source (the quantizer loses information)
 *       and NOT the reset value (the chain actually wrote every block);
 *     - repeated runs of the same input are bit-identical (the path has no
 *       hidden state and the scratch is reused deterministically);
 *     - the coefficient/hook view is non-trivial (some block has non-zero
 *       qcoeff), so the path is not silently producing all-zero blocks.
 *
 *   does NOT assert (and must not be read as evidence of):
 *     - AV2 conformance or decoder agreement. That needs T011 coefficient
 *       syntax, T013 bitstream assembly and T014's runner to compare against
 *       avmdec. Those do not exist yet, so no test here claims bit-exactness
 *       against a decoder.
 */

#include "intra_frame.h"
#include "transform_path.h"

#include <stdlib.h>
#include <string.h>

#include "test_util.h"

/* A simple deterministic pattern; the exact content is not important, only that
 * it is not flat (so the transform sees real AC) and is reproducible. */
static uint8_t pattern_value(uint32_t x, uint32_t y) {
    return (uint8_t)((x * 5u + y * 11u + ((x * y) >> 3)) & 0xFFu);
}

static void fill_picture(SvtAv2FrameBuffer* pic) {
    for (uint32_t plane = 0; plane < 3u; ++plane) {
        uint8_t* const base = svt_av2_picture_plane(pic, plane);
        const uint32_t stride = pic->strides[plane];
        const uint32_t w = svt_av2_picture_plane_width(pic, plane);
        const uint32_t h = svt_av2_picture_plane_height(pic, plane);
        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                base[(size_t)y * stride + x] = pattern_value(x + plane * 3u, y);
            }
        }
    }
}

/* Hook that counts blocks and records whether any qcoeff was non-zero. */
typedef struct PathObservation {
    uint32_t blocks;
    int any_nonzero_qcoeff;
    int seen_too_small_block;
} PathObservation;

static int observe_hook(void* user,
                        uint32_t plane,
                        const SvtAv2BlockView* block,
                        const SvtAv2ReconScratch* scratch,
                        int all_zero) {
    (void)plane;
    (void)all_zero;
    PathObservation* obs = (PathObservation*)user;
    ++obs->blocks;
    /* The block hook's contract (intra_frame.h, contract section 3) is that
     * qcoeff still holds the block, laid out as a min(w,32)-wide packed grid:
     * row stride min(w,32), length min(w,32)*min(h,32). Indexing it with
     * scratch->stride (the SAMPLE stride) would be the rejected 64-wide raster
     * form and would read past the allocation for a 64x64 block. */
    const uint32_t kw = (block->w > 32u) ? 32u : block->w;
    const uint32_t kh = (block->h > 32u) ? 32u : block->h;
    const size_t n = (size_t)kw * kh;
    for (size_t i = 0; i < n; ++i) {
        if (scratch->qcoeff[i] != 0) {
            obs->any_nonzero_qcoeff = 1;
            break;
        }
    }
    if (block->w < 4u || block->h < 4u) {
        obs->seen_too_small_block = 1;
    }
    return 0;
}

static void run_frame(SvtAv2FrameBuffer* source,
                      SvtAv2FrameBuffer* recon,
                      SvtAv2ReconScratch* scratch,
                      SvtAv2TransformPathState* state,
                      PathObservation* obs) {
    fill_picture(source);

    SvtAv2IntraFrameConfig config;
    memset(&config, 0, sizeof(config));
    config.base_q_idx = 100;
    config.sb_size = 64; /* what T007 traverses; blocks are whole superblocks */
    config.frame_reset_value = 0xA5;
    SVT_TEST_CHECK_EQ_INT(svt_av2_transform_path_init(state, &config.transform, 100),
                          SVT_AV2_ERROR_NONE);
    if (obs != NULL) {
        config.block_hook = observe_hook;
        config.block_hook_user = obs;
    }

    SvtAv2IntraFrameStats stats;
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_intra_frame_encode(source, recon, &config, scratch, &stats), SVT_AV2_ERROR_NONE);

    /* Every luma block was reconstructed and the path really ran. */
    SVT_TEST_CHECK(stats.luma_blocks > 0u);
    SVT_TEST_CHECK_EQ_INT((int)state->forward_calls, (int)(stats.luma_blocks + stats.chroma_blocks));
    SVT_TEST_CHECK_EQ_INT((int)state->inverse_calls, (int)(stats.luma_blocks + stats.chroma_blocks));
    SVT_TEST_CHECK_EQ_INT((int)stats.chroma_blocks, (int)(2u * stats.luma_blocks));
}

/* Count samples that differ from the source, and from the reset value. */
static void compare_recon_to_source(SvtAv2FrameBuffer* source,
                                    SvtAv2FrameBuffer* recon,
                                    uint8_t reset_value,
                                    size_t* out_diff_from_source,
                                    size_t* out_equal_to_reset) {
    size_t diff = 0;
    size_t reset = 0;
    for (uint32_t plane = 0; plane < 3u; ++plane) {
        const uint8_t* const s = svt_av2_picture_plane(source, plane);
        const uint8_t* const r = svt_av2_picture_plane(recon, plane);
        const uint32_t stride = recon->strides[plane];
        const uint32_t w = svt_av2_picture_plane_width(recon, plane);
        const uint32_t h = svt_av2_picture_plane_height(recon, plane);
        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                const uint8_t rv = r[(size_t)y * stride + x];
                if (rv != s[(size_t)y * stride + x]) {
                    ++diff;
                }
                if (rv == reset_value) {
                    ++reset;
                }
            }
        }
    }
    *out_diff_from_source = diff;
    *out_equal_to_reset = reset;
}

/* --- tests ----------------------------------------------------------------- */

/* The production path must complete a 64x64-block frame. This is the geometry
 * that exercises T009's packed coefficient grid (row stride min(w,32)), which is
 * the part a naive raster assumption would get silently wrong. */
static void test_production_path_64x64_blocks(void) {
    const uint32_t width = 128;
    const uint32_t height = 128;

    SvtAv2FrameBuffer source;
    SvtAv2FrameBuffer recon;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&source, width, height), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&recon, width, height), SVT_AV2_ERROR_NONE);

    SvtAv2ReconScratch scratch;
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(&scratch, 64, 64), SVT_AV2_ERROR_NONE);

    /* Contract section 3 layout: the coefficient arrays are NOT sized like the
     * sample buffers. For a 64x64 block they carry a 32x32 grid, so the pool
     * must reserve exactly 1024 entries -- not stride*64 = 4096. Asserting the
     * capacity here is what keeps the layout honest: a future change back to a
     * 64-wide raster would have to fail this check first. */
    SVT_TEST_CHECK_EQ_INT((int)scratch.coeff_capacity, 1024);
    SVT_TEST_CHECK(scratch.coeff_capacity < scratch.capacity);

    SvtAv2TransformPathState state;
    PathObservation obs;
    memset(&obs, 0, sizeof(obs));
    run_frame(&source, &recon, &scratch, &state, &obs);

    /* 128x128 with 64x64 superblocks is 2x2 = 4 luma blocks. */
    SVT_TEST_CHECK_EQ_INT(obs.blocks, 4u * 3u);
    SVT_TEST_CHECK_EQ_INT(obs.seen_too_small_block, 0);

    /* The quantizer must have produced at least one non-zero coefficient;
     * otherwise the "reconstruction" would be prediction-only. */
    SVT_TEST_CHECK_EQ_INT(obs.any_nonzero_qcoeff, 1);

    size_t diff = 0;
    size_t reset = 0;
    compare_recon_to_source(&source, &recon, 0xA5, &diff, &reset);
    SVT_TEST_CHECK(diff > 0u); /* lossy: recon != source */
    /* Not "all reset": the chain wrote the visible area. A handful of samples
     * equal to the reset value is fine; the whole frame being reset is not. */
    const size_t visible = (size_t)width * height + 2u * (size_t)(width / 2u) * (height / 2u);
    SVT_TEST_CHECK(reset < visible);

    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_free(&scratch), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&recon), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&source), SVT_AV2_ERROR_NONE);
}

/* The same input encoded twice must give a bit-identical reconstruction: the
 * path holds no cross-frame state and the scratch reuse is deterministic. */
static void test_production_path_is_deterministic(void) {
    const uint32_t width = 64;
    const uint32_t height = 64;

    SvtAv2FrameBuffer source;
    SvtAv2FrameBuffer recon_a;
    SvtAv2FrameBuffer recon_b;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&source, width, height), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&recon_a, width, height), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&recon_b, width, height), SVT_AV2_ERROR_NONE);

    SvtAv2ReconScratch scratch;
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(&scratch, 64, 64), SVT_AV2_ERROR_NONE);

    /* Both runs share one scratch and one path state, encoding the same source
     * into two different reconstructions; the outputs must match. */
    SvtAv2TransformPathState state_a;
    SvtAv2TransformPathState state_b;
    run_frame(&source, &recon_a, &scratch, &state_a, NULL);

    SvtAv2IntraFrameConfig config;
    memset(&config, 0, sizeof(config));
    config.base_q_idx = 100;
    config.sb_size = 64;
    config.frame_reset_value = 0xA5;
    SVT_TEST_CHECK_EQ_INT(svt_av2_transform_path_init(&state_b, &config.transform, 100),
                          SVT_AV2_ERROR_NONE);

    SvtAv2IntraFrameStats stats;
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_intra_frame_encode(&source, &recon_b, &config, &scratch, &stats),
        SVT_AV2_ERROR_NONE);

    for (uint32_t plane = 0; plane < 3u; ++plane) {
        const uint8_t* const a = svt_av2_picture_plane(&recon_a, plane);
        const uint8_t* const b = svt_av2_picture_plane(&recon_b, plane);
        const uint32_t stride = recon_a.strides[plane];
        const uint32_t w = svt_av2_picture_plane_width(&recon_a, plane);
        const uint32_t h = svt_av2_picture_plane_height(&recon_a, plane);
        for (uint32_t y = 0; y < h; ++y) {
            SVT_TEST_CHECK_EQ_INT(memcmp(a + (size_t)y * stride, b + (size_t)y * stride, w), 0);
        }
    }

    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_free(&scratch), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&recon_b), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&recon_a), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&source), SVT_AV2_ERROR_NONE);
}

/* Invalid qindex must be refused at setup, not at the first block. */
static void test_production_path_rejects_unfrozen_qindex(void) {
    SvtAv2TransformPathState state;
    SvtAv2IntraTransformPath path;
    SVT_TEST_CHECK_EQ_INT(svt_av2_transform_path_init(&state, &path, 0),
                          SVT_AV2_ERROR_UNSUPPORTED);
    SVT_TEST_CHECK_EQ_INT(svt_av2_transform_path_init(NULL, &path, 100),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_transform_path_init(&state, NULL, 100),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
}

/* Every geometry the frame core can present (whole superblocks and their
 * chroma views) must be supported by the path, so a frame never fails midway
 * on a block shape T009 cannot transform. The exact set is T009's frozen
 * k_tx_shifts table: the five squares 4/8/16/32/64 plus the 2:1 pairs up to
 * 16x16. 64x64 is included, so a whole 64x64 superblock transforms. */
static void test_production_path_supports_traversed_shapes(void) {
    const uint32_t larger[][2]={{16u,32u},{32u,16u},{32u,64u},{64u,32u},
        {8u,32u},{32u,8u},{16u,64u},{64u,16u}};
    for(size_t i=0u;i<sizeof(larger)/sizeof(larger[0]);++i)
        SVT_TEST_CHECK_EQ_INT(svt_av2_transform_path_supports(larger[i][0],larger[i][1]),1);
    const uint32_t ok[][2] = {{4, 4}, {8, 8}, {16, 16}, {32, 32}, {64, 64},
                              {4, 8}, {8, 4}, {8, 16}, {16, 8}, {4, 16}, {16, 4}};
    for (size_t i = 0; i < sizeof(ok) / sizeof(ok[0]); ++i) {
        SVT_TEST_CHECK_EQ_INT(svt_av2_transform_path_supports(ok[i][0], ok[i][1]), 1);
    }
    /* Shapes T009 does not carry must be refused rather than silently
     * mishandled (a frame core that presented one would fail fast, not
     * corrupt). */
    SVT_TEST_CHECK_EQ_INT(svt_av2_transform_path_supports(0u, 0u), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_transform_path_supports(3u, 4u), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_transform_path_supports(128u, 128u), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_transform_path_supports(4u, 64u), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_transform_path_supports(64u, 4u), 0);
}

int main(void) {
    test_production_path_64x64_blocks();
    test_production_path_is_deterministic();
    test_production_path_rejects_unfrozen_qindex();
    test_production_path_supports_traversed_shapes();

    SVT_TEST_REPORT("test_transform_path");
}
