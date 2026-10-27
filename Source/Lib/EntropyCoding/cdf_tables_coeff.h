/*
 * SVT-AV2 - default coefficient CDF values for M2 (T011).
 *
 * All four coefficient CDF categories are indexed by actual base_qindex.
 * Historical category-1 names and _q2 names remain unchanged; _by_qctx[0..3]
 * selects the complete set. Fixed AVM v1.0.0 commit 966a7d7 provides the values.
 * M5.3 extraction and exact q1/q2 comparison: artifacts/m53-b/extend_cdfs.py.
 *
 * Representation matches cdf_tables.h: forward cumulative boundaries scaled to
 * 32768, one entry per boundary, with the redundant trailing 32768 omitted (see
 * entropy_encoder.h). The AVM tables store `AVM_CDFn(b0..b_{n-2})` per symbol
 * set, so each emitted group has (n-1) boundaries.
 *
 * Spec references (AV2 Bitstream & Decoding Process Specification v1.0.0):
 *   - 5.20.7.27 coeffs(), 5.20.7.28 read_quant()
 *   - 8.3.2 cdf_selection_process: all_zero / eob_pt_* / eob_extra / coeff_base /
 *     coeff_base_eob / coeff_br / dc_sign
 *   - 9.3 default CDF tables
 *
 * Layout of each array (the row/context dimensions the caller must index).
 *
 * IMPORTANT: each row stores (n-1) boundary words for an n-symbol alphabet, so
 * the row stride is `symbols - 1`, NOT `symbols`. The trailing 32768 boundary is
 * implicit. Code that steps rows by the symbol count is wrong from row 1 on.
 * Brackets below give the row shape; the trailing "(n-1 w)" is the row stride:
 *   svt_av2_cdf_eob16      [3 eobCtx][5 syms -> 4 w]
 *   svt_av2_cdf_eob32      [3 eobCtx][6 syms -> 5 w]
 *   svt_av2_cdf_eob64      [3 eobCtx][7 syms -> 6 w]
 *   svt_av2_cdf_eob128     [3 eobCtx][8 syms -> 7 w]
 *   svt_av2_cdf_eob256     [3 eobCtx][8 syms -> 7 w]
 *   svt_av2_cdf_eob512     [3 eobCtx][8 syms -> 7 w]
 *   svt_av2_cdf_eob1024    [3 eobCtx][8 syms -> 7 w]
 *   svt_av2_cdf_eob_extra  [2 syms -> 1 w]
 *   svt_av2_cdf_coeff_base_eob     [TX_SIZES 5][4 ctx][3 syms -> 2 w]
 *   svt_av2_cdf_coeff_base_eob_uv  [4 ctx][3 syms -> 2 w]
 *   svt_av2_cdf_coeff_base_lf_eob  [TX_SIZES 5][4 ctx][5 syms -> 4 w]
 *   svt_av2_cdf_coeff_base_lf_eob_uv [4 ctx][5 syms -> 4 w]
 *   svt_av2_cdf_coeff_base         [TX_SIZES 5][20 ctx][TCQ 2][4 syms -> 3 w]
 *   svt_av2_cdf_coeff_base_uv      [12 ctx][4 syms -> 3 w]
 *   svt_av2_cdf_coeff_base_lf      [TX_SIZES 5][33 ctx][TCQ 2][6 syms -> 5 w]
 *   svt_av2_cdf_coeff_base_lf_uv   [12 ctx][6 syms -> 5 w]
 *   svt_av2_cdf_coeff_br           [7 ctx][4 syms -> 3 w]
 *   svt_av2_cdf_coeff_br_uv        [4 ctx][4 syms -> 3 w]
 *   svt_av2_cdf_coeff_br_lf        [14 ctx][4 syms -> 3 w]
 *   svt_av2_cdf_txb_skip           [2 predModeCtx][TX_SIZES 5][10 ctx][2 syms -> 1 w]
 *   svt_av2_cdf_v_txb_skip         [12 ctx][2 syms -> 1 w]
 *   svt_av2_cdf_dc_sign            [2 planeType][2 hidden][3 ctx][2 syms -> 1 w]
 */
#ifndef SVT_AV2_CDF_TABLES_COEFF_H
#define SVT_AV2_CDF_TABLES_COEFF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Coefficient CDF storage sizes (number of uint16_t boundaries per array). */
#define SVT_AV2_CDF_EOB16_COUNT 12u
#define SVT_AV2_CDF_EOB32_COUNT 15u
#define SVT_AV2_CDF_EOB64_COUNT 18u
#define SVT_AV2_CDF_EOB128_COUNT 21u
#define SVT_AV2_CDF_EOB256_COUNT 21u
#define SVT_AV2_CDF_EOB512_COUNT 21u
#define SVT_AV2_CDF_EOB1024_COUNT 21u
#define SVT_AV2_CDF_EOB_EXTRA_COUNT 1u
#define SVT_AV2_CDF_COEFF_BASE_EOB_COUNT 40u
#define SVT_AV2_CDF_COEFF_BASE_EOB_UV_COUNT 8u
#define SVT_AV2_CDF_COEFF_BASE_LF_EOB_COUNT 80u
#define SVT_AV2_CDF_COEFF_BASE_LF_EOB_UV_COUNT 16u
#define SVT_AV2_CDF_COEFF_BASE_COUNT 600u
#define SVT_AV2_CDF_COEFF_BASE_UV_COUNT 36u
#define SVT_AV2_CDF_COEFF_BASE_LF_COUNT 1650u
#define SVT_AV2_CDF_COEFF_BASE_LF_UV_COUNT 60u
#define SVT_AV2_CDF_COEFF_BR_COUNT 21u
#define SVT_AV2_CDF_COEFF_BR_UV_COUNT 12u
#define SVT_AV2_CDF_COEFF_BR_LF_COUNT 42u
#define SVT_AV2_CDF_TXB_SKIP_COUNT 100u
#define SVT_AV2_CDF_V_TXB_SKIP_COUNT 12u
#define SVT_AV2_CDF_DC_SIGN_COUNT 12u

extern const uint16_t svt_av2_cdf_eob16[SVT_AV2_CDF_EOB16_COUNT];
extern const uint16_t svt_av2_cdf_eob32[SVT_AV2_CDF_EOB32_COUNT];
extern const uint16_t svt_av2_cdf_eob64[SVT_AV2_CDF_EOB64_COUNT];
extern const uint16_t svt_av2_cdf_eob128[SVT_AV2_CDF_EOB128_COUNT];
extern const uint16_t svt_av2_cdf_eob256[SVT_AV2_CDF_EOB256_COUNT];
extern const uint16_t svt_av2_cdf_eob512[SVT_AV2_CDF_EOB512_COUNT];
extern const uint16_t svt_av2_cdf_eob1024[SVT_AV2_CDF_EOB1024_COUNT];
extern const uint16_t svt_av2_cdf_eob_extra[SVT_AV2_CDF_EOB_EXTRA_COUNT];
extern const uint16_t svt_av2_cdf_coeff_base_eob[SVT_AV2_CDF_COEFF_BASE_EOB_COUNT];
extern const uint16_t svt_av2_cdf_coeff_base_eob_uv[SVT_AV2_CDF_COEFF_BASE_EOB_UV_COUNT];
extern const uint16_t svt_av2_cdf_coeff_base_lf_eob[SVT_AV2_CDF_COEFF_BASE_LF_EOB_COUNT];
extern const uint16_t svt_av2_cdf_coeff_base_lf_eob_uv[SVT_AV2_CDF_COEFF_BASE_LF_EOB_UV_COUNT];
extern const uint16_t svt_av2_cdf_coeff_base[SVT_AV2_CDF_COEFF_BASE_COUNT];
extern const uint16_t svt_av2_cdf_coeff_base_uv[SVT_AV2_CDF_COEFF_BASE_UV_COUNT];
extern const uint16_t svt_av2_cdf_coeff_base_lf[SVT_AV2_CDF_COEFF_BASE_LF_COUNT];
extern const uint16_t svt_av2_cdf_coeff_base_lf_uv[SVT_AV2_CDF_COEFF_BASE_LF_UV_COUNT];
extern const uint16_t svt_av2_cdf_coeff_br[SVT_AV2_CDF_COEFF_BR_COUNT];
extern const uint16_t svt_av2_cdf_coeff_br_uv[SVT_AV2_CDF_COEFF_BR_UV_COUNT];
extern const uint16_t svt_av2_cdf_coeff_br_lf[SVT_AV2_CDF_COEFF_BR_LF_COUNT];
extern const uint16_t svt_av2_cdf_txb_skip[SVT_AV2_CDF_TXB_SKIP_COUNT];
extern const uint16_t svt_av2_cdf_v_txb_skip[SVT_AV2_CDF_V_TXB_SKIP_COUNT];
extern const uint16_t svt_av2_cdf_dc_sign[SVT_AV2_CDF_DC_SIGN_COUNT];

extern const uint16_t svt_av2_cdf_eob16_q2[12];
extern const uint16_t svt_av2_cdf_eob32_q2[15];
extern const uint16_t svt_av2_cdf_eob64_q2[18];
extern const uint16_t svt_av2_cdf_eob128_q2[21];
extern const uint16_t svt_av2_cdf_eob256_q2[21];
extern const uint16_t svt_av2_cdf_eob512_q2[21];
extern const uint16_t svt_av2_cdf_eob1024_q2[21];
extern const uint16_t svt_av2_cdf_eob_extra_q2[1];
extern const uint16_t svt_av2_cdf_coeff_base_eob_q2[40];
extern const uint16_t svt_av2_cdf_coeff_base_eob_uv_q2[8];
extern const uint16_t svt_av2_cdf_coeff_base_lf_eob_q2[80];
extern const uint16_t svt_av2_cdf_coeff_base_lf_eob_uv_q2[16];
extern const uint16_t svt_av2_cdf_coeff_base_q2[600];
extern const uint16_t svt_av2_cdf_coeff_base_uv_q2[36];
extern const uint16_t svt_av2_cdf_coeff_base_lf_q2[1650];
extern const uint16_t svt_av2_cdf_coeff_base_lf_uv_q2[60];
extern const uint16_t svt_av2_cdf_coeff_br_q2[21];
extern const uint16_t svt_av2_cdf_coeff_br_uv_q2[12];
extern const uint16_t svt_av2_cdf_coeff_br_lf_q2[42];
extern const uint16_t svt_av2_cdf_txb_skip_q2[100];
extern const uint16_t svt_av2_cdf_v_txb_skip_q2[12];
extern const uint16_t svt_av2_cdf_dc_sign_q2[12];


/* All four coefficient CDF contexts, selected by actual base_qindex. */
extern const uint16_t* const svt_av2_cdf_eob16_by_qctx[4];
extern const uint16_t* const svt_av2_cdf_eob32_by_qctx[4];
extern const uint16_t* const svt_av2_cdf_eob64_by_qctx[4];
extern const uint16_t* const svt_av2_cdf_eob128_by_qctx[4];
extern const uint16_t* const svt_av2_cdf_eob256_by_qctx[4];
extern const uint16_t* const svt_av2_cdf_eob512_by_qctx[4];
extern const uint16_t* const svt_av2_cdf_eob1024_by_qctx[4];
extern const uint16_t* const svt_av2_cdf_eob_extra_by_qctx[4];
extern const uint16_t* const svt_av2_cdf_coeff_base_eob_by_qctx[4];
extern const uint16_t* const svt_av2_cdf_coeff_base_eob_uv_by_qctx[4];
extern const uint16_t* const svt_av2_cdf_coeff_base_lf_eob_by_qctx[4];
extern const uint16_t* const svt_av2_cdf_coeff_base_lf_eob_uv_by_qctx[4];
extern const uint16_t* const svt_av2_cdf_coeff_base_by_qctx[4];
extern const uint16_t* const svt_av2_cdf_coeff_base_uv_by_qctx[4];
extern const uint16_t* const svt_av2_cdf_coeff_base_lf_by_qctx[4];
extern const uint16_t* const svt_av2_cdf_coeff_base_lf_uv_by_qctx[4];
extern const uint16_t* const svt_av2_cdf_coeff_br_by_qctx[4];
extern const uint16_t* const svt_av2_cdf_coeff_br_uv_by_qctx[4];
extern const uint16_t* const svt_av2_cdf_coeff_br_lf_by_qctx[4];
extern const uint16_t* const svt_av2_cdf_txb_skip_by_qctx[4];
extern const uint16_t* const svt_av2_cdf_v_txb_skip_by_qctx[4];
extern const uint16_t* const svt_av2_cdf_dc_sign_by_qctx[4];

/* Symbol counts (nsyms) of each table, i.e. the value the encoder passes to
 * svt_av2_entropy_encode_symbol() together with the corresponding row pointer. */
#define SVT_AV2_SYMS_EOB16 5u
#define SVT_AV2_SYMS_EOB32 6u
#define SVT_AV2_SYMS_EOB64 7u
#define SVT_AV2_SYMS_EOB128 8u
#define SVT_AV2_SYMS_EOB256 8u
#define SVT_AV2_SYMS_EOB512 8u
#define SVT_AV2_SYMS_EOB1024 8u
#define SVT_AV2_SYMS_EOB_EXTRA 2u
#define SVT_AV2_SYMS_COEFF_BASE_EOB 3u
#define SVT_AV2_SYMS_COEFF_BASE_EOB_LF 5u
#define SVT_AV2_SYMS_COEFF_BASE 4u
#define SVT_AV2_SYMS_COEFF_BASE_LF 6u
#define SVT_AV2_SYMS_COEFF_BR 4u
#define SVT_AV2_SYMS_TXB_SKIP 2u
#define SVT_AV2_SYMS_DC_SIGN 2u

#ifdef __cplusplus
}
#endif

#endif /* SVT_AV2_CDF_TABLES_COEFF_H */
