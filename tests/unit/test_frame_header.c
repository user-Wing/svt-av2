/*
 * T005 unit tests: minimal AV2 frame header writer.
 *
 * The expected bit pattern is derived from the AV2 v1.0.0 spec syntax and
 * semantics (frame_header_info() for single_picture_header_flag = 1 and
 * obu_type = OBU_CLOSED_LOOP_KEY), not from frame_header.c, so the two must
 * agree bit-for-bit.
 *
 * 22-bit header for base_q_idx = 100:
 *   uvlc cur_mfh_id                 0      -> "1"
 *   uvlc seq_header_id_in_frame_header 0   -> "1"
 *   allow_screen_content_tools      0
 *   allow_intrabc                   0
 *   disable_cdf_update              1
 *   uniform_tile_spacing_flag       1
 *   base_q_idx                     100     -> 0110 0100
 *   segmentation_enabled            0
 *   using_qmatrix                   0
 *   delta_q_present                 0
 *   apply_deblocking_filter[0]      0
 *   apply_deblocking_filter[1]      0
 *   tx_mode_select                  0
 *   reduced_tx_set                  0 (2 bits)
 *
 *   bits: 1 1 0 0 1 1 01100100 0 0 0 0 0 0 00
 *   finalize a 4-byte buffer with trailing bits -> trailing 1 then zeros:
 *   byte0 = 1100 1101 = 0xCD
 *   byte1 = 1001 0000 = 0x90
 *   byte2 = 0000 0001 = 0x01  (last data bit region + trailing "1")
 */
#include "bit_writer.h"
#include "frame_header.h"

#include "test_util.h"

#include <stdint.h>
#include <string.h>

static void test_header_length_bits(void) {
    uint8_t                 buf[8];
    SvtAv2BitWriter         bw;
    SvtAv2FrameHeaderConfig cfg;
    svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
    svt_av2_frame_header_config_default(&cfg, 64, 64);
    SVT_TEST_CHECK(svt_av2_frame_header_write(&bw, &cfg) == SVT_AV2_BW_OK);
    SVT_TEST_CHECK(svt_av2_bit_writer_bits_written(&bw) ==
                   SVT_AV2_FRAME_HEADER_M1_BITS);
}

static void test_header_defaults(void) {
    SvtAv2FrameHeaderConfig cfg;
    svt_av2_frame_header_config_default(&cfg, 64, 64);
    SVT_TEST_CHECK(cfg.base_q_idx == 100u); /* non-lossless */
    SVT_TEST_CHECK(cfg.frame_width == 64u);
    SVT_TEST_CHECK(cfg.frame_height == 64u);
}

static void test_header_bytes(void) {
    /* Build a 4-byte-aligned container: the 22-bit header, then byte_alignment,
     * then trailing_bits to make the whole payload 4 bytes. */
    uint8_t                 buf[4] = {0, 0, 0, 0};
    SvtAv2BitWriter         bw;
    SvtAv2FrameHeaderConfig cfg;
    svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
    svt_av2_frame_header_config_default(&cfg, 64, 64);
    SVT_TEST_CHECK(svt_av2_frame_header_write(&bw, &cfg) == SVT_AV2_BW_OK);
    SVT_TEST_CHECK(svt_av2_bit_writer_byte_alignment(&bw) == SVT_AV2_BW_OK);

    /* bits 0..21 hold the header; header bit 21 (tx/reduced 2-bit field) is 0.
     * After byte_alignment the first 3 bytes are: 0xCD 0x90 0x00. */
    SVT_TEST_CHECK(buf[0] == 0xCDu);
    SVT_TEST_CHECK(buf[1] == 0x90u);
    SVT_TEST_CHECK(buf[2] == 0x00u);
}

static void test_header_rejects_zero_quantizer(void) {
    /* base_q_idx = 0 would be lossless, which the M1 subset excludes. */
    uint8_t                 buf[8];
    SvtAv2BitWriter         bw;
    SvtAv2FrameHeaderConfig cfg;
    svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
    svt_av2_frame_header_config_default(&cfg, 64, 64);
    cfg.base_q_idx = 0u;
    SVT_TEST_CHECK(svt_av2_frame_header_write(&bw, &cfg) ==
                   SVT_AV2_BW_ERROR_RANGE);
}

static void test_header_null_args(void) {
    uint8_t          buf[8];
    SvtAv2BitWriter  bw;
    svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
    SVT_TEST_CHECK(svt_av2_frame_header_write(&bw, NULL) ==
                   SVT_AV2_BW_ERROR_NULL);
    SVT_TEST_CHECK(svt_av2_frame_header_write(NULL, NULL) ==
                   SVT_AV2_BW_ERROR_NULL);
}

static void test_m3_quantizer_width(void) {
    uint8_t out[8];
    SvtAv2BitWriter bw;
    SvtAv2FrameHeaderConfig cfg;
    svt_av2_frame_header_config_default(&cfg, 64u, 64u);
    cfg.bit_depth = 10u;
    cfg.base_q_idx = 148u;
    svt_av2_bit_writer_init(&bw, out, sizeof(out));
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_header_write(&bw, &cfg), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), SVT_AV2_FRAME_HEADER_M1_BITS + 1u);
    cfg.base_q_idx = 512u;
    svt_av2_bit_writer_init(&bw, out, sizeof(out));
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_header_write(&bw, &cfg), SVT_AV2_BW_ERROR_RANGE);
    cfg.bit_depth = 8u; cfg.chroma_format = 3u; cfg.base_q_idx = 100u;
    svt_av2_bit_writer_init(&bw, out, sizeof(out));
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_header_write(&bw, &cfg), SVT_AV2_BW_ERROR_UNSUPPORTED);
}

static void test_m4_inter_rejects_invalid_context(void) {
    uint8_t out[8];
    SvtAv2BitWriter bw;
    SvtAv2FrameHeaderConfig cfg;
    svt_av2_frame_header_config_default(&cfg, 64u, 64u);
    cfg.inter_frame = 1u;
    svt_av2_bit_writer_init(&bw, out, sizeof(out));
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_header_write(&bw, &cfg), SVT_AV2_BW_ERROR_UNSUPPORTED);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), 0u);
    cfg.multi_frame = 1u;
    cfg.inter_frame = 2u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_header_write(&bw, &cfg), SVT_AV2_BW_ERROR_UNSUPPORTED);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), 0u);
}

int main(void) {
    test_m4_inter_rejects_invalid_context();
    test_m3_quantizer_width();
    test_header_length_bits();
    test_header_defaults();
    test_header_bytes();
    test_header_rejects_zero_quantizer();
    test_header_null_args();

    SVT_TEST_REPORT("test_frame_header");
}
