/*
 * SVT-AV2 transform (M2, T009): scalar forward / inverse integer transforms.
 *
 * Scope frozen for this round (see docs/m1-m2-contracts.md section 3 and the
 * T009 task file):
 *   - 8-bit, 4:2:0, single thread, All-Intra, DC_PRED.
 *   - Primary transform type: DCT_DCT only (2-D separable DCT-II). No ADST,
 *     IDTX, DDTX/FDDT, no secondary transform, no cross-chroma transform.
 *   - Transform sizes (frozen, ALL IMPLEMENTED): 4x4, 8x8, 16x16 and the
 *     non-square pairs whose side lengths are all <= 16 (4x8, 8x4, 4x16, 16x4,
 *     8x16, 16x8), plus the AV2-M2 path-(A) pair TX_32X32 (chroma) and
 *     TX_64X64 (luma). [SVTAV2] Query svt_av2_tx_supported() for the
 *     authoritative set at runtime rather than inferring it from this list.
 *     TX_64X64 is deliberately asymmetric between directions: the inverse
 *     clamps its working kernel to 32x32 and replicates each sample 2x2
 *     (idct.c:648-649, :730-751), while the forward runs a real 64-point kernel,
 *     zeroes the high half and re-packs the surviving 32x32 low-frequency grid
 *     into the first 1024 indices (hybrid_fwd_txfm.c:952-1008). See the
 *     coefficient-layout contract on svt_av2_tx_coeff_count(): for a geometry
 *     with a side > 32 the carried array is the packed grid, NOT a w-wide
 *     raster and NOT w*h long (TX_64X64 -> 1024 items, row stride 32).
 *   - Also supports TX_16X32/32X16/32X64/64X32/8X32/32X8/16X64/64X16.
 *     Other rectangles (TX_8X64/64X8/4X64/64X4/...) remain unsupported.
 *     Nothing in the M2 frame path emits them (tx_mode TX_MODE_LARGEST with
 *     64x64 PARTITION_NONE yields square 64x64 luma / 32x32 chroma).
 *
 * IMPORTANT - this is not AV1. The AV2 inverse primary transform is a direct
 * matrix multiply against `tx_kernel_dct2_sizeN[INV_TXFM]`, not the AV1
 * butterfly IDCT. Do not "reuse" an AV1 kernel because the function names look
 * familiar. Anchors:
 *   - avm/av2/common/txb_common.c: const int tx_kernel_dct2_size{4,8,16,32,64}[..]
 *   - avm/av2/common/idct.c: inv_txfm_c(), inv_txfm_dct2_sizeN_c()
 *   - avm/av2/common/common_data.h: inv_tx_shift[TX_SIZES_ALL][2]
 *   - avm/av2/encoder/hybrid_fwd_txfm.c: fwd_txfm(), fwd_txfm_dct2_sizeN_c()
 *   - avm/av2/encoder/common_data.h: fwd_tx_shift[TX_SIZES_ALL][2]
 *   - avm/av2/common/blockd.h: av2_get_max_eob() (TX_64X64 -> 1024)
 *
 * Ownership (docs/m1-m2-paths.md section 2.1): everything under
 * Source/Lib/Transform/ belongs to T009.
 *
 * Consumers:
 *   - T010 quantizes `coeff` into `qcoeff`/`dqcoeff`.
 *   - T011 writes coefficient syntax from `qcoeff` (scan/EOB belong there).
 *   - T012 rebuilds from `residual` produced by svt_av2_inv_transform().
 *
 * Coefficient layout (frozen for T010/T011/T012):
 *   raster / row-major, `coeff[y * width + x]`, index 0 is DC.
 *   This is the caller-facing layout. The AVM kernels index their matrices
 *   column-major (`tx_mat[freq * size + k]`); the transposition is internal to
 *   this module and is never exposed.
 */
#ifndef SVT_AV2_TRANSFORM_H
#define SVT_AV2_TRANSFORM_H

#include <stddef.h>
#include <stdint.h>

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- frozen constants ------------------------------------------------------ */

/*
 * The transform coefficient scale exponent for this geometry, governed by the
 * quantizer (NOT applied at this module's output). Mirrored from
 *
 * avm/av2/common/idct.c:av2_get_tx_scale():  return (pels > 256) + (pels > 1024);
 *
 * The descaling lives in the quantizer (encodemb.c:376 forward, decodetxb.c:526/673 inverse);
 * svt_av2_fwd_transform() does not shift its output by it. Every size with
 * pels <= 256 (all the <=16x16 geometries) yields 0; TX_32X32 yields 1 and
 * TX_64X64 yields 2. It is exposed because T010's svt_av2_quant_params_init()
 * takes the same quantity and the two modules must not drift apart.
 *
 * CALLER CONVENTION (do not pass the carried grid): width/height are the
 * ORIGINAL transform geometry, matching AVM's tx_size_2d[tx_size] on which
 * av2_get_tx_scale() is defined. In particular TX_64X64 must be passed as
 * (64, 64) -> 2, NOT as the 32x32 coefficient grid it is packed into, which
 * would give 1 and under-shift the dequantizer by one bit (doubling every
 * dqcoeff) with no error reported. This is easy to get wrong because
 * svt_av2_tx_coeff_count(64, 64) == svt_av2_tx_coeff_count(32, 32) == 1024:
 * the coefficient COUNT cannot distinguish the two, only the geometry can.
 * (Reported by T010; AVM semantics confirmed at idct.c:761-765.)
 */
int svt_av2_tx_scale(uint32_t width, uint32_t height);

/*
 * The quantizer's log_scale for the frozen configuration.
 *
 * This is deliberately the SAME quantity as svt_av2_tx_scale(), not an
 * independent rule derived from the bit depth:
 *
 *   avm/av2/encoder/encodemb.c:819   qparam->log_scale = av2_get_tx_scale(tx_size);
 *   avm/avm_dsp/quantize.c:68,77     ... >> (16 - log_scale + AVM_QM_BITS)
 *                                    ... ROUND_POWER_OF_TWO_64(abs*dequant, 3) >> log_scale
 *
 * which is exactly the dequantizer step T010 implemented
 * (svt_av2_dequant_coeff: `round_power_of_two(q * dequant, 3); >>= tx_scale`).
 * Keeping one function for both prevents the transform and the quantizer from
 * drifting apart; a second, separately "derived" rule would be a latent bug.
 */
int svt_av2_tx_log_scale(uint32_t width, uint32_t height);

/*
 * In-place safe binding used by the inverse path: AVM documents that the inverse
 * transform may read and write the same buffer (idct.c comment at
 * av2_lossless_inv_idtx_add_c). The implementation allocates its own scratch, so
 * `residual == NULL` is not allowed; the guarantee is only that `dqcoeff` and
 * `residual` may point at different buffers and no caller-visible aliasing
 * restriction applies.
 */

#define SVT_AV2_TX_MAX_DIM 64
#define SVT_AV2_TX_MAX_PELS (SVT_AV2_TX_MAX_DIM * SVT_AV2_TX_MAX_DIM)

/* --- status ---------------------------------------------------------------- */

typedef enum SvtAv2TransformStatus {
    SVT_AV2_TX_OK = 0,
    /* A required pointer was NULL. */
    SVT_AV2_TX_ERR_NULL = -1,
    /* Unsupported geometry for this round: zero, a non-power-of-two side, or a
     * side pair outside the frozen set. Note the frozen set DOES include the
     * 32x32 and 64x64 squares (path (A)); what is excluded is the 32/64
     * rectangle family (TX_16X32, TX_16X64, ...). Query svt_av2_tx_supported()
     * rather than assuming any bound on the side length. */
    SVT_AV2_TX_ERR_UNSUPPORTED = -2,
    /* An input residual fell outside [-255, 255]. The forward transform does not
     * clamp in AVM, so accepting a larger value would silently produce a
     * coefficient scale the quantizer was not sized for. Refuse instead. */
    SVT_AV2_TX_ERR_RANGE = -3
} SvtAv2TransformStatus;

/* --- geometry helpers ------------------------------------------------------ */

/* 1 when the (width, height) pair is supported by this round. */
int svt_av2_tx_supported(uint32_t width, uint32_t height);

/* Number of coefficients for a supported geometry, or 0 when unsupported.
 * This is av2_get_max_eob() = min(width, 32) * min(height, 32): for the 64
 * dimensions AVM carries only the 1024-coefficient 32x32 low-frequency grid
 * (blockd.h), which is also the length the forward transform emits and the
 * inverse transform consumes. For width, height <= 32 it equals width*height. */
size_t svt_av2_tx_coeff_count(uint32_t width, uint32_t height);

/* --- inverse transform (decoder-specified process) ------------------------- */

/*
 * Inverse primary transform, the decoder's specified process reduced to
 * DCT_DCT, 8-bit, at the frozen sizes.
 *
 * `dqcoeff` is the dequantized coefficient block, row-major, index 0 = DC,
 * int32_t per docs/m1-m2-contracts.md section 3. It holds
 * svt_av2_tx_coeff_count(width, height) entries: width*height normally, 1024 for
 * TX_64X64 (whose working kernel is 32x32 and is then upsampled 2x2).
 * `residual` receives the inverse-transformed residual, row-major, `width *
 * height` entries, int16_t. The range of the output is exactly the AVM
 * `col_rng_min..col_rng_max` clamp for 8-bit, i.e. [-256, 255]; the values are
 * NOT clipped to [0,255] because this stage produces a residual, not pixels.
 *
 * Adding the prediction and clipping to [0,255] is T012's job.
 *
 * Returns SVT_AV2_TX_OK, or _ERR_NULL / _ERR_UNSUPPORTED.
 */
SvtAv2TransformStatus svt_av2_inv_transform(const int32_t* dqcoeff,
                                            int16_t* residual,
                                            uint32_t width,
                                            uint32_t height);

/* --- forward transform (strategy, not specified by the decoder) ------------ */

/*
 * Forward primary transform matching the AVM encoder's integer DCT-II path for
 * DCT_DCT at the frozen sizes (avm/av2/encoder/hybrid_fwd_txfm.c).
 *
 * `residual` is the signed residual block, row-major, `width * height` entries,
 * int16_t (docs/m1-m2-contracts.md section 2.3: range -255..255 for 8-bit).
 * `coeff` receives the transform coefficients, row-major, index 0 = DC,
 * svt_av2_tx_coeff_count(width, height) entries, int32_t: width*height normally,
 * 1024 for TX_64X64 (AVM computes a full 64-point DCT-II and then keeps only the
 * 32x32 low-frequency grid, hybrid_fwd_txfm.c:1003-1008).
 *
 * Correctness note (task file "correctness boundary"): the forward transform is NOT the
 * unique specified process. A bitstream is legal when its coefficients satisfy
 * the decoder's inverse process, not when they were produced by one particular
 * forward transform. This implementation reproduces the AVM encoder's integer
 * DCT-II so that it is byte-comparable with the reference encoder, which is a
 * stronger property than "legal". Overflow of the coefficient magnitude is not
 * possible for the frozen input range and sizes (checked by the tests); the
 * AVM forward path has no clamp and neither does this one.
 *
 * Returns SVT_AV2_TX_OK, _ERR_NULL, _ERR_UNSUPPORTED, or _ERR_RANGE when an
 * input residual is outside [-255, 255].
 */
SvtAv2TransformStatus svt_av2_fwd_transform(const int16_t* residual,
                                            int32_t* coeff,
                                            uint32_t width,
                                            uint32_t height);

/* --- exposed building blocks (for tests and for T012 diagnostics) ---------- */

/* The transpose-free shift pair used by each direction, exactly as AVM's
 * fwd_tx_shift[]/inv_tx_shift[] index them for the sizes this round supports.
 * Returns 0 on success and fills first/second; returns -1 when unsupported. */
int svt_av2_tx_fwd_shifts(uint32_t width, uint32_t height, int* first, int* second);
int svt_av2_tx_inv_shifts(uint32_t width, uint32_t height, int* first, int* second);

/* The 1-D DCT-II kernel coefficient matrix for a supported side length, in the
 * module's own row-major convention: `matrix[freq * size + sample]`. Pointers
 * stay valid for the lifetime of the program. Returns NULL for an unsupported
 * length. Exposed so the tests can pin the transcription rather than trusting a
 * copy of the constants. */
const int* svt_av2_tx_dct2_matrix(uint32_t size);

/* 1-D building blocks in the module's row-major convention. `line` is the
 * stride between successive vectors of the transform direction; `count` is the
 * number of vectors to process. Used by tests to pin rounding and shifting.
 *
 * `shift` must be >= 1 (AVM's forward variant tolerates shift == 0; the frozen
 * sizes never produce it, so this module rejects it). */
void svt_av2_tx_fwd_1d(const int* src, int* dst, int shift, uint32_t size, uint32_t count);
void svt_av2_tx_inv_1d(const int* src, int* dst, int shift, uint32_t size, uint32_t count,
                       int coef_min, int coef_max);

/*
 * AVM's round_shift(): `(value + (1 << (bit - 1))) >> bit` with an arithmetic
 * (floor) right shift on signed input. This is the rounding the transform uses
 * everywhere; it is NOT round-half-away-from-zero and is NOT the same expression
 * as T010's ROUND_POWER_OF_TWO_64 in the negative case, even though the two
 * happen to agree there. Exposed so the tests pin the sign behaviour.
 */
int32_t svt_av2_tx_round_shift(int64_t value, int bit);

/* Constants the tests need to assert against. */
#define SVT_AV2_TX_NEW_SQRT2 5793    /* forward sqrt2 normalisation, 2^12*sqrt(2) */
#define SVT_AV2_TX_NEW_INV_SQRT2 2896 /* inverse sqrt2 normalisation, 2^12/sqrt(2) */
#define SVT_AV2_TX_SQRT2_BITS 12

#ifdef __cplusplus
}
#endif

SvtAv2TransformStatus svt_av2_fwd_transform_bd(const int16_t* residual, int32_t* coeff,
    uint32_t width, uint32_t height, uint32_t bit_depth);
SvtAv2TransformStatus svt_av2_inv_transform_bd(const int32_t* dqcoeff, int16_t* residual,
    uint32_t width, uint32_t height, uint32_t bit_depth);
/* Single-picture primary subset: 0 DCT_DCT, 1 ADST_DCT, 2 DCT_ADST,
 * 3 tuned ADST_ADST.
 * Type 3 supports only square 4/8/16; no AV1 kernels, DDT or IST. */
SvtAv2TransformStatus svt_av2_fwd_transform_type_bd(const int16_t* residual,
    int32_t* coeff, uint32_t width, uint32_t height, uint32_t bit_depth, uint32_t tx_type);
SvtAv2TransformStatus svt_av2_inv_transform_type_bd(const int32_t* dqcoeff,
    int16_t* residual, uint32_t width, uint32_t height, uint32_t bit_depth, uint32_t tx_type);
#endif /* SVT_AV2_TRANSFORM_H */
