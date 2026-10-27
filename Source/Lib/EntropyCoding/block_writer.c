/*
 * SVT-AV2 - M1 minimal intra block writer (T005). See block_writer.h for the
 * symbol list, the freezing rationale and the spec references.
 */
#include "block_writer.h"

#include "cdf_tables.h"

int svt_av2_block_write_minimal_intra(SvtAv2EntropyEncoder* enc,
                                      uint32_t              base_q_idx) {
    if (enc == NULL) {
        return SVT_AV2_ENTROPY_ERROR_NULL;
    }

    /* CDF selection is only valid for the M1 coefficient context. The M1 frame
     * header uses base_q_idx = 100 (11.1.1 / docs/bitstream-frame-minimal.md),
     * which selects COEFF_CDF_Q_CTXS index 1; the tabulated all_zero CDFs are
     * for that index. Reject anything else rather than silently coding with a
     * mismatched CDF. */
    if (svt_av2_cdf_coeff_q_ctx(base_q_idx) != 1u) {
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    }

    int st;

    /* do_split = 0 -> PARTITION_NONE (6.19.3.2). Context for BLOCK_64X64 with
     * both neighbours at BLOCK_256X256 is 12. */
    if ((st = svt_av2_entropy_encode_symbol(
             enc, svt_av2_cdf_do_split_ctx12, SVT_AV2_CDF_DO_SPLIT_SYMS, 0u)) !=
        SVT_AV2_ENTROPY_OK) {
        return st;
    }

    /* y_mode_set = 0 then y_mode_index = 0 -> modeIdx 0 -> YMode = DC_PRED
     * (5.20.5.5). Contexts are 0 (no usable neighbours). */
    if ((st = svt_av2_entropy_encode_symbol(
             enc, svt_av2_cdf_y_mode_set, SVT_AV2_CDF_Y_MODE_SET_SYMS, 0u)) !=
        SVT_AV2_ENTROPY_OK) {
        return st;
    }
    if ((st = svt_av2_entropy_encode_symbol(
             enc, svt_av2_cdf_y_mode_index_0, SVT_AV2_CDF_Y_MODE_INDEX_SYMS,
             0u)) != SVT_AV2_ENTROPY_OK) {
        return st;
    }

    /* uv_mode = 0 -> UVMode = DC_PRED (5.20.5.5). Context 0 (YMode is not a
     * directional mode). */
    if ((st = svt_av2_entropy_encode_symbol(
             enc, svt_av2_cdf_uv_mode_cfl_not_allowed_0,
             SVT_AV2_CDF_UV_MODE_SYMS, 0u)) != SVT_AV2_ENTROPY_OK) {
        return st;
    }

    /* Three all_zero = 1 symbols: luma TX_64X64 (txSzCtx 4, ctx 0), chroma U
     * TX_32X32 (txSzCtx 3, ctx 6) and chroma V TX_32X32 (ctx 0, plane-2 CDF). */
    if ((st = svt_av2_entropy_encode_symbol(
             enc, svt_av2_cdf_txb_skip_q1_txsz4_ctx0,
             SVT_AV2_CDF_TXB_SKIP_SYMS, 1u)) != SVT_AV2_ENTROPY_OK) {
        return st;
    }
    if ((st = svt_av2_entropy_encode_symbol(
             enc, svt_av2_cdf_txb_skip_q1_txsz3_ctx6,
             SVT_AV2_CDF_TXB_SKIP_SYMS, 1u)) != SVT_AV2_ENTROPY_OK) {
        return st;
    }
    if ((st = svt_av2_entropy_encode_symbol(
             enc, svt_av2_cdf_v_txb_skip_q1_ctx0, SVT_AV2_CDF_TXB_SKIP_SYMS,
             1u)) != SVT_AV2_ENTROPY_OK) {
        return st;
    }

    return SVT_AV2_ENTROPY_OK;
}
