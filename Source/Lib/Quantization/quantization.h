/*
 * SVT-AV2 quantization / dequantization (M2, T010).
 *
 * M5.3 CQP scope:
 *   - Existing 8/10-bit AI/P scalar paths, no lossless.
 *   - User QP1..255; actual qindex1..255 (8-bit), 49..303 (10-bit).
 *   - Delta-q / segmentation / qmatrix remain disabled. The stream uses
 *     equal_ac_dc_q = 1 and the existing zero UV delta mapping, and
 *     the frame header writes y_dc_delta_q_enabled = 0, all *_delta_q = 0 and
 *     using_qmatrix = 0. The resulting quantizer state is therefore
 *     "one q per plane, DC == AC", and no quantizer matrix is applied.
 *   - No CRF/VBR/CBR, no perceptual quantization, no RDO / trellis search.
 *
 * Ownership (docs/m1-m2-paths.md section 2.1): everything under
 * Source/Lib/Quantization/ belongs to T010.
 *
 * Consumers:
 *   - T011 writes coefficient syntax from `qcoeff` and its sign/magnitude.
 *   - T012 rebuilds from `dqcoeff`.
 * Both arrays must stay valid simultaneously: this module never uses one buffer
 * as scratch for the other. See svt_av2_quantize_block().
 *
 * EOB / scan do NOT belong here. T011 computes scan order, last position and
 * EOB per specification; this module only guarantees the `qcoeff` contents.
 */
#ifndef SVT_AV2_QUANTIZATION_H
#define SVT_AV2_QUANTIZATION_H

#include <stddef.h>
#include <stdint.h>

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- frozen constants ------------------------------------------------------ */

/* Historical default qindex, retained for existing source callers. */
#define SVT_AV2_QUANT_FROZEN_QINDEX 100

/* Number of entries in the AV2 QTX base lookup table (q_clamped 0..24). */
#define SVT_AV2_QUANT_QLOOKUP_SIZE 25

/*
 * avm_bit_depth_t equivalent for this project. Only 8-bit is supported in M2;
 * 10-bit is M3. Kept as an explicit parameter because the AV2 quantizer formula
 * is bit-depth dependent.
 */
typedef enum SvtAv2QuantBitDepth {
    SVT_AV2_QUANT_BIT_DEPTH_8 = 8,
    SVT_AV2_QUANT_BIT_DEPTH_10 = 10,
    SVT_AV2_QUANT_BIT_DEPTH_12 = 12
} SvtAv2QuantBitDepth;

/* Return status. Every entry point reports failures explicitly; a zero return
 * never means "silently clamped". */
typedef enum SvtAv2QuantStatus {
    SVT_AV2_QUANT_OK = 0,
    /* An input was NULL. */
    SVT_AV2_QUANT_ERR_NULL = -1,
    /* Unsupported configuration (bit depth, qindex outside the supported CQP mapping,
     * coefficient count, tx scale). */
    SVT_AV2_QUANT_ERR_UNSUPPORTED = -2,
    /* The quantized level would not fit the encodable coefficient range.
     * The caller must reduce the residual/transform gain; this module will not
     * truncate silently. */
    SVT_AV2_QUANT_ERR_LEVEL_OVERFLOW = -3
} SvtAv2QuantStatus;

/* --- quantizer parameters (docs/m1-m2-contracts.md section 4) -------------- */

/*
 * Per-plane quantizer pair, in the QTX scale used by the AV2 transform
 * (coefficients carry 3 extra bits over the pixel depth for 8-bit input).
 *
 * `dc` and `ac` are only equal under the frozen stream configuration; they are
 * kept separate so that enabling delta-q later does not change this interface.
 */
typedef struct SvtAv2PlaneQuant {
    int32_t dc;
    int32_t ac;
} SvtAv2PlaneQuant;

typedef struct SvtAv2QuantParams {
    uint32_t base_q_idx;             /* frame actual qindex, including the 10-bit +48 offset */
    SvtAv2QuantBitDepth bit_depth;   /* 8 or 10 */
    SvtAv2PlaneQuant plane[3];       /* [0]=Y, [1]=U, [2]=V */

    /* Number of bits to shift the dequantized coefficient down by so that it is
     * expressed in the transform's own (Qn) domain. Mirrors
     * av2_get_tx_scale() from avm/av2/common/idct.c:
     *   pels > 256 -> 1, pels > 1024 -> 2, else 0.
     * A value of `s` means the transform returned Q(3-s) coefficients, so the
     * Q3 dequantized value is shifted right by `s`. */
    int tx_scale;

    /* Per-plane "the DC coefficient is quantized differently from AC".
     * Always true under the frozen stream (DC and AC use the same q index but
     * the quantizer tables still index 0 vs 1). Present so the block entry
     * points do not need to know about the plane tables. */
    int applies;
} SvtAv2QuantParams;

/*
 * Fills `params` for an 8-bit CQP actual qindex1..255,
 * no delta-q, no qmatrix. The _bd entry point supports 10-bit index49..303.
 *
 * `tx_scale` must be the value the transform module (T009) used:
 * 0 for <= 16x16, 1 for 16x32/32x16/32x32, 2 for 32x64/64x32/64x64.
 *
 * Index0/lossless and indices outside the mapping are explicitly rejected.
 */
SvtAv2QuantStatus svt_av2_quant_params_init(SvtAv2QuantParams* params, uint32_t base_q_idx, int tx_scale);

/*
 * Convenience wrapper deriving tx_scale from the transform geometry, following
 * av2_get_tx_scale(): pels = width * height, pels > 256 -> 1, pels > 1024 -> 2.
 * Returns SVT_AV2_QUANT_ERR_UNSUPPORTED when width or height is 0.
 *
 * CALLER CONVENTION: pass the ORIGINAL transform geometry, never the 32x32
 * carrier grid. For a 64x64 transform the carrier grid is 32x32 and the coeff
 * count is 1024 -- identical to a real 32x32 transform -- so this call cannot
 * tell the two apart. Passing (32,32) for a 64x64 block yields tx_scale 1
 * instead of 2 and makes dqcoeff systematically 2x too large, with no error.
 * Pass (64,64) -- see svt_av2_quant_params_init() and the T010 report.
 */
SvtAv2QuantStatus svt_av2_quant_params_init_tx(SvtAv2QuantParams* params,
                                               uint32_t base_q_idx,
                                               uint32_t tx_width,
                                               uint32_t tx_height);

/* --- raw quantizer value lookup (independent of the frozen configuration) -- */

/*
 * The AV2 QTX quantizer lookup, transcribed from av2_q_clamped()/qlookup() in
 * avm/av2/common/quant_common.c:
 *
 *   q == 0                  -> 64
 *   q in [1, 24]            -> ac_qlookup_QTX[q]
 *   q in [25, MAXQ]         -> ac_qlookup_QTX[((q - 1) % 24) + 1] << ((q - 1) / 24)
 *
 * `q_clamped` must already be in [0, 303] (maximum supported 10-bit). This is a
 * pure table lookup with no clamping: clamping belongs to the parser, not here.
 * Returns 0 for out-of-range input so callers cannot mistake a failure for a
 * valid quantizer.
 */
int32_t svt_av2_quant_qlookup(int q_clamped);

/* The 25-entry base table, exposed read-only for tests and for T011/T015 when
 * they need to reproduce the same values. */
const uint16_t* svt_av2_quant_qlookup_table(void);

/* --- dequantization (decoder reference path) ------------------------------- */

/*
 * Mirrors the decoder's dequantization exactly, for the no-qmatrix case:
 *
 *   avm/av2/decoder/decodetxb.c, av2_read_coeffs_txb():
 *     dq_coeff_hp = ((int64_t)level * dequant[!!pos]) & 0xffffff;
 *     dq_coeff    = (tran_low_t)ROUND_POWER_OF_TWO_64(dq_coeff_hp, QUANT_TABLE_BITS);
 *     dq_coeff    = dq_coeff >> shift;              // shift = av2_get_tx_scale
 *     if (sign) dq_coeff = -dq_coeff;
 *     tcoeffs[pos] = clamp(dq_coeff, min_value, max_value);
 *
 *   with QUANT_TABLE_BITS = 3, AVM_QM_BITS = 5,
 *        min_value = -(1 << (7 + bd)), max_value = (1 << (7 + bd)) - 1.
 *
 * `qcoeff` and `dqcoeff` are `int32_t` per the contract and MUST NOT alias.
 * Only `qcoeff[i] != 0` is processed; `dqcoeff[i]` is written for every i.
 *
 * Returns SVT_AV2_QUANT_OK, or SVT_AV2_QUANT_ERR_NULL / _ERR_UNSUPPORTED.
 */
SvtAv2QuantStatus svt_av2_dequantize_block(const SvtAv2QuantParams* params,
                                           const int32_t* qcoeff,
                                           int32_t* dqcoeff,
                                           size_t count,
                                           int plane);

/*
 * The same operation for a single coefficient, without clipping. Exposed so the
 * tests can check the exact rounding/sign of one value and so T012 can reason
 * about mid-block values. `pos_is_dc` selects the DC quantizer.
 */
int32_t svt_av2_dequant_coeff(const SvtAv2QuantParams* params, int32_t qcoeff, int plane, int pos_is_dc);

/* Dequantization clipping bounds used by the decoder for a bit depth. */
int32_t svt_av2_dequant_clip_min(SvtAv2QuantBitDepth bit_depth);
int32_t svt_av2_dequant_clip_max(SvtAv2QuantBitDepth bit_depth);

/* --- forward quantization ------------------------------------------------- */

/*
 * Encoder-owned deterministic forward quantizer, retaining the QP100 rounding
 * decision (factor48). AVM encoder fp tables use factor64, so this is not a
 * claim of identical forward decisions. Decoder reconstruction is normative:
 *
 *   deadzone:  level = 0 when abs_coeff << (1 + log_scale + QUANT_TABLE_BITS)
 *                       < dequant[!!pos]
 *   otherwise: abs_qcoeff = (abs_coeff + round[!!pos]) * quant[!!pos] >> shift
 *              with shift = 16 - log_scale + QUANT_FP_BITS
 *   sign:      qcoeff = (abs_qcoeff ^ sign) - sign
 *   dequant:   same expression as the decoder, so qcoeff and dqcoeff are
 *              generated from one source of truth.
 *
 * The deadzone forces |coeff| below ceil(dequant / 2^(4+log_scale)) to 0;
 * coefficients just above that threshold may still round to level 0.
 */
typedef struct SvtAv2QuantCoeffs {
    /* round[0] = round_DC, round[1] = round_AC at the table shift. Used
     * together with `log_scale` as ROUND_POWER_OF_TWO(round, log_scale), which
     * is what the AVM fp quantizer adds (av2_quantize.c:97-99,114). */
    int32_t round_QTX[2];
    /* quant_fp[0] = quant_DC, quant_fp[1] = quant_AC. Derived by invert_quant()
     * and cross-checked by test_quantization.c against the table formula. */
    int32_t quant_fp_QTX[2];
    /* dequant[0] = DC quantizer, dequant[1] = AC quantizer. */
    int32_t dequant_QTX[2];
    /* Right shift applied to (abs_coeff + round) * quant_fp, i.e. `shift`. */
    int shift;
    /* The transform's own scaling exponent, kept so the rounding term can be
     * scaled exactly as ROUND_POWER_OF_TWO(round, log_scale) at quantize time
     * (av2_quantize.c:97-99). */
    int log_scale;
    /* Scaled rounding used by the deadzone comparison. */
    int32_t zbin_QTX[2];
    /* Deadzone: levels whose |coeff| is below this are forced to 0 before any
     * rounding is added. Equal to dequant >> (1 + log_scale). */
    int32_t deadzone_QTX[2];
    /* Largest absolute level this configuration can emit. Levels above it make
     * the block fail with SVT_AV2_QUANT_ERR_LEVEL_OVERFLOW. */
    int32_t max_level;
} SvtAv2QuantCoeffs;

/*
 * Builds the forward quantizer tables. `log_scale` is the transform's own
 * scaling exponent (0 for area<=256, 1 for 16x32/32x16/32x32, 2 for
 * 32x64/64x32/64x64) and must match the transform's coefficient scale.
 */
SvtAv2QuantStatus svt_av2_quant_build_coeffs(const SvtAv2QuantParams* params,
                                             int log_scale,
                                             int plane,
                                             SvtAv2QuantCoeffs* coeffs);

/*
 * Forward quantizes `coeff` into `qcoeff` and produces the matching `dqcoeff`.
 *
 * Contract points that matter to T011/T012:
 *   - `qcoeff` and `dqcoeff` must be different non-aliasing buffers of `count`
 *     int32_t. Passing the same pointer returns SVT_AV2_QUANT_ERR_UNSUPPORTED:
 *     T011 needs the sign/magnitude of qcoeff while T012 needs dqcoeff, and
 *     overwriting one with the other loses information the syntax requires.
 *   - Every entry of both arrays is written, so a zero block is explicit
 *     (all-zero qcoeff) rather than "whatever was in the buffer".
 *   - `qcoeff[i] == 0` implies `dqcoeff[i] == 0`. The reverse need not hold
 *     because the normative 24-bit product mask can map a nonzero level to 0.
 *   - On SVT_AV2_QUANT_ERR_LEVEL_OVERFLOW the call returns before writing any
 *     output, so the caller cannot accidentally encode a truncated block.
 */
SvtAv2QuantStatus svt_av2_quantize_block(const SvtAv2QuantParams* params,
                                         const int32_t* coeff,
                                         int32_t* qcoeff,
                                         int32_t* dqcoeff,
                                         size_t count,
                                         int plane,
                                         int log_scale);

/*
 * Single-coefficient variant returns the decoder-masked value before clipping.
 * `pos_is_dc` selects the DC
 * quantizer and must be derived from the coefficient POSITION, never from its
 * value: the DC/AC choice is positional on the decoder side too
 * (`dequant[!!coeff_idx]`).
 */
SvtAv2QuantStatus svt_av2_quantize_coeff_idx(const SvtAv2QuantCoeffs* coeffs,
                                             int32_t coeff,
                                             int pos_is_dc,
                                             int32_t* qcoeff,
                                             int32_t* dqcoeff);

/* Convenience wrapper for the DC position (pos_is_dc = 1). */
SvtAv2QuantStatus svt_av2_quantize_coeff(const SvtAv2QuantCoeffs* coeffs,
                                         int32_t coeff,
                                         int32_t* qcoeff,
                                         int32_t* dqcoeff);

/*
 * Reports whether every qcoeff entry is zero, which is the encoder's
 * "all_zero block" signal for T011. Returns 1/0, or -1 on a NULL argument.
 * This is NOT the EOB: scan order and last position are T011's responsibility.
 */
int svt_av2_quant_all_zero(const int32_t* qcoeff, size_t count);

/* --- shared integer helpers (must match AVM exactly) ----------------------- */

/* ROUND_POWER_OF_TWO(value, n) from avm/avm_ports/mem.h. Requires value >= 0 and
 * n >= 0. Kept as a function so tests can pin the behaviour rather than trusting
 * a macro copy. */
int32_t svt_av2_round_power_of_two(int64_t value, int n);

/* Shift used by the forward quantizer: shift = 16 - log_scale + QUANT_FP_BITS. */
int svt_av2_quant_shift(int log_scale);

#ifdef __cplusplus
}
#endif

SvtAv2QuantStatus svt_av2_quant_params_init_tx_bd(SvtAv2QuantParams* params,
    uint32_t base_q_idx, uint32_t tx_width, uint32_t tx_height, uint32_t bit_depth);
#endif /* SVT_AV2_QUANTIZATION_H */
