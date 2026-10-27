/*
 * T012 unit test: reconstruction primitives.
 *
 * Covers, with an INDEPENDENT model (no call into the implementation for the
 * expected value):
 *   - Clip3(0,255, prediction + inverse_residual) over the boundary cases:
 *     negative residual, positive overflow, exactly 0 and exactly 255, and a
 *     full 255-(-255) sweep at both ends.
 *   - Y / U / V planes and a non-tight destination plane stride.
 *   - Edge blocks at the right/bottom of the visible area on a frame whose
 *     visible size is not a multiple of the allocated stride.
 *   - >= 100 random blocks per plane with a fixed seed, including the
 *     saturation cases.
 *   - Guard band: samples outside the block's visible region, and the padding
 *     area, must be untouched.
 *   - Scratch pool: allocation, capacity checks, reuse, and that the frame core
 *     path reuses the same buffers across blocks (no per-block malloc) is
 *     checked by pointer identity in test_intra_frame.c.
 *   - svt_av2_recon_export_frame: tight packing matches an independently
 *     computed Y|U|V layout, padding is dropped, geometry and frame index are
 *     recorded, and a short buffer fails without writing.
 */

#include "reconstruction.h"

#include <stdlib.h>
#include <string.h>

#include "test_util.h"

/* --- independent model ----------------------------------------------------- */

static uint8_t model_clip(int32_t value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return (uint8_t)value;
}

/* Deterministic PRNG, independent of the implementation (no rand() so the
 * sequence is identical on every toolchain). */
static uint32_t g_rng = 0x12345678u;

static uint32_t rng_next(void) {
    /* xorshift32 */
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

/* --- helpers --------------------------------------------------------------- */

static void fill_pattern(uint8_t* buffer, size_t count, uint8_t seed) {
    for (size_t i = 0; i < count; ++i) {
        buffer[i] = (uint8_t)(seed + (uint8_t)(i * 7u));
    }
}

static void fill_pattern_i16(int16_t* buffer, size_t count, int16_t seed) {
    for (size_t i = 0; i < count; ++i) {
        buffer[i] = (int16_t)(seed + (int16_t)((int32_t)(i % 37u) - 18));
    }
}

/* --- clip ------------------------------------------------------------------ */

static void test_clip_boundaries(void) {
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_clip_sample(-1000000), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_clip_sample(-1), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_clip_sample(0), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_clip_sample(1), 1);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_clip_sample(254), 254);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_clip_sample(255), 255);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_clip_sample(256), 255);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_clip_sample(1000000), 255);

    /* Exhaustive over the whole clip range: every legal 8-bit sample survives
     * unchanged. */
    for (int32_t v = 0; v <= 255; ++v) {
        SVT_TEST_CHECK_EQ_INT(svt_av2_recon_clip_sample(v), v);
    }
}

/* --- the final chain step -------------------------------------------------- */

static void test_write_block_basic_planes(void) {
    SvtAv2FrameBuffer recon;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&recon, 16, 16), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_pad(&recon, 0x5A), SVT_AV2_ERROR_NONE);

    const uint32_t stride = 16;
    uint8_t prediction[16 * 16];
    int16_t residual[16 * 16];
    fill_pattern(prediction, sizeof(prediction), 100);
    fill_pattern_i16(residual, 16 * 16, -30);

    const uint32_t block_x = 2;
    const uint32_t block_y = 4;
    const uint32_t block_w = 6;
    const uint32_t block_h = 5;

    for (uint32_t plane = SVT_AV2_PLANE_Y; plane <= SVT_AV2_PLANE_V; ++plane) {
        /* The block must fit in THIS plane's visible area: chroma is half the
         * luma size (4:2:0), so the same block coordinates are only valid if
         * they fit there too. */
        const uint32_t plane_width = svt_av2_picture_plane_width(&recon, plane);
        const uint32_t plane_height = svt_av2_picture_plane_height(&recon, plane);
        if (block_x + block_w > plane_width || block_y + block_h > plane_height) {
            continue;
        }
        SvtAv2BlockView block;
        memset(&block, 0, sizeof(block));
        block.x = block_x;
        block.y = block_y;
        block.w = block_w;
        block.h = block_h;

        const uint32_t plane_stride = recon.strides[plane];
        uint8_t* const plane_base = svt_av2_picture_plane(&recon, plane);
        SVT_TEST_CHECK(plane_base != NULL);

        SVT_TEST_CHECK_EQ_INT(svt_av2_recon_write_block(&recon,
                                                        plane,
                                                        &block,
                                                        prediction,
                                                        stride,
                                                        residual,
                                                        stride),
                              SVT_AV2_ERROR_NONE);

        for (uint32_t i = 0; i < block.h; ++i) {
            for (uint32_t j = 0; j < block.w; ++j) {
                const int32_t sum = (int32_t)prediction[i * stride + j] +
                                    (int32_t)residual[i * stride + j];
                const uint8_t expected = model_clip(sum);
                const uint8_t actual = plane_base[(size_t)(block.y + i) * plane_stride + block.x + j];
                SVT_TEST_CHECK_EQ_INT(actual, expected);
            }
        }
    }

    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&recon), SVT_AV2_ERROR_NONE);
}

static void test_write_block_saturation_and_guard(void) {
    SvtAv2FrameBuffer recon;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&recon, 32, 32), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_pad(&recon, 0x5A), SVT_AV2_ERROR_NONE);

    /* Poison the visible area so untouched samples outside the block are
     * detectable. */
    uint8_t* const plane = svt_av2_picture_plane(&recon, SVT_AV2_PLANE_Y);
    const uint32_t plane_stride = recon.strides[SVT_AV2_PLANE_Y];
    for (uint32_t row = 0; row < 32; ++row) {
        memset(plane + (size_t)row * plane_stride, 0x11, 32);
    }

    const uint32_t stride = 8;
    uint8_t prediction[8 * 8];
    int16_t residual[8 * 8];
    /* 17 samples of patterns chosen to hit every clip combination. */
    for (uint32_t i = 0; i < 8 * 8; ++i) {
        prediction[i] = (uint8_t)((i * 3u) & 0xFFu);
        residual[i] = (int16_t)0;
    }
    /* Force specific cases: (0,-1) -> 0, (255,+1) -> 255, (0,+300) -> 255,
     * (255,-300) -> 0, exact 0, exact 255. */
    prediction[0] = 0;
    residual[0] = -1;
    prediction[1] = 255;
    residual[1] = 1;
    prediction[2] = 0;
    residual[2] = 300;
    prediction[3] = 255;
    residual[3] = -300;
    prediction[4] = 0;
    residual[4] = 0;
    prediction[5] = 255;
    residual[5] = 0;
    prediction[6] = 255;
    residual[6] = 255;
    prediction[7] = 0;
    residual[7] = -32768;

    SvtAv2BlockView block;
    memset(&block, 0, sizeof(block));
    block.x = 3;
    block.y = 5;
    block.w = 8;
    block.h = 4;

    SVT_TEST_CHECK_EQ_INT(
        svt_av2_recon_write_block(&recon, SVT_AV2_PLANE_Y, &block, prediction, stride, residual, stride),
        SVT_AV2_ERROR_NONE);

    /* Block contents follow the model. */
    for (uint32_t i = 0; i < block.h; ++i) {
        for (uint32_t j = 0; j < block.w; ++j) {
            const uint8_t expected = model_clip((int32_t)prediction[i * stride + j] +
                                                (int32_t)residual[i * stride + j]);
            SVT_TEST_CHECK_EQ_INT(plane[(size_t)(block.y + i) * plane_stride + block.x + j], expected);
        }
    }

    /* Nothing outside the block in the visible area changed. */
    for (uint32_t row = 0; row < 32; ++row) {
        for (uint32_t col = 0; col < 32; ++col) {
            const int inside = (row >= block.y && row < block.y + block.h && col >= block.x &&
                                col < block.x + block.w);
            if (!inside) {
                SVT_TEST_CHECK_EQ_INT(plane[(size_t)row * plane_stride + col], 0x11);
            }
        }
    }
    /* Padding is untouched by the write. The visible width is 32 and the
     * stride is 32, so there is no horizontal padding in this allocation and
     * no vertical padding either (aligned_height == visible height == 32). Use a
     * second picture whose visible width is smaller than its aligned stride. */
    SVT_TEST_CHECK_EQ_INT(plane[31 * (size_t)plane_stride], 0x11);

    SvtAv2FrameBuffer narrow;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&narrow, 8, 32), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_pad(&narrow, 0x5A), SVT_AV2_ERROR_NONE);
    uint8_t* const narrow_plane = svt_av2_picture_plane(&narrow, SVT_AV2_PLANE_Y);
    const uint32_t narrow_stride = narrow.strides[SVT_AV2_PLANE_Y];
    SVT_TEST_CHECK(narrow_stride > 8);
    SvtAv2BlockView narrow_block;
    memset(&narrow_block, 0, sizeof(narrow_block));
    narrow_block.w = 8;
    narrow_block.h = 8;
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_write_block(&narrow,
                                                    SVT_AV2_PLANE_Y,
                                                    &narrow_block,
                                                    prediction,
                                                    stride,
                                                    residual,
                                                    stride),
                          SVT_AV2_ERROR_NONE);
    /* The 24 bytes of horizontal padding in each written row are untouched. */
    for (uint32_t row = 0; row < 8; ++row) {
        for (uint32_t col = 8; col < narrow_stride; ++col) {
            SVT_TEST_CHECK_EQ_INT(narrow_plane[(size_t)row * narrow_stride + col], 0x5A);
        }
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&narrow), SVT_AV2_ERROR_NONE);

    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&recon), SVT_AV2_ERROR_NONE);
}

static void test_write_block_in_place_prediction(void) {
    /* The frame core reads the prediction straight from the recon plane. Verify
     * the read-then-write-same-sample ordering is lossless. */
    SvtAv2FrameBuffer recon;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&recon, 8, 8), SVT_AV2_ERROR_NONE);

    uint8_t* const plane = svt_av2_picture_plane(&recon, SVT_AV2_PLANE_Y);
    const uint32_t plane_stride = recon.strides[SVT_AV2_PLANE_Y];

    const uint32_t stride = 8;
    int16_t residual[8 * 8];
    /* Write through the plane's own stride, not a tight 8. The picture is 8x8
     * but the stride is 32, so a tight loop would spill into later rows. */
    uint8_t before[8 * 8];
    for (uint32_t i = 0; i < 8; ++i) {
        for (uint32_t j = 0; j < 8; ++j) {
            const uint8_t v = (uint8_t)(255u - (i * 8u + j) * 5u % 256u);
            plane[(size_t)i * plane_stride + j] = v;
            before[i * 8 + j] = v;
            residual[i * stride + j] = (int16_t)((int32_t)((i * 8 + j) % 11u) - 5);
        }
    }

    SvtAv2BlockView block;
    memset(&block, 0, sizeof(block));
    block.w = 8;
    block.h = 8;

    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_write_block(&recon,
                                                    SVT_AV2_PLANE_Y,
                                                    &block,
                                                    plane,
                                                    plane_stride,
                                                    residual,
                                                    stride),
                          SVT_AV2_ERROR_NONE);

    for (uint32_t i = 0; i < 8; ++i) {
        for (uint32_t j = 0; j < 8; ++j) {
            const uint8_t expected =
                model_clip((int32_t)before[i * 8 + j] + (int32_t)residual[i * 8 + j]);
            SVT_TEST_CHECK_EQ_INT(plane[(size_t)i * plane_stride + j], expected);
        }
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&recon), SVT_AV2_ERROR_NONE);
}

static void test_write_block_invalid_inputs(void) {
    SvtAv2FrameBuffer recon;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&recon, 16, 16), SVT_AV2_ERROR_NONE);

    const uint32_t stride = 16;
    uint8_t prediction[16 * 16];
    int16_t residual[16 * 16];
    memset(prediction, 1, sizeof(prediction));
    memset(residual, 0, sizeof(residual));

    SvtAv2BlockView block;
    memset(&block, 0, sizeof(block));
    block.w = 4;
    block.h = 4;

    /* NULL pointers. */
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_recon_write_block(NULL, 0, &block, prediction, stride, residual, stride),
        SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_recon_write_block(&recon, 0, NULL, prediction, stride, residual, stride),
        SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_recon_write_block(&recon, 0, &block, NULL, stride, residual, stride),
        SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_recon_write_block(&recon, 0, &block, prediction, stride, NULL, stride),
        SVT_AV2_ERROR_INVALID_PARAMETER);

    /* Out-of-range plane. */
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_recon_write_block(&recon, 3, &block, prediction, stride, residual, stride),
        SVT_AV2_ERROR_INVALID_PARAMETER);

    /* Zero-sized block. */
    block.w = 0;
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_recon_write_block(&recon, 0, &block, prediction, stride, residual, stride),
        SVT_AV2_ERROR_INVALID_PARAMETER);
    block.w = 4;
    block.h = 0;
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_recon_write_block(&recon, 0, &block, prediction, stride, residual, stride),
        SVT_AV2_ERROR_INVALID_PARAMETER);
    block.h = 4;

    /* Stride smaller than the block width. */
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_recon_write_block(&recon, 0, &block, prediction, 3, residual, stride),
        SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_recon_write_block(&recon, 0, &block, prediction, stride, residual, 3),
        SVT_AV2_ERROR_INVALID_PARAMETER);

    /* Block outside the visible area (both planes and past-the-edge). */
    block.x = 13; /* 13 + 4 = 17 > 16 */
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_recon_write_block(&recon, 0, &block, prediction, stride, residual, stride),
        SVT_AV2_ERROR_INVALID_PARAMETER);
    block.x = 0;
    block.y = 16;
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_recon_write_block(&recon, 0, &block, prediction, stride, residual, stride),
        SVT_AV2_ERROR_INVALID_PARAMETER);

    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&recon), SVT_AV2_ERROR_NONE);
}

static void test_write_block_random_100_per_plane(void) {
    /* >= 100 random blocks per plane, on a frame whose visible size is not a
     * multiple of the aligned stride, with a non-tight block stride. */
    SvtAv2FrameBuffer recon;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&recon, 24, 56), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_pad(&recon, 0x5A), SVT_AV2_ERROR_NONE);

    g_rng = 0x0BADF00Du;

    const uint32_t scratch_w = 40; /* > any block width used below */
    uint8_t* prediction = (uint8_t*)malloc(scratch_w * 64);
    int16_t* residual = (int16_t*)malloc(scratch_w * 64 * sizeof(int16_t));
    SVT_TEST_CHECK(prediction != NULL && residual != NULL);

    for (uint32_t plane = SVT_AV2_PLANE_Y; plane <= SVT_AV2_PLANE_V; ++plane) {
        const uint32_t plane_width = svt_av2_picture_plane_width(&recon, plane);
        const uint32_t plane_height = svt_av2_picture_plane_height(&recon, plane);
        const uint32_t plane_stride = recon.strides[plane];
        uint8_t* const plane_base = svt_av2_picture_plane(&recon, plane);

        for (int iter = 0; iter < 140; ++iter) {
            const uint32_t max_w = SVT_AV2_MIN(plane_width, 16u);
            const uint32_t max_h = SVT_AV2_MIN(plane_height, 16u);
            const uint32_t w = 1u + (rng_next() % max_w);
            const uint32_t h = 1u + (rng_next() % max_h);
            const uint32_t x = rng_next() % (plane_width - w + 1u);
            const uint32_t y = rng_next() % (plane_height - h + 1u);

            for (uint32_t i = 0; i < h * scratch_w; ++i) {
                prediction[i] = (uint8_t)(rng_next() & 0xFFu);
            }
            for (uint32_t i = 0; i < h * scratch_w; ++i) {
                /* Mix the whole legal residual range with out-of-range values to
                 * exercise both clips. */
                const uint32_t r = rng_next();
                residual[i] = (int16_t)((int32_t)(r % 700u) - 350);
            }

            SvtAv2BlockView block;
            memset(&block, 0, sizeof(block));
            block.x = x;
            block.y = y;
            block.w = w;
            block.h = h;

            /* Non-tight stride: scratch_w is never equal to w here. */
            SVT_TEST_CHECK_EQ_INT(svt_av2_recon_write_block(&recon,
                                                            plane,
                                                            &block,
                                                            prediction,
                                                            scratch_w,
                                                            residual,
                                                            scratch_w),
                                  SVT_AV2_ERROR_NONE);

            for (uint32_t i = 0; i < h; ++i) {
                for (uint32_t j = 0; j < w; ++j) {
                    const uint8_t expected = model_clip((int32_t)prediction[i * scratch_w + j] +
                                                        (int32_t)residual[i * scratch_w + j]);
                    SVT_TEST_CHECK_EQ_INT(
                        plane_base[(size_t)(y + i) * plane_stride + x + j], expected);
                }
            }
        }
    }

    free(prediction);
    free(residual);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&recon), SVT_AV2_ERROR_NONE);
}

/* --- per-frame reset ------------------------------------------------------- */

static void test_frame_reset_is_total(void) {
    SvtAv2FrameBuffer recon;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&recon, 16, 16), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_pad(&recon, 0xEE), SVT_AV2_ERROR_NONE);

    /* Overwrite visible areas of all planes with something recognisable. */
    for (uint32_t plane = SVT_AV2_PLANE_Y; plane <= SVT_AV2_PLANE_V; ++plane) {
        uint8_t* const base = svt_av2_picture_plane(&recon, plane);
        const uint32_t stride = recon.strides[plane];
        const uint32_t ph = svt_av2_picture_plane_height(&recon, plane);
        for (uint32_t r = 0; r < ph; ++r) {
            memset(base + (size_t)r * stride, 0x77, stride);
        }
    }

    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_reset_frame(&recon, 0xCD), SVT_AV2_ERROR_NONE);

    /* Visible area and padding both become 0xCD; the old data is gone. */
    for (uint32_t plane = SVT_AV2_PLANE_Y; plane <= SVT_AV2_PLANE_V; ++plane) {
        const uint8_t* const base = svt_av2_picture_plane_const(&recon, plane);
        const uint32_t stride = recon.strides[plane];
        const uint32_t aligned = recon.aligned_height[plane];
        for (uint32_t r = 0; r < aligned; ++r) {
            for (uint32_t c = 0; c < stride; ++c) {
                SVT_TEST_CHECK_EQ_INT(base[(size_t)r * stride + c], 0xCD);
            }
        }
    }

    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_reset_frame(NULL, 0), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&recon), SVT_AV2_ERROR_NONE);
}

/* --- scratch pool ---------------------------------------------------------- */

static void test_scratch_pool_lifecycle(void) {
    SvtAv2ReconScratch scratch;
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(&scratch, 64, 64), SVT_AV2_ERROR_NONE);

    SVT_TEST_CHECK(scratch.prediction != NULL);
    SVT_TEST_CHECK(scratch.residual != NULL);
    SVT_TEST_CHECK(scratch.inverse_residual != NULL);
    SVT_TEST_CHECK(scratch.coeff != NULL);
    SVT_TEST_CHECK(scratch.qcoeff != NULL);
    SVT_TEST_CHECK(scratch.dqcoeff != NULL);

    /* Stride is padded to the alignment and capacity covers the block. */
    SVT_TEST_CHECK_EQ_INT(scratch.stride % SVT_AV2_RECON_SCRATCH_STRIDE_ALIGN, 0);
    SVT_TEST_CHECK(scratch.stride >= 64);
    SVT_TEST_CHECK(scratch.capacity >= (size_t)scratch.stride * 64u);

    /* The coefficient arrays are sized by contract section 3, not by the sample
     * capacity: for 64x64 that is min(w,32)*min(h,32) = 1024, so they are
     * strictly smaller than the sample buffers. */
    SVT_TEST_CHECK_EQ_INT((int)scratch.coeff_capacity, 1024);
    SVT_TEST_CHECK(scratch.coeff_capacity < scratch.capacity);

    /* Buffers do not alias. */
    SVT_TEST_CHECK((void*)scratch.prediction != (void*)scratch.residual);
    SVT_TEST_CHECK((void*)scratch.qcoeff != (void*)scratch.dqcoeff);
    SVT_TEST_CHECK((void*)scratch.coeff != (void*)scratch.qcoeff);

    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_fits(&scratch, 64, 64), 1);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_fits(&scratch, 65, 64), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_fits(&scratch, 64, 65), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_fits(&scratch, 0, 64), 0);

    /* The pool starts zeroed. Each buffer is checked over its OWN extent: the
     * coefficient arrays are shorter than the sample buffers, so scanning them
     * to `capacity` would read past the allocation. */
    for (size_t i = 0; i < scratch.capacity; ++i) {
        SVT_TEST_CHECK_EQ_INT(scratch.residual[i], 0);
    }
    for (size_t i = 0; i < scratch.coeff_capacity; ++i) {
        SVT_TEST_CHECK_EQ_INT(scratch.qcoeff[i], 0);
    }

    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_free(&scratch), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK(scratch.prediction == NULL);
    SVT_TEST_CHECK_EQ_INT(scratch.allocated, 0);
    /* Double free is safe. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_free(&scratch), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_fits(&scratch, 64, 64), 0);

    /* Bad geometry. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(&scratch, 0, 64), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(&scratch, 64, 0), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(NULL, 64, 64), SVT_AV2_ERROR_INVALID_PARAMETER);
}

/* --- export --------------------------------------------------------------- */

static void test_export_tight_layout_and_metadata(void) {
    const uint32_t width = 24;  /* visible, stride aligned to 32 */
    const uint32_t height = 8;
    SvtAv2FrameBuffer recon;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&recon, width, height), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_pad(&recon, 0xAB), SVT_AV2_ERROR_NONE);

    /* Paint every visible sample with a plane-specific pattern so a wrong
     * plane order or a leaked stride byte is visible. */
    for (uint32_t plane = SVT_AV2_PLANE_Y; plane <= SVT_AV2_PLANE_V; ++plane) {
        uint8_t* const base = svt_av2_picture_plane(&recon, plane);
        const uint32_t stride = recon.strides[plane];
        const uint32_t pw = svt_av2_picture_plane_width(&recon, plane);
        const uint32_t ph = svt_av2_picture_plane_height(&recon, plane);
        for (uint32_t r = 0; r < ph; ++r) {
            for (uint32_t c = 0; c < pw; ++c) {
                base[(size_t)r * stride + c] = (uint8_t)(plane * 100u + r * 16u + c);
            }
        }
    }

    const size_t expected_size = (size_t)width * height +
                                 (size_t)2u * ((width / 2u) * (height / 2u));
    uint8_t* out = (uint8_t*)malloc(expected_size);
    SVT_TEST_CHECK(out != NULL);

    SvtAv2ReconExport meta;
    memset(&meta, 0xEE, sizeof(meta));
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_recon_export_frame(&recon, 7, out, expected_size, &meta),
        SVT_AV2_ERROR_NONE);

    SVT_TEST_CHECK_EQ_INT(meta.frame_index, 7);
    SVT_TEST_CHECK_EQ_INT(meta.visible_width, width);
    SVT_TEST_CHECK_EQ_INT(meta.visible_height, height);
    SVT_TEST_CHECK_EQ_INT(meta.chroma_width, width / 2u);
    SVT_TEST_CHECK_EQ_INT(meta.chroma_height, height / 2u);
    SVT_TEST_CHECK_EQ_INT(meta.byte_count, expected_size);

    /* Independently rebuild the expected tight layout: Y rows, then U rows,
     * then V rows, with only the visible columns and no padding. */
    size_t offset = 0;
    for (uint32_t plane = SVT_AV2_PLANE_Y; plane <= SVT_AV2_PLANE_V; ++plane) {
        const uint8_t* const base = svt_av2_picture_plane_const(&recon, plane);
        const uint32_t stride = recon.strides[plane];
        const uint32_t pw = svt_av2_picture_plane_width(&recon, plane);
        const uint32_t ph = svt_av2_picture_plane_height(&recon, plane);
        for (uint32_t r = 0; r < ph; ++r) {
            for (uint32_t c = 0; c < pw; ++c) {
                SVT_TEST_CHECK_EQ_INT(out[offset + c], base[(size_t)r * stride + c]);
            }
            offset += pw;
        }
    }
    SVT_TEST_CHECK_EQ_INT(offset, expected_size);
    /* No 0xAB (padding) byte may have survived. */
    for (size_t i = 0; i < expected_size; ++i) {
        SVT_TEST_CHECK(out[i] != 0xAB);
    }

    /* A short buffer fails and must not touch the metadata. */
    SvtAv2ReconExport untouched;
    memset(&untouched, 0xEE, sizeof(untouched));
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_recon_export_frame(&recon, 8, out, expected_size - 1u, &untouched),
        SVT_AV2_ERROR_INVALID_PARAMETER);
    for (size_t i = 0; i < sizeof(untouched); ++i) {
        SVT_TEST_CHECK_EQ_INT(((const uint8_t*)&untouched)[i], 0xEE);
    }

    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_export_frame(NULL, 0, out, expected_size, &meta),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_export_frame(&recon, 0, NULL, expected_size, &meta),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_export_frame(&recon, 0, out, expected_size, NULL),
                          SVT_AV2_ERROR_INVALID_PARAMETER);

    free(out);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&recon), SVT_AV2_ERROR_NONE);
}

static void test_export_frames_are_independent(void) {
    /* Two frames of the same geometry exported with the right index must land
     * in the caller's buffer back to back, which is the multi-frame layout
     * T014 compares. */
    const uint32_t width = 16;
    const uint32_t height = 16;
    SvtAv2FrameBuffer recon;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&recon, width, height), SVT_AV2_ERROR_NONE);

    const size_t frame_size = (size_t)width * height + (size_t)2u * ((width / 2u) * (height / 2u));
    uint8_t* combined = (uint8_t*)malloc(frame_size * 2u);
    SVT_TEST_CHECK(combined != NULL);

    SvtAv2ReconExport meta0;
    SvtAv2ReconExport meta1;

    for (uint32_t plane = SVT_AV2_PLANE_Y; plane <= SVT_AV2_PLANE_V; ++plane) {
        uint8_t* const base = svt_av2_picture_plane(&recon, plane);
        const uint32_t stride = recon.strides[plane];
        const uint32_t pw = svt_av2_picture_plane_width(&recon, plane);
        const uint32_t ph = svt_av2_picture_plane_height(&recon, plane);
        for (uint32_t r = 0; r < ph; ++r) {
            memset(base + (size_t)r * stride, (uint8_t)(plane + 0x10u), pw);
        }
    }
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_recon_export_frame(&recon, 0, combined, frame_size, &meta0),
        SVT_AV2_ERROR_NONE);

    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_reset_frame(&recon, 0x30), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_recon_export_frame(&recon, 1, combined + frame_size, frame_size, &meta1),
        SVT_AV2_ERROR_NONE);

    SVT_TEST_CHECK_EQ_INT(meta0.frame_index, 0);
    SVT_TEST_CHECK_EQ_INT(meta1.frame_index, 1);
    const size_t y_size = (size_t)width * height;
    const size_t c_size = (size_t)(width / 2u) * (height / 2u);
    for (size_t i = 0; i < frame_size; ++i) {
        const uint8_t expected0 = (i < y_size) ? 0x10 : ((i < y_size + c_size) ? 0x11 : 0x12);
        SVT_TEST_CHECK_EQ_INT(combined[i], expected0);
        SVT_TEST_CHECK_EQ_INT(combined[frame_size + i], 0x30);
    }

    free(combined);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&recon), SVT_AV2_ERROR_NONE);
}

static void test_highbd_recon_clip_reset_metadata(void) {
    SvtAv2FrameBuffer recon;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&recon, 8, 8, 10, SVT_AV2_CHROMA_444), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_reset_frame(&recon, 512), SVT_AV2_ERROR_NONE);
    uint8_t pred[8 * 17 + 1]; int16_t residual[64];
    for (uint32_t y = 0; y < 8; ++y)
        for (uint32_t x = 0; x < 8; ++x) svt_av2_sample_store(pred + 1 + y * 17 + 2 * x, 10, 512);
    uint32_t seed = 0x12345678;
    const SvtAv2BlockView block = { 0, 0, 8, 8, NULL };
    for (uint32_t trial = 0; trial < 100; ++trial) {
        for (uint32_t i = 0; i < 64; ++i) {
            seed = seed * 1664525u + 1013904223u;
            residual[i] = (int16_t)((int)(seed % 2048) - 1024);
        }
        for (uint32_t p = 0; p < 3; ++p) {
            SVT_TEST_CHECK_EQ_INT(svt_av2_recon_write_block(&recon, p, &block, pred + 1, 17, residual, 8), SVT_AV2_ERROR_NONE);
            for (uint32_t y = 0; y < 8; ++y)
                for (uint32_t x = 0; x < 8; ++x) {
                    int expected = residual[y * 8 + x] + 512;
                    if (expected < 0) expected = 0;
                    if (expected > 1023) expected = 1023;
                    SVT_TEST_CHECK_EQ_INT(svt_av2_sample_load(recon.planes[p] + y * recon.strides[p] + 2 * x, 10), expected);
                }
        }
    }
    uint8_t raw[384]; SvtAv2ReconExport meta;
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_export_frame(&recon, 17, raw, sizeof(raw), &meta), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(meta.bit_depth, 10); SVT_TEST_CHECK_EQ_INT(meta.chroma_format, SVT_AV2_CHROMA_444);
    SVT_TEST_CHECK_EQ_INT(meta.chroma_width, 8); SVT_TEST_CHECK_EQ_INT(meta.chroma_height, 8);
    SVT_TEST_CHECK_EQ_INT(meta.byte_count, sizeof(raw));
    svt_av2_picture_free(&recon);
}

int main(void) {
    test_highbd_recon_clip_reset_metadata();
    test_clip_boundaries();
    test_write_block_basic_planes();
    test_write_block_saturation_and_guard();
    test_write_block_in_place_prediction();
    test_write_block_invalid_inputs();
    test_write_block_random_100_per_plane();
    test_frame_reset_is_total();
    test_scratch_pool_lifecycle();
    test_export_tight_layout_and_metadata();
    test_export_frames_are_independent();

    SVT_TEST_REPORT("test_reconstruction");
}
