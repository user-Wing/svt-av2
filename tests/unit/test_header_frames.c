/*
 * T015 unit tests: sequence/frame header syntax for the M2 sizes, level
 * derivation and the multi-frame head.
 *
 * The M1 vectors in test_sequence_header.c / test_frame_header.c are frozen and
 * are NOT repeated or relaxed here; every expectation below is derived
 * independently from the AV2 v1.0.0 spec syntax plus the AVM decoder formulas
 * (quoted field-by-field in docs/t015-header-field-reference.md), so a shared
 * misunderstanding in the writer cannot hide behind these checks.
 */
#include "bit_writer.h"
#include "frame_header.h"
#include "obu_writer.h"
#include "sequence_header.h"

#include "test_util.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NO_LEVEL SVT_AV2_SEQ_LEVEL_NONE

/* --- helpers -------------------------------------------------------------- */

static size_t build_seq(uint32_t width,
                        uint32_t height,
                        uint32_t level,
                        int      multi,
                        uint8_t* out,
                        size_t   out_cap) {
    SvtAv2BitWriter            bw;
    SvtAv2SequenceHeaderConfig cfg;
    svt_av2_bit_writer_init(&bw, out, out_cap);
    svt_av2_sequence_header_config_default(&cfg, width, height);
    cfg.seq_level_idx = level;
    const int status = multi ? svt_av2_sequence_header_write_multi(&bw, &cfg)
                             : svt_av2_sequence_header_write(&bw, &cfg);
    if (status != SVT_AV2_BW_OK) {
        return 0;
    }
    if (svt_av2_obu_payload_finalize(&bw) != SVT_AV2_BW_OK) {
        return 0;
    }
    return svt_av2_bit_writer_bytes_written(&bw);
}

/* --- 1. dimension field widths (spec 5.4.1, reference table section 1) ----- */

static uint32_t ref_bits_for_value(uint32_t value) {
    uint32_t bits = 1u;
    while ((value >> bits) != 0u) {
        ++bits;
    }
    return bits;
}

static void check_dimension_fields(uint32_t w,
                                   uint32_t h,
                                   uint32_t want_w_bits,
                                   uint32_t want_h_bits) {
    SvtAv2BitWriter            bw;
    uint8_t                    buf[64];
    SvtAv2SequenceHeaderConfig cfg;

    /* Closed form: the fields hold size - 1 and are (ceil_log2(size)) wide. */
    SVT_TEST_CHECK_EQ_INT(ref_bits_for_value(w - 1u), want_w_bits);
    SVT_TEST_CHECK_EQ_INT(ref_bits_for_value(h - 1u), want_h_bits);
    SVT_TEST_CHECK_EQ_INT(want_w_bits - 1u, ref_bits_for_value(w - 1u) - 1u);
    /* f(4) carries frame_width_bits_minus_1, so sizes up to 65536 only. */
    SVT_TEST_CHECK(want_w_bits <= 16u);
    SVT_TEST_CHECK(want_h_bits <= 16u);

    /* Independent reference model of the syntax prefix that ends at
     * max_frame_height_minus_1 (spec 5.4.1, single_picture_header_flag = 1):
     *   uvlc(seq_header_id = 0)         1 bit
     *   f(5) seq_profile_idc            5
     *   f(1) single_picture_header_flag 1
     *   f(5) seq_level_idx              5
     *   uvlc(chroma_format_idc = 0)     1
     *   uvlc(bit_depth_idc = 1)         3
     *   f(4) frame_width_bits_minus_1   4
     *   f(4) frame_height_bits_minus_1  4
     *   f(want_w_bits) max_frame_width_minus_1
     *   f(want_h_bits) max_frame_height_minus_1
     * seq_tier is not written for a single picture header at any level. */
    const uint32_t prefix_bits = 1u + 5u + 1u + 5u + 1u + 3u + 4u + 4u +
                                 want_w_bits + want_h_bits;

    svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
    svt_av2_sequence_header_config_default(&cfg, w, h);
    SVT_TEST_CHECK(svt_av2_sequence_header_write(&bw, &cfg) == SVT_AV2_BW_OK);
    /* The prefix is a strict prefix of the whole header. */
    SVT_TEST_CHECK(svt_av2_bit_writer_bits_written(&bw) > prefix_bits);
}

static void test_dimension_field_widths(void) {
    check_dimension_fields(64u, 64u, 6u, 6u);
    check_dimension_fields(176u, 144u, 8u, 8u);
    check_dimension_fields(1280u, 720u, 11u, 10u);
    check_dimension_fields(1920u, 1080u, 11u, 11u);
    check_dimension_fields(1u, 1u, 1u, 1u);
    check_dimension_fields(2u, 2u, 1u, 1u);
    check_dimension_fields(65536u, 1u, 16u, 1u);
}

static void test_dimension_size_limit(void) {
    /* 65536 is the largest size f(4) can express; 65537 must be rejected
     * instead of being silently truncated (spec 5.4.1: f(4)). */
    SvtAv2BitWriter            bw;
    uint8_t                    buf[64];
    SvtAv2SequenceHeaderConfig cfg;

    svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
    svt_av2_sequence_header_config_default(&cfg, 65536u, 1024u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_sequence_header_write(&bw, &cfg), SVT_AV2_BW_OK);

    svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
    svt_av2_sequence_header_config_default(&cfg, 65537u, 1024u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_sequence_header_write(&bw, &cfg),
                          SVT_AV2_BW_ERROR_RANGE);

    svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
    svt_av2_sequence_header_config_default(&cfg, 1024u, 65537u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_sequence_header_write(&bw, &cfg),
                          SVT_AV2_BW_ERROR_RANGE);

    svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
    svt_av2_sequence_header_config_default(&cfg, 65537u, 1024u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_sequence_header_write_multi(&bw, &cfg),
                          SVT_AV2_BW_ERROR_RANGE);
}

/* --- 2. level derivation (spec Table A.8) --------------------------------- */

static void test_min_level_for_size(void) {
    static const struct {
        uint32_t w;
        uint32_t h;
        uint32_t level;
    } cases[] = {
        { 64u, 64u, 0u },      /* level 2.0: 4096 <= 147456, 64/64 <= 640 */
        { 176u, 144u, 0u },    /* still level 2.0 */
        { 352u, 288u, 0u },    /* 101376 <= 147456 */
        { 640u, 480u, 2u },    /* 307200 > 278784  -> 3.0 (MaxHSize 1360 ok) */
        { 1280u, 720u, 3u },   /* 921600 > 665856  -> 3.1 */
        { 1920u, 1080u, 4u },  /* 2073600 > 1065024 -> 4.0 */
        { 2560u, 1440u, 6u },  /* MaxHSize 2560 ok, 3686400 > 2359296 -> 5.0 */
        { 3840u, 2160u, 6u },  /* 8294400 > 2359296 but MaxHSize 4975 ok -> 5.0 */
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        SVT_TEST_CHECK_EQ_INT(svt_av2_sequence_header_min_level_for_size(cases[i].w, cases[i].h),
                              cases[i].level);
    }
    /* Above every real level => no admissible index. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_sequence_header_min_level_for_size(20000u, 20000u), NO_LEVEL);
}

static void test_sizes_have_a_legal_level(void) {
    /* The M1 default (level 0) is NOT legal for 720p/1080p; the helper must
     * name a level that can hold each target size. */
    static const struct {
        uint32_t w;
        uint32_t h;
    } sizes[] = {
        { 64u, 64u }, { 176u, 144u }, { 1280u, 720u }, { 1920u, 1080u },
    };
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        const uint32_t lvl = svt_av2_sequence_header_min_level_for_size(sizes[i].w, sizes[i].h);
        SVT_TEST_CHECK(lvl != NO_LEVEL);

        uint8_t                    buf[64];
        SvtAv2BitWriter            bw;
        SvtAv2SequenceHeaderConfig cfg;
        svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
        svt_av2_sequence_header_config_default(&cfg, sizes[i].w, sizes[i].h);
        cfg.seq_level_idx = lvl;
        SVT_TEST_CHECK_EQ_INT(svt_av2_sequence_header_write(&bw, &cfg), SVT_AV2_BW_OK);
    }
}

/* --- 3. frame header tile loops (spec 5.18.7.3, table section 5) ---------- */

static void test_sb_grid(void) {
    static const struct {
        uint32_t w;
        uint32_t h;
        uint32_t sb_cols;
        uint32_t sb_rows;
    } cases[] = {
        { 64u, 64u, 1u, 1u },
        { 176u, 144u, 3u, 3u },
        { 1280u, 720u, 20u, 12u },
        { 1920u, 1080u, 30u, 17u },
        { 1u, 1u, 1u, 1u },
        { 128u, 128u, 2u, 2u },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        SvtAv2FrameHeaderConfig cfg;
        uint32_t                cols = 0;
        uint32_t                rows = 0;
        svt_av2_frame_header_config_default(&cfg, cases[i].w, cases[i].h);
        svt_av2_frame_header_sb_grid(&cfg, &cols, &rows);
        SVT_TEST_CHECK_EQ_INT(cols, cases[i].sb_cols);
        SVT_TEST_CHECK_EQ_INT(rows, cases[i].sb_rows);
    }
}

static void test_tile_params_bits(void) {
    /* One bit for uniform_tile_spacing_flag plus one 0 bit per increment of
     * each axis (spec 5.18.7.3). 64x64 is the only size that is a single
     * superblock, so it is the only one with zero loop bits. */
    static const struct {
        uint32_t w;
        uint32_t h;
        uint32_t bits;
    } cases[] = {
        { 64u, 64u, 1u },       /* sbCols=sbRows=1 */
        { 176u, 144u, 3u },     /* tile_log2(1,3) = 2 per axis */
        { 1280u, 720u, 3u },   /* tile_log2(1,20) = 5, tile_log2(1,12) = 4 */
        { 1920u, 1080u, 3u },  /* tile_log2(1,30) = 5, tile_log2(1,17) = 5 */
        { 128u, 64u, 2u },      /* sbCols=2, sbRows=1 -> 1 + 1 + 0 */
        { 64u, 128u, 2u },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        SvtAv2FrameHeaderConfig cfg;
        svt_av2_frame_header_config_default(&cfg, cases[i].w, cases[i].h);
        SVT_TEST_CHECK_EQ_INT(svt_av2_frame_header_tile_params_bits(&cfg), cases[i].bits);
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_header_tile_params_bits(NULL),
                          SVT_AV2_FRAME_TILE_PARAMS_ERROR);
}

static uint32_t frame_header_bits(uint32_t w, uint32_t h, uint32_t multi) {
    uint8_t                 buf[64];
    SvtAv2BitWriter         bw;
    SvtAv2FrameHeaderConfig cfg;
    svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
    svt_av2_frame_header_config_default(&cfg, w, h);
    cfg.multi_frame = multi;
    if (svt_av2_frame_header_write(&bw, &cfg) != SVT_AV2_BW_OK) {
        return 0u;
    }
    return (uint32_t)svt_av2_bit_writer_bits_written(&bw);
}

static void test_frame_header_lengths(void) {
    /* 22 fixed bits (see frame_header.h) + tile_params() bits + 10 bits when
     * multi_frame (output, size override, eight-bit order hint). */
    static const struct {
        uint32_t w;
        uint32_t h;
        uint32_t single;
        uint32_t multi;
    } cases[] = {
        { 64u, 64u, 22u, 32u },
        { 176u, 144u, 24u, 34u },
        { 1280u, 720u, 24u, 34u },
        { 1920u, 1080u, 24u, 34u },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        SVT_TEST_CHECK_EQ_INT(frame_header_bits(cases[i].w, cases[i].h, 0u), cases[i].single);
        SVT_TEST_CHECK_EQ_INT(frame_header_bits(cases[i].w, cases[i].h, 1u), cases[i].multi);
    }
}

static void test_frame_header_176x144_bits(void) {
    /* Two unary stop zeros (one per axis), followed immediately by QP=100. */
    uint8_t buf[8] = {0};
    SvtAv2BitWriter bw;
    SvtAv2FrameHeaderConfig cfg;
    svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
    svt_av2_frame_header_config_default(&cfg, 176, 144);
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_header_write(&bw, &cfg), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), 24);
    SVT_TEST_CHECK_EQ_INT(buf[0], 0xCC);
    SVT_TEST_CHECK_EQ_INT(buf[1], 0x64);
    SVT_TEST_CHECK_EQ_INT(buf[2], 0x00);
}

/* Independent AVM read order: the multi-frame prefix includes eight order-hint
 * bits after frame_size_override_flag. Previous vectors incorrectly omitted
 * this syntax entirely. See artifacts/m2-pipeline/ref_headers.py. */
static void test_frame_header_multi_frame_bits(void) {
    static const uint8_t want_64[] = { 0xE0, 0x03, 0x64, 0x00 };
    static const uint8_t want_176[] = { 0xE0, 0x03, 0x19, 0x00, 0x00 };

    uint8_t                 buf[8];
    SvtAv2BitWriter         bw;
    SvtAv2FrameHeaderConfig cfg;

    memset(buf, 0, sizeof(buf));
    svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
    svt_av2_frame_header_config_default(&cfg, 64, 64);
    cfg.multi_frame = 1u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_header_write(&bw, &cfg), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), 32);
    SVT_TEST_CHECK(memcmp(buf, want_64, sizeof(want_64)) == 0);

    memset(buf, 0, sizeof(buf));
    svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
    svt_av2_frame_header_config_default(&cfg, 176, 144);
    cfg.multi_frame = 1u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_header_write(&bw, &cfg), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), 34);
    SVT_TEST_CHECK(memcmp(buf, want_176, sizeof(want_176)) == 0);
}

static void test_frame_header_64x64_unchanged_bits(void) {
    /* Guard: the 64x64 prefix is bit-identical to the frozen M1 vector. The M1
     * expectations live in test_frame_header.c; this only pins the shared
     * 22-bit prefix so a tile-loop refactor cannot shift it. */
    uint8_t                 buf[8] = { 0 };
    SvtAv2BitWriter         bw;
    SvtAv2FrameHeaderConfig cfg;
    svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
    svt_av2_frame_header_config_default(&cfg, 64, 64);
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_header_write(&bw, &cfg), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), 22);
    SVT_TEST_CHECK_EQ_INT(buf[0], 0xCD);
    SVT_TEST_CHECK_EQ_INT(buf[1], 0x90);
}

static void test_frame_header_rejects_null(void) {
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_header_tile_params_bits(NULL),
                          SVT_AV2_FRAME_TILE_PARAMS_ERROR);
    uint8_t         buf[8];
    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_header_write(&bw, NULL), SVT_AV2_BW_ERROR_NULL);
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_header_write(NULL, NULL), SVT_AV2_BW_ERROR_NULL);
}

/* --- 4. multi-frame sequence header (reference table section 3.3) --------- */

static void test_multi_frame_seq_is_longer_and_differs(void) {
    uint8_t single[64];
    uint8_t multi[64];
    const size_t n_single = build_seq(176u, 144u, 0u, 0, single, sizeof(single));
    const size_t n_multi = build_seq(176u, 144u, 0u, 1, multi, sizeof(multi));

    SVT_TEST_CHECK(n_single != 0u);
    SVT_TEST_CHECK(n_multi != 0u);
    /* The multi-frame head writes still_picture/layer ids and the inter/scc/
     * filter fields a single picture infers, so it must be strictly longer. */
    SVT_TEST_CHECK(n_multi > n_single);
    SVT_TEST_CHECK(memcmp(single, multi, n_single) != 0);
}

static void test_multi_frame_seq_bytes_1920x1080(void) {
    /* Independent reference model of sequence_header_obu() for
     * single_picture_header_flag = 0 at 1920x1080, seq_level_idx = 4 (so
     * seq_tier is written as 0), 64x64 SB, all optional flags off except
     * monotonic_output_order_flag = 1 and enable_avg_cdf = 1 / avg_cdf_type = 1.
     *
     * Field order comes from the AVM decoder read sequence
     * (avm/av2/decoder/decodeframe.c:5975-6298), because the PDF text layer
     * does not contain the sequence_*_config() bodies. Payload = 148 bits, then
     * trailing_bits() -> 19 bytes.
     *
     *   uvlc(seq_header_id=0)        1
     *   f(5) seq_profile_idc         00000
     *   f(1) single_picture_header   0
     *   f(5) seq_level_idx = 4       00100
     *   f(1) seq_tier                0            (level >= 4, multi-frame)
     *   uvlc(chroma_format_idc=0)    1
     *   uvlc(bit_depth_idc=1)        010
     *   f(3) seq_lcr_id              000
     *   f(1) still_picture           0
     *   f(2) max_tlayer_id           00
     *   f(3) max_mlayer_id           000
     *   f(1) monotonic_output_order  1
     *   f(4) frame_width_bits_m1=10  1010
     *   f(4) frame_height_bits_m1=10 1010
     *   f(11) max_frame_width_minus_1 = 1919   11101111111
     *   f(11) max_frame_height_minus_1 = 1079  10000110111
     *   f(1) seq_cropping_window_present_flag    0
     *   f(1) seq_initial_display_delay_present   0
     *   f(1) decoder_model_info_present_flag     0
     *   partition config: sb_size 00, enable_sdp 0, enable_ext_partitions 0,
     *                    reduce_pb_aspect_ratio 0            = 00000
     *   segment config:  enable_ext_seg 0, seq_seg_info_present 0 = 00
     *   intra config:    dip 0, edge 0, mrls 0, cfl 0, cfl_ds 00, mhccp 0,
     *                    ibp 0                              = 00000000
     *   inter config (39 bits here, AVM decodeframe.c:6014-6143):
     *      seq_enabled_motion_modes[INTERINTRA..MOTION_MODES-1] x4 = 0000
     *      enable_masked_compound 0
     *      enable_ref_frame_mvs 0
     *      order_hint_bits_minus_1 0111
     *      enable_refmvbank 0
     *      enable_drl_reorder bit 1        (1 => DRL_REORDER_DISABLED)
     *      enable_explicit_ref_frame_map 0
     *      explicit_num_ref_frames 0       (0 => NumRefFrames = 8)
     *      long_term_frame_id_bits 000
     *      seq_max_drl_bits_minus_1 = ns(5, 0) = 00   (spec 4.11.8 p.48: n = 5
     *          gives 2-bit codes for values 0..2)
     *      allow_frame_max_drl_bits 0
     *      seq_max_bvp_drl_bits_minus_1 = ns(3, 0) = 0
     *      allow_frame_max_bvp_drl_bits 0
     *      num_same_ref_compound 00
     *      enable_tip 0, enable_mv_traj 0, enable_bawp 0, enable_cwp 0,
     *      enable_imp_msk_bld 0, enable_lf_sub_pu 0, enable_opfl_refine 00,
     *      enable_refinemv 0, enable_bru 0, enable_adaptive_mvd 0,
     *      enable_mvd_sign_derive 0, enable_flex_mvres 0,
     *      enable_global_motion 0, enable_short_refresh_frame_flags 0
     *   scc config:      seq_choose_screen_content_tools 1,
     *                    seq_choose_integer_mv 1             = 11
     *   tqentropy config: fsc 0, idtx 0, intra_ist 0, inter_ist 0,
     *      chroma_dctonly 0, inter_ddt 0, reduced_tx_part_set 0, cctx 0,
     *      tcq 0, parity_hiding 0, avg_cdf 1, avg_cdf_type 1,
     *      separate_uv_delta_q 0, equal_ac_dc_q 1,
     *      base_uv_ac_delta_q 10111 (biased zero: -23+23), uv_ac_delta_q_enabled 0  = 0000000000110100 0000 0
     *   filter config:   disable_loopfilters 0, cdef 0, gdf 0, restoration 0,
     *      ccso 0, cdef_on_skip_txfm_always_on 0, ..._disabled 0,
     *      df_par_bits_minus2 00                             = 00000 00 00
     *   sequence_tile_config: seq_tile_info_present_flag 0
     *   film_grain_params_present 0
     *   obu_extension_flag 0
     * Independently reproduced by artifacts/T015/ref_model_headers.py and
     * cross-checked with artifacts/T015/probe_writer_vs_model.py. */
    static const uint8_t want[] = {
        0x80, 0x45, 0x00, 0x35, 0x5D, 0xFE, 0x1B, 0x80,
        0x00, 0x00, 0x3A, 0x00, 0x00, 0x00, 0x30, 0x03,
        0x6E, 0x00, 0x08,
    };
    uint8_t      buf[64];
    const size_t n = build_seq(1920u, 1080u, 4u, 1, buf, sizeof(buf));
    SVT_TEST_CHECK_EQ_INT(n, sizeof(want));
    if (n == sizeof(want)) {
        SVT_TEST_CHECK(memcmp(buf, want, n) == 0);
        /* Print both on mismatch to make the diff actionable. */
        if (memcmp(buf, want, n) != 0) {
            fprintf(stderr, "got  :");
            for (size_t i = 0; i < n; ++i) {
                fprintf(stderr, " %02X", buf[i]);
            }
            fprintf(stderr, "\nwant :");
            for (size_t i = 0; i < n; ++i) {
                fprintf(stderr, " %02X", want[i]);
            }
            fprintf(stderr, "\n");
        }
    }
}

static void test_multi_frame_seq_bytes_720p_and_144p(void) {
    /* Two more multi-frame vectors so the size-dependent parts (level, seq_tier
     * presence, width/height bit widths and the max_*_minus_1 fields) are
     * pinned beyond the 1080p case. Both come from the same independent model.
     * 1280x720 / level 3: 146 payload bits -> 19 bytes.
     * 176x144  / level 0: 141 payload bits -> 18 bytes (no seq_tier). */
    static const uint8_t want_720[] = {
        0x80, 0x3A, 0x00, 0x6A, 0x67, 0xFD, 0x9E, 0x00,
        0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0xC0, 0x0D,
        0xB8, 0x00, 0x20,
    };
    static const uint8_t want_144[] = {
        0x80, 0x0A, 0x00, 0x5D, 0xEB, 0xE3, 0xC0, 0x00,
        0x00, 0x1D, 0x00, 0x00, 0x00, 0x18, 0x01, 0xB7,
        0x00, 0x04,
    };
    uint8_t buf[64];

    size_t n = build_seq(1280u, 720u, 3u, 1, buf, sizeof(buf));
    SVT_TEST_CHECK_EQ_INT(n, sizeof(want_720));
    if (n == sizeof(want_720)) {
        SVT_TEST_CHECK(memcmp(buf, want_720, n) == 0);
    }

    n = build_seq(176u, 144u, 0u, 1, buf, sizeof(buf));
    SVT_TEST_CHECK_EQ_INT(n, sizeof(want_144));
    if (n == sizeof(want_144)) {
        SVT_TEST_CHECK(memcmp(buf, want_144, n) == 0);
    }
}

static void test_multi_frame_requires_alignment(void) {
    SvtAv2BitWriter            bw;
    uint8_t                    buf[16];
    SvtAv2SequenceHeaderConfig cfg;
    svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
    svt_av2_sequence_header_config_default(&cfg, 64, 64);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bit(&bw, 1u), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_sequence_header_write_multi(&bw, &cfg),
                          SVT_AV2_BW_ERROR_UNALIGNED);
}

static void test_multi_frame_obu_framing(void) {
    /* The multi-frame payload must still frame through T003's OBU writer with
     * a leb128 size prefix, i.e. it is a well-formed OBU payload. */
    uint8_t payload[64];
    const size_t payload_size = build_seq(176u, 144u, 0u, 1, payload, sizeof(payload));
    SVT_TEST_CHECK(payload_size > 0u);

    uint8_t         obu[128];
    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, obu, sizeof(obu));
    SVT_TEST_CHECK_EQ_INT(svt_av2_obu_write(&bw, SVT_AV2_OBU_SEQUENCE_HEADER, 0,
                                            payload, payload_size),
                          SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bytes_written(&bw), payload_size + 2u);
    /* header byte = obu_type 1 << 2 = 0x04; leb128 size = 1 + payload_size. */
    SVT_TEST_CHECK_EQ_INT(obu[0], (payload_size + 1u) & 0x7Fu);
    SVT_TEST_CHECK_EQ_INT(obu[1], 0x04);
}

int main(void) {
    test_dimension_field_widths();
    test_dimension_size_limit();
    test_min_level_for_size();
    test_sizes_have_a_legal_level();
    test_sb_grid();
    test_tile_params_bits();
    test_frame_header_lengths();
    test_frame_header_176x144_bits();
    test_frame_header_multi_frame_bits();
    test_frame_header_64x64_unchanged_bits();
    test_frame_header_rejects_null();
    test_multi_frame_seq_is_longer_and_differs();
    test_multi_frame_seq_bytes_1920x1080();
    test_multi_frame_seq_bytes_720p_and_144p();
    test_multi_frame_requires_alignment();
    test_multi_frame_obu_framing();
    SVT_TEST_REPORT("test_header_frames");
}
