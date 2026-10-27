/*
 * SVT-AV2 - default context CDF values for the M1 minimal tile (T005).
 * See cdf_tables.h for provenance, representation and spec references.
 *
 * Every value below is copied from the AV2 v1.0.0 specification attachment for
 * the named table. The attachment stores triplets {boundary, rate, count}; only
 * the boundary values are reproduced (the trailing 32768 is implicit).
 */
#include "cdf_tables.h"

/* Default_Y_Mode_Set_Cdf[INTRA_MODE_SETS + 1]
 *   {28863, 31022, 31724, 93, 0} -> boundaries 28863, 31022, 31724. */
const uint16_t svt_av2_cdf_y_mode_set[SVT_AV2_CDF_Y_MODE_SET_SYMS] = {
    28863, 31022, 31724
};

/* Default_Y_Mode_Index_Cdf[0][MODE_INDEX_COUNT + 1] */
const uint16_t svt_av2_cdf_y_mode_index_0[SVT_AV2_CDF_Y_MODE_INDEX_SYMS] = {
    15175, 20075, 21728, 24098, 26405, 27655, 28860
};

/* Default_Uv_Mode_Cfl_Not_Allowed_Cdf[0][CHROMA_MODE_COUNT + 1] */
const uint16_t
    svt_av2_cdf_uv_mode_cfl_not_allowed_0[SVT_AV2_CDF_UV_MODE_SYMS] = {
        9363, 20957, 22865, 24753, 26411, 27983, 30428
    };

/* Fixed AVM v1.0.0 default_uv_mode_cdf[1], directional luma context. */
const uint16_t svt_av2_cdf_uv_mode_cfl_not_allowed_1[SVT_AV2_CDF_UV_MODE_SYMS] = {
    21282, 23610, 28208, 29311, 30348, 31158, 31491
};

/* Default_Do_Split_Cdf[0][12] = {20492, 16, 0} -> boundary 20492. */
const uint16_t svt_av2_cdf_do_split_ctx12[SVT_AV2_CDF_DO_SPLIT_SYMS] = {
    20492
};

/* Default_Txb_Skip_Cdf[1][0][4][0] = {23062, 62, 0} -> boundary 23062. */
const uint16_t svt_av2_cdf_txb_skip_q1_txsz4_ctx0[SVT_AV2_CDF_TXB_SKIP_SYMS] = {
    23062
};

/* Default_Txb_Skip_Cdf[1][0][3][6] = {4208, 31, 0} -> boundary 4208. */
const uint16_t svt_av2_cdf_txb_skip_q1_txsz3_ctx6[SVT_AV2_CDF_TXB_SKIP_SYMS] = {
    4208
};

/* Default_V_Txb_Skip_Cdf[1][0] = {1341, 93, 0} -> boundary 1341. */
const uint16_t svt_av2_cdf_v_txb_skip_q1_ctx0[SVT_AV2_CDF_TXB_SKIP_SYMS] = {
    1341
};

uint32_t svt_av2_cdf_coeff_q_ctx(uint32_t base_q_idx) {
    if (base_q_idx <= 90u) {
        return 0u;
    }
    if (base_q_idx <= 140u) {
        return 1u;
    }
    if (base_q_idx <= 190u) {
        return 2u;
    }
    return 3u;
}
