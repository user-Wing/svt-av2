/*
 * T004 unit tests: minimal AV2 sequence header writer.
 *
 * The expected bytes come from an independent reference model written from the
 * AV2 v1.0.0 spec syntax (not from sequence_header.c), so the two must agree
 * bit-for-bit rather than sharing a single misunderstanding.
 */
#include "bit_writer.h"
#include "obu_writer.h"
#include "sequence_header.h"

#include "test_util.h"

#include <stdint.h>
#include <string.h>

/* --- helpers -------------------------------------------------------------- */

static int bytes_eq(const uint8_t* got, const uint8_t* want, size_t n) {
    return memcmp(got, want, n) == 0;
}

/* Writes a sequence header for the given size and appends the payload trailing
 * bits, returning the finalized payload length (0 on any error). */
static size_t build_payload(uint32_t width,
                            uint32_t height,
                            uint32_t level,
                            uint8_t* out,
                            size_t   out_cap) {
    SvtAv2BitWriter                 bw;
    SvtAv2SequenceHeaderConfig      cfg;
    svt_av2_bit_writer_init(&bw, out, out_cap);
    svt_av2_sequence_header_config_default(&cfg, width, height);
    cfg.seq_level_idx = level;
    if (svt_av2_sequence_header_write(&bw, &cfg) != SVT_AV2_BW_OK) {
        return 0;
    }
    if (svt_av2_obu_payload_finalize(&bw) != SVT_AV2_BW_OK) {
        return 0;
    }
    return svt_av2_bit_writer_bytes_written(&bw);
}

/* --- descriptor tests ----------------------------------------------------- */

static void test_uvlc(void) {
    static const struct {
        uint32_t value;
        uint32_t bits;
        uint8_t  byte0;
    } cases[] = {
        { 0, 1, 0x80 }, /* "1"          */
        { 1, 3, 0x40 }, /* "010"        */
        { 2, 3, 0x60 }, /* "011"        */
        { 3, 5, 0x20 }, /* "00100"      */
        { 7, 7, 0x10 }, /* "0001000"    */
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        uint8_t         buf[4] = { 0 };
        SvtAv2BitWriter bw;
        svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
        SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_uvlc(&bw, cases[i].value), SVT_AV2_BW_OK);
        SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), cases[i].bits);
        SVT_TEST_CHECK_EQ_INT(buf[0], cases[i].byte0);
    }

    /* UINT32_MAX cannot be encoded (value + 1 would wrap). */
    uint8_t         buf[8] = { 0 };
    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_uvlc(&bw, 0xFFFFFFFFu), SVT_AV2_BW_ERROR_RANGE);
}

static void test_ns(void) {
    static const struct {
        uint32_t n;
        uint32_t value;
        uint32_t bits;
        uint8_t  byte0;
    } cases[] = {
        { 1, 0, 0, 0x00 }, /* ns(1) encodes nothing */
        { 3, 0, 1, 0x00 }, /* "0"    */
        { 3, 1, 2, 0x80 }, /* "10"   */
        { 3, 2, 2, 0xC0 }, /* "11"   */
        { 5, 0, 2, 0x00 }, /* "00"   */
        { 5, 3, 3, 0xC0 }, /* "110"  */
        { 5, 4, 3, 0xE0 }, /* "111"  */
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        uint8_t         buf[4] = { 0 };
        SvtAv2BitWriter bw;
        svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
        SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_ns(&bw, cases[i].n, cases[i].value), SVT_AV2_BW_OK);
        SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), cases[i].bits);
        SVT_TEST_CHECK_EQ_INT(buf[0], cases[i].byte0);
    }

    uint8_t         buf[4] = { 0 };
    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_ns(&bw, 0, 0), SVT_AV2_BW_ERROR_INVALID_ARG);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_ns(&bw, 3, 3), SVT_AV2_BW_ERROR_INVALID_ARG);
}

/* --- finalized payload vectors (independent reference model) -------------- */
/* base_uv_ac_delta_q stores delta + 23: zero chroma offset is 10111. */

static void test_payload_176x144(void) {
    static const uint8_t want[] = {
        0x82, 0x0A, 0x77, 0xAF, 0x8F, 0x00,
        0x00, 0x40, 0x01, 0xB8, 0x00, 0x80,
    };
    uint8_t buf[64];
    size_t  n = build_payload(176, 144, 0, buf, sizeof(buf));
    SVT_TEST_CHECK_EQ_INT(n, sizeof(want));
    if (n == sizeof(want)) {
        SVT_TEST_CHECK(bytes_eq(buf, want, n));
    }
}

static void test_payload_64x64(void) {
    static const uint8_t want[] = {
        0x82, 0x0A, 0x55, 0xFF, 0xF0, 0x00,
        0x04, 0x00, 0x1B, 0x80, 0x08,
    };
    uint8_t buf[64];
    size_t  n = build_payload(64, 64, 0, buf, sizeof(buf));
    SVT_TEST_CHECK_EQ_INT(n, sizeof(want));
    if (n == sizeof(want)) {
        SVT_TEST_CHECK(bytes_eq(buf, want, n));
    }
}

static void test_payload_other_sizes(void) {
    static const struct {
        uint32_t width;
        uint32_t height;
        uint32_t level;
        uint8_t  want[16];
        size_t   len;
    } cases[] = {
        /* byte[1] carries seq_level_idx[3:0]: level 1 -> 0x1A, level 4 -> 0x4A. */
        { 352, 288, 1, { 0x82, 0x1A, 0x88, 0xAF, 0xC7, 0xC0, 0x00, 0x10,
                         0x00, 0x6E, 0x00, 0x20 }, 12 },
        { 1920, 1080, 4, { 0x82, 0x4A, 0xAA, 0xEF, 0xF0, 0xDC, 0x00, 0x01,
                           0x00, 0x06, 0xE0, 0x02 }, 12 },
        { 640, 360, 1, { 0x82, 0x1A, 0x98, 0x9F, 0xEC, 0xE0, 0x00, 0x08,
                         0x00, 0x37, 0x00, 0x10 }, 12 },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        uint8_t buf[64];
        size_t  n = build_payload(cases[i].width, cases[i].height, cases[i].level, buf, sizeof(buf));
        SVT_TEST_CHECK_EQ_INT(n, cases[i].len);
        if (n == cases[i].len) {
            SVT_TEST_CHECK(bytes_eq(buf, cases[i].want, n));
        }
    }
}

/* --- full OBU framing through T003's writer ------------------------------- */

static void test_obu_framing_176x144(void) {
    uint8_t payload[64];
    size_t  payload_size = build_payload(176, 144, 0, payload, sizeof(payload));
    SVT_TEST_CHECK_EQ_INT(payload_size, 12);

    uint8_t          obu[128];
    SvtAv2BitWriter  bw;
    svt_av2_bit_writer_init(&bw, obu, sizeof(obu));
    SVT_TEST_CHECK_EQ_INT(svt_av2_obu_write(&bw, SVT_AV2_OBU_SEQUENCE_HEADER, 0, payload, payload_size),
                          SVT_AV2_BW_OK);

    /* leb128(1 + 12 = 13) = 0x0D; header = (type 1 << 2) = 0x04. */
    static const uint8_t want_head[] = { 0x0D, 0x04, 0x82, 0x0A, 0x77, 0xAF, 0x8F, 0x00,
                                         0x00, 0x40, 0x01, 0xB8, 0x00, 0x80 };
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bytes_written(&bw), sizeof(want_head));
    if (svt_av2_bit_writer_bytes_written(&bw) == sizeof(want_head)) {
        SVT_TEST_CHECK(bytes_eq(obu, want_head, sizeof(want_head)));
    }
}

/* --- configuration defaults and error handling ---------------------------- */

static void test_config_default(void) {
    SvtAv2SequenceHeaderConfig cfg;
    svt_av2_sequence_header_config_default(&cfg, 352, 288);
    SVT_TEST_CHECK_EQ_INT(cfg.seq_header_id, 0);
    SVT_TEST_CHECK_EQ_INT(cfg.seq_profile_idc, SVT_AV2_SEQ_PROFILE_MAIN_420_10_IP0);
    SVT_TEST_CHECK_EQ_INT(cfg.seq_level_idx, 0);
    SVT_TEST_CHECK_EQ_INT(cfg.max_frame_width, 352);
    SVT_TEST_CHECK_EQ_INT(cfg.max_frame_height, 288);
    SVT_TEST_CHECK_EQ_INT(cfg.use_256x256_superblock, 0);
    SVT_TEST_CHECK_EQ_INT(cfg.use_128x128_superblock, 0);
    SVT_TEST_CHECK_EQ_INT(cfg.seq_tile_info_present_flag, 0);
}

static int write_cfg(const SvtAv2SequenceHeaderConfig* cfg) {
    uint8_t         buf[64];
    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
    return svt_av2_sequence_header_write(&bw, cfg);
}

static void test_errors(void) {
    SvtAv2SequenceHeaderConfig cfg;

    svt_av2_sequence_header_config_default(&cfg, 64, 64);

    cfg.seq_header_id = 16; /* MAX_SEQ_NUM is 16 */
    SVT_TEST_CHECK_EQ_INT(write_cfg(&cfg), SVT_AV2_BW_ERROR_RANGE);

    svt_av2_sequence_header_config_default(&cfg, 64, 64);
    cfg.seq_profile_idc = 32;
    SVT_TEST_CHECK_EQ_INT(write_cfg(&cfg), SVT_AV2_BW_ERROR_RANGE);

    svt_av2_sequence_header_config_default(&cfg, 64, 64);
    cfg.seq_level_idx = 22; /* reserved range 22..30 */
    SVT_TEST_CHECK_EQ_INT(write_cfg(&cfg), SVT_AV2_BW_ERROR_RANGE);

    svt_av2_sequence_header_config_default(&cfg, 64, 64);
    cfg.seq_level_idx = 31; /* Maximum parameters is valid */
    SVT_TEST_CHECK_EQ_INT(write_cfg(&cfg), SVT_AV2_BW_OK);

    svt_av2_sequence_header_config_default(&cfg, 0, 64);
    SVT_TEST_CHECK_EQ_INT(write_cfg(&cfg), SVT_AV2_BW_ERROR_INVALID_ARG);

    svt_av2_sequence_header_config_default(&cfg, 64, 64);
    cfg.seq_tile_info_present_flag = 1; /* tile_params() is outside M1 */
    SVT_TEST_CHECK_EQ_INT(write_cfg(&cfg), SVT_AV2_BW_ERROR_UNSUPPORTED);

    /* NULL handling. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_sequence_header_write(NULL, &cfg), SVT_AV2_BW_ERROR_NULL);
    {
        uint8_t         buf[8];
        SvtAv2BitWriter bw;
        svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
        SVT_TEST_CHECK_EQ_INT(svt_av2_sequence_header_write(&bw, NULL), SVT_AV2_BW_ERROR_NULL);
    }
}

static void test_no_space(void) {
    SvtAv2SequenceHeaderConfig cfg;
    svt_av2_sequence_header_config_default(&cfg, 1920, 1080);
    cfg.seq_level_idx = 4;

    /* A buffer too small for the whole header must fail with NO_SPACE. The
     * combination is a sequence of primitive writes, so per contract only each
     * primitive (and svt_av2_obu_write) is atomic; earlier writes may already
     * be committed, so we must not assert bit_count == 0 here. */
    uint8_t         buf[4] = { 0 };
    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
    SVT_TEST_CHECK_EQ_INT(svt_av2_sequence_header_write(&bw, &cfg), SVT_AV2_BW_ERROR_NO_SPACE);

    /* A zero-capacity counting writer rejects the very first primitive, so
     * nothing is committed - the contract's no-partial-write guarantee. */
    SvtAv2BitWriter empty;
    svt_av2_bit_writer_init(&empty, NULL, 0u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_sequence_header_write(&empty, &cfg), SVT_AV2_BW_ERROR_NO_SPACE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&empty), 0);
}

static void test_unaligned_rejected(void) {
    SvtAv2SequenceHeaderConfig cfg;
    svt_av2_sequence_header_config_default(&cfg, 64, 64);

    uint8_t         buf[64];
    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bit(&bw, 1u), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_sequence_header_write(&bw, &cfg), SVT_AV2_BW_ERROR_UNALIGNED);
}

static void test_m3_supported_profile_pairs(void) {
    uint8_t out[128];
    SvtAv2BitWriter bw;
    SvtAv2SequenceHeaderConfig cfg;
    svt_av2_sequence_header_config_default(&cfg, 64u, 64u);
    cfg.bit_depth = 10u;
    svt_av2_bit_writer_init(&bw, out, sizeof(out));
    SVT_TEST_CHECK_EQ_INT(svt_av2_sequence_header_write_multi(&bw, &cfg), SVT_AV2_BW_OK);
    cfg.chroma_format = 3u;
    svt_av2_bit_writer_init(&bw, out, sizeof(out));
    SVT_TEST_CHECK_EQ_INT(svt_av2_sequence_header_write_multi(&bw, &cfg), SVT_AV2_BW_ERROR_UNSUPPORTED);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), 0u);
    cfg.seq_profile_idc = 4u;
    svt_av2_bit_writer_init(&bw, out, sizeof(out));
    SVT_TEST_CHECK_EQ_INT(svt_av2_sequence_header_write_multi(&bw, &cfg), SVT_AV2_BW_OK);
    cfg.bit_depth = 8u;
    svt_av2_bit_writer_init(&bw, out, sizeof(out));
    SVT_TEST_CHECK_EQ_INT(svt_av2_sequence_header_write_multi(&bw, &cfg), SVT_AV2_BW_ERROR_UNSUPPORTED);
    cfg.bit_depth = 12u;
    svt_av2_bit_writer_init(&bw, out, sizeof(out));
    SVT_TEST_CHECK_EQ_INT(svt_av2_sequence_header_write_multi(&bw, &cfg), SVT_AV2_BW_ERROR_UNSUPPORTED);
}

int main(void) {
    test_m3_supported_profile_pairs();
    test_uvlc();
    test_ns();
    test_payload_176x144();
    test_payload_64x64();
    test_payload_other_sizes();
    test_obu_framing_176x144();
    test_config_default();
    test_errors();
    test_no_space();
    test_unaligned_rejected();
    SVT_TEST_REPORT("test_sequence_header");
}
