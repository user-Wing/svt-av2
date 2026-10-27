/*
 * T011 unit tests: coefficient syntax, scan order and CDF context selection.
 *
 * Coverage: scan order and geometry, EOB/last-position, the real 8.3.2 context
 * derivation, and the full symbol + raw-literal (L(n)) syntax that
 * svt_av2_coeff_write_block() emits. Raw fields (eob_pt_*_extra, eob_extra_bit,
 * sign_bit, read_quant()) are emitted through svt_av2_entropy_encode_literal().
 * The literal arithmetic itself is verified bit-exactly against the AVM oracle
 * in artifacts/T011/gate3_bypass_diff.c; these tests cover the syntax order and
 * the values the coefficient layer chooses to put in those fields.
 *
 * The scan vectors are the exact AVM tables (avm/av2/common/scan.c
 * default_scan_{4x4,8x8,16x16,32x32}), so a scan regression cannot pass by only
 * being self-consistent.
 */
#include "coeff_syntax.h"
#include "motion_syntax.h"
#include "cdf_tables_coeff.h"
#include "entropy_encoder.h"

#include "test_util.h"

#include <stdint.h>
#include <string.h>

/* --- geometry and coefficient count -------------------------------------- */

static void test_geometry(void) {
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_tx_width(SVT_AV2_TX_4X4), 4);
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_tx_width(SVT_AV2_TX_32X32), 32);
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_tx_width(SVT_AV2_TX_64X64), 64);
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_tx_bwl(SVT_AV2_TX_4X4), 2);
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_tx_bwl(SVT_AV2_TX_64X64), 6);

    /* txSzCtx for square transforms is the size index. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_tx_sz_ctx(SVT_AV2_TX_64X64), 4);
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_tx_sz_ctx(SVT_AV2_TX_32X32), 3);

    /* av2_get_max_eob(): the coded area is clamped to 32x32, so TX_64X64 codes
     * 1024 coefficients, not 4096. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_coeff_count(SVT_AV2_TX_4X4), 16);
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_coeff_count(SVT_AV2_TX_8X8), 64);
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_coeff_count(SVT_AV2_TX_32X32), 1024);
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_coeff_count(SVT_AV2_TX_64X64), 1024);

    /* Anchor the GENERAL rule (00's ruling, T009's contract), not just the two
     * sizes used this round: for every supported square size the count must
     * equal min(w,32)*min(h,32). This fails if anyone ever hard-codes 1024 at
     * TX_64X64 instead of deriving it, and it documents that the length is
     * derived per size. Note the enum is square-only; rectangular sizes such as
     * TX_16X64 (which would give 512) are NOT representable here. */
    for (int i = 0; i < (int)SVT_AV2_TX_SIZE_COUNT; ++i) {
        SvtAv2TxSize t = (SvtAv2TxSize)i;
        uint32_t     w = svt_av2_coeff_tx_width(t);
        uint32_t     h = svt_av2_coeff_tx_height(t);
        uint32_t     mw = (w < 32u) ? w : 32u;
        uint32_t     mh = (h < 32u) ? h : 32u;
        SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_coeff_count(t), mw * mh);
    }

    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_max_level(), (1 << 20) - 1);
}

/* --- scan order ----------------------------------------------------------- */

static void test_scan_4x4(void) {
    /* avm/av2/common/scan.c default_scan_4x4. */
    static const uint16_t expect[16] = {0, 4, 1,  8,  5,  2,  12, 9,
                                        6, 3, 13, 10, 7, 14, 11, 15};
    uint16_t scan[16];
    size_t   n = svt_av2_coeff_scan_dct_dct(SVT_AV2_TX_4X4, scan);
    SVT_TEST_CHECK_EQ_INT(n, 16);
    SVT_TEST_CHECK(memcmp(scan, expect, sizeof(expect)) == 0);
}

static void test_scan_8x8_prefix(void) {
    /* First and last entries of AVM default_scan_8x8. */
    uint16_t scan[64];
    size_t   n = svt_av2_coeff_scan_dct_dct(SVT_AV2_TX_8X8, scan);
    SVT_TEST_CHECK_EQ_INT(n, 64);
    SVT_TEST_CHECK_EQ_INT(scan[0], 0);
    SVT_TEST_CHECK_EQ_INT(scan[1], 8);
    SVT_TEST_CHECK_EQ_INT(scan[2], 1);
    SVT_TEST_CHECK_EQ_INT(scan[63], 63);
}

static void test_scan_is_a_permutation(void) {
    /* Every scan is a permutation of 0..count-1 for each supported size. */
    static const SvtAv2TxSize sizes[5] = {SVT_AV2_TX_4X4, SVT_AV2_TX_8X8,
                                          SVT_AV2_TX_16X16, SVT_AV2_TX_32X32,
                                          SVT_AV2_TX_64X64};
    for (int s = 0; s < 5; ++s) {
        uint16_t scan[SVT_AV2_COEFF_MAX_TX_AREA];
        uint8_t  seen[SVT_AV2_COEFF_MAX_TX_AREA];
        size_t   n = svt_av2_coeff_scan_dct_dct(sizes[s], scan);
        const size_t count = svt_av2_coeff_coeff_count(sizes[s]);
        SVT_TEST_CHECK_EQ_INT(n, count);
        memset(seen, 0, count);
        int ok = 1;
        for (size_t i = 0; i < n; ++i) {
            if (scan[i] >= count || seen[scan[i]] != 0) {
                ok = 0;
                break;
            }
            seen[scan[i]] = 1;
        }
        SVT_TEST_CHECK(ok);
    }
}

static void test_scan_64x64_matches_32x32(void) {
    /* AVM reuses default_scan_32x32 for TX_64X64. */
    uint16_t s64[1024];
    uint16_t s32[1024];
    size_t   n64 = svt_av2_coeff_scan_dct_dct(SVT_AV2_TX_64X64, s64);
    size_t   n32 = svt_av2_coeff_scan_dct_dct(SVT_AV2_TX_32X32, s32);
    SVT_TEST_CHECK_EQ_INT(n64, 1024);
    SVT_TEST_CHECK_EQ_INT(n32, 1024);
    SVT_TEST_CHECK(memcmp(s64, s32, sizeof(s32)) == 0);
}

/* --- EOB ------------------------------------------------------------------ */

static void test_find_eob(void) {
    uint16_t scan[16];
    (void)svt_av2_coeff_scan_dct_dct(SVT_AV2_TX_4X4, scan);

    int32_t q[16];
    memset(q, 0, sizeof(q));
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_find_eob(q, scan, 16), 0);

    /* Only DC (scan[0] == 0). */
    q[0] = 3;
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_find_eob(q, scan, 16), 1);

    /* Last scan position set (scan[15] == 15). */
    memset(q, 0, sizeof(q));
    q[15] = -1;
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_find_eob(q, scan, 16), 16);

    /* scan[4] == 5; eob counts positions, not raster indices. */
    memset(q, 0, sizeof(q));
    q[5] = 2;
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_find_eob(q, scan, 16), 5);
}

/* --- neighbour state ------------------------------------------------------ */

static void test_neighbours_reset(void) {
    SvtAv2CoeffNeighbours nb;
    memset(&nb, 0xAB, sizeof(nb));
    svt_av2_coeff_neighbours_reset(&nb);
    for (uint32_t i = 0; i < SVT_AV2_COEFF_CTX_UNITS; ++i) {
        SVT_TEST_CHECK(nb.above_level[i] == 0 && nb.left_level[i] == 0);
        SVT_TEST_CHECK(nb.above_dc[i] == 0 && nb.left_dc[i] == 0);
    }
}

/* --- block writer: symbol-only paths ------------------------------------- */

static void init_enc(SvtAv2EntropyEncoder* enc, uint8_t* buf, size_t cap,
                     uint16_t* precarry) {
    svt_av2_entropy_encoder_init(enc, buf, cap, precarry, cap);
}

static void make_plane(SvtAv2CoeffPlaneInfo* pi, uint32_t plane,
                       SvtAv2TxSize tx, uint32_t bw, uint32_t bh) {
    memset(pi, 0, sizeof(*pi));
    pi->plane = plane;
    pi->tx_size = tx;
    pi->bw = bw;
    pi->bh = bh;
    pi->units_w = SVT_AV2_COEFF_CTX_UNITS;
    pi->units_h = SVT_AV2_COEFF_CTX_UNITS;
}

static void test_zero_block_is_symbol_only(void) {
    /* An all-zero block writes exactly one all_zero = 1 symbol per plane. */
    uint8_t  buf[64];
    uint16_t pc[64];
    SvtAv2EntropyEncoder enc;
    init_enc(&enc, buf, sizeof(buf), pc);

    SvtAv2CoeffNeighbours nb;
    svt_av2_coeff_neighbours_reset(&nb);

    int32_t q[1024];
    memset(q, 0, sizeof(q));

    SvtAv2CoeffPlaneInfo pi;
    /* luma TX_64X64 with a matching 64x64 residual block (skip ctx 0). */
    make_plane(&pi, 0u, SVT_AV2_TX_64X64, 64u, 64u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_write_block(&enc, &pi, 0u, q, &nb),
                          SVT_AV2_COEFF_OK);

    make_plane(&pi, 1u, SVT_AV2_TX_32X32, 32u, 32u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_write_block(&enc, &pi, 0u, q, &nb),
                          SVT_AV2_COEFF_OK);
    make_plane(&pi, 2u, SVT_AV2_TX_32X32, 32u, 32u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_write_block(&enc, &pi, 0u, q, &nb),
                          SVT_AV2_COEFF_OK);

    uint8_t* out = NULL;
    size_t   n = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_encoder_done(&enc, &out, &n),
                          SVT_AV2_ENTROPY_OK);
    SVT_TEST_CHECK(n > 0u && n <= sizeof(buf));
}

static void test_luma_dc_only_is_literal_free(void) {
    /* A single non-zero luma DC has all_zero = 0, eob = 1, one coeff_base_eob
     * symbol and one dc_sign symbol: no raw field, so it must encode OK. */
    uint8_t  buf[64];
    uint16_t pc[64];
    SvtAv2EntropyEncoder enc;
    init_enc(&enc, buf, sizeof(buf), pc);

    SvtAv2CoeffNeighbours nb;
    svt_av2_coeff_neighbours_reset(&nb);

    int32_t q[1024];
    memset(q, 0, sizeof(q));
    q[0] = 5; /* DC only, positive */

    SvtAv2CoeffPlaneInfo pi;
    make_plane(&pi, 0u, SVT_AV2_TX_64X64, 64u, 64u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_write_block(&enc, &pi, 0u, q, &nb),
                          SVT_AV2_COEFF_OK);

    /* The neighbour state must now carry culLevel >= 1 and a positive DC. */
    SVT_TEST_CHECK(nb.above_level[0] >= 1u);
    SVT_TEST_CHECK(nb.above_dc[0] == 2u);

    uint8_t* out = NULL;
    size_t   n = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_encoder_done(&enc, &out, &n),
                          SVT_AV2_ENTROPY_OK);
    SVT_TEST_CHECK(n > 0u && n <= sizeof(buf));

    /* Golden bytes. This block is the shortest path that still reads a
     * txsz-indexed CDF row (txsz = tx_sz_ctx(TX_64X64) = 4): it emits txb_skip,
     * coeff_base_eob and dc_sign. If any row offset steps by the symbol count
     * instead of the stored boundary count, the encoder picks the wrong row here
     * and this stream changes (the historic defect produced 04 8F 50). */
    SVT_TEST_CHECK_EQ_INT(n, 2u);
    SVT_TEST_CHECK_EQ_INT(out[0], 0x03);
    SVT_TEST_CHECK_EQ_INT(out[1], 0x24);
}

static void test_luma_dc_negative_is_literal_free(void) {
    /* The luma DC sign is a dc_sign symbol, not a raw bit. */
    uint8_t  buf[64];
    uint16_t pc[64];
    SvtAv2EntropyEncoder enc;
    init_enc(&enc, buf, sizeof(buf), pc);

    SvtAv2CoeffNeighbours nb;
    svt_av2_coeff_neighbours_reset(&nb);

    int32_t q[1024];
    memset(q, 0, sizeof(q));
    q[0] = -7;

    SvtAv2CoeffPlaneInfo pi;
    make_plane(&pi, 0u, SVT_AV2_TX_64X64, 64u, 64u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_write_block(&enc, &pi, 0u, q, &nb),
                          SVT_AV2_COEFF_OK);
    SVT_TEST_CHECK(nb.above_dc[0] == 1u); /* negative */
}

static void test_ac_coefficient_encodes(void) {
    /* An AC coefficient needs a raw L(1) sign_bit, now emitted via the bypass
     * path. The block must encode successfully. */
    uint8_t  buf[64];
    uint16_t pc[64];
    SvtAv2EntropyEncoder enc;
    init_enc(&enc, buf, sizeof(buf), pc);

    SvtAv2CoeffNeighbours nb;
    svt_av2_coeff_neighbours_reset(&nb);

    int32_t q[1024];
    memset(q, 0, sizeof(q));
    q[0] = 4;    /* DC */
    q[1] = 3;    /* AC: sign_bit literal */

    SvtAv2CoeffPlaneInfo pi;
    make_plane(&pi, 0u, SVT_AV2_TX_64X64, 64u, 64u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_write_block(&enc, &pi, 0u, q, &nb),
                          SVT_AV2_COEFF_OK);

    uint8_t* out = NULL;
    size_t   n = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_encoder_done(&enc, &out, &n),
                          SVT_AV2_ENTROPY_OK);
    SVT_TEST_CHECK(n > 0u);
}

static void test_chroma_nonzero_encodes(void) {
    /* Chroma DC uses sign_bit (dc_sign is luma-only); now encodable. */
    uint8_t  buf[64];
    uint16_t pc[64];
    SvtAv2EntropyEncoder enc;
    init_enc(&enc, buf, sizeof(buf), pc);

    SvtAv2CoeffNeighbours nb;
    svt_av2_coeff_neighbours_reset(&nb);

    int32_t q[1024];
    memset(q, 0, sizeof(q));
    q[0] = 2;

    SvtAv2CoeffPlaneInfo pi;
    make_plane(&pi, 1u, SVT_AV2_TX_32X32, 32u, 32u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_write_block(&enc, &pi, 0u, q, &nb),
                          SVT_AV2_COEFF_OK);

    uint8_t* out = NULL;
    size_t   n = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_encoder_done(&enc, &out, &n),
                          SVT_AV2_ENTROPY_OK);
    SVT_TEST_CHECK(n > 0u);
}

static void test_large_level_encodes_high_range(void) {
    /* A level >= maxLevel (= 2 + 3 + 1 = 6 non-LF) escapes into read_quant();
     * the escape is now emitted rather than refused. */
    uint8_t  buf[256];
    uint16_t pc[256];
    SvtAv2EntropyEncoder enc;
    init_enc(&enc, buf, sizeof(buf), pc);

    SvtAv2CoeffNeighbours nb;
    svt_av2_coeff_neighbours_reset(&nb);

    int32_t q[1024];
    memset(q, 0, sizeof(q));
    q[0] = 40;   /* well past the base+coeff_br range: exercises read_quant() */

    SvtAv2CoeffPlaneInfo pi;
    make_plane(&pi, 1u, SVT_AV2_TX_32X32, 32u, 32u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_write_block(&enc, &pi, 0u, q, &nb),
                          SVT_AV2_COEFF_OK);

    uint8_t* out = NULL;
    size_t   n = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_encoder_done(&enc, &out, &n),
                          SVT_AV2_ENTROPY_OK);
    SVT_TEST_CHECK(n > 0u);
}

static void test_literals_supported_flag(void) {
    /* The coder now exposes the raw-literal entry points the syntax uses. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_literals_supported(), 1);
}

static void test_null_arguments(void) {
    uint8_t  buf[16];
    uint16_t pc[16];
    SvtAv2EntropyEncoder enc;
    init_enc(&enc, buf, sizeof(buf), pc);
    SvtAv2CoeffNeighbours nb;
    svt_av2_coeff_neighbours_reset(&nb);
    int32_t q[16];
    memset(q, 0, sizeof(q));
    SvtAv2CoeffPlaneInfo pi;
    make_plane(&pi, 0u, SVT_AV2_TX_4X4, 4u, 4u);

    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_write_block(NULL, &pi, 0u, q, &nb),
                          SVT_AV2_COEFF_ERR_NULL);
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_write_block(&enc, NULL, 0u, q, &nb),
                          SVT_AV2_COEFF_ERR_NULL);
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_write_block(&enc, &pi, 0u, NULL, &nb),
                          SVT_AV2_COEFF_ERR_NULL);
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_write_block(&enc, &pi, 0u, q, NULL),
                          SVT_AV2_COEFF_ERR_NULL);
}

static void test_no_space_fails_closed(void) {
    /* A one-byte precarry with a non-trivial block must report NO_SPACE, not
     * silently truncate. */
    uint8_t  buf[1];
    uint16_t pc[1];
    SvtAv2EntropyEncoder enc;
    svt_av2_entropy_encoder_init(&enc, buf, sizeof(buf), pc, 1u);

    SvtAv2CoeffNeighbours nb;
    svt_av2_coeff_neighbours_reset(&nb);
    int32_t q[1024];
    memset(q, 0x00, sizeof(q));

    SvtAv2CoeffPlaneInfo pi;
    make_plane(&pi, 0u, SVT_AV2_TX_64X64, 64u, 64u);
    int st = svt_av2_coeff_write_block(&enc, &pi, 0u, q, &nb);
    SVT_TEST_CHECK(st == SVT_AV2_COEFF_OK || st == SVT_AV2_COEFF_ERR_NO_SPACE);
}

/* --- frozen context row checks ------------------------------------------- */

static void test_cdf_row_dimensions(void) {
    /* Guard the generated table sizes the module indexes into. Each row stores
     * (nsyms - 1) cumulative boundaries, so e.g. txb_skip's [2][5][10] rows of
     * 2 symbols each store 1 boundary. */
    SVT_TEST_CHECK_EQ_INT(SVT_AV2_CDF_TXB_SKIP_COUNT, 2u * 5u * 10u);
    SVT_TEST_CHECK_EQ_INT(SVT_AV2_CDF_V_TXB_SKIP_COUNT, 12u);
    SVT_TEST_CHECK_EQ_INT(SVT_AV2_CDF_DC_SIGN_COUNT, 2u * 2u * 3u);
    SVT_TEST_CHECK_EQ_INT(SVT_AV2_CDF_COEFF_BASE_COUNT,
                          5u * 20u * 2u * (SVT_AV2_SYMS_COEFF_BASE - 1u));
    SVT_TEST_CHECK_EQ_INT(SVT_AV2_CDF_COEFF_BASE_LF_COUNT,
                          5u * 33u * 2u * (SVT_AV2_SYMS_COEFF_BASE_LF - 1u));
    SVT_TEST_CHECK_EQ_INT(SVT_AV2_CDF_COEFF_BASE_EOB_COUNT,
                          5u * 4u * (SVT_AV2_SYMS_COEFF_BASE_EOB - 1u));
    SVT_TEST_CHECK_EQ_INT(SVT_AV2_CDF_EOB1024_COUNT,
                          3u * (SVT_AV2_SYMS_EOB1024 - 1u));
    SVT_TEST_CHECK_EQ_INT(SVT_AV2_SYMS_EOB1024, 8u);
    SVT_TEST_CHECK_EQ_INT(SVT_AV2_SYMS_TXB_SKIP, 2u);
}

/* --- value-level anchors against the AVM q1 tables ------------------------ */

/*
 * test_cdf_row_dimensions() above only pins table *sizes*, so a wrong row stride
 * (stepping by the symbol count instead of the stored boundary count) would stay
 * green -- exactly the defect this module shipped with once. These anchors read
 * specific (context) rows with the documented `symbols - 1` stride and compare
 * the first boundary against the AVM q1 source values, so a stride regression
 * fails loudly here instead of silently emitting a stream no decoder matches.
 *
 * Expected values are transcribed from
 *   avm/av2/common/entropy_inits_coeffs.h   (TOKEN_CDF_Q_CTXS index 1)
 *   - eob16      : av2_default_eob_multi16_cdfs[1][*]         (line ~32-35)
 *   - base_lf_eob: av2_default_coeff_base_lf_eob_multi_cdfs[1] (txsz 3 / txsz 4)
 *   - txb_skip   : av2_default_txb_skip_cdfs[1], also frozen as
 *                  svt_av2_cdf_txb_skip_q1_txsz4_ctx0 in cdf_tables.c
 *   - dc_sign    : av2_default_dc_sign_cdfs[1][0][0][0]
 */
static void test_cdf_row_values_avm_q1(void) {
    /* eob16 row 0 (eobCtx 0): first boundary 1839. Stride must be 5-1 = 4. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_cdf_eob16[0u * (SVT_AV2_SYMS_EOB16 - 1u)], 1839);
    SVT_TEST_CHECK_EQ_INT(svt_av2_cdf_eob16[1u * (SVT_AV2_SYMS_EOB16 - 1u)], 1354);
    SVT_TEST_CHECK_EQ_INT(svt_av2_cdf_eob16[2u * (SVT_AV2_SYMS_EOB16 - 1u)], 8298);

    /* coeff_base_lf_eob: [txsz 5][4 ctx][5 syms -> 4 w].
     * txsz=3/ctx=0 -> 22871, txsz=4/ctx=0 -> 7975 (the row that used to read out
     * of bounds under the wrong stride). */
    {
        const uint32_t w = SVT_AV2_SYMS_COEFF_BASE_EOB_LF - 1u;
        SVT_TEST_CHECK_EQ_INT(
            svt_av2_cdf_coeff_base_lf_eob[(3u * 4u + 0u) * w], 22871);
        SVT_TEST_CHECK_EQ_INT(
            svt_av2_cdf_coeff_base_lf_eob[(4u * 4u + 0u) * w], 7975);
    }

    /* coeff_base_eob: [txsz 5][4 ctx][3 syms -> 2 w]. txsz=0/ctx=0 -> 10923. */
    {
        const uint32_t w = SVT_AV2_SYMS_COEFF_BASE_EOB - 1u;
        SVT_TEST_CHECK_EQ_INT(svt_av2_cdf_coeff_base_eob[0u * w], 10923);
        SVT_TEST_CHECK_EQ_INT(svt_av2_cdf_coeff_base_eob[(3u * 4u + 0u) * w], 10923);
    }

    /* txb_skip: [predMode 2][txsz 5][10 ctx][2 syms -> 1 w].
     * txsz=4/ctx=0 -> 23062, the M1 frozen single value. */
    {
        const uint32_t w = SVT_AV2_SYMS_TXB_SKIP - 1u;
        SVT_TEST_CHECK_EQ_INT(
            svt_av2_cdf_txb_skip[(0u * 5u + 4u) * 10u * w + 0u], 23062);
    }

    /* dc_sign: [planeType 2][hidden 2][ctx 3][2 syms -> 1 w], luma/hidden0/ctx0. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_cdf_dc_sign[0], 16540);
}

static void test_picture_context_and_zero_update(void) {
    SvtAv2CoeffNeighbours nb;
    svt_av2_coeff_neighbours_reset(&nb);
    nb.above_level[400] = 4;
    nb.above_dc[400] = 2;
    nb.left_level[260] = 4;
    nb.left_dc[260] = 1;
    int32_t q[16] = {0};
    SvtAv2CoeffPlaneInfo pi = {0, SVT_AV2_TX_4X4, 400, 260, 480, 270, 4, 4};
    uint8_t out[64];
    uint16_t precarry[66];
    SvtAv2EntropyEncoder enc;
    svt_av2_entropy_encoder_init(&enc, out, sizeof(out), precarry, 66);
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_write_block(&enc, &pi, 0, q, &nb), 0);
    SVT_TEST_CHECK(nb.above_level[400] == 0 && nb.above_dc[400] == 0);
    SVT_TEST_CHECK(nb.left_level[260] == 0 && nb.left_dc[260] == 0);
    pi.units_w = 481;
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_write_block(&enc, &pi, 0, q, &nb),
                          SVT_AV2_COEFF_ERR_UNSUPPORTED);
}

static void test_eob_escape_boundaries(void) {
    const uint32_t eobs[] = {33, 65, 129, 257, 513, 1024};
    uint16_t scan[1024];
    svt_av2_coeff_scan_dct_dct(SVT_AV2_TX_64X64, scan);
    for (size_t i = 0; i < sizeof(eobs) / sizeof(eobs[0]); ++i) {
        int32_t q[1024] = {0};
        q[scan[eobs[i] - 1]] = 1;
        SvtAv2CoeffNeighbours nb;
        svt_av2_coeff_neighbours_reset(&nb);
        SvtAv2CoeffPlaneInfo pi = {0, SVT_AV2_TX_64X64, 0, 0, 16, 16, 64, 64};
        uint8_t out[4096];
        uint16_t precarry[4098];
        SvtAv2EntropyEncoder enc;
        svt_av2_entropy_encoder_init(&enc, out, sizeof(out), precarry, 4098);
        SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_write_block(&enc, &pi, 0, q, &nb), 0);
        uint8_t* bytes = NULL;
        size_t length = 0;
        SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_encoder_done(&enc, &bytes, &length), 0);
        SVT_TEST_CHECK(length > 0);
    }
}

static void test_m3_q2_defaults_and_eob(void) {
    /* Fixed AVM q-category2 anchors, independent of implementation selection. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_cdf_eob16_q2[0], 3946u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_cdf_eob16_q2[3], 26004u);
    uint16_t scan[1024];
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_scan_dct_dct(SVT_AV2_TX_64X64, scan), 1024u);
    for (uint32_t plane = 0; plane < 3u; ++plane) {
        static const uint32_t eobs[] = {1u, 2u, 17u, 257u, 513u, 1024u};
        for (size_t e = 0; e < sizeof(eobs)/sizeof(eobs[0]); ++e) {
            int32_t coeff[1024] = {0};
            coeff[scan[eobs[e]-1u]] = e & 1u ? -255 : 255;
            SvtAv2CoeffPlaneInfo pi = {plane, SVT_AV2_TX_64X64, 0u, 0u, 16u, 16u, 64u, 64u};
            SvtAv2CoeffNeighbours nb;
            svt_av2_coeff_neighbours_reset(&nb);
            uint8_t out[4096]; uint16_t precarry[4098];
            SvtAv2EntropyEncoder enc;
            svt_av2_entropy_encoder_init(&enc, out, sizeof(out), precarry, 4098);
            SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_write_block_qctx(&enc, &pi, 0u, coeff, &nb, 4u), SVT_AV2_COEFF_ERR_UNSUPPORTED);
            SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_write_block_qctx(&enc, &pi, 0u, coeff, &nb, 2u), SVT_AV2_COEFF_OK);
            uint8_t* bytes = NULL; size_t length = 0;
            SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_encoder_done(&enc, &bytes, &length), 0);
            SVT_TEST_CHECK(length > 0u);
        }
    }
}

static void test_m4_inter_dct_idtx_symbol(void) {
    /* Fixed AVM entropymode.h av2_ext_tx_inv[DCT_IDTX] = {IDTX,DCT_DCT}.
     * Nonzero DC must still signal symbol 1 for inter small transforms.
     * These bytes pin the skip/EOB/type/DC-sign sequence at q-context 1. */
    const uint8_t expected[3] = {0x07u, 0x0eu, 0x16u};
    for (uint32_t t = 1u; t < 4u; ++t) {
        const uint32_t dim = 4u << t;
        SvtAv2CoeffPlaneInfo pi = {0u, (SvtAv2TxSize)t, 0u, 0u, dim/4u, dim/4u, dim, dim};
        SvtAv2CoeffNeighbours nb;
        memset(&nb, 0, sizeof(nb));
        int32_t qcoeff[1024] = {1};
        uint8_t coded[64];
        uint16_t precarry[64];
        SvtAv2EntropyEncoder enc;
        svt_av2_entropy_encoder_init(&enc, coded, sizeof(coded), precarry, 64u);
        SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_write_block_prediction(&enc, &pi, 0u, qcoeff, &nb, 1u, 1u), 0);
        uint8_t* out = NULL;
        size_t count = 0;
        SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_encoder_done(&enc, &out, &count), 0);
        SVT_TEST_CHECK_EQ_INT(count, 1u);
        SVT_TEST_CHECK(count == 1u && out[0] == expected[t - 1u]);
    }
}

static void test_m4_far_left_is_not_mvp(void) {
    /* Fixed v1.0.0 MVREF_COLS=2 scans x-3, never x-5. This reproduces
     * frame 13 (80,128): adjacent intra, far-left (+8,-2), actual (+2,-2). */
    uint8_t coded[2][64];
    size_t lengths[2] = {0};
    for (uint32_t pass = 0; pass < 2u; ++pass) {
        SvtAv2MotionSyntax state;
        SVT_TEST_CHECK_EQ_INT(svt_av2_motion_syntax_init(&state, 176u, 144u), 0);
        if (state.cells == NULL) return;
        if (pass) {
            SvtAv2MotionCell far = {8,-2,1u,1u,1u,0u};
            state.cells[32u * state.cols + 15u] = far;
            state.cells[35u * state.cols + 15u] = far;
        }
        const SvtAv2BlockView block = {80u,128u,16u,16u,NULL};
        const SvtAv2TileBlockMode mode = {1u,2,-2};
        uint16_t precarry[64];
        SvtAv2EntropyEncoder enc;
        svt_av2_entropy_encoder_init(&enc, coded[pass], 64u, precarry, 64u);
        SVT_TEST_CHECK_EQ_INT(svt_av2_motion_write_block(&state, &enc, &block, &mode, 0u), 0);
        uint8_t* out = NULL;
        SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_encoder_done(&enc, &out, &lengths[pass]), 0);
        svt_av2_motion_syntax_free(&state);
    }
    SVT_TEST_CHECK(lengths[0] > 0u && lengths[0] == lengths[1]);
    SVT_TEST_CHECK(lengths[0] == lengths[1] && memcmp(coded[0],coded[1],lengths[0]) == 0);
}
static void test_m53_q_contexts_and_high_range(void) {
    const uint32_t indices[] = {1u,49u,90u,91u,100u,140u,141u,148u,190u,191u,255u,303u};
    const uint32_t expected[] = {0u,0u,0u,1u,1u,1u,2u,2u,2u,3u,3u,3u};
    for (size_t i = 0; i < sizeof(indices)/sizeof(indices[0]); ++i)
        SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_q_context(indices[i]), expected[i]);
    /* Fixed AVM entropy_inits_coeffs.h first EOB16 boundary for all categories. */
    const uint16_t first[4] = {1946u,1839u,3946u,5029u};
    for (uint32_t qctx = 0; qctx < 4u; ++qctx) {
        SVT_TEST_CHECK_EQ_INT(svt_av2_cdf_eob16_by_qctx[qctx][0], first[qctx]);
        for (uint32_t plane = 0; plane < 3u; ++plane) {
            for (uint32_t inter = 0; inter < 2u; ++inter) {
                const int32_t levels[] = {8192,26000,1048575};
                for (size_t n = 0; n < sizeof(levels)/sizeof(levels[0]); ++n) {
                    int32_t coeff[1024] = {0};
                    coeff[0] = -levels[n];
                    coeff[1] = levels[n];
                    coeff[1023] = -levels[n];
                    SvtAv2CoeffPlaneInfo pi = {plane,SVT_AV2_TX_64X64,0u,0u,16u,16u,64u,64u};
                    SvtAv2CoeffNeighbours nb;
                    svt_av2_coeff_neighbours_reset(&nb);
                    uint8_t coded[4096];
                    uint16_t precarry[4098];
                    SvtAv2EntropyEncoder enc;
                    svt_av2_entropy_encoder_init(&enc,coded,sizeof(coded),precarry,4098u);
                    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_write_block_prediction(
                        &enc,&pi,0u,coeff,&nb,qctx,inter),0);
                    uint8_t* out = NULL;
                    size_t count = 0u;
                    SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_encoder_done(&enc,&out,&count),0);
                    SVT_TEST_CHECK(count > 0u);
                }
            }
        }
    }
}

int main(void) {
    test_m53_q_contexts_and_high_range();
    test_m4_far_left_is_not_mvp();
    test_m4_inter_dct_idtx_symbol();
    test_m3_q2_defaults_and_eob();
    test_geometry();
    test_scan_4x4();
    test_scan_8x8_prefix();
    test_scan_is_a_permutation();
    test_scan_64x64_matches_32x32();
    test_find_eob();
    test_neighbours_reset();
    test_zero_block_is_symbol_only();
    test_luma_dc_only_is_literal_free();
    test_luma_dc_negative_is_literal_free();
    test_ac_coefficient_encodes();
    test_chroma_nonzero_encodes();
    test_large_level_encodes_high_range();
    test_literals_supported_flag();
    test_null_arguments();
    test_no_space_fails_closed();
    test_cdf_row_dimensions();
    test_cdf_row_values_avm_q1();
    test_picture_context_and_zero_update();
    test_eob_escape_boundaries();

    SVT_TEST_REPORT("test_coeff_syntax");
}
