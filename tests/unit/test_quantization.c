/*
 * T010 unit tests: quantization / dequantization.
 *
 * The tests come in three layers, deliberately kept separate so a failure tells
 * you which layer broke:
 *
 *   1. Reference-table tests. Every constant (ac_qlookup_QTX, qlookup, the
 *      per-plane DC/AC quantizers, the dequant clip bounds) is compared against
 *      values independently derived from AVM and written out in full here. These
 *      are NOT round-trips: they pin absolute numbers.
 *   2. Decoder-path tests. The dequantizer is checked against the decoder
 *      formula transcribed term by term, including rounding, sign and clipping.
 *   3. Property tests over fixed-seed random blocks, plus the boundary cases the
 *      task file requires: zero blocks, positive/negative DC/AC, the
 *      quantization threshold neighbourhood, the largest legal magnitude, Y/U/V
 *      differences, rounding sign, and dequantization overflow.
 *
 * A forward->inverse round trip is never used as the sole correctness criterion
 * (docs/m1-m2-contracts.md section 3 forbids it).
 */
#include "test_util.h"

#include <stdint.h>
#include <string.h>

#include "quantization.h"

/* --- 1. reference tables --------------------------------------------------- */

/*
 * ac_qlookup_QTX[25], transcribed independently from
 * avm/av2/common/quant_common.c. Written out here rather than read from the
 * module under test so that a transcription error in either place shows up as a
 * mismatch.
 */
static const uint16_t k_expected_qlookup[25] = {
    64, 40, 41, 43, 44, 45, 47, 48, 49, 51, 52, 54, 55,
    57, 59, 60, 62, 64, 66, 68, 70, 72, 74, 76, 78
};

/* Hand-computed qlookup() results: q 0..24 is the table, q >= 25 is
 * table[((q-1)%24)+1] << ((q-1)/24). */
static const struct {
    int q;
    int32_t expected;
} k_expected_qlookup_values[] = {
    { 0, 64 },    { 1, 40 },    { 2, 41 },    { 12, 55 },   { 23, 76 },   { 24, 78 },
    /* 25..48 use the 2^1 row: table[((q-1)%24)+1] << 1 */
    { 25, 80 },   { 26, 82 },   { 36, 110 },  { 47, 152 },  { 48, 156 },
    /* 49..72 use the 2^2 row */
    { 49, 160 },  { 50, 164 },  { 72, 312 },
    /* 73..96 use the 2^3 row */
    { 73, 320 },  { 96, 624 },
    /* 97..120 use the 2^4 row -- the frozen QP lives here */
    { 97, 640 },  { 98, 656 },  { 99, 688 },  { 100, 704 }, { 101, 720 },
    /* top of the 8-bit range: ((255-1)/24) = 10 */
    { 255, 61440 },
};

/* The frozen per-plane quantizer at base_q_idx = 100. */
static const int32_t k_frozen_q100 = 704;

static void test_qlookup_table_matches_reference(void) {
    const uint16_t* table = svt_av2_quant_qlookup_table();
    SVT_TEST_CHECK(table != NULL);
    for (int i = 0; i < 25; ++i) {
        SVT_TEST_CHECK_EQ_INT(table[i], k_expected_qlookup[i]);
    }
}

static void test_qlookup_values(void) {
    for (size_t i = 0; i < sizeof(k_expected_qlookup_values) / sizeof(k_expected_qlookup_values[0]); ++i) {
        const int q = k_expected_qlookup_values[i].q;
        const int32_t expected = k_expected_qlookup_values[i].expected;
        if (svt_av2_quant_qlookup(q) != expected) {
            fprintf(stderr, "FAIL qlookup(%d) = %d, expected %d\n", q, svt_av2_quant_qlookup(q), expected);
            ++g_svt_av2_test_failures;
        }
        ++g_svt_av2_test_checks;
    }

    /* Outside the supported lookup range: rejected, not silently clamped. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_qlookup(-1), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_qlookup(304), 0);
}

/* Walk the whole legal 8-bit qindex range and compare against a reimplementation
 * of the AVM recurrence. This catches an off-by-one in the periodic extension
 * that a handful of spot checks would miss. */
static void test_qlookup_full_range(void) {
    int mismatches = 0;
    for (int q = 0; q <= 303; ++q) {
        int32_t expected;
        if (q < 25) {
            expected = k_expected_qlookup[q];
        } else {
            expected = (int32_t)k_expected_qlookup[((q - 1) % 24) + 1] << ((q - 1) / 24);
        }
        if (svt_av2_quant_qlookup(q) != expected) {
            ++mismatches;
        }
    }
    SVT_TEST_CHECK_EQ_INT(mismatches, 0);
}

/* --- 2. parameter setup ---------------------------------------------------- */

static void test_frozen_params(void) {
    SvtAv2QuantParams params;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init(&params, 100, 0), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(params.base_q_idx, 100);
    SVT_TEST_CHECK_EQ_INT(params.bit_depth, SVT_AV2_QUANT_BIT_DEPTH_8);
    SVT_TEST_CHECK_EQ_INT(params.tx_scale, 0);
    for (int plane = 0; plane < 3; ++plane) {
        SVT_TEST_CHECK_EQ_INT(params.plane[plane].dc, k_frozen_q100);
        SVT_TEST_CHECK_EQ_INT(params.plane[plane].ac, k_frozen_q100);
    }
}

static void test_params_reject_outside_mapping(void) {
    /* Reject lossless and indices outside the 8-bit CQP mapping. */
    const uint32_t rejected[] = { 0, 256, 303, UINT32_MAX };
    SvtAv2QuantParams params;
    for (size_t i = 0; i < sizeof(rejected) / sizeof(rejected[0]); ++i) {
        if (svt_av2_quant_params_init(&params, rejected[i], 0) != SVT_AV2_QUANT_ERR_UNSUPPORTED) {
            fprintf(stderr, "FAIL base_q_idx=%u was not rejected\n", rejected[i]);
            ++g_svt_av2_test_failures;
        }
        ++g_svt_av2_test_checks;
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init(NULL, 100, 0), SVT_AV2_QUANT_ERR_NULL);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init(&params, 100, 3), SVT_AV2_QUANT_ERR_UNSUPPORTED);
}

/* av2_get_tx_scale(): pels > 256 -> 1, pels > 1024 -> 2. */
static void test_tx_scale_derivation(void) {
    SvtAv2QuantParams params;
    const struct {
        uint32_t w, h;
        int expected;
    } cases[] = {
        { 4, 4, 0 },     { 4, 8, 0 },    { 8, 8, 0 },    { 8, 16, 0 },
        { 16, 16, 0 },   { 16, 32, 1 },  { 32, 16, 1 },  { 16, 64, 1 },
        { 32, 32, 1 },   { 32, 64, 2 },  { 64, 32, 2 },  { 64, 64, 2 },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init_tx(&params, 100, cases[i].w, cases[i].h), SVT_AV2_QUANT_OK);
        if (params.tx_scale != cases[i].expected) {
            fprintf(stderr, "FAIL tx_scale(%ux%u) = %d, expected %d\n",
                    cases[i].w, cases[i].h, params.tx_scale, cases[i].expected);
            ++g_svt_av2_test_failures;
        }
        ++g_svt_av2_test_checks;
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init_tx(&params, 100, 0, 8), SVT_AV2_QUANT_ERR_UNSUPPORTED);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init_tx(&params, 100, 8, 0), SVT_AV2_QUANT_ERR_UNSUPPORTED);
}

/* --- 3. dequantization: decoder reference path ----------------------------- */

/* Reimplementation of the decoder's expression, written from decodetxb.c rather
 * than calling into the module, so agreement is evidence and not a tautology. */
static int32_t decoder_reference_dequant(int32_t level, int32_t dequant, int shift, int bd) {
    const int64_t magnitude = level < 0 ? -(int64_t)level : level;
    const int64_t hp = (magnitude * dequant) % 16777216;
    int32_t dq = (int32_t)((hp + 4) / 8) / (1 << shift);
    if (level < 0) dq = -dq;
    const int32_t min_v = -(int32_t)((uint32_t)1 << (7 + bd));
    const int32_t max_v = (int32_t)(((uint32_t)1 << (7 + bd)) - 1);
    if (dq < min_v) {
        dq = min_v;
    } else if (dq > max_v) {
        dq = max_v;
    }
    return dq;
}

static void test_dequant_clip_bounds(void) {
    /* 8-bit: min = -(1 << 15), max = (1 << 15) - 1. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_dequant_clip_min(SVT_AV2_QUANT_BIT_DEPTH_8), -32768);
    SVT_TEST_CHECK_EQ_INT(svt_av2_dequant_clip_max(SVT_AV2_QUANT_BIT_DEPTH_8), 32767);
    SVT_TEST_CHECK_EQ_INT(svt_av2_dequant_clip_min(SVT_AV2_QUANT_BIT_DEPTH_10), -131072);
    SVT_TEST_CHECK_EQ_INT(svt_av2_dequant_clip_max(SVT_AV2_QUANT_BIT_DEPTH_10), 131071);
    SVT_TEST_CHECK_EQ_INT(svt_av2_dequant_clip_min(SVT_AV2_QUANT_BIT_DEPTH_12), -524288);
    SVT_TEST_CHECK_EQ_INT(svt_av2_dequant_clip_max(SVT_AV2_QUANT_BIT_DEPTH_12), 524287);
}

/* Exact hand-computed dequant values at q = 704, QUANT_TABLE_BITS = 3.
 *   dq = ROUND(level * 704 / 8) = ROUND(level * 88)
 *   then >> tx_scale and clipped to [-32768, 32767]. */
static void test_dequant_exact_values(void) {
    SvtAv2QuantParams params;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init(&params, 100, 0), SVT_AV2_QUANT_OK);

    /* level * 704 = level * 88 * 8, so ROUND(./8) is exact: 88 * level. */
    const struct {
        int32_t level;
        int32_t expected;
    } cases[] = {
        { 0, 0 },       { 1, 88 },       { -1, -88 },     { 2, 176 },      { -2, -176 },
        { 3, 264 },     { 7, 616 },      { -7, -616 },    { 10, 880 },     { -10, -880 },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const int32_t got = svt_av2_dequant_coeff(&params, cases[i].level, 0, 1);
        if (got != cases[i].expected) {
            fprintf(stderr, "FAIL dequant(%d) = %d, expected %d\n", cases[i].level, got, cases[i].expected);
            ++g_svt_av2_test_failures;
        }
        ++g_svt_av2_test_checks;

        /* Decoder reference agreement for the same point. */
        SVT_TEST_CHECK_EQ_INT(got, decoder_reference_dequant(cases[i].level, k_frozen_q100, 0, 8));
    }
}

/* Rounding sign: ROUND_POWER_OF_TWO rounds .5 up (towards +infinity for
 * non-negative input), and the module applies it to the magnitude only, so the
 * result is symmetric. A negative level must not round away from zero
 * differently than the positive one. */
static void test_dequant_rounding_sign(void) {
    SvtAv2QuantParams params;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init(&params, 100, 0), SVT_AV2_QUANT_OK);

    for (int32_t level = 1; level <= 200; ++level) {
        const int32_t pos = svt_av2_dequant_coeff(&params, level, 0, 1);
        const int32_t neg = svt_av2_dequant_coeff(&params, -level, 0, 1);
        if (neg != -pos) {
            fprintf(stderr, "FAIL dequant sign asymmetry at level %d: +%d vs %d\n", level, pos, neg);
            ++g_svt_av2_test_failures;
        }
        ++g_svt_av2_test_checks;
    }
}

/* tx_scale shifts the dequantized coefficient down. */
static void test_dequant_tx_scale(void) {
    SvtAv2QuantParams p0, p1, p2;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init(&p0, 100, 0), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init(&p1, 100, 1), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init(&p2, 100, 2), SVT_AV2_QUANT_OK);

    const int32_t level = 5;
    SVT_TEST_CHECK_EQ_INT(svt_av2_dequant_coeff(&p0, level, 0, 1), 440);
    SVT_TEST_CHECK_EQ_INT(svt_av2_dequant_coeff(&p1, level, 0, 1), 220);
    SVT_TEST_CHECK_EQ_INT(svt_av2_dequant_coeff(&p2, level, 0, 1), 110);

    /* The dequantizer step for one level, at each tx_scale the first version
     * actually uses (TX_32X32 -> 1, TX_64X64 -> 2; see the anchor test above).
     * dq = ROUND_POWER_OF_TWO(1 * 704, 3) >> log_scale, avm/av2/decoder/
     * decodetxb.c:918-922 with shift = av2_get_tx_scale(tx_size) (:673). */
    SVT_TEST_CHECK_EQ_INT(svt_av2_dequant_coeff(&p0, 1, 0, 1), 88);
    SVT_TEST_CHECK_EQ_INT(svt_av2_dequant_coeff(&p1, 1, 0, 1), 44);
    SVT_TEST_CHECK_EQ_INT(svt_av2_dequant_coeff(&p2, 1, 0, 1), 22);

    /* Rounding happens before the tx_scale shift, so an odd intermediate must
     * not be truncated early: level 3 -> round(3*704, 3) = 264 -> 132 -> 66. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_dequant_coeff(&p0, 3, 0, 1), 264);
    SVT_TEST_CHECK_EQ_INT(svt_av2_dequant_coeff(&p1, 3, 0, 1), 132);
    SVT_TEST_CHECK_EQ_INT(svt_av2_dequant_coeff(&p2, 3, 0, 1), 66);
}

/* Dequantization overflow: the largest encodable level must clamp, not wrap. */
static void test_dequant_overflow_clamps(void) {
    SvtAv2QuantParams params;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init(&params, 100, 0), SVT_AV2_QUANT_OK);

    int32_t qcoeff[4];
    int32_t dqcoeff[4];

    /* level 8191 * 704 / 8 = 720,808 -> well above the 8-bit clip of 32767. */
    qcoeff[0] = 8191;
    qcoeff[1] = -8191;
    qcoeff[2] = 0;
    qcoeff[3] = 37;
    SVT_TEST_CHECK_EQ_INT(svt_av2_dequantize_block(&params, qcoeff, dqcoeff, 4, 0), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(dqcoeff[0], 32767);
    SVT_TEST_CHECK_EQ_INT(dqcoeff[1], -32768);
    SVT_TEST_CHECK_EQ_INT(dqcoeff[2], 0);
    SVT_TEST_CHECK_EQ_INT(dqcoeff[3], 37 * 88);

    /* A pathological level far beyond anything the quantizer emits must still
     * clamp rather than invoke undefined behaviour on the shift. */
    qcoeff[0] = 0x0FFFFFFF;
    SVT_TEST_CHECK_EQ_INT(svt_av2_dequantize_block(&params, qcoeff, dqcoeff, 1, 0), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(dqcoeff[0], 32767);
    /* 0x0FFFFFFF * 704 fits int64 comfortably; verify the module did not overflow
     * to a negative intermediate. */
    SVT_TEST_CHECK_EQ_INT(decoder_reference_dequant(0x0FFFFFFF, k_frozen_q100, 0, 8), 32767);
}

/* Zero block: all-zero qcoeff produces all-zero dqcoeff, both explicitly. */
static void test_dequant_zero_block(void) {
    SvtAv2QuantParams params;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init(&params, 100, 0), SVT_AV2_QUANT_OK);

    int32_t qcoeff[64];
    int32_t dqcoeff[64];
    memset(qcoeff, 0, sizeof(qcoeff));
    memset(dqcoeff, 0x5A, sizeof(dqcoeff)); /* poison */

    SVT_TEST_CHECK_EQ_INT(svt_av2_dequantize_block(&params, qcoeff, dqcoeff, 64, 0), SVT_AV2_QUANT_OK);
    int nonzero = 0;
    for (int i = 0; i < 64; ++i) {
        if (dqcoeff[i] != 0) {
            ++nonzero;
        }
    }
    SVT_TEST_CHECK_EQ_INT(nonzero, 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_all_zero(dqcoeff, 64), 1);
}

/* Y / U / V must all resolve to the same quantizer under the frozen stream, but
 * the plane parameter must still be validated. */
static void test_dequant_planes(void) {
    SvtAv2QuantParams params;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init(&params, 100, 0), SVT_AV2_QUANT_OK);

    for (int plane = 0; plane < 3; ++plane) {
        SVT_TEST_CHECK_EQ_INT(svt_av2_dequant_coeff(&params, 9, plane, 1), 9 * 88);
        SVT_TEST_CHECK_EQ_INT(svt_av2_dequant_coeff(&params, 9, plane, 0), 9 * 88);
    }

    int32_t q = 1, dq = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_dequantize_block(&params, &q, &dq, 1, 3), SVT_AV2_QUANT_ERR_UNSUPPORTED);
    SVT_TEST_CHECK_EQ_INT(svt_av2_dequantize_block(&params, &q, &dq, 1, -1), SVT_AV2_QUANT_ERR_UNSUPPORTED);
    SVT_TEST_CHECK_EQ_INT(svt_av2_dequantize_block(NULL, &q, &dq, 1, 0), SVT_AV2_QUANT_ERR_NULL);
    SVT_TEST_CHECK_EQ_INT(svt_av2_dequantize_block(&params, NULL, &dq, 1, 0), SVT_AV2_QUANT_ERR_NULL);
    SVT_TEST_CHECK_EQ_INT(svt_av2_dequantize_block(&params, &q, NULL, 1, 0), SVT_AV2_QUANT_ERR_NULL);
}

/* --- 4. forward quantizer tables ------------------------------------------- */

/*
 * av2_build_quantizer() formulas for the frozen (non-lossless) configuration,
 * reimplemented here from the AVM source text:
 *   round    = (48 * q) >> (7 + 3)
 *   quant_fp = (1 << (16 + 4 + 3)) / q
 */
static void test_forward_tables(void) {
    SvtAv2QuantParams params;
    SvtAv2QuantCoeffs coeffs;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init(&params, 100, 0), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_build_coeffs(&params, 0, 0, &coeffs), SVT_AV2_QUANT_OK);

    const int32_t q = k_frozen_q100;
    SVT_TEST_CHECK_EQ_INT(coeffs.dequant_QTX[0], q);
    SVT_TEST_CHECK_EQ_INT(coeffs.dequant_QTX[1], q);
    SVT_TEST_CHECK_EQ_INT(coeffs.round_QTX[0], (48 * q) >> 10);
    SVT_TEST_CHECK_EQ_INT(coeffs.round_QTX[1], (48 * q) >> 10);
    SVT_TEST_CHECK_EQ_INT(coeffs.quant_fp_QTX[0], (int32_t)((1u << 23) / (uint32_t)q));
    SVT_TEST_CHECK_EQ_INT(coeffs.quant_fp_QTX[1], (int32_t)((1u << 23) / (uint32_t)q));
    SVT_TEST_CHECK_EQ_INT(coeffs.shift, 16 - 0 + 4);
    SVT_TEST_CHECK_EQ_INT(coeffs.max_level, (1 << 20) - 1);

    /* Concrete values for q = 704: round = 33, quant_fp = 11915, shift = 20. */
    SVT_TEST_CHECK_EQ_INT(coeffs.round_QTX[0], 33);
    SVT_TEST_CHECK_EQ_INT(coeffs.quant_fp_QTX[0], 11915);

    /* Deadzone: ceil(dequant / 2^(1 + log_scale + QUANT_TABLE_BITS)), i.e.
     * 704 / 2^4 = 44 at log_scale 0 (av2_quantize.c:109-111). */
    SVT_TEST_CHECK_EQ_INT(coeffs.deadzone_QTX[0], 44);

    /* log_scale changes the shift, the deadzone and the rounding together. */
    SvtAv2QuantCoeffs c1, c2;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_build_coeffs(&params, 1, 0, &c1), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_build_coeffs(&params, 2, 0, &c2), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(c1.shift, 19);
    SVT_TEST_CHECK_EQ_INT(c2.shift, 18);
    SVT_TEST_CHECK_EQ_INT(c1.deadzone_QTX[0], (704 + 31) >> 5); /* 2^(1+1+3) = 32 */
    SVT_TEST_CHECK_EQ_INT(c2.deadzone_QTX[0], (704 + 63) >> 6); /* 2^(1+2+3) = 64 */
    SVT_TEST_CHECK_EQ_INT(c1.log_scale, 1);
    SVT_TEST_CHECK_EQ_INT(c2.log_scale, 2);
}

/* --- 5. forward quantization thresholds and signs -------------------------- */

/* The quantizer's behaviour at and around the deadzone boundary, written from
 * avm/av2/encoder/av2_quantize.c:97-121 rather than by calling into the module,
 * so agreement is evidence and not a tautology.
 *
 *   deadzone: abs_coeff << (1 + log_scale + QUANT_TABLE_BITS) >= dequant
 *   round:    ROUND_POWER_OF_TWO(round_QTX, log_scale)
 *   level:    ((abs_coeff + round) * quant_fp) >> (16 - log_scale + QUANT_FP_BITS)
 */
static int32_t reference_forward_level(int32_t abs_coeff,
                                      int32_t dequant,
                                      int32_t round,
                                      int32_t quant_fp,
                                      int log_scale,
                                      int shift) {
    const int deadzone = (dequant + (int32_t)(((uint32_t)1 << (1 + log_scale + 3)) - 1)) >> (1 + log_scale + 3);
    if (abs_coeff < deadzone) {
        return 0;
    }
    const int32_t log_scaled_round = (int32_t)(((int64_t)round + ((int64_t)1 << log_scale) / 2) >> log_scale);
    return (int32_t)((((int64_t)abs_coeff + log_scaled_round) * quant_fp) >> shift);
}

static void test_forward_threshold(void) {
    SvtAv2QuantParams params;
    SvtAv2QuantCoeffs coeffs;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init(&params, 100, 0), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_build_coeffs(&params, 0, 0, &coeffs), SVT_AV2_QUANT_OK);

    /* dequant = 704, so the AVM deadzone is ceil(704 / 2^(1+0+3)) = 44: every
     * |coeff| < 44 is forced to level 0 regardless of rounding. The first
     * coefficient that actually produces a non-zero level is 56, where
     * ((56 + 33) * 11915) >> 20 == 1. */
    const int32_t deadzone = coeffs.deadzone_QTX[0];
    SVT_TEST_CHECK_EQ_INT(deadzone, 44);

    for (int32_t c = -400; c <= 400; ++c) {
        int32_t q = 0, dq = 0;
        SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_coeff_idx(&coeffs, c, 1, &q, &dq), SVT_AV2_QUANT_OK);

        const int32_t abs_c = (c < 0) ? -c : c;
        const int32_t sign = (c < 0) ? -1 : 1;
        const int32_t expected_level = reference_forward_level(abs_c, coeffs.dequant_QTX[0], coeffs.round_QTX[0],
                                                               coeffs.quant_fp_QTX[0], coeffs.log_scale,
                                                               coeffs.shift);
        const int32_t expected_q = expected_level * sign;

        if (q != expected_q) {
            fprintf(stderr, "FAIL forward(%d): qcoeff %d, expected %d\n", c, q, expected_q);
            ++g_svt_av2_test_failures;
        }
        ++g_svt_av2_test_checks;

        /* Sign is carried exactly, magnitude symmetry must hold. */
        if (q != 0) {
            const int32_t expected_dq = decoder_reference_dequant(expected_level, k_frozen_q100, 0, 8) * sign;
            if (dq != expected_dq) {
                fprintf(stderr, "FAIL forward(%d): dqcoeff %d, expected %d\n", c, dq, expected_dq);
                ++g_svt_av2_test_failures;
            }
            ++g_svt_av2_test_checks;
        } else if (dq != 0) {
            fprintf(stderr, "FAIL forward(%d): zero level but dqcoeff %d\n", c, dq);
            ++g_svt_av2_test_failures;
            ++g_svt_av2_test_checks;
        }
    }

    /* Below the deadzone: 43 -> 0. At/above it but still rounding to zero: 44 ->
     * 0 (the deadzone only ENABLES the rounding path, it does not force level
     * 1). 55 -> 0 and the first level-1 coefficient is 56:
     *   (44 + 33) * 11915 >> 20 == 0
     *   (55 + 33) * 11915 >> 20 == 0
     *   (56 + 33) * 11915 >> 20 == 1
     * At 352 the level is 4, not 1. Pinning all four points keeps a future
     * "deadzone widened by one bit" mistake from passing. */
    int32_t q = -1, dq = -1;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_coeff_idx(&coeffs, 43, 1, &q, &dq), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(q, 0);
    SVT_TEST_CHECK_EQ_INT(dq, 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_coeff_idx(&coeffs, 44, 1, &q, &dq), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(q, 0);
    SVT_TEST_CHECK_EQ_INT(dq, 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_coeff_idx(&coeffs, 55, 1, &q, &dq), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(q, 0);
    SVT_TEST_CHECK_EQ_INT(dq, 0);
    /* The first non-zero level. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_coeff_idx(&coeffs, 56, 1, &q, &dq), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(q, 1);
    SVT_TEST_CHECK_EQ_INT(dq, 88);
    /* 352 sits well above the deadzone and quantizes to level 4. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_coeff_idx(&coeffs, 352, 1, &q, &dq), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(q, 4);
    SVT_TEST_CHECK_EQ_INT(dq, 352);
    /* Negative mirrors of the two decisive points. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_coeff_idx(&coeffs, -43, 1, &q, &dq), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(q, 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_coeff_idx(&coeffs, -56, 1, &q, &dq), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(q, -1);
    SVT_TEST_CHECK_EQ_INT(dq, -88);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_coeff_idx(&coeffs, -352, 1, &q, &dq), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(q, -4);
    SVT_TEST_CHECK_EQ_INT(dq, -352);
}

/* --- 5b. Sizes that are actually used by the M2 first version --------------- */

/* 00's ruling (2026-10-07): the frozen frame header hard-codes
 * tx_mode_select = 0 -> TX_MODE_LARGEST (Source/Lib/Bitstream/frame_header.c:
 * 245-248; avm/av2/common/common_data.h:703-706). At the top-level 64x64
 * PARTITION_NONE that resolves, via tx_size_from_tx_mode (avm/av2/common/
 * blockd.h:2580-2589),
 *   luma   : max_txsize_rect_lookup[BLOCK_64X64] = TX_64X64 -> pels 4096 -> tx_scale 2
 *   chroma : av2_get_max_uv_txsize(BLOCK_64X64, 1, 1) (blockd.h:3208-3215)
 *            -> get_plane_block_size -> BLOCK_32X32 -> max_txsize_rect_lookup
 *            = TX_32X32 -> pels 1024 -> tx_scale 1
 * so the two sizes the first version actually quantizes are 64x64 and 32x32,
 * i.e. tx_scale 2 and tx_scale 1. That is why log_scale must not be treated as
 * "always 0": every log_scale-dependent quantity changes together, and the
 * anchors below pin each one against avm/av2/encoder/av2_quantize.c:97-121.
 *
 * Values are derived from the AVM expressions, not read out of the module:
 *   deadzone = ceil(dequant / 2^(1 + log_scale + QUANT_TABLE_BITS))
 *   round'   = ROUND_POWER_OF_TWO(round_QTX, log_scale)
 *   shift    = 16 - log_scale + QUANT_FP_BITS
 *   level    = ((|c| + round') * quant_fp) >> shift
 *   dq       = ROUND_POWER_OF_TWO(level * dequant, QUANT_TABLE_BITS) >> log_scale
 */
static void test_forward_anchors_tx_scale_1_2(void) {
    SvtAv2QuantParams params;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init(&params, 100, 0), SVT_AV2_QUANT_OK);

    /* (log_scale, expected deadzone, expected shift, expected round',
     *  first non-zero coeff -> level, dq,
     *  far anchor coeff -> level, dq) */
    struct {
        int log_scale;
        int32_t deadzone;
        int32_t shift;
        int32_t round;
        int32_t first_coeff;
        int32_t first_level;
        int32_t first_dq;
        int32_t anchor_coeff;
        int32_t anchor_level;
        int32_t anchor_dq;
    } const cases[] = {
        /* 4x4..16x16 (pels <= 256): the previously pinned reference case. */
        { 0, 44, 20, 33, 56, 1, 88, 352, 4, 352 },
        /* TX_32X32 (pels 1024 -> tx_scale 1): the chroma size of the first
         * version. dequant 704 / 2^5 = 22, round 33 >> 1 = 17, shift 19. */
        { 1, 22, 19, 17, 28, 1, 44, 704, 16, 704 },
        /* TX_64X64 (pels 4096 -> tx_scale 2): the luma size of the first
         * version. dequant 704 / 2^6 = 11, round 33 >> 2 = 8, shift 18. */
        { 2, 11, 18, 8, 15, 1, 22, 1408, 64, 1408 },
    };

    for (size_t t = 0; t < sizeof(cases) / sizeof(cases[0]); ++t) {
        const int ls = cases[t].log_scale;
        SvtAv2QuantCoeffs coeffs;
        SVT_TEST_CHECK_EQ_INT(svt_av2_quant_build_coeffs(&params, ls, 0, &coeffs), SVT_AV2_QUANT_OK);

        SVT_TEST_CHECK_EQ_INT(coeffs.log_scale, ls);
        SVT_TEST_CHECK_EQ_INT(coeffs.shift, cases[t].shift);
        SVT_TEST_CHECK_EQ_INT(coeffs.deadzone_QTX[0], cases[t].deadzone);
        SVT_TEST_CHECK_EQ_INT(coeffs.deadzone_QTX[1], cases[t].deadzone);
        SVT_TEST_CHECK_EQ_INT(svt_av2_round_power_of_two((int64_t)coeffs.round_QTX[0], ls), cases[t].round);

        /* The decisive boundary of this size: the last zero and the first
         * non-zero coefficient. */
        int32_t q = -1, dq = -1;
        SVT_TEST_CHECK_EQ_INT(
            svt_av2_quantize_coeff_idx(&coeffs, cases[t].first_coeff - 1, 1, &q, &dq), SVT_AV2_QUANT_OK);
        SVT_TEST_CHECK_EQ_INT(q, 0);
        SVT_TEST_CHECK_EQ_INT(dq, 0);

        SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_coeff_idx(&coeffs, cases[t].first_coeff, 1, &q, &dq), SVT_AV2_QUANT_OK);
        SVT_TEST_CHECK_EQ_INT(q, cases[t].first_level);
        SVT_TEST_CHECK_EQ_INT(dq, cases[t].first_dq);

        SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_coeff_idx(&coeffs, cases[t].anchor_coeff, 1, &q, &dq), SVT_AV2_QUANT_OK);
        SVT_TEST_CHECK_EQ_INT(q, cases[t].anchor_level);
        SVT_TEST_CHECK_EQ_INT(dq, cases[t].anchor_dq);

        /* Sign symmetry at the far anchor. */
        SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_coeff_idx(&coeffs, -cases[t].anchor_coeff, 1, &q, &dq), SVT_AV2_QUANT_OK);
        SVT_TEST_CHECK_EQ_INT(q, -cases[t].anchor_level);
        SVT_TEST_CHECK_EQ_INT(dq, -cases[t].anchor_dq);
    }
}

/* qcoeff == 0 and dqcoeff == 0 are equivalent, which is what lets T011 emit an
 * explicit zero block. */
static void test_zero_level_implies_zero_dq(void) {
    SvtAv2QuantParams params;
    SvtAv2QuantCoeffs coeffs;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init(&params, 100, 0), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_build_coeffs(&params, 0, 0, &coeffs), SVT_AV2_QUANT_OK);

    int disagreements = 0;
    for (int32_t c = -2000; c <= 2000; ++c) {
        int32_t q = 0, dq = 0;
        if (svt_av2_quantize_coeff_idx(&coeffs, c, 1, &q, &dq) != SVT_AV2_QUANT_OK) {
            continue;
        }
        if ((q == 0) != (dq == 0)) {
            ++disagreements;
        }
    }
    SVT_TEST_CHECK_EQ_INT(disagreements, 0);
}

/* DC/AC selection is positional: at the frozen qindex both quantizers are the
 * same, so we must verify the parameter plumbing instead by giving the two
 * positions different quantizers. */
static void test_dc_ac_positional_selection(void) {
    SvtAv2QuantParams params;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init(&params, 100, 0), SVT_AV2_QUANT_OK);
    params.plane[0].ac = params.plane[0].dc * 2; /* non-frozen, plumbing only */

    SvtAv2QuantCoeffs coeffs;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_build_coeffs(&params, 0, 0, &coeffs), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK(!(coeffs.quant_fp_QTX[0] == coeffs.quant_fp_QTX[1]));

    /* A zero coefficient at the DC position and at an AC position must both
     * give zero regardless of which quantizer is selected. */
    int32_t q = 9, dq = 9;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_coeff_idx(&coeffs, 0, 1, &q, &dq), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(q, 0);
    SVT_TEST_CHECK_EQ_INT(dq, 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_coeff_idx(&coeffs, 0, 0, &q, &dq), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(q, 0);
    SVT_TEST_CHECK_EQ_INT(dq, 0);

    /* With a doubled AC quantizer, the same magnitude yields a smaller AC level
     * than DC level. This is what makes the previous bug (choosing the quantizer
     * from the coefficient value) detectable. */
    int32_t q_dc = 0, dq_dc = 0, q_ac = 0, dq_ac = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_coeff_idx(&coeffs, -10000, 1, &q_dc, &dq_dc), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_coeff_idx(&coeffs, -10000, 0, &q_ac, &dq_ac), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK(q_dc < 0);
    SVT_TEST_CHECK(q_ac < 0);
    SVT_TEST_CHECK((q_dc < 0 ? -q_dc : q_dc) > (q_ac < 0 ? -q_ac : q_ac));
    SVT_TEST_CHECK_EQ_INT(dq_dc, -88 * (q_dc < 0 ? -q_dc : q_dc));
    SVT_TEST_CHECK_EQ_INT(dq_ac, -88 * 2 * (q_ac < 0 ? -q_ac : q_ac));
}

/* --- 6. block API contract ------------------------------------------------- */

static void test_block_rejects_aliasing(void) {
    SvtAv2QuantParams params;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init(&params, 100, 0), SVT_AV2_QUANT_OK);

    int32_t buf[16];
    memset(buf, 0, sizeof(buf));
    /* qcoeff and dqcoeff must be distinct buffers: T011 needs qcoeff's sign and
     * magnitude while T012 needs dqcoeff, so one buffer cannot serve both. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_block(&params, buf, buf, buf, 16, 0, 0), SVT_AV2_QUANT_ERR_UNSUPPORTED);
}

static void test_block_rejects_bad_args(void) {
    SvtAv2QuantParams params;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init(&params, 100, 0), SVT_AV2_QUANT_OK);

    int32_t in[16] = { 0 };
    int32_t q[16];
    int32_t dq[16];

    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_block(NULL, in, q, dq, 16, 0, 0), SVT_AV2_QUANT_ERR_NULL);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_block(&params, NULL, q, dq, 16, 0, 0), SVT_AV2_QUANT_ERR_NULL);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_block(&params, in, NULL, dq, 16, 0, 0), SVT_AV2_QUANT_ERR_NULL);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_block(&params, in, q, NULL, 16, 0, 0), SVT_AV2_QUANT_ERR_NULL);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_block(&params, in, q, dq, 0, 0, 0), SVT_AV2_QUANT_ERR_UNSUPPORTED);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_block(&params, in, q, dq, 16, 0, 3), SVT_AV2_QUANT_ERR_UNSUPPORTED);
}

static void test_block_overflow_is_atomic(void) {
    SvtAv2QuantParams params;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init(&params, 100, 0), SVT_AV2_QUANT_OK);

    int32_t in[4] = { 100, 200, 300, INT32_MAX };
    int32_t q[4];
    int32_t dq[4];
    memset(q, 0x11, sizeof(q));
    memset(dq, 0x22, sizeof(dq));

    /* A coefficient large enough to exceed the encodable level range must fail
     * the whole block without writing partial output. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_block(&params, in, q, dq, 4, 0, 0),
                          SVT_AV2_QUANT_ERR_LEVEL_OVERFLOW);
    for (int i = 0; i < 4; ++i) {
        SVT_TEST_CHECK_EQ_INT(q[i], 0x11111111);
        SVT_TEST_CHECK_EQ_INT(dq[i], 0x22222222);
    }
}

static void test_block_writes_every_entry(void) {
    SvtAv2QuantParams params;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init(&params, 100, 0), SVT_AV2_QUANT_OK);

    int32_t in[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    int32_t q[8];
    int32_t dq[8];
    memset(q, 0x7F, sizeof(q));
    memset(dq, 0x7F, sizeof(dq));

    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_block(&params, in, q, dq, 8, 0, 0), SVT_AV2_QUANT_OK);
    for (int i = 0; i < 8; ++i) {
        SVT_TEST_CHECK_EQ_INT(q[i], 0);
        SVT_TEST_CHECK_EQ_INT(dq[i], 0);
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_all_zero(q, 8), 1);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_all_zero(NULL, 8), -1);

    q[3] = 1;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_all_zero(q, 8), 0);
}

/* --- 7. fixed-seed random blocks ------------------------------------------- */

/* Deterministic LCG so the vectors are reproducible without a random header. */
static uint32_t g_lcg_state = 0x12345678u;

static void lcg_seed(uint32_t seed) { g_lcg_state = seed; }

static uint32_t lcg_next(void) {
    g_lcg_state = g_lcg_state * 1664525u + 1013904223u;
    return g_lcg_state;
}

/* Random coefficient sets across the plausible transform range, checked against
 * the independently written reference expressions. */
static void test_random_blocks_against_reference(void) {
    SvtAv2QuantParams params;
    SvtAv2QuantCoeffs coeffs;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init(&params, 100, 0), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_build_coeffs(&params, 0, 0, &coeffs), SVT_AV2_QUANT_OK);

    int32_t in[64];
    int32_t q[64];
    int32_t dq[64];

    int blocks = 0;
    int coeff_mismatches = 0;
    int dq_mismatches = 0;

    for (uint32_t seed = 1; seed <= 128; ++seed) {
        lcg_seed(seed * 2654435761u);
        for (int i = 0; i < 64; ++i) {
            /* Mix small values (to exercise the deadzone) with larger ones. */
            const uint32_t r = lcg_next();
            if ((r & 3u) == 0u) {
                in[i] = (int32_t)(r % 600u) - 300;
            } else {
                in[i] = (int32_t)(r % 20000u) - 10000;
            }
        }

        if (svt_av2_quantize_block(&params, in, q, dq, 64, 0, 0) != SVT_AV2_QUANT_OK) {
            fprintf(stderr, "FAIL random block seed %u returned an error\n", seed);
            ++g_svt_av2_test_failures;
            ++g_svt_av2_test_checks;
            continue;
        }
        ++blocks;

        for (int i = 0; i < 64; ++i) {
            const int32_t c = in[i];
            const int32_t abs_c = (c < 0) ? -c : c;
            const int32_t sign = (c < 0) ? -1 : 1;
            const int32_t level = reference_forward_level(abs_c, coeffs.dequant_QTX[0], coeffs.round_QTX[0],
                                                          coeffs.quant_fp_QTX[0], coeffs.log_scale, coeffs.shift);
            if (q[i] != level * sign) {
                ++coeff_mismatches;
            }
            const int32_t expected_dq = (level == 0) ? 0 : decoder_reference_dequant(level, k_frozen_q100, 0, 8) * sign;
            if (dq[i] != expected_dq) {
                ++dq_mismatches;
            }
        }
    }

    SVT_TEST_CHECK_EQ_INT(blocks, 128);
    SVT_TEST_CHECK_EQ_INT(coeff_mismatches, 0);
    SVT_TEST_CHECK_EQ_INT(dq_mismatches, 0);
}

/* Independent qcoeff -> dqcoeff consistency: re-running the dequantizer on the
 * emitted qcoeff must reproduce dqcoeff exactly. This is the invariant T011 and
 * T012 depend on (one source of truth for the level). */
static void test_qcoeff_to_dqcoeff_consistency(void) {
    SvtAv2QuantParams params;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init(&params, 100, 0), SVT_AV2_QUANT_OK);

    int32_t in[64];
    int32_t q[64];
    int32_t dq[64];
    int32_t dq_again[64];

    int mismatches = 0;
    for (uint32_t seed = 1; seed <= 100; ++seed) {
        lcg_seed(seed * 40503u);
        for (int i = 0; i < 64; ++i) {
            in[i] = (int32_t)(lcg_next() % 40000u) - 20000;
        }
        if (svt_av2_quantize_block(&params, in, q, dq, 64, 0, 0) != SVT_AV2_QUANT_OK) {
            continue;
        }
        if (svt_av2_dequantize_block(&params, q, dq_again, 64, 0) != SVT_AV2_QUANT_OK) {
            ++mismatches;
            continue;
        }
        for (int i = 0; i < 64; ++i) {
            if (dq[i] != dq_again[i]) {
                ++mismatches;
            }
        }
    }
    SVT_TEST_CHECK_EQ_INT(mismatches, 0);
}

/* Random blocks for every plane and every transform scale: the plumbing must not
 * depend on the plane or on the transform size. */
static void test_random_blocks_all_planes_and_scales(void) {
    int failures = 0;

    for (int plane = 0; plane < 3; ++plane) {
        for (int log_scale = 0; log_scale <= 2; ++log_scale) {
            SvtAv2QuantParams params;
            SvtAv2QuantCoeffs coeffs;
            if (svt_av2_quant_params_init(&params, 100, log_scale) != SVT_AV2_QUANT_OK) {
                ++failures;
                continue;
            }
            if (svt_av2_quant_build_coeffs(&params, log_scale, plane, &coeffs) != SVT_AV2_QUANT_OK) {
                ++failures;
                continue;
            }

            int32_t in[16];
            int32_t q[16];
            int32_t dq[16];
            for (uint32_t seed = 1; seed <= 20; ++seed) {
                lcg_seed(seed * 7919u + (uint32_t)plane * 131u + (uint32_t)log_scale * 17u);
                for (int i = 0; i < 16; ++i) {
                    in[i] = (int32_t)(lcg_next() % 20000u) - 10000;
                }
                if (svt_av2_quantize_block(&params, in, q, dq, 16, plane, log_scale) != SVT_AV2_QUANT_OK) {
                    ++failures;
                    continue;
                }
                for (int i = 0; i < 16; ++i) {
                    const int32_t c = in[i];
                    const int32_t abs_c = (c < 0) ? -c : c;
                    const int32_t sign = (c < 0) ? -1 : 1;
                    const int32_t level =
                        reference_forward_level(abs_c, coeffs.dequant_QTX[0], coeffs.round_QTX[0],
                                                coeffs.quant_fp_QTX[0], coeffs.log_scale, coeffs.shift);
                    if (q[i] != level * sign) {
                        ++failures;
                    }
                }
            }
        }
    }
    SVT_TEST_CHECK_EQ_INT(failures, 0);
}

/* --- 8. largest legal magnitude and integer-width safety ------------------- */

/*
 * Determine the largest |coeff| the forward quantizer can accept while still
 * producing a level within the encodable range, and check the boundary exactly.
 * This pins the documented operating range instead of leaving it implicit.
 */
static void test_max_legal_magnitude_boundary(void) {
    SvtAv2QuantParams params;
    SvtAv2QuantCoeffs coeffs;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init(&params, 100, 0), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_build_coeffs(&params, 0, 0, &coeffs), SVT_AV2_QUANT_OK);

    /* Binary search the exact forward-level ceiling at the decoder 20-bit limit. */
    const int32_t max_level = coeffs.max_level;
    int32_t lo = 0;
    int32_t hi = INT32_MAX;
    while (lo + 1 < hi) {
        const int32_t mid = lo + (hi - lo) / 2;
        int32_t q = 0, dq = 0;
        if (svt_av2_quantize_coeff_idx(&coeffs, mid, 1, &q, &dq) == SVT_AV2_QUANT_OK) {
            lo = mid;
        } else {
            hi = mid;
        }
    }

    /* The largest accepted magnitude must produce exactly max_level. */
    int32_t q = 0, dq = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_coeff_idx(&coeffs, lo, 1, &q, &dq), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(q, max_level);
    /* One more must be refused. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_coeff_idx(&coeffs, lo + 1, 1, &q, &dq),
                          SVT_AV2_QUANT_ERR_LEVEL_OVERFLOW);
    /* Negative mirror. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_coeff_idx(&coeffs, -lo, 1, &q, &dq), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(q, -max_level);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_coeff_idx(&coeffs, -(lo + 1), 1, &q, &dq),
                          SVT_AV2_QUANT_ERR_LEVEL_OVERFLOW);

    if (lo < 90000000 || lo > 95000000) {
        fprintf(stderr, "FAIL max legal |coeff| = %d is outside the expected range for q=704\n", lo);
        ++g_svt_av2_test_failures;
    }
    ++g_svt_av2_test_checks;
}

/* int32 extremes must be handled without signed overflow. INT32_MIN is the
 * classic trap: -INT32_MIN overflows. */
static void test_int32_extremes(void) {
    SvtAv2QuantParams params;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init(&params, 100, 0), SVT_AV2_QUANT_OK);

    int32_t in[3];
    int32_t q[3];
    int32_t dq[3];
    in[0] = INT32_MAX;
    in[1] = INT32_MIN;
    in[2] = 0;

    /* Both extremes exceed the encodable range and must be refused, not
     * truncated or wrapped. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_block(&params, in, q, dq, 3, 0, 0), SVT_AV2_QUANT_ERR_LEVEL_OVERFLOW);
}

/* The shared rounding helper must match ROUND_POWER_OF_TWO exactly, including
 * the "round half up" behaviour. */
static void test_round_power_of_two_helper(void) {
    SVT_TEST_CHECK_EQ_INT(svt_av2_round_power_of_two(0, 3), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_round_power_of_two(3, 3), 0); /* (3+4)>>3 = 0 */
    SVT_TEST_CHECK_EQ_INT(svt_av2_round_power_of_two(4, 3), 1); /* (4+4)>>3 = 1 */
    SVT_TEST_CHECK_EQ_INT(svt_av2_round_power_of_two(7, 3), 1);
    SVT_TEST_CHECK_EQ_INT(svt_av2_round_power_of_two(8, 3), 1);
    SVT_TEST_CHECK_EQ_INT(svt_av2_round_power_of_two(11, 3), 1);
    SVT_TEST_CHECK_EQ_INT(svt_av2_round_power_of_two(12, 3), 2);
    SVT_TEST_CHECK_EQ_INT(svt_av2_round_power_of_two(1000, 3), 125);
    /* 2^40 >> 3 = 2^37, which does not fit int32; the helper returns int32_t, so
     * pin the documented truncation instead of pretending it is exact. The
     * operands are non-const locals because MSVC reports a constant cast that
     * truncates as warning C4310, which /WX turns into an error. */
    int64_t big_value = (int64_t)1 << 40;
    int32_t big_expected = (int32_t)(big_value >> 3);
    SVT_TEST_CHECK_EQ_INT(svt_av2_round_power_of_two(big_value, 3), big_expected);
    SVT_TEST_CHECK_EQ_INT(svt_av2_round_power_of_two(((int64_t)1 << 31) - 1, 0), 2147483647);

    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_shift(0), 20);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_shift(1), 19);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_shift(2), 18);
}

static void test_bd_random_quant_dequant(void) {
    const uint32_t sides[] = { 4, 8, 16, 32, 64 };
    uint32_t seed = 0x12345678u;
    for (uint32_t bd = 8; bd <= 10; bd += 2)
        for (uint32_t g = 0; g < 5; ++g) {
            const uint32_t side = sides[g], qindex = bd == 8 ? 100 : 148;
            const int scale = (side * side > 256) + (side * side > 1024);
            const int32_t quant = bd == 8 ? 704 : 2816;
            const int32_t rounding = (48 * quant) >> 10;
            const int32_t reciprocal = (1 << 23) / quant;
            SvtAv2QuantParams params;
            SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init_tx_bd(&params, qindex, side, side, bd), SVT_AV2_QUANT_OK);
            SVT_TEST_CHECK_EQ_INT(params.plane[0].ac, quant);
            for (uint32_t trial = 0; trial < 100; ++trial) {
                int32_t coeff[1024], q[1024], dq[1024];
                const uint32_t count = side > 32 ? 1024 : side * side;
                for (uint32_t i = 0; i < count; ++i) {
                    seed = seed * 1664525u + 1013904223u;
                    coeff[i] = (int32_t)(seed % 60001) - 30000;
                }
                SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_block(&params, coeff, q, dq, count, (int)(trial % 3), scale), SVT_AV2_QUANT_OK);
                for (uint32_t i = 0; i < count; ++i) {
                    const int32_t absolute = coeff[i] < 0 ? -coeff[i] : coeff[i];
                    const int32_t sign = coeff[i] < 0 ? -1 : 1;
                    const int32_t level = reference_forward_level(absolute, quant, rounding, reciprocal, scale, 20 - scale);
                    SVT_TEST_CHECK_EQ_INT(q[i], level * sign);
                    int32_t expected = (int32_t)(((int64_t)q[i] * quant + 4) >> 3) >> scale;
                    const int32_t minimum = -(1 << (bd + 7)), maximum = (1 << (bd + 7)) - 1;
                    if (expected < minimum) expected = minimum;
                    if (expected > maximum) expected = maximum;
                    SVT_TEST_CHECK_EQ_INT(dq[i], expected);
                }
            }
        }
}

static int32_t reference_cqp_quant(uint32_t index) {
    if (index == 0u) return 64;
    uint32_t cycles = (index - 1u) / 24u;
    int32_t value = k_expected_qlookup[1u + (index - 1u) % 24u];
    while (cycles--) value *= 2;
    return value;
}
static int64_t reference_cqp_level(int32_t coeff, int32_t quant, int scale) {
    const int64_t magnitude = coeff < 0 ? -(int64_t)coeff : coeff;
    if (magnitude * (INT64_C(1) << (4 + scale)) < quant) return 0;
    const int64_t rounding = ((int64_t)48 * quant) / 1024;
    const int64_t scaled_rounding = (rounding + (INT64_C(1) << scale) / 2) / (INT64_C(1) << scale);
    return ((magnitude + scaled_rounding) * (8388608 / quant)) / (INT64_C(1) << (20 - scale));
}
static void test_all_cqp_values(void) {
    const uint32_t sides[] = {4, 8, 16, 32, 64};
    uint32_t seed = 0x537139abu;
    uint32_t maximum_level = 0u;
    for (uint32_t bd = 8; bd <= 10; bd += 2) {
        SvtAv2QuantParams invalid;
        const uint32_t lower = bd == 8u ? 1u : 49u, upper = bd == 8u ? 255u : 303u;
        SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init_tx_bd(&invalid, lower - 1u, 4, 4, bd), SVT_AV2_QUANT_ERR_UNSUPPORTED);
        SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init_tx_bd(&invalid, upper + 1u, 4, 4, bd), SVT_AV2_QUANT_ERR_UNSUPPORTED);
        for (uint32_t qp = 1; qp <= 255; ++qp) {
            const uint32_t index = qp + (bd == 10u ? 48u : 0u);
            const int32_t quant = reference_cqp_quant(index);
            SVT_TEST_CHECK_EQ_INT(svt_av2_quant_qlookup((int)index), quant);
            for (size_t g = 0; g < sizeof(sides) / sizeof(sides[0]); ++g) {
                const uint32_t side = sides[g], count = side > 32u ? 1024u : side * side;
                const int scale = (side * side > 256u) + (side * side > 1024u);
                SvtAv2QuantParams params;
                SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init_tx_bd(&params, index, side, side, bd), SVT_AV2_QUANT_OK);
                SVT_TEST_CHECK_EQ_INT(params.tx_scale, scale);
                for (int plane = 0; plane < 3; ++plane) {
                    SVT_TEST_CHECK_EQ_INT(params.plane[plane].dc, quant);
                    SVT_TEST_CHECK_EQ_INT(params.plane[plane].ac, quant);
                    for (uint32_t trial = 0; trial < 4u; ++trial) {
                        int32_t input[1024], q[1024], dq[1024];
                        const int32_t bound = 1 << (bd + 7u);
                        for (uint32_t i = 0; i < count; ++i) {
                            seed = seed * 1664525u + 1013904223u;
                            input[i] = (int32_t)(seed % (uint32_t)(2 * bound + 1)) - bound;
                        }
                        SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_block(&params, input, q, dq, count, plane, scale), SVT_AV2_QUANT_OK);
                        for (uint32_t i = 0; i < count; ++i) {
                            const int64_t level = reference_cqp_level(input[i], quant, scale);
                            const int32_t expected = input[i] < 0 ? -(int32_t)level : (int32_t)level;
                            SVT_TEST_CHECK_EQ_INT(q[i], expected);
                            SVT_TEST_CHECK_EQ_INT(dq[i], decoder_reference_dequant(expected, quant, scale, (int)bd));
                            if ((uint32_t)level > maximum_level) maximum_level = (uint32_t)level;
                        }
                    }
                    SvtAv2QuantCoeffs coeffs;
                    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_build_coeffs(&params, scale, plane, &coeffs), SVT_AV2_QUANT_OK);
                    const int32_t extremes[] = {INT32_MIN, INT32_MAX, 0, 1, -1};
                    for (size_t e = 0; e < sizeof(extremes) / sizeof(extremes[0]); ++e) {
                        int32_t q = 123, dq = 456;
                        const int64_t level = reference_cqp_level(extremes[e], quant, scale);
                        const SvtAv2QuantStatus status = svt_av2_quantize_coeff(&coeffs, extremes[e], &q, &dq);
                        if (level > (1 << 20) - 1) {
                            SVT_TEST_CHECK_EQ_INT(status, SVT_AV2_QUANT_ERR_LEVEL_OVERFLOW);
                            SVT_TEST_CHECK_EQ_INT(q, 123); SVT_TEST_CHECK_EQ_INT(dq, 456);
                        } else {
                            SVT_TEST_CHECK_EQ_INT(status, SVT_AV2_QUANT_OK);
                            const int32_t expected = extremes[e] < 0 ? -(int32_t)level : (int32_t)level;
                            SVT_TEST_CHECK_EQ_INT(q, expected);
                            const int64_t hp = (level * quant) % 16777216;
                            const int32_t expected_dq = (int32_t)((hp + 4) / 8) / (1 << scale);
                            SVT_TEST_CHECK_EQ_INT(dq, extremes[e] < 0 ? -expected_dq : expected_dq);
                        }
                    }
                }
            }
        }
    }
    SVT_TEST_CHECK(maximum_level > 8191u); /* The expanded ceiling is exercised. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_qlookup(303), 245760);
}
static void test_decoder_mask_sign_and_clip(void) {
    for (uint32_t bd = 8; bd <= 10; bd += 2)
        for (uint32_t qp = 1; qp <= 255; ++qp)
            for (int scale = 0; scale <= 2; ++scale) {
                const uint32_t index = qp + (bd == 10u ? 48u : 0u);
                const int32_t quant = reference_cqp_quant(index);
                SvtAv2QuantParams params;
                const uint32_t side = scale == 0 ? 16u : scale == 1 ? 32u : 64u;
                svt_av2_quant_params_init_tx_bd(&params, index, side, side, bd);
                const int32_t boundary = 16777216 / quant;
                const int32_t levels[] = {1,-1,boundary-1,1-boundary,boundary,-boundary,
                    boundary+1,-boundary-1,(1<<20)-1,1-(1<<20),INT32_MIN,INT32_MAX};
                int32_t actual[sizeof(levels)/sizeof(levels[0])];
                SVT_TEST_CHECK_EQ_INT(svt_av2_dequantize_block(&params, levels, actual,
                    sizeof(levels)/sizeof(levels[0]), 0), SVT_AV2_QUANT_OK);
                for (size_t i = 0; i < sizeof(levels)/sizeof(levels[0]); ++i) {
                    const int32_t expected = decoder_reference_dequant(levels[i], quant, scale, (int)bd);
                    SVT_TEST_CHECK_EQ_INT(actual[i], expected);
                }
            }
}

/* Frozen pre-optimization block algorithm: retain its complete DC preflight,
 * positional quantization and decoder dequantization as a differential oracle. */
static SvtAv2QuantStatus reference_block_preflight(const SvtAv2QuantParams* params,
    const int32_t* coeff, int32_t* qcoeff, int32_t* dqcoeff, size_t count,
    int plane, int log_scale) {
    SvtAv2QuantCoeffs tables;
    SvtAv2QuantStatus status = svt_av2_quant_build_coeffs(params, log_scale, plane, &tables);
    if (status != SVT_AV2_QUANT_OK) return status;
    for (size_t i = 0; i < count; ++i) {
        int32_t q = 0, dq = 0;
        status = svt_av2_quantize_coeff_idx(&tables, coeff[i], 1, &q, &dq);
        if (status != SVT_AV2_QUANT_OK) return status;
    }
    for (size_t i = 0; i < count; ++i) {
        status = svt_av2_quantize_coeff_idx(&tables, coeff[i], i == 0, &qcoeff[i], &dqcoeff[i]);
        if (status != SVT_AV2_QUANT_OK) return status;
    }
    return svt_av2_dequantize_block(params, qcoeff, dqcoeff, count, plane);
}

static void test_peak_preflight_matches_original_block(void) {
    uint32_t seed = 846275u;
    for (uint32_t bd = 8u; bd <= 10u; bd += 2u)
        for (uint32_t qp = 1u; qp <= 255u; ++qp)
            for (int tx_scale = 0; tx_scale <= 2; ++tx_scale)
                for (int log_scale = 0; log_scale <= 2; ++log_scale)
                    for (int plane = 0; plane < 3; ++plane)
                        for (int pair = 0; pair < 3; ++pair) {
                            SvtAv2QuantParams params;
                            const uint32_t side = tx_scale == 0 ? 16u : tx_scale == 1 ? 32u : 64u;
                            svt_av2_quant_params_init_tx_bd(&params,
                                qp + (bd == 10u ? 48u : 0u), side, side, bd);
                            if (pair == 1) params.plane[plane].ac *= 2;
                            if (pair == 2) params.plane[plane].ac /= 2;
                            SvtAv2QuantCoeffs tables;
                            svt_av2_quant_build_coeffs(&params, log_scale, plane, &tables);
                            /* Find the exact DC overflow boundary independently
                             * of the block's peak scan, keeping all arithmetic wide. */
                            int64_t low = 0, high = INT32_MAX;
                            while (low < high) {
                                const int64_t middle = low + (high - low + 1) / 2;
                                int32_t q = 0, dq = 0;
                                if (svt_av2_quantize_coeff_idx(&tables, (int32_t)middle, 1,
                                    &q, &dq) == SVT_AV2_QUANT_OK) low = middle;
                                else high = middle - 1;
                            }
                            for (unsigned trial = 0u; trial < 6u; ++trial) {
                                int32_t input[17], actual_q[17], actual_dq[17];
                                int32_t reference_q[17], reference_dq[17];
                                for (size_t i = 0u; i < 17u; ++i) {
                                    seed = seed * 1664525u + 1013904223u;
                                    input[i] = (int32_t)(seed % 65537u) - 32768;
                                    actual_q[i] = reference_q[i] = 1234567;
                                    actual_dq[i] = reference_dq[i] = -7654321;
                                }
                                if (trial == 0u) memset(input, 0, sizeof(input));
                                if (trial == 1u) {
                                    for (size_t i = 0u; i < 17u; ++i) {
                                        const int32_t edge = tables.deadzone_QTX[i == 0u ? 0 : 1];
                                        input[i] = (edge + (int32_t)(i % 3u) - 1) * (i % 2u ? -1 : 1);
                                    }
                                }
                                if (trial == 2u) input[0] = INT32_MIN;
                                if (trial == 3u) input[16] = INT32_MIN;
                                if (trial == 4u) input[8] = -(int32_t)low;
                                if (trial == 5u) input[16] = low < INT32_MAX ? (int32_t)(low + 1) : INT32_MAX;
                                const size_t count = trial == 0u ? 1u : 17u;
                                const SvtAv2QuantStatus expected = reference_block_preflight(&params,
                                    input, reference_q, reference_dq, count, plane, log_scale);
                                SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_block(&params, input,
                                    actual_q, actual_dq, count, plane, log_scale), expected);
                                SVT_TEST_CHECK(memcmp(actual_q, reference_q, sizeof(actual_q)) == 0);
                                SVT_TEST_CHECK(memcmp(actual_dq, reference_dq, sizeof(actual_dq)) == 0);
                            }
                        }
}

int main(void) {
    test_peak_preflight_matches_original_block();
    test_all_cqp_values();
    test_decoder_mask_sign_and_clip();
    test_bd_random_quant_dequant();
    test_qlookup_table_matches_reference();
    test_qlookup_values();
    test_qlookup_full_range();

    test_frozen_params();
    test_params_reject_outside_mapping();
    test_tx_scale_derivation();

    test_dequant_clip_bounds();
    test_dequant_exact_values();
    test_dequant_rounding_sign();
    test_dequant_tx_scale();
    test_dequant_overflow_clamps();
    test_dequant_zero_block();
    test_dequant_planes();

    test_forward_tables();
    test_forward_threshold();
    test_forward_anchors_tx_scale_1_2();
    test_zero_level_implies_zero_dq();
    test_dc_ac_positional_selection();

    test_block_rejects_aliasing();
    test_block_rejects_bad_args();
    test_block_overflow_is_atomic();
    test_block_writes_every_entry();

    test_random_blocks_against_reference();
    test_qcoeff_to_dqcoeff_consistency();
    test_random_blocks_all_planes_and_scales();

    test_max_legal_magnitude_boundary();
    test_int32_extremes();
    test_round_power_of_two_helper();

    SVT_TEST_REPORT("test_quantization");
}
