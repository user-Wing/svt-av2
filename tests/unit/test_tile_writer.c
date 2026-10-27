/*
 * T005 unit tests: minimal CLOSED_LOOP_KEY tile writer and its entropy payload.
 *
 * The expected tile payload bytes were produced by an independent model: the
 * AV2 v1.0.0 spec syntax was traced by hand, the seven CDFs were read directly
 * from the spec attachment tables, and the resulting symbol stream was coded
 * with a port of the reference range coder (avm/avm_dsp/entenc.c). The same
 * bytes are what the read-only AVM oracle (avmdec.exe) accepts as a one-frame
 * 64x64 CLOSED_LOOP_KEY picture, so this test pins the exact bitstream rather
 * than only re-checking internal consistency.
 *
 * Expected tile payload (4 bytes): E6 C8 00 12
 *   E6 C8 00 : is_first_tile_group(1) + 22-bit frame header, byte aligned
 *   12       : range-coded 7 symbols
 *             do_split=0, y_mode_set=0, y_mode_index=0, uv_mode=0,
 *             all_zero(luma)=1, all_zero(U)=1, all_zero(V)=1
 */
#include "bit_writer.h"
#include "block_writer.h"
#include "entropy_encoder.h"
#include "frame_header.h"
#include "tile_writer.h"

#include "test_util.h"

#include <stdint.h>
#include <string.h>

/* The frozen M1 frame header descriptor length, in bits. */
#define SVT_AV2_M1_FRAME_HEADER_BITS SVT_AV2_FRAME_HEADER_M1_BITS

static int bytes_eq(const uint8_t* got, const uint8_t* want, size_t n) {
    return memcmp(got, want, n) == 0;
}

static void test_frame_header_bit_length(void) {
    /* The header alone must consume exactly SVT_AV2_M1_FRAME_HEADER_BITS bits,
     * with no trailing padding. Checked via the bit writer's position. */
    uint8_t             buf[8];
    SvtAv2BitWriter     bw;
    SvtAv2FrameHeaderConfig cfg;
    svt_av2_bit_writer_init(&bw, buf, sizeof(buf));
    svt_av2_frame_header_config_default(&cfg, 64, 64);
    SVT_TEST_CHECK(svt_av2_frame_header_write(&bw, &cfg) == SVT_AV2_BW_OK);
    SVT_TEST_CHECK(svt_av2_bit_writer_bits_written(&bw) ==
                   SVT_AV2_M1_FRAME_HEADER_BITS);
}

static void test_tile_payload_exact_bytes(void) {
    /* Full tile_group_obu() payload for the M1 64x64 CLOSED_LOOP_KEY picture. */
    static const uint8_t expect[4] = {0xE6, 0xC8, 0x00, 0x12};

    SvtAv2FrameHeaderConfig cfg;
    svt_av2_frame_header_config_default(&cfg, 64, 64);

    uint8_t tile[SVT_AV2_TILE_PAYLOAD_MAX_BYTES];
    size_t  tile_size = 0;
    int st = svt_av2_tile_writer_write_closed_loop_key(&cfg, tile, sizeof(tile),
                                                       &tile_size);
    SVT_TEST_CHECK(st == 0);
    SVT_TEST_CHECK(tile_size == sizeof(expect));
    SVT_TEST_CHECK(bytes_eq(tile, expect, sizeof(expect)));
}

static void test_tile_payload_no_space_fails_closed(void) {
    /* A buffer one byte short must fail rather than truncate silently. */
    SvtAv2FrameHeaderConfig cfg;
    svt_av2_frame_header_config_default(&cfg, 64, 64);

    uint8_t tile[3];
    size_t  tile_size = 123; /* poison: must not be read on failure */
    int st = svt_av2_tile_writer_write_closed_loop_key(&cfg, tile, sizeof(tile),
                                                       &tile_size);
    SVT_TEST_CHECK(st != 0);
}

static void test_block_symbols_independent_roundtrip(void) {
    /* The policy layer writes exactly SVT_AV2_BLOCK_MIN_SYMBOLS symbols and the
     * result must match the reference byte 0x12 (the coded form of the seven
     * symbols with the real spec CDFs). */
    /* The M1 block is a fixed 7-symbol payload: three all_zero symbols plus the
     * four do_split/seg/skip/DC symbols, which code to a single byte. Size the
     * scratch explicitly rather than from SVT_AV2_TILE_PAYLOAD_MAX_BYTES, which
     * is documented as the M1 payload bound and not as coder scratch. */
    uint8_t             coded[16];
    uint16_t            precarry[18];
    SvtAv2EntropyEncoder enc;
    svt_av2_entropy_encoder_init(&enc, coded, sizeof(coded), precarry,
                                 sizeof(precarry) / sizeof(precarry[0]));
    SVT_TEST_CHECK(svt_av2_block_write_minimal_intra(&enc, 100) ==
                   SVT_AV2_ENTROPY_OK);

    uint8_t* out = NULL;
    size_t   n   = 0;
    SVT_TEST_CHECK(svt_av2_entropy_encoder_done(&enc, &out, &n) ==
                   SVT_AV2_ENTROPY_OK);
    SVT_TEST_CHECK(n == 1u);
    SVT_TEST_CHECK(n == 1u && out[0] == 0x12u);
}

static void test_block_rejects_wrong_quantizer(void) {
    /* The tabulated all_zero CDFs are for COEFF_CDF_Q_CTXS index 1 (base_q_idx
     * in (90,140]). Any other quantizer must be rejected, not coded with a
     * mismatched CDF. */
    uint8_t             coded[8];
    uint16_t            precarry[8];
    SvtAv2EntropyEncoder enc;
    svt_av2_entropy_encoder_init(&enc, coded, sizeof(coded), precarry, 8);
    SVT_TEST_CHECK(svt_av2_block_write_minimal_intra(&enc, 40) ==
                   SVT_AV2_ENTROPY_ERROR_RANGE);
}

static void test_m5_p_modes_and_inter_metadata(void) {
    SvtAv2FrameHeaderConfig cfg;
    svt_av2_frame_header_config_default(&cfg, 192u, 64u);
    cfg.multi_frame = 1u;
    cfg.inter_frame = 1u;
    cfg.m5_intra_modes = 1u;
    cfg.reduced_tx_set = 2u;
    const int32_t zero[1024] = {0};
    SvtAv2TileCoeffBlock blocks[9];
    memset(blocks, 0, sizeof(blocks));
    for (uint32_t b = 0; b < 3u; ++b) {
        for (uint32_t p = 0; p < 3u; ++p) {
            SvtAv2TileCoeffBlock* block = blocks + b*3u + p;
            block->plane = p;
            block->x = b * (p ? 32u : 64u);
            block->width = block->height = p ? 32u : 64u;
            block->qcoeff = zero;
            block->intra_mode = b == 0u ? 1u : 2u;
        }
    }
    const SvtAv2TileBlockMode modes[3] = {{0u,0,0},{1u,0,0},{0u,0,0}};
    uint8_t first[1024], second[1024];
    size_t first_size = 0u, second_size = 0u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_writer_write_inter_coeffs(&cfg,
        blocks, 9u, modes, 3u, first, sizeof(first), &first_size), 0);
    /* An inter block's intra metadata cannot leak into the following H block. */
    blocks[3].intra_mode = 255u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_writer_write_inter_coeffs(&cfg,
        blocks, 9u, modes, 3u, second, sizeof(second), &second_size), 0);
    SVT_TEST_CHECK(first_size == second_size &&
        memcmp(first, second, first_size) == 0);
    blocks[6].intra_mode = 3u;
    SVT_TEST_CHECK(svt_av2_tile_writer_write_inter_coeffs(&cfg,
        blocks, 9u, modes, 3u, second, sizeof(second), &second_size) != 0);
    /* The disabled M4 path never consumes any intra-mode metadata. */
    cfg.m5_intra_modes = 0u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_writer_write_inter_coeffs(&cfg,
        blocks, 9u, modes, 3u, second, sizeof(second), &second_size), 0);
}

static void test_m52_partition_plan_controls_coeff_order(void) {
    const int32_t zero[1024] = {0};
    const SvtAv2TileBlockMode modes[4] = {{0u,0,0},{1u,0,0},{0u,0,0},{1u,0,0}};
    for (uint32_t format = 0; format < 3u; ++format) {
        SvtAv2FrameHeaderConfig cfg;
        svt_av2_frame_header_config_default(&cfg, 64u, 64u);
        cfg.bit_depth = format ? 10u : 8u;
        cfg.chroma_format = format == 2u ? 3u : 1u;
        cfg.base_q_idx = format ? 148u : 100u;
        cfg.multi_frame = 1u;
        cfg.reduced_tx_set = 2u;
        uint16_t decision = 1u;
        cfg.partition_decisions = &decision;
        cfg.partition_decision_count = 1u;
        SvtAv2TileCoeffBlock blocks[12];
        memset(blocks, 0, sizeof(blocks));
        for (uint32_t q = 0; q < 4u; ++q) {
            for (uint32_t p = 0; p < 3u; ++p) {
                SvtAv2TileCoeffBlock* block = blocks + q*3u + p;
                const uint32_t size = p && format != 2u ? 16u : 32u;
                block->plane = p;
                block->x = (q & 1u) * size;
                block->y = (q >> 1u) * size;
                block->width = block->height = size;
                block->qcoeff = zero;
                block->intra_mode = q & 1u ? 2u : 1u;
            }
        }
        uint8_t out[2048];
        size_t count = 0u;
        for (uint32_t adaptive_mode = 0; adaptive_mode < 2u; ++adaptive_mode) {
            cfg.m5_intra_modes = adaptive_mode;
            cfg.inter_frame = 0u;
            SVT_TEST_CHECK_EQ_INT(svt_av2_tile_writer_write_closed_loop_key_coeffs(
                &cfg, blocks, 12u, out, sizeof(out), &count, NULL), 0);
            cfg.inter_frame = 1u;
            SVT_TEST_CHECK_EQ_INT(svt_av2_tile_writer_write_inter_coeffs(
                &cfg, blocks, 12u, modes, 4u, out, sizeof(out), &count), 0);
        }
        /* Writer must consume the shared plan, never infer a split from coeffs. */
        decision = 0u;
        SVT_TEST_CHECK(svt_av2_tile_writer_write_inter_coeffs(
            &cfg, blocks, 12u, modes, 4u, out, sizeof(out), &count) != 0);
        decision = 2u;
        SVT_TEST_CHECK(svt_av2_tile_writer_write_inter_coeffs(
            &cfg, blocks, 12u, modes, 4u, out, sizeof(out), &count) != 0);
        cfg.partition_decisions = NULL;
        SVT_TEST_CHECK(svt_av2_tile_writer_write_inter_coeffs(
            &cfg, blocks, 12u, modes, 4u, out, sizeof(out), &count) != 0);
    }
}

static void test_m53_supported_qindex_range(void) {
    const int32_t zero[1024] = {0};
    for (uint32_t format = 0; format < 3u; ++format) {
        SvtAv2FrameHeaderConfig cfg;
        svt_av2_frame_header_config_default(&cfg,64u,64u);
        cfg.bit_depth = format ? 10u : 8u;
        cfg.chroma_format = format == 2u ? 3u : 1u;
        cfg.multi_frame = 1u;
        cfg.reduced_tx_set = 2u;
        SvtAv2TileCoeffBlock blocks[3];
        memset(blocks,0,sizeof(blocks));
        for (uint32_t p = 0; p < 3u; ++p) {
            blocks[p].plane = p;
            blocks[p].width = blocks[p].height = p && format != 2u ? 32u : 64u;
            blocks[p].qcoeff = zero;
        }
        const uint32_t valid[] = {format ? 49u : 1u,90u,91u,140u,141u,190u,191u,format ? 303u : 255u};
        uint8_t out[256];
        size_t count = 0;
        for (size_t i = 0; i < sizeof(valid)/sizeof(valid[0]); ++i) {
            cfg.base_q_idx = valid[i];
            SVT_TEST_CHECK_EQ_INT(svt_av2_tile_writer_write_closed_loop_key_coeffs(
                &cfg,blocks,3u,out,sizeof(out),&count,NULL),0);
        }
        cfg.base_q_idx = format ? 48u : 0u;
        SVT_TEST_CHECK(svt_av2_tile_writer_write_closed_loop_key_coeffs(
            &cfg,blocks,3u,out,sizeof(out),&count,NULL) != 0);
        cfg.base_q_idx = format ? 304u : 256u;
        SVT_TEST_CHECK(svt_av2_tile_writer_write_closed_loop_key_coeffs(
            &cfg,blocks,3u,out,sizeof(out),&count,NULL) != 0);
    }
}

int main(void) {
    test_m53_supported_qindex_range();
    test_m52_partition_plan_controls_coeff_order();
    test_m5_p_modes_and_inter_metadata();
    test_frame_header_bit_length();
    test_tile_payload_exact_bytes();
    test_tile_payload_no_space_fails_closed();
    test_block_symbols_independent_roundtrip();
    test_block_rejects_wrong_quantizer();

    SVT_TEST_REPORT("test_tile_writer");
}
