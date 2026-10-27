/*
 * T012: production bind of T009 (transform) + T010 (quantization).
 * See transform_path.h for why this file exists and what it does NOT do.
 *
 * This file adds no arithmetic of its own: it re-derives the per-block quantizer
 * state from T010 and calls T009's forward/inverse transform. The coefficient
 * layout is T009's OWN packed layout and is passed through UNCHANGED: for a
 * block side > 32 T009 carries only the top-left 32x32 low-frequency grid
 * (AVM av2_get_max_eob), re-packed into the first `min(w,32) * min(h,32)`
 * entries with row stride `min(w,32)`. That is exactly the contract the frame
 * core and T011 use (m1-m2-contracts.md section 3: "row stride =
 * min(w,32)", TX_64X64 -> 1024 entries, not 4096), so there is nothing to
 * convert -- converting to a (side x side) raster here would be a silent
 * mis-index for T011's scan, which is built on the 32-wide grid. No
 * coefficient is moved, copied or zero-filled in this file.
 */
#include "transform_path.h"

#include <string.h>

#include "picture.h"
#include "quantization.h"
#include "transform.h"

/* T009's packed coefficient exchange width for a block side. */
static uint32_t path_kernel_width(uint32_t side) { return (side > 32u) ? 32u : side; }

/* --- forward: residual -> coeff -> qcoeff -> dqcoeff ---------------------- */

static SvtAv2Error path_forward_type(void* user,
                                uint32_t plane,
                                uint32_t block_w,
                                uint32_t block_h,
                                const int16_t* residual,
                                uint32_t stride,
                                int32_t* coeff,
                                int32_t* qcoeff,
                                int32_t* dqcoeff,
                                int* all_zero_out, uint32_t tx_type) {
    SvtAv2TransformPathState* state = (SvtAv2TransformPathState*)user;
    if (state == NULL || residual == NULL || coeff == NULL || qcoeff == NULL || dqcoeff == NULL ||
        all_zero_out == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (plane > SVT_AV2_PLANE_V || !svt_av2_transform_path_supports(block_w, block_h)) {
        return SVT_AV2_ERROR_UNSUPPORTED;
    }
    if (stride < block_w) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    /* T010 re-derives the quantizer for this block's own geometry; nothing is
     * cached, so a later change of block size cannot read a stale quantizer. */
    SvtAv2QuantParams params;
    if (svt_av2_quant_params_init_tx_bd(&params, state->base_q_idx, block_w, block_h, state->bit_depth) !=
        SVT_AV2_QUANT_OK) {
        return SVT_AV2_ERROR_UNSUPPORTED;
    }
    const int log_scale = svt_av2_tx_log_scale(block_w, block_h);

    /* Pack the strided residual block for the transform (block-local, so the
     * top-left of the block is sample (0,0) of the transform input). */
    int16_t packed_res[SVT_AV2_TX_MAX_PELS];
    for (uint32_t i = 0; i < block_h; ++i) {
        memcpy(packed_res + (size_t)i * block_w,
               residual + (size_t)i * stride,
               (size_t)block_w * sizeof(int16_t));
    }

    /* T009 writes the kernel-layout coefficients into a private buffer; the
     * caller-facing coefficient arrays use that SAME packed layout, so the
     * first `count` entries are copied out below (contract section 3). */
    int32_t packed_coeff[SVT_AV2_TX_MAX_PELS];
    if (svt_av2_fwd_transform_type_bd(packed_res, packed_coeff, block_w, block_h, state->bit_depth, tx_type) != SVT_AV2_TX_OK) {
        return SVT_AV2_ERROR_UNSUPPORTED;
    }

    const uint32_t kw = path_kernel_width(block_w);
    const uint32_t kh = path_kernel_width(block_h);
    const size_t count = (size_t)kw * kh; /* what T009/T010 actually carry */

    memcpy(coeff, packed_coeff, count * sizeof(int32_t));

    /* Quantise the carried grid: the count must be the carried count so the
     * qcoeff/dqcoeff contract is the same one T011 reads (row stride min(w,32)). */
    if (state->quantize_block(&params, packed_coeff, qcoeff, dqcoeff, count, (int)plane,
                               log_scale) != SVT_AV2_QUANT_OK) {
        /* Includes LEVEL_OVERFLOW: refuse rather than emit a truncated block. */
        return SVT_AV2_ERROR_UNSUPPORTED;
    }

    ++state->forward_calls;
    *all_zero_out = svt_av2_quant_all_zero(qcoeff, count) == 1;
    return SVT_AV2_ERROR_NONE;
}

/* --- inverse: dqcoeff -> inverse residual --------------------------------- */

static SvtAv2Error path_inverse_type(void* user,
                                uint32_t plane,
                                uint32_t block_w,
                                uint32_t block_h,
                                const int32_t* dqcoeff,
                                uint32_t stride,
                                int16_t* inverse_residual, uint32_t tx_type) {
    SvtAv2TransformPathState* state = (SvtAv2TransformPathState*)user;
    if (state == NULL || dqcoeff == NULL || inverse_residual == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (plane > SVT_AV2_PLANE_V || !svt_av2_transform_path_supports(block_w, block_h)) {
        return SVT_AV2_ERROR_UNSUPPORTED;
    }
    if (stride < block_w) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    /* RDOQ or replay may have changed the coefficients since forward. Inspect
     * the final carried grid, and preserve row padding and call accounting. */
    if (tx_type == 0u && (state->bit_depth == 8u || state->bit_depth == 10u) &&
        svt_av2_quant_all_zero(dqcoeff,
            (size_t)path_kernel_width(block_w) * path_kernel_width(block_h)) == 1) {
        for (uint32_t y = 0u; y < block_h; ++y)
            memset(inverse_residual + (size_t)y * stride, 0, (size_t)block_w * sizeof(int16_t));
        ++state->inverse_calls;
        return SVT_AV2_ERROR_NONE;
    }

    /* `dqcoeff` arrives in T009's packed layout (row stride min(w,32),
     * `kw * kh` entries) because `forward` passed it through unchanged, so
     * T009's inverse reader can consume it in place -- no gather, no copy, and
     * no scratch buffer that would need a zero-fill. */
    int16_t packed_res[SVT_AV2_TX_MAX_PELS];
    if (svt_av2_inv_transform_type_bd(dqcoeff, packed_res, block_w, block_h, state->bit_depth, tx_type) != SVT_AV2_TX_OK) {
        return SVT_AV2_ERROR_UNSUPPORTED;
    }

    /* Scatter the residual rows back into the strided block buffer. The residual
     * is a sample grid, so it does use the block-local stride. */
    for (uint32_t i = 0; i < block_h; ++i) {
        memcpy(inverse_residual + (size_t)i * stride,
               packed_res + (size_t)i * block_w,
               (size_t)block_w * sizeof(int16_t));
    }

    ++state->inverse_calls;
    return SVT_AV2_ERROR_NONE;
}

static SvtAv2Error path_forward(void* user, uint32_t plane, uint32_t w, uint32_t h,
    const int16_t* residual, uint32_t stride, int32_t* coeff, int32_t* qcoeff,
    int32_t* dqcoeff, int* all_zero) {
    return path_forward_type(user, plane, w, h, residual, stride, coeff, qcoeff, dqcoeff, all_zero, 0u);
}

static SvtAv2Error path_inverse(void* user, uint32_t plane, uint32_t w, uint32_t h,
    const int32_t* dqcoeff, uint32_t stride, int16_t* residual) {
    return path_inverse_type(user, plane, w, h, dqcoeff, stride, residual, 0u);
}

int svt_av2_transform_path_supports(uint32_t block_w, uint32_t block_h) {
    return svt_av2_tx_supported(block_w, block_h) != 0;
}

SvtAv2Error svt_av2_transform_path_init(SvtAv2TransformPathState* state,
                                        SvtAv2IntraTransformPath* path,
                                        uint32_t base_q_idx) {
    return svt_av2_transform_path_init_bd(state, path, base_q_idx, 8);
}
SvtAv2Error svt_av2_transform_path_init_bd(SvtAv2TransformPathState* state,
    SvtAv2IntraTransformPath* path, uint32_t base_q_idx, uint32_t bit_depth) {
    if (state == NULL || path == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    /* Fail at setup, not on the first block, if the caller drifts from the
     * frozen configuration. T010 owns this check; we do not duplicate the
     * constant. */
    SvtAv2QuantParams probe;
    if (svt_av2_quant_params_init_tx_bd(&probe, base_q_idx, 4, 4, bit_depth) != SVT_AV2_QUANT_OK) {
        return SVT_AV2_ERROR_UNSUPPORTED;
    }

    memset(state, 0, sizeof(*state));
    state->base_q_idx = base_q_idx;
    state->bit_depth = bit_depth;
    state->quantize_block = svt_av2_quantize_block_select();

    path->user = state;
    path->forward = path_forward;
    path->inverse = path_inverse;
    path->forward_type = path_forward_type;
    path->inverse_type = path_inverse_type;
    path->name = "T009/T010 production (DCT_DCT, 4x4..64x64, q=100)";
    return SVT_AV2_ERROR_NONE;
}


int svt_av2_transform_path_cacheable(const SvtAv2IntraTransformPath* path,
    uint32_t base_q_idx, uint32_t bit_depth) {
    if (path==NULL || path->user==NULL || path->forward!=path_forward ||
        path->forward_type!=path_forward_type || path->inverse!=path_inverse ||
        path->inverse_type!=path_inverse_type) return 0;
    const SvtAv2TransformPathState* state=(const SvtAv2TransformPathState*)path->user;
    return state->base_q_idx==base_q_idx && state->bit_depth==bit_depth &&
        state->quantize_block==svt_av2_quantize_block_select();
}
