/*
 * T001 unit tests: shared geometry and bit-depth helpers.
 *
 * These helpers feed both Y4M frame sizing and encoder configuration
 * validation, so their boundary and overflow behaviour is part of the M0
 * contract.
 */
#include "common.h"

#include "test_util.h"

#include <stdint.h>

static void test_chroma_helpers(void) {
    SVT_TEST_CHECK_EQ_INT(svt_av2_chroma_shift_x(SVT_AV2_CHROMA_400), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_chroma_shift_x(SVT_AV2_CHROMA_420), 1);
    SVT_TEST_CHECK_EQ_INT(svt_av2_chroma_shift_x(SVT_AV2_CHROMA_422), 1);
    SVT_TEST_CHECK_EQ_INT(svt_av2_chroma_shift_x(SVT_AV2_CHROMA_444), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_chroma_shift_y(SVT_AV2_CHROMA_420), 1);
    SVT_TEST_CHECK_EQ_INT(svt_av2_chroma_shift_y(SVT_AV2_CHROMA_422), 0);

    SVT_TEST_CHECK_EQ_INT(svt_av2_plane_count(SVT_AV2_CHROMA_400), 1);
    SVT_TEST_CHECK_EQ_INT(svt_av2_plane_count(SVT_AV2_CHROMA_420), 3);

    SVT_TEST_CHECK_EQ_INT(svt_av2_bytes_per_sample(SVT_AV2_BIT_DEPTH_8), 1);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bytes_per_sample(SVT_AV2_BIT_DEPTH_10), 2);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bytes_per_sample(SVT_AV2_BIT_DEPTH_12), 2);

    SVT_TEST_CHECK_EQ_INT(svt_av2_is_high_bit_depth(SVT_AV2_BIT_DEPTH_8), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_is_high_bit_depth(SVT_AV2_BIT_DEPTH_10), 1);

    SVT_TEST_CHECK_EQ_STR(svt_av2_chroma_format_name(SVT_AV2_CHROMA_420), "4:2:0");
    SVT_TEST_CHECK_EQ_STR(svt_av2_chroma_format_name(SVT_AV2_CHROMA_444), "4:4:4");
    SVT_TEST_CHECK_EQ_STR(svt_av2_chroma_format_name((SvtAv2ChromaFormat)99), "unknown");
}

static void test_plane_dimensions(void) {
    uint32_t lw = 0, lh = 0, cw = 0, ch = 0;

    SVT_TEST_CHECK_EQ_INT(svt_av2_compute_plane_dimensions(352, 288, SVT_AV2_CHROMA_420, &lw, &lh, &cw, &ch),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(lw, 352);
    SVT_TEST_CHECK_EQ_INT(lh, 288);
    SVT_TEST_CHECK_EQ_INT(cw, 176);
    SVT_TEST_CHECK_EQ_INT(ch, 144);

    /* Odd luma dimensions round chroma up. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_compute_plane_dimensions(3, 3, SVT_AV2_CHROMA_420, &lw, &lh, &cw, &ch),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(cw, 2);
    SVT_TEST_CHECK_EQ_INT(ch, 2);

    /* 4:0:0 has no chroma planes. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_compute_plane_dimensions(16, 16, SVT_AV2_CHROMA_400, &lw, &lh, &cw, &ch),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(cw, 0);
    SVT_TEST_CHECK_EQ_INT(ch, 0);

    /* NULL outputs are allowed. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_compute_plane_dimensions(16, 16, SVT_AV2_CHROMA_420, NULL, NULL, NULL, NULL),
                          SVT_AV2_ERROR_NONE);

    /* Invalid geometry / format. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_compute_plane_dimensions(0, 16, SVT_AV2_CHROMA_420, &lw, &lh, &cw, &ch),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_compute_plane_dimensions(16, 0, SVT_AV2_CHROMA_420, &lw, &lh, &cw, &ch),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_compute_plane_dimensions(16, 16, (SvtAv2ChromaFormat)42, &lw, &lh, &cw, &ch),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
}

static void test_sample_count(void) {
    size_t samples = 0;

    SVT_TEST_CHECK_EQ_INT(svt_av2_compute_frame_sample_count(2, 2, SVT_AV2_CHROMA_420, &samples),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(samples, 2 * 2 + 2 * (1 * 1)); /* 4 + 2 */

    SVT_TEST_CHECK_EQ_INT(svt_av2_compute_frame_sample_count(2, 2, SVT_AV2_CHROMA_400, &samples),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(samples, 4);

    /* NULL output. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_compute_frame_sample_count(2, 2, SVT_AV2_CHROMA_420, NULL),
                          SVT_AV2_ERROR_INVALID_PARAMETER);

    /* Geometry whose sample count cannot be represented in size_t must be
     * rejected, never returned truncated. */
    samples = 123;
    SVT_TEST_CHECK_EQ_INT(svt_av2_compute_frame_sample_count(0xFFFFFFFFu, 0xFFFFFFFFu, SVT_AV2_CHROMA_444, &samples),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(samples, 123); /* untouched on failure */

    /* Largest geometry that still fits exactly must succeed. 4:0:0 avoids the
     * chroma multiplier, so 0x10000 * 0x10000 luma samples fit in size_t. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_compute_frame_sample_count(0x10000u, 0x10000u, SVT_AV2_CHROMA_400, &samples),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK(samples == (size_t)0x10000u * (size_t)0x10000u);
}

int main(void) {
    test_chroma_helpers();
    test_plane_dimensions();
    test_sample_count();

    SVT_TEST_REPORT("test_common");
}
