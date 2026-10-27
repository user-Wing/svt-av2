/*
 * SVT-AV2 quantization / dequantization implementation (M2, T010).
 *
 * Every numeric constant below is transcribed from the AVM reference and is
 * cross-checked by tests/unit/test_quantization.c. See quantization.h for the
 * frozen scope and the specification/AVM anchors.
 */
#include "quantization_internal.h"

#include <string.h>

#if defined(SVT_AV2_HAVE_QUANT_AVX2)
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
static int quant_avx2_supported(void) {
#if defined(_MSC_VER)
    int r[4];
    __cpuidex(r, 0, 0); if (r[0] < 7) return 0;
    __cpuidex(r, 1, 0); if ((r[2] & 0x1c000000) != 0x1c000000) return 0;
    if ((_xgetbv(0) & 6u) != 6u) return 0;
    __cpuidex(r, 7, 0); return (r[1] & 32) != 0;
#else
    unsigned a, b, c, d, lo, hi;
    if (__get_cpuid_max(0, NULL) < 7u) return 0;
    __cpuid(1, a, b, c, d); if ((c & 0x1c000000u) != 0x1c000000u) return 0;
    __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0)); (void)hi;
    if ((lo & 6u) != 6u) return 0;
    __cpuid_count(7, 0, a, b, c, d); return (b & 32u) != 0u;
#endif
}
#endif

/* --- constants transcribed from AVM ---------------------------------------- */

/*
 * avm/avm_dsp/avm_dsp_common.h:
 *   #define QUANT_TABLE_BITS 3
 *   #define QUANT_FP_BITS    4
 *   #define AVM_QM_BITS      5
 */
#define SVT_AV2_QUANT_TABLE_BITS 3
#define SVT_AV2_QUANT_FP_BITS 4
#define SVT_AV2_AVM_QM_BITS 5

/*
 * avm/av2/encoder/av2_quantize.c, av2_build_quantizer():
 *   const int qrounding_factor = q == 0 ? 64 : 48;
 *   const int qrounding_factor_fp = 64;
 *   quants->y_round[q][i]     = (qrounding_factor * quant_QTX) >> (7 + QUANT_TABLE_BITS);
 *   quants->y_round_fp[q][i]  = (qrounding_factor_fp * quant_QTX) >> (7 + QUANT_TABLE_BITS);
 *   quants->y_zbin[q][i]      = ROUND_POWER_OF_TWO(qzbin_factor * quant_QTX, 7 + QUANT_TABLE_BITS);
 * and get_qzbin_factor() for 8-bit returns 64 when q == 0 else 84 when
 * quant < (148 << QUANT_TABLE_BITS) else 80.
 *
 * The supported CQP path is never lossless (base_q_idx != 0), so the q != 0
 * branch is the only one reachable; the lossless branches are omitted rather
 * than carried as dead code.
 */
#define SVT_AV2_QUANT_ROUNDING_FACTOR 48
#define SVT_AV2_QUANT_QBITS_DENOM (7 + SVT_AV2_QUANT_TABLE_BITS)

/*
 * avm/av2/common/quant_common.h:
 *   #define MAXQ_8_BITS 255
 * and av2/common/av2_common_int.h MAXQ_FOR_GIVEN_BIT_DEPTH(8-bit) = MAXQ_8_BITS.
 */
#define SVT_AV2_QUANT_MAXQ_8BIT 255

/*
 * avm/av2/decoder/decodetxb.c:
 *   const int32_t max_value = (1 << (7 + xd->bd)) - 1;
 *   const int32_t min_value = -(1 << (7 + xd->bd));
 */
#define SVT_AV2_QUANT_CLIP_SHIFT 7

/* Fixed AVM v1 decodetxb.c:907 rejects level >> 20. Low CQP values
 * require more than the old QP100-only 8191 ceiling, especially TX64. */
#define SVT_AV2_QUANT_MAX_LEVEL ((1 << 20) - 1)
#define SVT_AV2_QUANT_MAXQ_10BIT (SVT_AV2_QUANT_MAXQ_8BIT + 48)

/* clang-format off */
/*
 * avm/av2/common/quant_common.c, ac_qlookup_QTX[25].
 *
 *   //      64,                                              q_index = 0
 *   // Q =  2^((q_index + 127)/24)                           q_index in [1, 24]
 *   //      Q[(q_index - 1) % 24) + 1] * 2^((q_index-1)/24)  q_index in [25, 255]
 */
static const uint16_t k_ac_qlookup_qtx[SVT_AV2_QUANT_QLOOKUP_SIZE] = {
    64, 40, 41, 43, 44, 45, 47, 48, 49, 51, 52, 54, 55,
    57, 59, 60, 62, 64, 66, 68, 70, 72, 74, 76, 78
};
/* clang-format on */

/* --- lookup helpers -------------------------------------------------------- */

const uint16_t* svt_av2_quant_qlookup_table(void) { return k_ac_qlookup_qtx; }

int32_t svt_av2_quant_qlookup(int q_clamped) {
    if (q_clamped < 0 || q_clamped > SVT_AV2_QUANT_MAXQ_10BIT) {
        return 0;
    }
    if (q_clamped < SVT_AV2_QUANT_QLOOKUP_SIZE) {
        return (int32_t)k_ac_qlookup_qtx[q_clamped];
    }
    /* avm/av2/common/quant_common.c qlookup(), q_clamped >= 25. */
    return (int32_t)k_ac_qlookup_qtx[((q_clamped - 1) % 24) + 1] << ((q_clamped - 1) / 24);
}

static int svt_av2_quant_maxq(SvtAv2QuantBitDepth bit_depth) {
    switch (bit_depth) {
    case SVT_AV2_QUANT_BIT_DEPTH_8:
        return SVT_AV2_QUANT_MAXQ_8BIT;
    case SVT_AV2_QUANT_BIT_DEPTH_10:
        /* MAXQ_10_BITS = MAXQ_8_BITS + 2 * MAXQ_OFFSET, MAXQ_OFFSET = 24. */
        return SVT_AV2_QUANT_MAXQ_8BIT + 48;
    case SVT_AV2_QUANT_BIT_DEPTH_12:
        /* MAXQ_12_BITS = MAXQ_8_BITS + 4 * MAXQ_OFFSET. */
        return SVT_AV2_QUANT_MAXQ_8BIT + 96;
    default:
        return 0;
    }
}

int32_t svt_av2_round_power_of_two(int64_t value, int n) {
    /* avm/avm_ports/mem.h: ((value) + ((((int64_t)1 << (n)) >> 1))) >> (n) */
    return (int32_t)((value + (((int64_t)1 << n) >> 1)) >> n);
}

int svt_av2_quant_shift(int log_scale) { return 16 - log_scale + SVT_AV2_QUANT_FP_BITS; }

int32_t svt_av2_dequant_clip_min(SvtAv2QuantBitDepth bit_depth) {
    return -(int32_t)((uint32_t)1 << (SVT_AV2_QUANT_CLIP_SHIFT + (int)bit_depth));
}

int32_t svt_av2_dequant_clip_max(SvtAv2QuantBitDepth bit_depth) {
    return (int32_t)(((uint32_t)1 << (SVT_AV2_QUANT_CLIP_SHIFT + (int)bit_depth)) - 1);
}

/*
 * avm/av2/common/quant_common.c, av2_dc_quant_QTX()/av2_ac_quant_QTX() through
 * get_q(qindex, delta, bit_depth):
 *
 *   if ((qindex == 0) && (delta <= 0)) return ac_qlookup_QTX[0];
 *   q_clamped = clamp(qindex + delta, 1, MAXQ_FOR_GIVEN_BIT_DEPTH(bit_depth));
 *   return qlookup(q_clamped);
 *
 * The frozen path passes delta == 0 for every plane and component, so the only
 * reachable behaviour is qlookup(clamp(qindex, 1, MAXQ)).
 */
static int32_t svt_av2_quant_q_for(int qindex, int delta, SvtAv2QuantBitDepth bit_depth) {
    if (qindex == 0 && delta <= 0) {
        return (int32_t)k_ac_qlookup_qtx[0];
    }
    const int maxq = svt_av2_quant_maxq(bit_depth);
    int q_clamped = qindex + delta;
    if (q_clamped < 1) {
        q_clamped = 1;
    } else if (q_clamped > maxq) {
        q_clamped = maxq;
    }
    return svt_av2_quant_qlookup(q_clamped);
}

static SvtAv2QuantStatus quant_params_init_bd(SvtAv2QuantParams*, uint32_t, int, uint32_t);

/* --- parameter setup ------------------------------------------------------- */

SvtAv2QuantStatus svt_av2_quant_params_init(SvtAv2QuantParams* params, uint32_t base_q_idx, int tx_scale) {
    return quant_params_init_bd(params, base_q_idx, tx_scale, 8);
}

static SvtAv2QuantStatus quant_params_init_bd(SvtAv2QuantParams* params, uint32_t base_q_idx, int tx_scale, uint32_t bit_depth) {
    if (params == NULL) {
        return SVT_AV2_QUANT_ERR_NULL;
    }
    /* User CQP1..255: 8-bit actual index1..255; 10-bit index49..303.
     * Index0/lossless and 10-bit indices below the fixed +48 mapping remain
     * unsupported. Do not clamp: syntax and reconstruction must share index. */
    if ((bit_depth != 8u && bit_depth != 10u) ||
        base_q_idx < 1u + (bit_depth - 8u) * 24u ||
        base_q_idx > 255u + (bit_depth - 8u) * 24u) {
        return SVT_AV2_QUANT_ERR_UNSUPPORTED;
    }
    if (tx_scale < 0 || tx_scale > 2) {
        return SVT_AV2_QUANT_ERR_UNSUPPORTED;
    }

    memset(params, 0, sizeof(*params));
    params->base_q_idx = base_q_idx;
    params->bit_depth = (SvtAv2QuantBitDepth)bit_depth;
    params->tx_scale = tx_scale;
    params->applies = 1;

    /*
     * The frozen stream disables every delta-q:
     *   - sequence header writes equal_ac_dc_q = 1, so base_y_dc_delta_q and
     *     base_uv_dc_delta_q are inferred 0 and base_uv_dc_delta_q :=
     *     base_uv_ac_delta_q = 0 (decodeframe.c read_sequence_header_quant,
     *     2026-10-07 tree);
     *   - uv_ac_delta_q_enabled = 0, so u/v_ac_delta_q = 0;
     *   - y_dc_delta_q_enabled = 0, so y_dc_delta_q = 0;
     *   - uv_dc_delta_q_enabled = 0, so u/v_dc_delta_q = 0.
     * Therefore every plane/component resolves to qlookup(qindex) with
     * qindex = base_q_idx.
     *
     * Note the decoder-side clamping detail this keeps honest: even with
     * delta == 0, av2_q_clamped() clamps to >= 1 unless (qindex == 0 &&
     * delta + base <= 0). At qindex = 100 the clamp is inert, but the code path
     * is still the specification's, so enabling delta-q later cannot silently
     * change the meaning of this function.
     */
    for (int plane = 0; plane < 3; ++plane) {
        params->plane[plane].dc = svt_av2_quant_q_for((int)base_q_idx, 0, params->bit_depth);
        params->plane[plane].ac = svt_av2_quant_q_for((int)base_q_idx, 0, params->bit_depth);
    }
    return SVT_AV2_QUANT_OK;
}

SvtAv2QuantStatus svt_av2_quant_params_init_tx(SvtAv2QuantParams* params,
                                               uint32_t base_q_idx,
                                               uint32_t tx_width,
                                               uint32_t tx_height) {
    if (tx_width == 0 || tx_height == 0) {
        return SVT_AV2_QUANT_ERR_UNSUPPORTED;
    }
    const uint32_t pels = tx_width * tx_height;
    /* avm/av2/common/idct.c: av2_get_tx_scale(). */
    const int tx_scale = (pels > 256 ? 1 : 0) + (pels > 1024 ? 1 : 0);
    return svt_av2_quant_params_init(params, base_q_idx, tx_scale);
}

SvtAv2QuantStatus svt_av2_quant_params_init_tx_bd(SvtAv2QuantParams* params,
    uint32_t base_q_idx, uint32_t tx_width, uint32_t tx_height, uint32_t bit_depth) {
    if (tx_width == 0 || tx_height == 0) return SVT_AV2_QUANT_ERR_UNSUPPORTED;
    const uint32_t pels = tx_width * tx_height;
    return quant_params_init_bd(params, base_q_idx, (pels > 256) + (pels > 1024), bit_depth);
}

/* --- dequantization -------------------------------------------------------- */

int32_t svt_av2_dequant_coeff(const SvtAv2QuantParams* params, int32_t qcoeff, int plane, int pos_is_dc) {
    if (params == NULL || plane < 0 || plane > 2) {
        return 0;
    }
    const int32_t dequant = pos_is_dc ? params->plane[plane].dc : params->plane[plane].ac;
    /*
     * No qmatrix in the frozen stream (using_qmatrix = 0). The AVM decoder still
     * assigns qm level 15 and a NULL iqmatrix in that case
     * (decodeframe.c setup_qm_params/setup_quant_matrices), and get_dqv() with a
     * NULL iqmatrix is exactly `dequant[!!coeff_idx]`. So the no-qmatrix branch
     * here is the decoder's actual behaviour, not a simplification.
     */
    /* The decoder reads a magnitude, masks the product to 24 bits, rounds,
     * shifts and only then applies the sign (fixed v1 decodetxb.c:919-928). */
    const int64_t magnitude = qcoeff < 0 ? -(int64_t)qcoeff : qcoeff;
    const int64_t dq_coeff_hp = (magnitude * dequant) & 0xffffff;
    const int32_t magnitude_dq = svt_av2_round_power_of_two(dq_coeff_hp, SVT_AV2_QUANT_TABLE_BITS) >> params->tx_scale;
    return qcoeff < 0 ? -magnitude_dq : magnitude_dq;
}

SvtAv2QuantStatus svt_av2_dequantize_block(const SvtAv2QuantParams* params,
                                           const int32_t* qcoeff,
                                           int32_t* dqcoeff,
                                           size_t count,
                                           int plane) {
    if (params == NULL || qcoeff == NULL || dqcoeff == NULL) {
        return SVT_AV2_QUANT_ERR_NULL;
    }
    if (plane < 0 || plane > 2 || count == 0) {
        return SVT_AV2_QUANT_ERR_UNSUPPORTED;
    }
    const int32_t min_value = svt_av2_dequant_clip_min(params->bit_depth);
    const int32_t max_value = svt_av2_dequant_clip_max(params->bit_depth);

    for (size_t i = 0; i < count; ++i) {
        int32_t dq = svt_av2_dequant_coeff(params, qcoeff[i], plane, i == 0);
        /* decodetxb.c: tcoeffs[pos] = clamp(dq_coeff, min_value, max_value). */
        if (dq < min_value) {
            dq = min_value;
        } else if (dq > max_value) {
            dq = max_value;
        }
        dqcoeff[i] = dq;
    }
    return SVT_AV2_QUANT_OK;
}

/* --- forward quantizer tables ---------------------------------------------- */

/*
 * avm/av2/encoder/av2_quantize.c, invert_quant():
 *
 *   t = d;
 *   for (l = 0; t > 1; l++) t >>= 1;
 *   const int lcap = AVMMIN(l, 15);
 *   const int m = (int)(1 + (1U << (16 + lcap)) / (d >> (l - lcap)));
 *   *quant = (int32_t)(m - (1 << 16));
 *   *shift = 1 << (16 - l + QUANT_TABLE_BITS);
 *
 * Only `quant` is used by the integer path this module implements; the
 * `quant_shift` half of invert_quant() belongs to the non-fp (avm_highbd_quantize_b)
 * routine. `quant_fp_QTX` is built directly by the table formula, exactly as
 * av2_build_quantizer() does, so no reciprocal needs to be derived here.
 */
static int32_t svt_av2_quant_fp(int32_t quant_qtx) {
    /* av2_build_quantizer: (1 << (16 + QUANT_FP_BITS + QUANT_TABLE_BITS)) / quant_QTX */
    return (int32_t)(((uint32_t)1 << (16 + SVT_AV2_QUANT_FP_BITS + SVT_AV2_QUANT_TABLE_BITS)) / (uint32_t)quant_qtx);
}

static int32_t svt_av2_quant_round(int32_t quant_qtx) {
    return (int32_t)(((int64_t)SVT_AV2_QUANT_ROUNDING_FACTOR * quant_qtx) >> SVT_AV2_QUANT_QBITS_DENOM);
}

static int32_t svt_av2_quant_deadzone(int32_t dequant_qtx, int log_scale) {
    /*
     * avm/av2/encoder/av2_quantize.c, highbd_quantize_fp_helper_c, the
     * no-qmatrix branch (av2_quantize.c:101-115). With TCQ disabled in M2,
     * abs_coeff_scale == dequant_ptr_scale == 1, so the condition reduces to
     *
     *   abs_coeff << (1 + log_scale + QUANT_TABLE_BITS) >= dequant
     *
     * i.e. the smallest surviving |coeff| is
     *   ceil(dequant / 2^(1 + log_scale + QUANT_TABLE_BITS))
     * which for an exact power-of-two divisor equals
     *   (dequant + 2^(1+log_scale+3) - 1) >> (1 + log_scale + 3).
     *
     * The previous revision divided by 2^(1 + log_scale) only and therefore
     * left the quantizer's deadzone 8x too wide (QUANT_TABLE_BITS = 3): the
     * coefficients in between were forced to level 0, so qcoeff disagreed with
     * the AVM encoder and, worse, with the decoder the stream is checked
     * against. See the T010 report for the worked q=704 example.
     */
    const int shift = 1 + log_scale + SVT_AV2_QUANT_TABLE_BITS;
    return (dequant_qtx + (((int32_t)1 << shift) - 1)) >> shift;
}

SvtAv2QuantStatus svt_av2_quant_build_coeffs(const SvtAv2QuantParams* params,
                                             int log_scale,
                                             int plane,
                                             SvtAv2QuantCoeffs* coeffs) {
    if (params == NULL || coeffs == NULL) {
        return SVT_AV2_QUANT_ERR_NULL;
    }
    if (plane < 0 || plane > 2 || log_scale < 0 || log_scale > 2) {
        return SVT_AV2_QUANT_ERR_UNSUPPORTED;
    }

    const int32_t dc = params->plane[plane].dc;
    const int32_t ac = params->plane[plane].ac;
    if (dc <= 0 || ac <= 0) {
        return SVT_AV2_QUANT_ERR_UNSUPPORTED;
    }

    memset(coeffs, 0, sizeof(*coeffs));
    coeffs->dequant_QTX[0] = dc;
    coeffs->dequant_QTX[1] = ac;
    coeffs->round_QTX[0] = svt_av2_quant_round(dc);
    coeffs->round_QTX[1] = svt_av2_quant_round(ac);
    coeffs->quant_fp_QTX[0] = svt_av2_quant_fp(dc);
    coeffs->quant_fp_QTX[1] = svt_av2_quant_fp(ac);
    coeffs->deadzone_QTX[0] = svt_av2_quant_deadzone(dc, log_scale);
    coeffs->deadzone_QTX[1] = svt_av2_quant_deadzone(ac, log_scale);
    coeffs->zbin_QTX[0] = coeffs->deadzone_QTX[0];
    coeffs->zbin_QTX[1] = coeffs->deadzone_QTX[1];
    coeffs->shift = svt_av2_quant_shift(log_scale);
    coeffs->log_scale = log_scale;
    coeffs->max_level = SVT_AV2_QUANT_MAX_LEVEL;
    return SVT_AV2_QUANT_OK;
}

/* --- forward quantization -------------------------------------------------- */

SvtAv2QuantStatus svt_av2_quantize_coeff_idx(const SvtAv2QuantCoeffs* coeffs,
                                             int32_t coeff,
                                             int pos_is_dc,
                                             int32_t* qcoeff,
                                             int32_t* dqcoeff) {
    if (coeffs == NULL || qcoeff == NULL || dqcoeff == NULL) {
        return SVT_AV2_QUANT_ERR_NULL;
    }
    /*
     * The DC/AC selection is positional, mirroring the decoder's
     * `dequant[!!coeff_idx]`. It must NOT be inferred from the coefficient value:
     * a zero AC coefficient would otherwise be quantized with the DC quantizer,
     * and a non-zero DC coefficient with the AC quantizer.
     */
    const int idx = pos_is_dc ? 0 : 1;

    const int32_t coeff_sign = (coeff < 0) ? -1 : 0;
    /* AVMSIGN-based abs: (coeff ^ sign) - sign, i.e. two's-complement abs. */
    const int64_t abs_coeff = coeff < 0 ? -(int64_t)coeff : coeff;

    int32_t level = 0;
    if (abs_coeff >= coeffs->deadzone_QTX[idx]) {
        /*
         * av2_quantize.c:97-99/114: the rounding term is
         * ROUND_POWER_OF_TWO(round_QTX, log_scale), not the raw table value.
         * At log_scale == 0 the two are identical; for larger transforms the
         * unscaled round would bias the level.
         */
        const int32_t log_scaled_round = svt_av2_round_power_of_two((int64_t)coeffs->round_QTX[idx], coeffs->log_scale);
        const int64_t tmp = (int64_t)abs_coeff + log_scaled_round;
        const int64_t abs_qcoeff = ((tmp * coeffs->quant_fp_QTX[idx]) >> coeffs->shift);
        if (abs_qcoeff > (int64_t)coeffs->max_level) {
            /*
             * Refuse rather than truncate: T011 would code a level that does not
             * describe the same residual T012 reconstructs from.
             */
            return SVT_AV2_QUANT_ERR_LEVEL_OVERFLOW;
        }
        level = (int32_t)abs_qcoeff;
    }
    *qcoeff = (level ^ coeff_sign) - coeff_sign;

    /*
     * Keep `qcoeff` and `dqcoeff` generated from the same level, so the
     * coefficients T011 codes and the coefficients T012 reconstructs cannot
     * drift apart. This is deliberately not "quantize twice"; it is the decoder
     * formula applied to the level produced above.
     */
    const int32_t dequant = coeffs->dequant_QTX[idx];
    int64_t dq = ((int64_t)level * dequant) & 0xffffff;
    dq = (dq + (((int64_t)1 << SVT_AV2_QUANT_TABLE_BITS) >> 1)) >> SVT_AV2_QUANT_TABLE_BITS;
    /*
     * av2_quantize.c:118-121: the round happens BEFORE the log_scale shift, so
     * the intermediate must not be truncated early. Omitting this shift leaves
     * dqcoeff 2^log_scale times too large for every size above 16x16, which is
     * exactly the tx_scale 1 (TX_32X32) and 2 (TX_64X64) case the frozen frame
     * header selects (Source/Lib/Bitstream/frame_header.c:245-248).
     */
    dq >>= coeffs->log_scale;
    *dqcoeff = (int32_t)((dq ^ coeff_sign) - coeff_sign);
    return SVT_AV2_QUANT_OK;
}

/* DC-position convenience wrapper for callers that only quantize one value. */
SvtAv2QuantStatus svt_av2_quantize_coeff(const SvtAv2QuantCoeffs* coeffs,
                                         int32_t coeff,
                                         int32_t* qcoeff,
                                         int32_t* dqcoeff) {
    return svt_av2_quantize_coeff_idx(coeffs, coeff, 1, qcoeff, dqcoeff);
}

static SvtAv2QuantStatus quantize_block_impl(const SvtAv2QuantParams* params,
                                         const int32_t* coeff,
                                         int32_t* qcoeff,
                                         int32_t* dqcoeff,
                                         size_t count,
                                         int plane,
                                         int log_scale, SvtAv2QuantKernel kernel) {
    if (params == NULL || coeff == NULL || qcoeff == NULL || dqcoeff == NULL) {
        return SVT_AV2_QUANT_ERR_NULL;
    }
    /*
     * The contract forbids sharing one buffer between qcoeff and dqcoeff
     * (docs/m1-m2-contracts.md section 4). Refusing aliasing here is what makes
     * the contract enforceable instead of a comment.
     */
    if (qcoeff == dqcoeff) {
        return SVT_AV2_QUANT_ERR_UNSUPPORTED;
    }
    if (plane < 0 || plane > 2 || count == 0) {
        return SVT_AV2_QUANT_ERR_UNSUPPORTED;
    }

    SvtAv2QuantCoeffs coeffs;
    SvtAv2QuantStatus status = svt_av2_quant_build_coeffs(params, log_scale, plane, &coeffs);
    if (status != SVT_AV2_QUANT_OK) {
        return status;
    }

    /*
     * Overflow is checked for the whole block before anything is written, so a
     * failing block leaves both output arrays untouched rather than half-filled.
     * The DC quantizer is the smallest quantizer, so it yields the largest
     * level; its level is monotone in absolute coefficient magnitude, so one
     * probe of the peak bounds the whole block before the AC pass below.
     */
    int32_t peak_coeff = coeff[0];
    int64_t peak_magnitude = peak_coeff < 0 ? -(int64_t)peak_coeff : peak_coeff;
    for (size_t i = 1; i < count; ++i) {
        const int64_t magnitude = coeff[i] < 0 ? -(int64_t)coeff[i] : coeff[i];
        if (magnitude > peak_magnitude) {
            peak_magnitude = magnitude;
            peak_coeff = coeff[i];
        }
    }
    int32_t probe_q = 0, probe_dq = 0;
    status = svt_av2_quantize_coeff_idx(&coeffs, peak_coeff, 1, &probe_q, &probe_dq);
    if (status != SVT_AV2_QUANT_OK) return status;

    if (kernel != NULL && count <= 1024u && (count & 7u) == 0u && params->tx_scale == log_scale &&
        (params->bit_depth == SVT_AV2_QUANT_BIT_DEPTH_8 || params->bit_depth == SVT_AV2_QUANT_BIT_DEPTH_10) &&
        coeffs.dequant_QTX[0] == coeffs.dequant_QTX[1]) {
        const uintptr_t c = (uintptr_t)coeff, q = (uintptr_t)qcoeff, d = (uintptr_t)dqcoeff;
        const size_t bytes = count * sizeof(*coeff);
        if ((c < q ? q - c : c - q) >= bytes && (c < d ? d - c : c - d) >= bytes &&
            (q < d ? d - q : q - d) >= bytes) {
            kernel(&coeffs, svt_av2_dequant_clip_min(params->bit_depth),
                svt_av2_dequant_clip_max(params->bit_depth), coeff, qcoeff, dqcoeff, count);
            return SVT_AV2_QUANT_OK;
        }
    }

    /*
     * The frozen configuration has identical DC and AC quantizers, so the DC/AC
     * position distinction below is currently inert; it is kept positional
     * because that is what the decoder indexes on, and enabling delta-q must only
     * change svt_av2_quant_params_init().
     */
    for (size_t i = 0; i < count; ++i) {
        const SvtAv2QuantStatus st = svt_av2_quantize_coeff_idx(&coeffs, coeff[i], i == 0, &qcoeff[i], &dqcoeff[i]);
        if (st != SVT_AV2_QUANT_OK) {
            return st;
        }
    }
    /* Match decoder clipping as well as mask/round/sign before inverse TX. */
    return svt_av2_dequantize_block(params, qcoeff, dqcoeff, count, plane);
}

SvtAv2QuantStatus svt_av2_quantize_block(const SvtAv2QuantParams* p, const int32_t* c,
    int32_t* q, int32_t* d, size_t n, int plane, int scale) {
    return quantize_block_impl(p, c, q, d, n, plane, scale, NULL);
}
#if defined(SVT_AV2_HAVE_QUANT_AVX2)
static SvtAv2QuantStatus quantize_block_avx2(const SvtAv2QuantParams* p, const int32_t* c,
    int32_t* q, int32_t* d, size_t n, int plane, int scale) {
    return quantize_block_impl(p, c, q, d, n, plane, scale, svt_av2_quantize_avx2);
}
#endif
SvtAv2QuantizeBlockFn svt_av2_quantize_block_select(void) {
#if defined(SVT_AV2_HAVE_QUANT_AVX2)
    if (quant_avx2_supported()) return quantize_block_avx2;
#endif
    return svt_av2_quantize_block;
}

int svt_av2_quant_all_zero(const int32_t* qcoeff, size_t count) {
    if (qcoeff == NULL) {
        return -1;
    }
    for (size_t i = 0; i < count; ++i) {
        if (qcoeff[i] != 0) {
            return 0;
        }
    }
    return 1;
}
