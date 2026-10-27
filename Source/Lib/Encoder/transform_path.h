/*
 * T012: production bind of T009 (transform) + T010 (quantization) to the frame
 * core's SvtAv2IntraTransformPath seam.
 *
 * Why this file exists
 * --------------------
 * intra_frame.c knows the frozen chain
 *   residual -> coeff -> qcoeff -> dqcoeff -> inverse residual
 * but deliberately knows nothing about how the transform or the quantizer are
 * implemented. The seam SvtAv2IntraTransformPath is that boundary. This file is
 * the single place where the real T009/T010 modules are bound to it, so:
 *   - the frame core stays free of T009/T010 headers and of their frozen
 *     constants (sizes, scales, qindex), and
 *   - there is exactly one obvious place to look when the two must agree.
 *
 * The path is a plain value: it carries no allocation of its own, so it costs
 * nothing to have one per frame core and it is trivially reusable.
 *
 * Scope (matches T009/T010 as frozen in this round)
 * -------------------------------------------------
 *   - 8-bit, 4:2:0, DCT_DCT only, All-Intra, single thread.
 *   - The supported block geometry is whatever T009 reports via
 *     svt_av2_tx_supported(); this file never hard-codes a size limit. As of
 *     this round that set is the squares 4/8/16/32/64 plus 4x8/8x4/8x16/16x8/
 *     4x16/16x4. In particular 64x64 IS supported (luma) and 32x32 IS supported
 *     (chroma), which is exactly 00's ruling for the frozen frame header
 *     (tx_mode_select = 0 -> TX_MODE_LARGEST: luma TX_64X64, chroma TX_32X32).
 *     A single top-level PARTITION_NONE at sb_size = 64 therefore encodes, and
 *     is what test_transform_path.c and the acceptance probe exercise.
 *     A block geometry T009 does not support is still reported as
 *     SVT_AV2_ERROR_UNSUPPORTED rather than silently mishandled -- but that is
 *     now a genuine T009 query, not a local <=16x16 cap.
 *   - base_q_idx is frozen to SVT_AV2_QUANT_FROZEN_QINDEX (100).
 *
 * This module does not re-implement any transform, quantizer or rounding: it
 * only marshals arguments. Every numeric decision (including the deadzone, the
 * dequantizer shift and the coefficient clamping) belongs to T009/T010 and is
 * read from their frozen API.
 */
#ifndef SVT_AV2_TRANSFORM_PATH_H
#define SVT_AV2_TRANSFORM_PATH_H

#include <stdint.h>

#include "common.h"
#include "intra_frame.h"
#include "../Quantization/quantization_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Fills `path` with the production T009+T010 binding and stores its state in
 * `state`, which the caller owns and must keep alive for as long as `path` is
 * used (one per frame core).
 *
 * `base_q_idx` must be the frame's base_q_idx and is checked against the frozen
 * value here, so a caller that drifts from the frozen configuration fails at
 * setup rather than at the first block. Returns:
 *   SVT_AV2_ERROR_NONE             path is ready
 *   SVT_AV2_ERROR_INVALID_PARAMETER NULL argument
 *   SVT_AV2_ERROR_UNSUPPORTED      base_q_idx is not the frozen qindex
 *
 * The bound quantizer state is derived per block from that block's own
 * geometry (svt_av2_quant_params_init_tx), because the transform scale depends
 * on the block size; nothing is cached across blocks.
 */
typedef struct SvtAv2TransformPathState {
    /* Chosen qindex, kept so the per-block quantizer init can be re-derived. */
    uint32_t base_q_idx;
    uint32_t bit_depth;
    /* Counters, so a caller (or a test) can prove the path actually ran. */
    uint64_t forward_calls;
    uint64_t inverse_calls;
    SvtAv2QuantizeBlockFn quantize_block;
} SvtAv2TransformPathState;

SvtAv2Error svt_av2_transform_path_init(SvtAv2TransformPathState* state,
                                        SvtAv2IntraTransformPath* path,
                                        uint32_t base_q_idx);

/*
 * Reports whether the transform path supports a block of block_w x block_h
 * samples. Exposed so the frame core (or a caller configuring sb_size) can
 * reject an unusable geometry up front instead of failing on the first block.
 */
int svt_av2_transform_path_supports(uint32_t block_w, uint32_t block_h);

/* Exact memoization is only valid for this immutable production binding. */
int svt_av2_transform_path_cacheable(const SvtAv2IntraTransformPath* path,
    uint32_t base_q_idx, uint32_t bit_depth);

#ifdef __cplusplus
}
#endif

SvtAv2Error svt_av2_transform_path_init_bd(SvtAv2TransformPathState* state,
    SvtAv2IntraTransformPath* path, uint32_t base_q_idx, uint32_t bit_depth);
#endif /* SVT_AV2_TRANSFORM_PATH_H */
