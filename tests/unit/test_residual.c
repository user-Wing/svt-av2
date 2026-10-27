/*
 * T008 unit tests: signed residual (source - prediction).
 *
 * Contract 2.3: residual = source - prediction, range -255..+255, type int16_t,
 * and explicitly NOT a truncated uint8_t subtraction. The range coverage below is
 * the point of this file: every extreme pair (0-255 = -255 and 255-0 = +255) and
 * a full random sweep, checked against a value computed from the two inputs
 * rather than from the implementation.
 */

#include "intra_prediction.h"
#include "picture.h"
#include "test_util.h"

#include <stdlib.h>
#include <string.h>

/* Fill the visible area of one plane with a coordinate hash. */
static void fill_pattern(SvtAv2FrameBuffer* pic, uint32_t plane, uint32_t base) {
    uint8_t* const p = svt_av2_picture_plane(pic, plane);
    const uint32_t w = svt_av2_picture_plane_width(pic, plane);
    const uint32_t h = svt_av2_picture_plane_height(pic, plane);
    const uint32_t stride = pic->strides[plane];
    for (uint32_t row = 0; row < h; ++row) {
        for (uint32_t col = 0; col < w; ++col) {
            uint32_t v = (base + row * 37u + col * 91u + plane * 13u);
            v ^= v >> 7;
            v *= 0x9E3779B9u;
            p[(size_t)row * stride + col] = (uint8_t)(v & 0xFFu);
        }
    }
}

static uint8_t plane_at(const SvtAv2FrameBuffer* pic, uint32_t plane, uint32_t row, uint32_t col) {
    return svt_av2_picture_plane_const(pic, plane)[(size_t)row * pic->strides[plane] + col];
}

/* Checks one residual case over a whole plane, against a per-sample expectation. */
static void check_residual_region(const SvtAv2FrameBuffer* src,
                                  const SvtAv2FrameBuffer* pred,
                                  uint32_t plane,
                                  uint32_t x,
                                  uint32_t y,
                                  uint32_t w,
                                  uint32_t h) {
    SvtAv2IntraBlock blk;
    memset(&blk, 0, sizeof(blk));
    blk.x = x;
    blk.y = y;
    blk.w = w;
    blk.h = h;
    blk.plane = plane;

    /* The residual output is BLOCK-LOCAL (see the header): the block's own w x h
     * samples start at index 0, so block->x/block->y select the source region but
     * do not displace the output. Use a stride wider than w so that a row
     * overrun or a wrong row pitch is visible, and pad the allocation so a write
     * past the block is caught by the sentinel check below. */
    const uint32_t stride = w + 5u;
    const uint32_t rows = h + 3u;
    const size_t count = (size_t)stride * rows;
    int16_t* const buf = (int16_t*)malloc(count * sizeof(int16_t));
    SVT_TEST_CHECK(buf != NULL);
    if (buf == NULL) {
        return;
    }
    /* Sentinel: any sample the implementation writes outside the requested region
     * will differ from this. */
    for (size_t i = 0; i < count; ++i) {
        buf[i] = (int16_t)-32768;
    }

    SVT_TEST_CHECK_EQ_INT(svt_av2_residual_compute(src, pred, &blk, buf, stride),
                          SVT_AV2_ERROR_NONE);

    for (uint32_t i = 0; i < h; ++i) {
        for (uint32_t j = 0; j < w; ++j) {
            const int expected = (int)plane_at(src, plane, y + i, x + j) -
                                 (int)plane_at(pred, plane, y + i, x + j);
            SVT_TEST_CHECK(expected >= -255 && expected <= 255);
            SVT_TEST_CHECK_EQ_INT(buf[(size_t)i * stride + j], expected);
        }
    }

    /* Every sample outside the block's own w x h region must be untouched, both
     * in the padding columns of each written row and in the rows after h. */
    for (uint32_t i = 0; i < rows; ++i) {
        for (uint32_t j = 0; j < stride; ++j) {
            const int inside = (i < h && j < w) ? 1 : 0;
            if (!inside) {
                SVT_TEST_CHECK_EQ_INT(buf[(size_t)i * stride + j], -32768);
            }
        }
    }
    free(buf);
}

static void test_extreme_range(void) {
    /* The two extremes the contract names: source 0 / prediction 255 gives -255,
     * source 255 / prediction 0 gives +255. Both must survive intact, which a
     * uint8_t subtraction could not do. */
    SvtAv2FrameBuffer src;
    SvtAv2FrameBuffer pred;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&src, 64, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&pred, 64, 64), SVT_AV2_ERROR_NONE);

    uint8_t zero = 0u;
    uint8_t max = 255u;

    for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
        uint8_t* s = svt_av2_picture_plane(&src, p);
        uint8_t* d = svt_av2_picture_plane(&pred, p);
        memset(s, zero, (size_t)src.aligned_height[p] * src.strides[p]);
        memset(d, max, (size_t)pred.aligned_height[p] * pred.strides[p]);
    }
    /* src - pred = 0 - 255 = -255 everywhere. */
    for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
        const uint32_t pw = svt_av2_picture_plane_width(&src, p);
        const uint32_t ph = svt_av2_picture_plane_height(&src, p);
        check_residual_region(&src, &pred, p, 0, 0, pw, ph);
        check_residual_region(&src, &pred, p, 0, 0, 8, 8);
    }

    /* Swap: src - pred = 255 - 0 = +255. */
    for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
        uint8_t* s = svt_av2_picture_plane(&src, p);
        uint8_t* d = svt_av2_picture_plane(&pred, p);
        memset(s, max, (size_t)src.aligned_height[p] * src.strides[p]);
        memset(d, zero, (size_t)pred.aligned_height[p] * pred.strides[p]);
    }
    for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
        const uint32_t pw = svt_av2_picture_plane_width(&src, p);
        const uint32_t ph = svt_av2_picture_plane_height(&src, p);
        check_residual_region(&src, &pred, p, 0, 0, pw, ph);
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&src), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&pred), SVT_AV2_ERROR_NONE);
}

static void test_constant_and_identity(void) {
    SvtAv2FrameBuffer src;
    SvtAv2FrameBuffer pred;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&src, 64, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&pred, 64, 64), SVT_AV2_ERROR_NONE);

    /* Equal images: every residual sample is exactly 0 (the M1 zero-residual
     * case, which must still hold now that the path is real). */
    for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
        fill_pattern(&src, p, 7u);
        fill_pattern(&pred, p, 7u);
    }
    for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
        const uint32_t pw = svt_av2_picture_plane_width(&src, p);
        const uint32_t ph = svt_av2_picture_plane_height(&src, p);
        check_residual_region(&src, &pred, p, 0, 0, pw, ph);
    }

    /* Constant source against a constant prediction, hand-derived. */
    uint8_t s_val = 200u;
    uint8_t d_val = 30u;
    for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
        memset(svt_av2_picture_plane(&src, p), s_val,
               (size_t)src.aligned_height[p] * src.strides[p]);
        memset(svt_av2_picture_plane(&pred, p), d_val,
               (size_t)pred.aligned_height[p] * pred.strides[p]);
    }
    SvtAv2IntraBlock blk;
    memset(&blk, 0, sizeof(blk));
    blk.x = 0;
    blk.y = 0;
    blk.w = 16;
    blk.h = 16;
    blk.plane = SVT_AV2_PLANE_Y;
    int16_t buf[16 * 16];
    SVT_TEST_CHECK_EQ_INT(svt_av2_residual_compute(&src, &pred, &blk, buf, 16),
                          SVT_AV2_ERROR_NONE);
    for (int i = 0; i < 16 * 16; ++i) {
        SVT_TEST_CHECK_EQ_INT(buf[i], 170);
    }

    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&src), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&pred), SVT_AV2_ERROR_NONE);
}

static void test_random_regions_all_planes(void) {
    SvtAv2FrameBuffer src;
    SvtAv2FrameBuffer pred;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&src, 64, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&pred, 64, 64), SVT_AV2_ERROR_NONE);

    for (uint32_t iter = 0; iter < 200; ++iter) {
        for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
            fill_pattern(&src, p, iter * 31u + 1u);
            fill_pattern(&pred, p, iter * 17u + 5u);
        }
        for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
            const uint32_t pw = svt_av2_picture_plane_width(&src, p);
            const uint32_t ph = svt_av2_picture_plane_height(&src, p);
            /* Whole plane, plus a bottom-right partial block, plus assorted
             * sizes including rectangular ones. */
            check_residual_region(&src, &pred, p, 0, 0, pw, ph);
            check_residual_region(&src, &pred, p, pw - 4u, ph - 4u, 4, 4);
            check_residual_region(&src, &pred, p, pw - 8u, ph - 8u, 8, 8);
            if (pw >= 16u && ph >= 16u) {
                check_residual_region(&src, &pred, p, 0, 0, 16, 8);
                check_residual_region(&src, &pred, p, 8, 8, 8, 16);
                check_residual_region(&src, &pred, p, pw - 16u, 0, 16, 4);
            }
        }
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&src), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&pred), SVT_AV2_ERROR_NONE);
}

static void test_residual_of_prediction_is_used(void) {
    /* The end-to-end shape T012 will use: source -> prediction -> residual. The
     * residual must be exactly source minus the predicted DC, which for a constant
     * source and the first block (which predicts 128) is a known constant. */
    SvtAv2FrameBuffer src;
    SvtAv2FrameBuffer recon;
    SvtAv2FrameBuffer pred;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&src, 64, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&recon, 64, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&pred, 64, 64), SVT_AV2_ERROR_NONE);

    uint8_t value = 100u;
    for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
        memset(svt_av2_picture_plane(&src, p), value,
               (size_t)src.aligned_height[p] * src.strides[p]);
        memset(svt_av2_picture_plane(&recon, p), 0,
               (size_t)recon.aligned_height[p] * recon.strides[p]);
        memset(svt_av2_picture_plane(&pred, p), 0,
               (size_t)pred.aligned_height[p] * pred.strides[p]);
    }

    /* First block: no neighbours, so DC is 128 and the residual is 100 - 128. */
    SvtAv2IntraBlock blk;
    memset(&blk, 0, sizeof(blk));
    blk.x = 0;
    blk.y = 0;
    blk.w = 8;
    blk.h = 8;
    blk.plane = SVT_AV2_PLANE_Y;

    SvtAv2IntraReference ref;
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_reference_build(&recon, &blk, &ref),
                          SVT_AV2_ERROR_NONE);
    uint8_t* const pred_base = svt_av2_picture_plane(&pred, SVT_AV2_PLANE_Y);
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_intra_dc_predict_flags(&ref, &blk, 0, 0, pred_base, pred.strides[SVT_AV2_PLANE_Y]),
        SVT_AV2_ERROR_NONE);

    int16_t buf[8 * 8];
    SVT_TEST_CHECK_EQ_INT(svt_av2_residual_compute(&src, &pred, &blk, buf, 8),
                          SVT_AV2_ERROR_NONE);
    for (int i = 0; i < 8 * 8; ++i) {
        SVT_TEST_CHECK_EQ_INT(buf[i], -28); /* 100 - 128 */
    }

    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&src), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&recon), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&pred), SVT_AV2_ERROR_NONE);
}

static void test_invalid_arguments(void) {
    SvtAv2FrameBuffer src;
    SvtAv2FrameBuffer pred;
    SvtAv2FrameBuffer other;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&src, 64, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&pred, 64, 64), SVT_AV2_ERROR_NONE);
    /* A different geometry: 64x32 luma, 32x16 chroma. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&other, 32, 32), SVT_AV2_ERROR_NONE);

    SvtAv2IntraBlock blk;
    memset(&blk, 0, sizeof(blk));
    blk.w = 8;
    blk.h = 8;
    blk.plane = SVT_AV2_PLANE_Y;
    int16_t buf[64 * 64];

    SVT_TEST_CHECK_EQ_INT(svt_av2_residual_compute(NULL, &pred, &blk, buf, 64),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_residual_compute(&src, NULL, &blk, buf, 64),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_residual_compute(&src, &pred, NULL, buf, 64),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_residual_compute(&src, &pred, &blk, NULL, 64),
                          SVT_AV2_ERROR_INVALID_PARAMETER);

    /* Geometry mismatch between the two images must be rejected. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_residual_compute(&src, &other, &blk, buf, 64),
                          SVT_AV2_ERROR_INVALID_PARAMETER);

    /* Zero size, oversize, out-of-range plane, region outside the visible area. */
    blk.w = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_residual_compute(&src, &pred, &blk, buf, 64),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    blk.w = 8;
    blk.h = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_residual_compute(&src, &pred, &blk, buf, 64),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    blk.h = 128;
    SVT_TEST_CHECK_EQ_INT(svt_av2_residual_compute(&src, &pred, &blk, buf, 64),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    blk.h = 8;
    blk.plane = 7;
    SVT_TEST_CHECK_EQ_INT(svt_av2_residual_compute(&src, &pred, &blk, buf, 64),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    blk.plane = SVT_AV2_PLANE_Y;
    blk.x = 60;
    SVT_TEST_CHECK_EQ_INT(svt_av2_residual_compute(&src, &pred, &blk, buf, 64),
                          SVT_AV2_ERROR_INVALID_PARAMETER);

    /* Stride smaller than the BLOCK width would make rows overlap. The buffer is
     * block-local, so the plane width is irrelevant: a stride of 7 is legal for
     * an 8-wide block only if it is >= 8, so 7 must be rejected even though the
     * plane is 64 wide. */
    blk.x = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_residual_compute(&src, &pred, &blk, buf, 7),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    /* A stride exactly equal to the block width is the minimum legal value. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_residual_compute(&src, &pred, &blk, buf, 8),
                          SVT_AV2_ERROR_NONE);

    /* A chroma region is validated against the chroma plane. */
    blk.plane = SVT_AV2_PLANE_U;
    blk.x = 28;
    SVT_TEST_CHECK_EQ_INT(svt_av2_residual_compute(&src, &pred, &blk, buf, 64),
                          SVT_AV2_ERROR_INVALID_PARAMETER);

    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&src), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&pred), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&other), SVT_AV2_ERROR_NONE);
}

int main(void) {
    test_extreme_range();
    test_constant_and_identity();
    test_random_regions_all_planes();
    test_residual_of_prediction_is_used();
    test_invalid_arguments();

    SVT_TEST_REPORT("test_residual");
}
