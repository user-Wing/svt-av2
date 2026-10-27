/*
 * T011 unit tests: tile_writer capacity contract.
 *
 * The M1 tile writer used a fixed 32-byte stack scratch, which cannot hold the
 * entropy payload of a real frame. svt_av2_tile_writer_write_closed_loop_key_scratch()
 * lets the caller supply the scratch and
 * svt_av2_tile_writer_max_payload_bytes() reports a size to allocate. These
 * tests pin the contract that the bitstream never truncates silently: a short
 * output buffer fails with NO_SPACE and reports the required size in *out_size.
 *
 * The frozen M1 vector E6 C8 00 12 and the M1 entry point's behaviour are
 * asserted unchanged in tests/unit/test_tile_writer.c.
 */
#include "tile_writer.h"
#include "frame_header.h"
#include "entropy_encoder.h"

#include "test_util.h"

#include <stdint.h>
#include <string.h>

static void test_max_payload_bytes_bound(void) {
    /* The bound must cover the header plus one coded byte per luma sample, and
     * it must grow with the frame, not be a fixed constant. */
    const size_t b64  = svt_av2_tile_writer_max_payload_bytes(64u, 64u);
    const size_t b720 = svt_av2_tile_writer_max_payload_bytes(1280u, 720u);
    SVT_TEST_CHECK(b64 >= 4u);
    SVT_TEST_CHECK(b720 > b64);
    SVT_TEST_CHECK(b720 >= (size_t)1280 * 720);
}

static void test_m1_vector_still_exact(void) {
    /* The M1 entry point must keep producing E6 C8 00 12 for 64x64. */
    static const uint8_t expect[4] = {0xE6, 0xC8, 0x00, 0x12};
    SvtAv2FrameHeaderConfig cfg;
    svt_av2_frame_header_config_default(&cfg, 64u, 64u);

    uint8_t tile[64];
    size_t  n = 0;
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_tile_writer_write_closed_loop_key(&cfg, tile, sizeof(tile), &n),
        0);
    SVT_TEST_CHECK_EQ_INT(n, sizeof(expect));
    SVT_TEST_CHECK(memcmp(tile, expect, sizeof(expect)) == 0);
}

static void test_scratch_path_matches_default(void) {
    /* Supplying scratch must not change the produced bytes. */
    SvtAv2FrameHeaderConfig cfg;
    svt_av2_frame_header_config_default(&cfg, 64u, 64u);

    uint8_t  coded[64];
    uint16_t precarry[64];
    SvtAv2TileWriterScratch scratch = {coded, sizeof(coded), precarry, 64u};

    uint8_t out[64];
    size_t  n = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_writer_write_closed_loop_key_scratch(
                              &cfg, out, sizeof(out), &n, &scratch),
                          0);
    SVT_TEST_CHECK_EQ_INT(n, 4u);

    uint8_t out2[64];
    size_t  n2 = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_writer_write_closed_loop_key(
                              &cfg, out2, sizeof(out2), &n2),
                          0);
    SVT_TEST_CHECK(n == n2);
    SVT_TEST_CHECK(memcmp(out, out2, n) == 0);
}

static void test_short_buffer_reports_required_size(void) {
    /* A 3-byte buffer for a 4-byte payload: fail, and *out_size must be 4 so a
     * caller can retry instead of guessing. Nothing is written. */
    SvtAv2FrameHeaderConfig cfg;
    svt_av2_frame_header_config_default(&cfg, 64u, 64u);

    uint8_t tile[3];
    memset(tile, 0xEE, sizeof(tile));
    size_t n = 0;
    int    st = svt_av2_tile_writer_write_closed_loop_key(&cfg, tile, sizeof(tile),
                                                          &n);
    SVT_TEST_CHECK(st != 0);
    SVT_TEST_CHECK_EQ_INT(n, 4u);
}

static void test_scratch_too_small_fails(void) {
    /* Too little entropy scratch must be reported, never truncated. */
    SvtAv2FrameHeaderConfig cfg;
    svt_av2_frame_header_config_default(&cfg, 64u, 64u);

    uint8_t  coded[1];
    uint16_t precarry[1];
    SvtAv2TileWriterScratch scratch = {coded, sizeof(coded), precarry, 1u};

    uint8_t out[64];
    size_t  n = 0;
    int     st = svt_av2_tile_writer_write_closed_loop_key_scratch(
        &cfg, out, sizeof(out), &n, &scratch);
    SVT_TEST_CHECK(st != 0);
}

static void test_null_arguments(void) {
    SvtAv2FrameHeaderConfig cfg;
    svt_av2_frame_header_config_default(&cfg, 64u, 64u);
    uint8_t out[16];
    size_t  n = 0;
    SVT_TEST_CHECK(svt_av2_tile_writer_write_closed_loop_key(NULL, out,
                                                             sizeof(out), &n) != 0);
    SVT_TEST_CHECK(svt_av2_tile_writer_write_closed_loop_key(&cfg, NULL,
                                                             sizeof(out), &n) != 0);
    SVT_TEST_CHECK(svt_av2_tile_writer_write_closed_loop_key(&cfg, out,
                                                             sizeof(out), NULL) != 0);
}

static void test_m2_coefficients_and_retry(void) {
    int32_t qcoeff[3][1024] = {{0}};
    SvtAv2TileCoeffBlock blocks[3] = {
        {0, 0, 0, 64, 64, qcoeff[0], 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 32, 32, qcoeff[1], 0, 0, 0, 0, 0, 0, 0},
        {2, 0, 0, 32, 32, qcoeff[2], 0, 0, 0, 0, 0, 0, 0}
    };
    /* Last raster entry forces EOB=1024 and its two-bit escape. High-range
     * positive/negative DC values exercise literals and both chroma planes. */
    qcoeff[0][1023] = 3;
    qcoeff[0][0] = -742;
    qcoeff[1][0] = 371;
    qcoeff[2][0] = -371;
    SvtAv2FrameHeaderConfig cfg;
    svt_av2_frame_header_config_default(&cfg, 64u, 64u);
    uint8_t out[8192], retry[8192], short_out[8];
    size_t n = 0, n2 = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_writer_write_closed_loop_key_coeffs(
        &cfg, blocks, 3u, out, sizeof(out), &n, NULL), 0);
    SVT_TEST_CHECK(n > 4u);
    memset(short_out, 0xa5, sizeof(short_out));
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_writer_write_closed_loop_key_coeffs(
        &cfg, blocks, 3u, short_out, sizeof(short_out), &n2, NULL), SVT_AV2_BW_ERROR_NO_SPACE);
    SVT_TEST_CHECK(n2 >= n);
    for (size_t i = 0; i < sizeof(short_out); ++i) SVT_TEST_CHECK(short_out[i] == 0xa5);
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_writer_write_closed_loop_key_coeffs(
        &cfg, blocks, 3u, retry, sizeof(retry), &n2, NULL), 0);
    SVT_TEST_CHECK(n2 == n && memcmp(out, retry, n) == 0);
    SVT_TEST_CHECK(svt_av2_tile_writer_write_closed_loop_key_coeffs(
        &cfg, blocks, 2u, retry, sizeof(retry), &n2, NULL) != 0);
    qcoeff[0][0] = 1 << 20; /* one above the decoder's supported coefficient range */
    memset(retry, 0xa5, sizeof(retry));
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_writer_write_closed_loop_key_coeffs(
        &cfg, blocks, 3u, retry, sizeof(retry), &n2, NULL), SVT_AV2_ENTROPY_ERROR_RANGE);
    SVT_TEST_CHECK(retry[0] == 0xa5 && retry[sizeof(retry) - 1u] == 0xa5);
}

int main(void) {
    test_max_payload_bytes_bound();
    test_m1_vector_still_exact();
    test_scratch_path_matches_default();
    test_short_buffer_reports_required_size();
    test_scratch_too_small_fails();
    test_null_arguments();
    test_m2_coefficients_and_retry();

    SVT_TEST_REPORT("test_tile_writer_capacity");
}
