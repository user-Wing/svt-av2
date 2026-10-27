/*
 * SVT-AV2 - default context CDF values for the M1 minimal tile (T005).
 *
 * Scope: only the CDF arrays the M1 single-frame path actually reads. The
 * values are copied verbatim from the AV2 v1.0.0 specification attachments
 * (Annex, "Default_*_Cdf" headers); they are NOT shared with the decoder.
 *
 * Representation: each array here is the forward cumulative distribution used
 * by spec 8.2 read_symbol(), i.e. cdf[k] is the cumulative frequency of symbols
 * 0..k scaled to 32768, with cdf[nsyms-1] == 32768. The attachment files store
 * one boundary value per array entry plus the adaptation rate and use count,
 * so only the boundary values are tabulated here. Adaptation is not modelled:
 * the frozen M1 frame header sets disable_cdf_update = 1.
 *
 * Spec references (AV2 Bitstream & Decoding Process Specification v1.0.0):
 *   - 9.3 default CDF tables (init_coeff_cdfs / init_non_coeff_cdfs)
 *   - 8.2.2 init_symbol(), 8.3.2 cdf_selection_process()
 *   - 6.19.3.2 read_partition() do_split CDF selection
 *   - 6.19.7.23 coeffs() all_zero CDF selection
 *   - 5.20.5.5 read_intra_y_mode()/read_intra_uv_mode() CDF selection
 */
#ifndef SVT_AV2_CDF_TABLES_H
#define SVT_AV2_CDF_TABLES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Number of entries in a "TileXxxCdf" array. These forward-CDF arrays carry one
 * boundary per symbol and omit the redundant trailing 32768; the encoder derives
 * the last interval's upper boundary itself. The value is nsyms. */
#define SVT_AV2_CDF_Y_MODE_SET_SYMS         4u /* INTRA_MODE_SETS */
#define SVT_AV2_CDF_Y_MODE_INDEX_SYMS       7u /* MODE_INDEX_COUNT */
#define SVT_AV2_CDF_UV_MODE_SYMS            7u /* CHROMA_MODE_COUNT */
#define SVT_AV2_CDF_DO_SPLIT_SYMS           2u
#define SVT_AV2_CDF_TXB_SKIP_SYMS           2u

/* Forward CDFs (nsyms entries; the implicit last boundary is 32768). */

/* Default_Y_Mode_Set_Cdf (4 symbols). */
extern const uint16_t svt_av2_cdf_y_mode_set[SVT_AV2_CDF_Y_MODE_SET_SYMS];

/* Default_Y_Mode_Index_Cdf[0] (7 symbols). */
extern const uint16_t svt_av2_cdf_y_mode_index_0[SVT_AV2_CDF_Y_MODE_INDEX_SYMS];

/* Default_Uv_Mode_Cfl_Not_Allowed_Cdf[0] (7 symbols). */
extern const uint16_t svt_av2_cdf_uv_mode_cfl_not_allowed_0[SVT_AV2_CDF_UV_MODE_SYMS];
extern const uint16_t svt_av2_cdf_uv_mode_cfl_not_allowed_1[SVT_AV2_CDF_UV_MODE_SYMS];

/* Default_Do_Split_Cdf[0][12] (2 symbols): BLOCK_64X64 partition context. */
extern const uint16_t svt_av2_cdf_do_split_ctx12[SVT_AV2_CDF_DO_SPLIT_SYMS];

/* Default_Txb_Skip_Cdf[qidx][0][txSzCtx][ctx] for base_q_idx = 100 (qidx = 1):
 * luma TX_64X64 -> txSzCtx 4, ctx 0; chroma TX_32X32 -> txSzCtx 3, ctx 6. */
extern const uint16_t svt_av2_cdf_txb_skip_q1_txsz4_ctx0[SVT_AV2_CDF_TXB_SKIP_SYMS];
extern const uint16_t svt_av2_cdf_txb_skip_q1_txsz3_ctx6[SVT_AV2_CDF_TXB_SKIP_SYMS];

/* Default_V_Txb_Skip_Cdf[1][0] (V_TXB_SKIP_CONTEXTS is per-plane, ctx 0). */
extern const uint16_t svt_av2_cdf_v_txb_skip_q1_ctx0[SVT_AV2_CDF_TXB_SKIP_SYMS];

/* Maps base_q_idx to the COEFF_CDF_Q_CTXS index used by init_coeff_cdfs():
 * <= 90 -> 0, <= 140 -> 1, <= 190 -> 2, else 3 (spec 6.17). */
uint32_t svt_av2_cdf_coeff_q_ctx(uint32_t base_q_idx);

#ifdef __cplusplus
}
#endif

#endif /* SVT_AV2_CDF_TABLES_H */
