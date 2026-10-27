#include "intra_mode_map.h"
/*
 * SVT-AV2 - coefficient (transform block) syntax for M2 (T011).
 * See coeff_syntax.h for scope, the frozen configuration and the spec mapping,
 * and docs/coeff-syntax.md for the clause-by-clause justification of every
 * symbol and context this file emits.
 *
 * The encoder is the inverse of AVM's decoder path
 *   avm/av2/decoder/decodetxb.c : av2_read_coeffs_txb()
 * and matches AVM's encoder path
 *   avm/av2/encoder/encodetxb.c : av2_write_coeffs_txb()
 * symbol for symbol, with the CDF selections of spec 8.3.2 and the default
 * tables of cdf_tables_coeff.h (from avm/av2/common/entropy_inits_coeffs.h).
 */
#include "coeff_syntax.h"
#include "entropy_models.h"

#include "cdf_tables.h"
#include "cdf_tables_coeff.h"

/* ---- spec constants (avm/av2/common/entropy.h) ---------------------------- */

#define SVT_AV2_SIG_REF_DIFF_OFFSET_NUM 5u
#define SVT_AV2_LF_BASE_SYMBOLS 6u
#define SVT_AV2_LF_NUM_BASE_LEVELS 4u /* LF_BASE_SYMBOLS - 2 */
#define SVT_AV2_NUM_BASE_LEVELS 2u
#define SVT_AV2_COEFF_BASE_RANGE 3u /* BR_CDF_SIZE - 1 */
#define SVT_AV2_BR_CDF_SIZE 4u
/* MAX_BASE_BR_RANGE = COEFF_BASE_RANGE + NUM_BASE_LEVELS + 1 = 6 */
#define SVT_AV2_MAX_BASE_BR_RANGE 6u
#define SVT_AV2_LF_SIG_COEF_CONTEXTS_2D_UV 8u
#define SVT_AV2_TXB_SKIP_CONTEXTS 10u
#define SVT_AV2_V_TXB_SKIP_CONTEXT_OFFSET 6u
#define SVT_AV2_SIG_COEF_CONTEXTS_EOB 4u
#define SVT_AV2_EOB_PT_INDEX_COUNT 8u
#define SVT_AV2_LF_2D_LIM 4u /* row + col limit, luma, TX_CLASS_2D */
#define SVT_AV2_LF_2D_LIM_UV 1u /* row + col limit, chroma, TX_CLASS_2D */
/* TX_PAD_HOR = 4 (TX_PAD_HOR_LOG2 = 2), TX_PAD_TOP = 1 (enums.h). */
#define SVT_AV2_TX_PAD_HOR 4u
#define SVT_AV2_TX_PAD_TOP 1u
#define SVT_AV2_MAX_SCAN_COEFFS \
    (SVT_AV2_COEFF_MAX_TX_DIM * SVT_AV2_COEFF_MAX_TX_DIM)
/* rows of data + top/bottom/trailing padding; multiple of the stride so the
 * "+2 rows" reads used by the 4-neighbour templates stay in bounds. */
#define SVT_AV2_LEVEL_BUF_SIZE \
    ((SVT_AV2_COEFF_MAX_TX_DIM + 8u) * \
     (SVT_AV2_COEFF_MAX_TX_DIM + SVT_AV2_TX_PAD_HOR))

/* The TX_CLASS_2D neighbour templates: Sig_Ref_Diff_Offset[TX_CLASS_2D]
 * (spec p.615) and Mag_Ref_Offset_With_Tx_Class[TX_CLASS_2D] (spec p.603). */
static const int8_t k_sig_ref_diff_2d[5][2] = {
    {0, 1}, {1, 0}, {1, 1}, {0, 2}, {2, 0}
};
static const int8_t k_mag_ref_2d[3][2] = {
    {0, 1}, {1, 0}, {1, 1}
};

static uint32_t min_u32(uint32_t a, uint32_t b) { return (a < b) ? a : b; }

/* ---- geometry ------------------------------------------------------------- */

uint32_t svt_av2_coeff_tx_sz_ctx(SvtAv2TxSize tx_size) {
    static const uint8_t contexts[SVT_AV2_TX_SIZE_COUNT] = {0u,1u,2u,3u,4u,1u,1u,2u,2u,4u,4u,3u,3u,3u,3u,2u,2u,1u,1u,3u,3u,2u,2u};
    return (uint32_t)tx_size < SVT_AV2_TX_SIZE_COUNT ? contexts[tx_size] : 0u;
}

uint32_t svt_av2_coeff_tx_width(SvtAv2TxSize tx_size) {
    static const uint8_t widths[SVT_AV2_TX_SIZE_COUNT] = {4u,8u,16u,32u,64u,4u,8u,8u,16u,32u,64u,16u,32u,16u,64u,8u,32u,4u,16u,8u,64u,4u,32u};
    return (uint32_t)tx_size < SVT_AV2_TX_SIZE_COUNT ? widths[tx_size] : 0u;
}

uint32_t svt_av2_coeff_tx_height(SvtAv2TxSize tx_size) {
    static const uint8_t heights[SVT_AV2_TX_SIZE_COUNT] = {4u,8u,16u,32u,64u,8u,4u,16u,8u,64u,32u,32u,16u,64u,16u,32u,8u,16u,4u,64u,8u,32u,4u};
    return (uint32_t)tx_size < SVT_AV2_TX_SIZE_COUNT ? heights[tx_size] : 0u;
}

uint32_t svt_av2_coeff_tx_bwl(SvtAv2TxSize tx_size) {
    uint32_t width = svt_av2_coeff_tx_width(tx_size), log = 0u;
    while (width > 1u) { width >>= 1u; ++log; }
    return log;
}

int svt_av2_coeff_tx_size_from_dims(uint32_t width, uint32_t height, SvtAv2TxSize* tx_size) {
    if (tx_size == NULL) return SVT_AV2_COEFF_ERR_NULL;
    for (uint32_t i = 0u; i < SVT_AV2_TX_SIZE_COUNT; ++i)
        if (svt_av2_coeff_tx_width((SvtAv2TxSize)i) == width &&
            svt_av2_coeff_tx_height((SvtAv2TxSize)i) == height) {
            *tx_size = (SvtAv2TxSize)i;
            return SVT_AV2_COEFF_OK;
        }
    return SVT_AV2_COEFF_ERR_UNSUPPORTED;
}

/* av2_get_adjusted_tx_size(): 64-wide/tall transforms are clamped to 32
 * (avm/av2/common/blockd.h). The coded coefficient grid, the scan and the
 * neighbour level buffer all use the adjusted size; the transform-size context
 * (txSzCtx) and the block-size comparison use the original. */
static uint32_t adjusted_dim(SvtAv2TxSize tx_size) {
    const uint32_t d = svt_av2_coeff_tx_width(tx_size);
    return (d > 32u) ? 32u : d;
}

static uint32_t adjusted_height(SvtAv2TxSize tx_size) {
    return min_u32(svt_av2_coeff_tx_height(tx_size), 32u);
}

/* txsize_log2_minus4[tx_size] (avm/av2/common/txb_common.c). Selects the eob
 * CDF group: 0 -> 16, 2 -> 64, 4 -> 256, 6 -> 1024. */
static uint32_t eob_multi_size_of(SvtAv2TxSize tx_size) {
    static const uint8_t k_txsize_log2_minus4[SVT_AV2_TX_SIZE_COUNT] = {0,2,4,6,6,1,1,3,3,6,6,5,5,5,5,4,4,2,2,4,4,3,3};
    return k_txsize_log2_minus4[(uint32_t)tx_size];
}

/* log2 of the adjusted grid width (get_txb_bwl over the adjusted size). */
static uint32_t adjusted_bwl(SvtAv2TxSize tx_size) {
    const uint32_t d = adjusted_dim(tx_size);
    uint32_t l = 0u;
    while ((4u << l) < d) {
        ++l;
    }
    return 2u + l;
}

uint32_t svt_av2_coeff_q_context(uint32_t base_qindex) {
    return base_qindex <= 90u ? 0u : base_qindex <= 140u ? 1u : base_qindex <= 190u ? 2u : 3u;
}

int32_t svt_av2_coeff_max_level(void) {
    /* The decoder rejects a level whose top bit is above bit 19
     * ((level >> 20) != 0 -> corrupt frame, decodetxb.c:907). */
    return (int32_t)((1u << 20) - 1u);
}

/* AVM get_padded_idx() within the level buffer. */
static int lvl_idx(int pos, uint32_t bwl) {
    return pos + ((pos >> bwl) << 2);
}

/* ==========================================================================
 * Scan order for DCT_DCT (TX_CLASS_2D)
 * ========================================================================== */

size_t svt_av2_coeff_scan_dct_dct(SvtAv2TxSize tx_size, uint16_t* scan) {
    if (scan == NULL) {
        return 0u;
    }
    if ((uint32_t)tx_size >= (uint32_t)SVT_AV2_TX_SIZE_COUNT) {
        return 0u;
    }
    /* The coded coefficient grid is clamped to 32x32: av2_get_max_eob() returns
     * min(w,32)*min(h,32), so TX_64X64 carries a 1024-coefficient 32x32 grid and
     * AVM reuses default_scan_32x32 for it (avm/av2/common/scan.c:1799-1807 and
     * the note at :1804-1806). Verified against AVM's tables for 4x4..32x32. */
    const uint32_t w = adjusted_dim(tx_size);
    const uint32_t h = adjusted_height(tx_size);
    size_t n = 0u;
    /* Anti-diagonal traversal d = row + col, walking bottom-left to top-right,
     * which is the AV2/AOM DCT_DCT diagonal scan. */
    for (uint32_t d = 0; d <= w + h - 2u; ++d) {
        const uint32_t row_start = (d < h) ? d : (h - 1u);
        const uint32_t row_end = (d >= w) ? (d - (w - 1u)) : 0u;
        for (uint32_t row = row_start + 1u; row-- > row_end;) {
            if (n >= (size_t)(w * h)) {
                return n;
            }
            scan[n++] = (uint16_t)(row * w + (d - row));
        }
    }
    return n;
}

/* Number of coded coefficients for tx_size: av2_get_max_eob(), i.e. the 32x32
 * clamped area (avm/av2/common/blockd.h:3477-3490). */
uint32_t svt_av2_coeff_coeff_count(SvtAv2TxSize tx_size) {
    if ((uint32_t)tx_size >= (uint32_t)SVT_AV2_TX_SIZE_COUNT) {
        return 0u;
    }
    return adjusted_dim(tx_size) * adjusted_height(tx_size);
}

uint32_t svt_av2_coeff_find_eob(const int32_t* qcoeff, const uint16_t* scan,
                                size_t count) {
    if (qcoeff == NULL || scan == NULL) {
        return 0u;
    }
    for (size_t c = count; c > 0u; --c) {
        if (qcoeff[scan[c - 1u]] != 0) {
            return (uint32_t)c;
        }
    }
    return 0u;
}

/* ==========================================================================
 * Neighbour state
 * ========================================================================== */

void svt_av2_coeff_neighbours_reset(SvtAv2CoeffNeighbours* nb) {
    if (nb == NULL) {
        return;
    }
    for (uint32_t i = 0; i < SVT_AV2_COEFF_CTX_UNITS; ++i) {
        nb->above_level[i] = 0u;
        nb->left_level[i] = 0u;
        nb->above_dc[i] = 0u;
        nb->left_dc[i] = 0u;
    }
}

/* ==========================================================================
 * Context derivation (spec 8.3.2)
 * ========================================================================== */

/* get_lf_limits(row, col, TX_CLASS_2D, plane) (5.20.7.27 p.230). */
static int lf_limits_2d(int row, int col, uint32_t plane) {
    return (plane == 0u) ? ((row + col) < (int)SVT_AV2_LF_2D_LIM)
                         : ((row + col) < (int)SVT_AV2_LF_2D_LIM_UV);
}

/* get_lower_levels_ctx_eob(bwl, height, c) (8.3.2 p.601). */
static uint32_t lower_levels_ctx_eob(uint32_t bwl, uint32_t height, uint32_t c) {
    if (c == 0u) {
        return 0u;
    }
    if (c <= ((height << bwl) / 8u)) {
        return 1u;
    }
    if (c <= ((height << bwl) / 4u)) {
        return 2u;
    }
    return 3u;
}

/*
 * Neighbour magnitude sum for TX_CLASS_2D. `lv` points at the padded level
 * buffer base (AVM's set_levels() result) and the reads use `stride` =
 * width + TX_PAD_HOR so that the {1,1}, {0,2} and {2,0} templates land where
 * AVM's byte-offset reads land. Off-block neighbours hit zeroed padding, which
 * is exactly AVM's behaviour (clip_maxN[0] == 0).
 */
static uint32_t nz_mag_2d(const uint8_t* lv, uint32_t bwl, int row, int col,
                          uint32_t limit, uint32_t plane) {
    const int stride = (int)((1u << bwl) + SVT_AV2_TX_PAD_HOR);
    const int base = lvl_idx(row << bwl, bwl) + col;
    uint32_t mag = 0u;
    const uint32_t references = plane == 0u ? SVT_AV2_SIG_REF_DIFF_OFFSET_NUM : 3u;
    for (uint32_t i = 0; i < references; ++i) {
        const uint32_t v =
            lv[base + k_sig_ref_diff_2d[i][0] * stride + k_sig_ref_diff_2d[i][1]];
        mag += (v > limit) ? limit : v;
    }
    return mag;
}

/* coeff_base lfCtx for plane 0, TX_CLASS_2D (8.3.2 p.600). */
static uint32_t lf_ctx_2d(uint32_t ctx, uint32_t c, int row, int col) {
    if (c == 0u) {
        return min_u32(ctx, 8u);
    }
    if ((row + col) < 2) {
        return min_u32(ctx, 6u) + 9u;
    }
    return min_u32(ctx, 4u) + 16u;
}

/* coeff_base hfCtx for plane 0, TX_CLASS_2D (8.3.2 p.600). */
static uint32_t hf_ctx_2d(uint32_t ctx, int row, int col) {
    const uint32_t ctx2 = min_u32(ctx, 4u);
    if ((row + col) < 6) {
        return ctx2;
    }
    if ((row + col) < 8) {
        return ctx2 + 5u;
    }
    return ctx2 + 10u;
}

/* coeff_base uvCtx for plane > 0, TX_CLASS_2D (8.3.2 p.600).
 *
 * The spec has one uvCtx formula for plane > 0 (8.3.2 p.600): `ctx =
 * Min(uvCtx, 3)`, then `+4` for V. That single formula covers both the
 * low-frequency and higher-frequency regions, as AVM confirms: for TX_CLASS_2D
 * get_nz_map_ctx_from_stats_lf_chroma (txb_common.h:408-412) and
 * get_nz_map_ctx_from_stats_chroma (:472-482) both return
 * `plane == U ? ctx : ctx + 4` with the same clamp. There is therefore no
 * separate low-frequency UV context function. */
static uint32_t uv_ctx_2d(uint32_t ctx, uint32_t plane) {
    const uint32_t ctx2 = min_u32(ctx, 3u);
    return (plane == 1u) ? ctx2 : ctx2 + 4u;
}

/* get_br_ctx / get_br_lf_ctx (spec 8.3.2 p.603, TX_CLASS_2D). The `is_lf`
 * argument selects the low-frequency branch, exactly as the spec's
 * "if (isLf) ctx = mag + 7 else ctx = mag" (p.603 line 360). */
static uint32_t br_ctx_2d(const uint8_t* lv, uint32_t bwl, int row, int col,
                          uint32_t plane, int is_lf) {
    const uint32_t clip = SVT_AV2_MAX_BASE_BR_RANGE - 1u; /* 5 */
    const int stride = (int)((1u << bwl) + SVT_AV2_TX_PAD_HOR);
    const int base = lvl_idx(row << bwl, bwl) + col;
    uint32_t mag = 0u;
    for (uint32_t i = 0; i < 3u; ++i) {
        const uint32_t v =
            lv[base + k_mag_ref_2d[i][0] * stride + k_mag_ref_2d[i][1]];
        mag += (v > clip) ? clip : v;
    }
    mag = min_u32((mag + 1u) >> 1, 6u);
    if (plane > 0u) {
        return min_u32(mag, 3u);
    }
    if ((row | col) == 0) {
        /* pos == 0: ctx = mag (2D), regardless of isLf. */
        return mag;
    }
    return is_lf ? (mag + 7u) : mag;
}

/* dc_sign context (8.3.2 p.602). */
static uint32_t dc_sign_ctx_of(const SvtAv2CoeffNeighbours* nb,
                               const SvtAv2CoeffPlaneInfo* pi) {
    int dc_sign = 0;
    const uint32_t w4 = (svt_av2_coeff_tx_width(pi->tx_size)) >> 2;
    const uint32_t h4 = (svt_av2_coeff_tx_height(pi->tx_size)) >> 2;
    for (uint32_t k = 0; k < w4; ++k) {
        if (pi->x4 + k >= pi->units_w) {
            break;
        }
        const uint8_t s = nb->above_dc[pi->x4 + k];
        if (s == 1u) {
            --dc_sign;
        } else if (s == 2u) {
            ++dc_sign;
        }
    }
    for (uint32_t k = 0; k < h4; ++k) {
        if (pi->y4 + k >= pi->units_h) {
            break;
        }
        const uint8_t s = nb->left_dc[pi->y4 + k];
        if (s == 1u) {
            --dc_sign;
        } else if (s == 2u) {
            ++dc_sign;
        }
    }
    return (dc_sign < 0) ? 1u : (dc_sign > 0) ? 2u : 0u;
}

/* ==========================================================================
 * EOB token
 * ========================================================================== */

/* av2_eob_group_start / av2_eob_offset_bits (avm/av2/common/txb_common.c:399),
 * the encoder inverse of the 5.20.7.27 eob reconstruction. */
static const uint16_t k_eob_group_start[12] = {0,   1,   2,   3,   5,   9,
                                               17,  33,  65,  129, 257, 513};
static const uint8_t k_eob_offset_bits[12] = {0, 0, 0, 1, 2, 3,
                                              4, 5, 6, 7, 8, 9};

/* get_eob_pos_token(eob, &extra) (avm/av2/encoder/encodetxb.c:180). */
static uint32_t eob_pos_token(uint32_t eob, uint32_t* extra) {
    if (eob <= 2u) {
        *extra = 0u;
        return eob;
    }
    for (uint32_t i = 12u; i-- > 0u;) {
        if (eob >= (uint32_t)k_eob_group_start[i]) {
            *extra = eob - (uint32_t)k_eob_group_start[i];
            return i;
        }
    }
    *extra = 0u;
    return 0u;
}

static const uint16_t* eob_cdf_row(uint32_t eob_multi_size, uint32_t eob_ctx,
                                   uint32_t* nsyms, uint32_t q_ctx) {
    switch (eob_multi_size) {
    case 0:
        *nsyms = SVT_AV2_SYMS_EOB16;
        return svt_av2_cdf_eob16_by_qctx[q_ctx] +
               (size_t)eob_ctx * (SVT_AV2_SYMS_EOB16 - 1u);
    case 1:
        *nsyms = SVT_AV2_SYMS_EOB32;
        return svt_av2_cdf_eob32_by_qctx[q_ctx] +
               (size_t)eob_ctx * (SVT_AV2_SYMS_EOB32 - 1u);
    case 2:
        *nsyms = SVT_AV2_SYMS_EOB64;
        return svt_av2_cdf_eob64_by_qctx[q_ctx] +
               (size_t)eob_ctx * (SVT_AV2_SYMS_EOB64 - 1u);
    case 3:
        *nsyms = SVT_AV2_SYMS_EOB128;
        return svt_av2_cdf_eob128_by_qctx[q_ctx] +
               (size_t)eob_ctx * (SVT_AV2_SYMS_EOB128 - 1u);
    case 4:
        *nsyms = SVT_AV2_SYMS_EOB256;
        return svt_av2_cdf_eob256_by_qctx[q_ctx] +
               (size_t)eob_ctx * (SVT_AV2_SYMS_EOB256 - 1u);
    case 5:
        *nsyms = SVT_AV2_SYMS_EOB512;
        return svt_av2_cdf_eob512_by_qctx[q_ctx] +
               (size_t)eob_ctx * (SVT_AV2_SYMS_EOB512 - 1u);
    default:
        *nsyms = SVT_AV2_SYMS_EOB1024;
        return svt_av2_cdf_eob1024_by_qctx[q_ctx] +
               (size_t)eob_ctx * (SVT_AV2_SYMS_EOB1024 - 1u);
    }
}

/* Width of the eob_pt_*_extra raw field. The spec writes L(1) for eobMultisize
 * == 4 and L(2) for 5/6 (5.20.7.27 p.225), matching AVM encodetxb.c:651-668.
 * Only 4/5/6 reach this field; eob_multi_size < 4 uses a full-width eob_pt
 * symbol instead. */
static uint32_t eob_pt_extra_bits(uint32_t eob_multi_size) {
    return (eob_multi_size == 4u) ? 1u : 2u;
}

/* ==========================================================================
 * Literal (raw bit) support
 *
 * coeffs() interleaves entropy symbols with raw L(n) fields: eob_pt_*_extra,
 * eob_extra_bit, sign_bit and the read_quant() high-range field. The M1 range
 * coder had no literal interface; per 00's ruling of 2026-10-07 the additive
 * svt_av2_entropy_encode_literal()/_encode_bit() entry points were added to
 * entropy_encoder.{h,c} (pure addition; symbol path and the M1 frozen vector
 * are unchanged), so those raw fields are now emitted with the exact AVM
 * bypass arithmetic (od_ec_encode_literal_bypass, MSB first).
 *
 * High-range read_quant() escapes use adaptive Rice/Exp-Golomb syntax up to
 * the fixed decoder ceiling (2^20 - 1), including low-QP coefficient levels.
 * ========================================================================== */
int svt_av2_coeff_literals_supported(void) { return 1; }

/* ==========================================================================
 * Block writer
 * ========================================================================== */

typedef struct CoeffBlk {
    const uint16_t* scan;
    size_t          nscan;
    uint32_t        bwl;      /* log2 of the coded grid width (adjusted) */
    uint32_t        width;    /* coded coefficient grid width  (adjusted) */
    uint32_t        height;   /* coded coefficient grid height (adjusted) */
    uint32_t        count;    /* coded coefficient count = width*height of the
                               * ADJUSTED grid, i.e. 1024 for TX_64X64 (the
                               * unadjusted 64x64 would be 4096); see
                               * svt_av2_coeff_coeff_count(). */
    uint32_t        ctx_w;    /* transform width in samples (unadjusted) */
    uint32_t        ctx_h;    /* transform height in samples (unadjusted) */
    uint32_t        eob;
    uint32_t        txs_ctx;
    uint32_t        eob_multi_size;
    uint8_t*        lv; /* padded level buffer base */
    uint8_t         lv_buf[SVT_AV2_LEVEL_BUF_SIZE];
} CoeffBlk;

static int emit_sym(SvtAv2EntropyEncoder* enc, uint32_t model, const uint16_t* cdf,
                    uint32_t nsyms, uint32_t s) {
    const int st = svt_av2_entropy_encode_model(enc, model, cdf, nsyms, s);
    if (st == SVT_AV2_ENTROPY_OK) {
        return SVT_AV2_COEFF_OK;
    }
    return (st == SVT_AV2_ENTROPY_ERROR_NO_SPACE) ? SVT_AV2_COEFF_ERR_NO_SPACE
                                                  : SVT_AV2_COEFF_ERR_UNSUPPORTED;
}

/* Emits n_bits raw (bypass) bits, most significant first. n_bits == 0 is a
 * no-op. Mirrors the spec's L(n) reads; the arithmetic is AVM's
 * od_ec_encode_literal_bypass, independently verified bit-exactly in
 * artifacts/T011/gate3_bypass_diff.c. */
static int emit_lit(SvtAv2EntropyEncoder* enc, uint32_t value, int n_bits) {
    /* avm_write_literal emits at most eight bypass bits per call. High-range
     * escapes may exceed the low-level coder's sixteen-bit entry limit. */
    while (n_bits > 0) {
        const int chunk = n_bits > 8 ? 8 : n_bits;
        n_bits -= chunk;
        const uint32_t part = (value >> n_bits) & ((1u << chunk) - 1u);
        const int st = svt_av2_entropy_encode_literal(enc, part, chunk);
        if (st != SVT_AV2_ENTROPY_OK) {
            return st == SVT_AV2_ENTROPY_ERROR_NO_SPACE ? SVT_AV2_COEFF_ERR_NO_SPACE
                                                       : SVT_AV2_COEFF_ERR_UNSUPPORTED;
        }
    }
    return SVT_AV2_COEFF_OK;
}

/* write_low_range() inverse (spec 5.20.7.27 p.228 / read_low_range). Present
 * only when the base symbol saturated; `level` is already known to satisfy
 * level > baseLevels for the region. */
static int write_low_range(SvtAv2EntropyEncoder* enc, uint32_t model, const uint16_t* cdf,
                           uint32_t level, int is_lf, uint32_t plane) {
    uint32_t base_levels;
    if (is_lf) {
        base_levels = (plane > 0u) ? (SVT_AV2_LF_NUM_BASE_LEVELS + 1u)
                                   : (SVT_AV2_LF_NUM_BASE_LEVELS + 1u);
    } else {
        base_levels = SVT_AV2_NUM_BASE_LEVELS + 1u;
    }
    uint32_t br = level - base_levels;
    for (uint32_t idx = 0; idx < SVT_AV2_COEFF_BASE_RANGE;
         idx += SVT_AV2_BR_CDF_SIZE - 1u) {
        const uint32_t k = min_u32(br, SVT_AV2_BR_CDF_SIZE - 1u);
        const int st = emit_sym(enc, model, cdf, SVT_AV2_BR_CDF_SIZE, k);
        if (st != SVT_AV2_COEFF_OK) {
            return st;
        }
        if (k < SVT_AV2_BR_CDF_SIZE - 1u) {
            break;
        }
        br -= k;
    }
    return SVT_AV2_COEFF_OK;
}

/* Spec GetMsb(x): index of the most significant set bit, i.e. floor(log2(x)).
 * Defined as -1 for x == 0 (the Clip3 that follows lifts it to 1). */
static int get_msb_spec(uint32_t x) {
    int b = -1;
    while (x > 0u) {
        ++b;
        x >>= 1;
    }
    return b;
}

/* read_quant() high-range escape, spec 5.20.7.28 p.231 (and its encoder inverse
 * AVM encodetxb.c write_high_range()/write_adaptive_hr()/
 * write_truncated_rice()/write_exp_golomb()). Called only when the level
 * saturates the base+coeff_br range, i.e. quant >= maxLevel - allowTcq with
 * allowTcq = 0 (TCQ disabled this round), so `x = level - maxLevel`.
 *
 * The widths here are NOT fixed a priori: the unary q_length_bit run and the
 * golomb_length_bit run determine the coeff_rem width. That is fine for the
 * ENCODER because it computes x from the known level first; this is the reason
 * the reference *decoder* pass is described as not forward-safe, but the
 * encoder direction is deterministic. `hr_level_avg` is updated exactly as the
 * spec's read_quant() does so the next coefficient sees the same predictor. */
static int write_high_range(SvtAv2EntropyEncoder* enc, uint32_t x,
                            int32_t* hr_level_avg) {
    const int64_t pred = (int64_t)*hr_level_avg; /* lvlShift = 0 (allowTcq = 0) */
    int m = get_msb_spec((uint32_t)pred);
    uint32_t q, rem_bits;
    int st;

    if (m < 1) m = 1;
    if (m > 6) m = 6;
    const uint32_t k = (uint32_t)m + 1u;
    const uint32_t c_max = min_u32((uint32_t)m + 4u, 6u);

    q = x >> (uint32_t)m;
    if (q >= c_max) {
        /* Unary: c_max zero bits (q_length_bit = 0 each). */
        st = emit_lit(enc, 0u, (int)c_max);
        if (st != SVT_AV2_COEFF_OK) {
            return st;
        }
        /* Exp-Golomb order k on (x - (c_max << m)). */
        {
            const uint32_t v = x - (c_max << (uint32_t)m);
            const uint32_t g = v + (1u << k);
            const int len = get_msb_spec(g) + 1;
            /* len > k always (spec assert). */
            st = emit_lit(enc, 0u, len - 1 - (int)k);
            if (st != SVT_AV2_COEFF_OK) {
                return st;
            }
            st = emit_lit(enc, g, len);
            if (st != SVT_AV2_COEFF_OK) {
                return st;
            }
        }
        rem_bits = 0u; /* the Golomb branch already carried the remainder */
    } else {
        /* Unary: q zero bits then a terminating 1. */
        st = emit_lit(enc, 0u, (int)q);
        if (st != SVT_AV2_COEFF_OK) {
            return st;
        }
        st = emit_lit(enc, 1u, 1);
        if (st != SVT_AV2_COEFF_OK) {
            return st;
        }
        rem_bits = (uint32_t)m;
        st = emit_lit(enc, x & ((1u << (uint32_t)m) - 1u), (int)rem_bits);
        if (st != SVT_AV2_COEFF_OK) {
            return st;
        }
    }
    (void)rem_bits;

    /* hrLevelAvg = ((x << lvlShift) + hrLevelAvg) >> 1, lvlShift = 0. */
    *hr_level_avg = (int32_t)(((int64_t)x + (int64_t)*hr_level_avg) >> 1);
    return SVT_AV2_COEFF_OK;
}

/* all_zero context, plane 0 (8.3.2 p.598). */
static uint32_t all_zero_ctx_luma(const SvtAv2CoeffNeighbours* nb,
                                  const SvtAv2CoeffPlaneInfo* pi,
                                  uint32_t width, uint32_t height) {
    if (pi->bw == width && pi->bh == height) {
        return 0u;
    }
    uint32_t top = 0u;
    uint32_t left = 0u;
    const uint32_t w4 = width >> 2;
    const uint32_t h4 = height >> 2;
    for (uint32_t k = 0; k < w4; ++k) {
        if (pi->x4 + k >= pi->units_w) {
            break;
        }
        top |= nb->above_level[pi->x4 + k];
    }
    for (uint32_t k = 0; k < h4; ++k) {
        if (pi->y4 + k >= pi->units_h) {
            break;
        }
        left |= nb->left_level[pi->y4 + k];
    }
    top = min_u32(top, 4u);
    left = min_u32(left, 4u);
    return (top + left + 3u) >> 1;
}

/* OR-reduction of the level and DC contexts along one edge. */
static uint32_t or_edge(const uint8_t* level, const uint8_t* dc, uint32_t from,
                        uint32_t n, uint32_t limit) {
    uint32_t acc = 0u;
    for (uint32_t i = 0; i < n; ++i) {
        if (from + i >= limit) {
            break;
        }
        acc |= level[from + i];
        acc |= dc[from + i];
    }
    return acc;
}

int svt_av2_coeff_write_block_transform_intra(SvtAv2EntropyEncoder* enc,
                              const SvtAv2CoeffPlaneInfo* plane_info,
                              uint8_t eob_u_nonzero, const int32_t* qcoeff,
                              SvtAv2CoeffNeighbours* nb, uint32_t q_ctx, uint32_t is_inter,
                              uint32_t reduced_tx_set, uint32_t tx_type, uint32_t intra_mode) {
    if (enc == NULL || plane_info == NULL || qcoeff == NULL || nb == NULL) {
        return SVT_AV2_COEFF_ERR_NULL;
    }
    if ((q_ctx > 3u) || is_inter > 1u) return SVT_AV2_COEFF_ERR_UNSUPPORTED;
    const SvtAv2CoeffPlaneInfo* pi = plane_info;
    if (pi->plane > 2u ||
        (uint32_t)pi->tx_size >= (uint32_t)SVT_AV2_TX_SIZE_COUNT ||
        (is_inter && pi->tx_size >= SVT_AV2_TX_4X8) ||
        pi->units_w > SVT_AV2_COEFF_CTX_UNITS || pi->units_h > SVT_AV2_COEFF_CTX_UNITS ||
        pi->x4 >= pi->units_w || pi->y4 >= pi->units_h) {
        return SVT_AV2_COEFF_ERR_UNSUPPORTED;
    }
    if (tx_type != 0u && (tx_type > (M6_FLIP_TX ? 8u : 3u) || is_inter || pi->plane != 0u ||
        (pi->tx_size > SVT_AV2_TX_16X16 &&
         !(reduced_tx_set == 0u && (pi->tx_size == SVT_AV2_TX_8X16 || pi->tx_size == SVT_AV2_TX_16X8))) ||
        reduced_tx_set > 1u ||
        (reduced_tx_set == 1u && tx_type != 3u) ||
        (reduced_tx_set == 0u && pi->tx_size == SVT_AV2_TX_4X4 && intra_mode == 7u && tx_type == 1u)))
        return SVT_AV2_COEFF_ERR_UNSUPPORTED;
#if M6_FLIP_TX
    if (tx_type>3u && svt_av2_intra_tx_symbol(intra_mode,
        svt_av2_coeff_tx_width(pi->tx_size),svt_av2_coeff_tx_height(pi->tx_size),tx_type)>=7u)
        return SVT_AV2_COEFF_ERR_UNSUPPORTED;
#endif
    const uint32_t plane = pi->plane;
    const SvtAv2TxSize tx_size = pi->tx_size;

    CoeffBlk cb;
    /* Coded grid + raster indexing use the AVM-adjusted (<=32) transform size;
     * the transform-size context and the block-size comparison use the
     * unadjusted one. See adjusted_dim(). */
    cb.width = adjusted_dim(tx_size);
    cb.height = adjusted_height(tx_size);
    cb.bwl = adjusted_bwl(tx_size);
    cb.count = svt_av2_coeff_coeff_count(tx_size);
    cb.ctx_w = svt_av2_coeff_tx_width(tx_size);
    cb.ctx_h = svt_av2_coeff_tx_height(tx_size);
    cb.txs_ctx = svt_av2_coeff_tx_sz_ctx(tx_size);
    cb.eob_multi_size = eob_multi_size_of(tx_size);

    uint16_t scan_buf[SVT_AV2_MAX_SCAN_COEFFS];
    cb.nscan = svt_av2_coeff_scan_dct_dct(tx_size, scan_buf);
    cb.scan = scan_buf;
    if (cb.nscan != (size_t)cb.count) {
        return SVT_AV2_COEFF_ERR_UNSUPPORTED;
    }

    const uint32_t stride = cb.width + SVT_AV2_TX_PAD_HOR;
    cb.lv = cb.lv_buf + SVT_AV2_TX_PAD_TOP * stride;
    for (size_t i = 0; i < SVT_AV2_LEVEL_BUF_SIZE; ++i) {
        cb.lv_buf[i] = 0u;
    }

    /* --- 1. all_zero (5.20.7.27 p.224; CDF 8.3.2 p.598) ------------------ */
    cb.eob = svt_av2_coeff_find_eob(qcoeff, scan_buf, cb.count);
    /* Intra EOB=1 implies DCT_DCT in the frozen decoder. */
    if (tx_type != 0u && cb.eob == 1u) return SVT_AV2_COEFF_ERR_UNSUPPORTED;
    const uint32_t az_symbol = (cb.eob == 0u) ? 1u : 0u;
    {
        int st;
        if (plane == 2u) {
            const uint32_t w4 = cb.ctx_w >> 2;
            const uint32_t h4 = cb.ctx_h >> 2;
            const uint32_t above =
                or_edge(nb->above_level, nb->above_dc, pi->x4, w4, pi->units_w);
            const uint32_t left =
                or_edge(nb->left_level, nb->left_dc, pi->y4, h4, pi->units_h);
            uint32_t ctx = ((above != 0u) ? 1u : 0u) + ((left != 0u) ? 1u : 0u);
            if (pi->bw * pi->bh > cb.ctx_w * cb.ctx_h) {
                ctx += 3u;
            }
            ctx += eob_u_nonzero ? SVT_AV2_V_TXB_SKIP_CONTEXT_OFFSET : 0u;
            if (ctx >= SVT_AV2_CDF_V_TXB_SKIP_COUNT) {
                return SVT_AV2_COEFF_ERR_UNSUPPORTED;
            }
            st = emit_sym(enc, SVT_AV2_MODEL_V_SKIP + ctx, svt_av2_cdf_v_txb_skip_by_qctx[q_ctx] + ctx, SVT_AV2_SYMS_TXB_SKIP,
                          az_symbol);
        } else {
            uint32_t ctx;
            if (plane == 1u) {
                const uint32_t w4 = cb.ctx_w >> 2;
                const uint32_t h4 = cb.ctx_h >> 2;
                const uint32_t above = or_edge(nb->above_level, nb->above_dc,
                                               pi->x4, w4, pi->units_w);
                const uint32_t left = or_edge(nb->left_level, nb->left_dc, pi->y4,
                                              h4, pi->units_h);
                ctx = ((above != 0u) ? 1u : 0u) + ((left != 0u) ? 1u : 0u) + 6u;
            } else {
                ctx = all_zero_ctx_luma(nb, pi, cb.ctx_w, cb.ctx_h);
            }
            if (ctx >= SVT_AV2_TXB_SKIP_CONTEXTS) {
                return SVT_AV2_COEFF_ERR_UNSUPPORTED;
            }
            /* pred_mode_ctx = 0: intra, FSC off. */
            const uint16_t* row_cdf =
                svt_av2_cdf_txb_skip_by_qctx[q_ctx] +
                ((size_t)is_inter * 5u * SVT_AV2_TXB_SKIP_CONTEXTS + (size_t)cb.txs_ctx * SVT_AV2_TXB_SKIP_CONTEXTS + ctx) *
                    (SVT_AV2_SYMS_TXB_SKIP - 1u);
            st = emit_sym(enc, SVT_AV2_MODEL_TXB_SKIP + is_inter * 50u + cb.txs_ctx * 10u + ctx, row_cdf, SVT_AV2_SYMS_TXB_SKIP, az_symbol);
        }
        if (st != SVT_AV2_COEFF_OK) {
            return st;
        }
    }
    if (cb.eob == 0u) {
        for (uint32_t i = 0; i < cb.ctx_w / 4u && pi->x4 + i < SVT_AV2_COEFF_CTX_UNITS; ++i) {
            nb->above_level[pi->x4 + i] = 0u;
            nb->above_dc[pi->x4 + i] = 0u;
        }
        for (uint32_t i = 0; i < cb.ctx_h / 4u && pi->y4 + i < SVT_AV2_COEFF_CTX_UNITS; ++i) {
            nb->left_level[pi->y4 + i] = 0u;
            nb->left_dc[pi->y4 + i] = 0u;
        }
        return SVT_AV2_COEFF_OK;
    }

    /* --- 2. eob token, eob_extra, eob_pt_*_extra (5.20.7.27 p.225) ------- */
    {
        const uint32_t eob_ctx = (plane > 0u) ? 2u : is_inter;
        const uint32_t eob_multi_size = cb.eob_multi_size;
        uint32_t eob_extra = 0u;
        const uint32_t eob_pt = eob_pos_token(cb.eob, &eob_extra);
        uint32_t nsyms = 0u;
        const uint16_t* cdf = eob_cdf_row(eob_multi_size, eob_ctx, &nsyms, q_ctx);
        int st;
        if (eob_multi_size < 4u) {
            st = emit_sym(enc, SVT_AV2_MODEL_EOB16 + eob_multi_size * 3u + eob_ctx, cdf, nsyms, eob_pt - 1u);
        } else {
            const uint32_t eob_pt_low =
                min_u32(eob_pt - 1u, SVT_AV2_EOB_PT_INDEX_COUNT - 1u);
            st = emit_sym(enc, SVT_AV2_MODEL_EOB16 + eob_multi_size * 3u + eob_ctx, cdf, nsyms, eob_pt_low);
            if (st == SVT_AV2_COEFF_OK &&
                eob_pt_low == SVT_AV2_EOB_PT_INDEX_COUNT - 1u) {
                /* eob_pt_*_extra: (eob_pt - 1 - eob_pt_low) raw bits. */
                const uint32_t extra = eob_pt - 1u - eob_pt_low;
                st = emit_lit(enc, extra, (int)eob_pt_extra_bits(eob_multi_size));
            }
        }
        if (st != SVT_AV2_COEFF_OK) {
            return st;
        }

        const uint32_t eob_offset_bits = k_eob_offset_bits[eob_pt];
        if (eob_offset_bits > 0u) {
            const uint32_t top = (eob_extra >> (eob_offset_bits - 1u)) & 1u;
            st = emit_sym(enc, SVT_AV2_MODEL_EOB_EXTRA, svt_av2_cdf_eob_extra_by_qctx[q_ctx], SVT_AV2_SYMS_EOB_EXTRA, top);
            if (st != SVT_AV2_COEFF_OK) {
                return st;
            }
            if (eob_offset_bits > 1u) {
                /* eob_extra_bit L(eob_offset_bits - 1): the low bits of the
                 * eob offset. */
                const uint32_t low =
                    eob_extra & ((1u << (eob_offset_bits - 1u)) - 1u);
                st = emit_lit(enc, low, (int)(eob_offset_bits - 1u));
                if (st != SVT_AV2_COEFF_OK) {
                    return st;
                }
            }
        }
    }

    if (!is_inter && plane == 0u && reduced_tx_set == 1u &&
        cb.ctx_w <= 16u && cb.ctx_h <= 16u && cb.eob > 1u) {
        const uint16_t cdf = 16384u;
        uint32_t square_context = 0u;
        for (uint32_t side = 4u; side < min_u32(cb.ctx_w,cb.ctx_h); side *= 2u) ++square_context;
        const int st = emit_sym(enc, SVT_AV2_MODEL_INTRA_TX + square_context,
            &cdf, 2u, tx_type == 3u);
        if (st != 0) return st;
    }
    if (!is_inter && plane == 0u && reduced_tx_set == 0u &&
        cb.ctx_w <= 16u && cb.ctx_h <= 16u && cb.eob > 1u) {
        static const uint16_t cdf[3][6] = {
            {5026u,16816u,19974u,23404u,26845u,30499u},
            {14910u,25257u,26964u,29323u,30237u,31535u},
            {13759u,26108u,27688u,29793u,30265u,31576u}};
        uint32_t square_context = 0u;
        for (uint32_t side = 4u; side < min_u32(cb.ctx_w,cb.ctx_h); side *= 2u) ++square_context;
#if M6_FLIP_TX
        const uint32_t symbol=svt_av2_intra_tx_symbol(intra_mode,cb.ctx_w,cb.ctx_h,tx_type);
        if (symbol>=7u) return SVT_AV2_COEFF_ERR_UNSUPPORTED;
#else
        uint32_t symbol = tx_type == 0u ? 0u : tx_type == 3u ? 1u : tx_type + 1u;
        /* Fixed v1 av2_md_idx2type: 4x4 Paeth omits ADST_DCT. */
        if (pi->tx_size == SVT_AV2_TX_4X4 && intra_mode == 7u && tx_type == 2u) symbol = 2u;
#endif
        const int st = emit_sym(enc, SVT_AV2_MODEL_INTRA_TX_FULL + square_context,
            cdf[square_context], 7u, symbol);
        if (st != 0) return st;
    }
    /* Fixed AVM LONG_SIDE_32/64 sets still signal DCT when primary search
     * is disabled on rectangles. Frame reduced set 2 bypasses all intra types. */
    if (!is_inter && plane == 0u && reduced_tx_set <= 1u && cb.eob > 1u &&
        (cb.ctx_w >= 32u || cb.ctx_h >= 32u) && min_u32(cb.ctx_w,cb.ctx_h) < 32u) {
        uint32_t short_context = 0u;
        for (uint32_t side = 4u; side < min_u32(cb.ctx_w,cb.ctx_h); side *= 2u) ++short_context;
        int st = 0;
        if ((cb.ctx_w <= 32u && cb.ctx_h <= 32u)) {
            const uint16_t cdf = 36u;
            st = emit_sym(enc, SVT_AV2_MODEL_LONG_TX32, &cdf, 2u, 1u);
        }
        if (st == 0) {
            static const uint16_t cdfs[2][3] = {{26700u,32160u,32748u},{26915u,32411u,32748u}};
            st = emit_sym(enc, SVT_AV2_MODEL_LONG_TX_SHORT + short_context - 1u,
                cdfs[short_context - 1u], 4u, 0u);
        }
        if (st != 0) return st;
    }
    /* Inter DCT_IDTX reduced set: DCT_DCT is symbol one, uniform binary.
     * Intra DCT and 64x64 transforms are implied; chroma inherits luma type. */
    if (is_inter && plane == 0u && pi->tx_size < SVT_AV2_TX_64X64) {
        const uint16_t cdf = 16384u;
        const uint32_t diag = ((cb.eob - 1u) >> cb.bwl) + ((cb.eob - 1u) & (cb.width - 1u));
        const uint32_t ectx = diag < 2u ? 1u : diag > cb.width + cb.height - 4u ? 2u : 0u;
        const int st = emit_sym(enc, SVT_AV2_MODEL_INTER_TX + ectx * 4u + (uint32_t)pi->tx_size, &cdf, 2u, 1u);
        if (st != 0) return st;
    }
    /* --- 3. magnitudes, scan order c = eob-1 down to 1 (5.20.7.27 p.228) -- */
    for (uint32_t c = cb.eob - 1u; c > 0u; --c) {
        const uint32_t pos = cb.scan[c];
        const int row = (int)(pos >> cb.bwl);
        const int col = (int)(pos - ((uint32_t)row << cb.bwl));
        const int is_eob = (c == (cb.eob - 1u));
        const int is_lf = lf_limits_2d(row, col, plane);
        const int64_t qv = (int64_t)qcoeff[pos];
        const uint32_t level = (uint32_t)((qv < 0) ? -qv : qv);
        const uint32_t mag = nz_mag_2d(cb.lv, cb.bwl, row, col,
                                       (is_lf && is_eob) ? 3u : (is_lf ? 5u : 3u), plane);
        const uint32_t ctx = (mag + 1u) >> 1;

        uint32_t sym;
        const uint16_t* cdf;
        uint32_t ns, model;
        if (is_eob) {
            const uint32_t eob_ctx_idx =
                lower_levels_ctx_eob(cb.bwl, cb.height, c);
            if (plane > 0u) {
                if (is_lf) {
                    cdf = svt_av2_cdf_coeff_base_lf_eob_uv_by_qctx[q_ctx] +
                          (size_t)eob_ctx_idx *
                              (SVT_AV2_SYMS_COEFF_BASE_EOB_LF - 1u);
                    model = SVT_AV2_MODEL_LF_EOB_UV + (uint32_t)((size_t)eob_ctx_idx);
                    ns = SVT_AV2_SYMS_COEFF_BASE_EOB_LF;
                    sym = min_u32(level, SVT_AV2_LF_BASE_SYMBOLS - 1u) - 1u;
                } else {
                    cdf = svt_av2_cdf_coeff_base_eob_uv_by_qctx[q_ctx] +
                          (size_t)eob_ctx_idx *
                              (SVT_AV2_SYMS_COEFF_BASE_EOB - 1u);
                    model = SVT_AV2_MODEL_BASE_EOB_UV + (uint32_t)((size_t)eob_ctx_idx);
                    ns = SVT_AV2_SYMS_COEFF_BASE_EOB;
                    sym = min_u32(level, 3u) - 1u;
                }
            } else if (is_lf) {
                cdf = svt_av2_cdf_coeff_base_lf_eob_by_qctx[q_ctx] +
                      ((size_t)cb.txs_ctx * 4u + eob_ctx_idx) *
                          (SVT_AV2_SYMS_COEFF_BASE_EOB_LF - 1u);
                model = SVT_AV2_MODEL_LF_EOB + (uint32_t)(((size_t)cb.txs_ctx * 4u + eob_ctx_idx));
                ns = SVT_AV2_SYMS_COEFF_BASE_EOB_LF;
                sym = min_u32(level, SVT_AV2_LF_BASE_SYMBOLS - 1u) - 1u;
            } else {
                cdf = svt_av2_cdf_coeff_base_eob_by_qctx[q_ctx] +
                      ((size_t)cb.txs_ctx * 4u + eob_ctx_idx) *
                          (SVT_AV2_SYMS_COEFF_BASE_EOB - 1u);
                model = SVT_AV2_MODEL_BASE_EOB + (uint32_t)(((size_t)cb.txs_ctx * 4u + eob_ctx_idx));
                ns = SVT_AV2_SYMS_COEFF_BASE_EOB;
                sym = min_u32(level, 3u) - 1u;
            }
        } else if (plane > 0u) {
            if (is_lf) {
                cdf = svt_av2_cdf_coeff_base_lf_uv_by_qctx[q_ctx] +
                      (size_t)uv_ctx_2d(ctx, plane) *
                          (SVT_AV2_SYMS_COEFF_BASE_LF - 1u);
                model = SVT_AV2_MODEL_LF_UV + (uint32_t)((size_t)uv_ctx_2d(ctx, plane));
                ns = SVT_AV2_SYMS_COEFF_BASE_LF;
                sym = min_u32(level, SVT_AV2_LF_BASE_SYMBOLS - 1u);
            } else {
                cdf = svt_av2_cdf_coeff_base_uv_by_qctx[q_ctx] +
                      (size_t)uv_ctx_2d(ctx, plane) *
                          (SVT_AV2_SYMS_COEFF_BASE - 1u);
                model = SVT_AV2_MODEL_BASE_UV + (uint32_t)((size_t)uv_ctx_2d(ctx, plane));
                ns = SVT_AV2_SYMS_COEFF_BASE;
                sym = min_u32(level, 3u);
            }
        } else if (is_lf) {
            cdf = svt_av2_cdf_coeff_base_lf_by_qctx[q_ctx] +
                  (((size_t)cb.txs_ctx * 33u + lf_ctx_2d(ctx, c, row, col)) *
                   2u) *
                      (SVT_AV2_SYMS_COEFF_BASE_LF - 1u);
            model = SVT_AV2_MODEL_LF + (uint32_t)((((size_t)cb.txs_ctx * 33u + lf_ctx_2d(ctx, c, row, col)) *
                   2u));
            ns = SVT_AV2_SYMS_COEFF_BASE_LF;
            sym = min_u32(level, SVT_AV2_LF_BASE_SYMBOLS - 1u);
        } else {
            cdf = svt_av2_cdf_coeff_base_by_qctx[q_ctx] +
                  (((size_t)cb.txs_ctx * 20u + hf_ctx_2d(ctx, row, col)) * 2u) *
                      (SVT_AV2_SYMS_COEFF_BASE - 1u);
            model = SVT_AV2_MODEL_BASE + (uint32_t)((((size_t)cb.txs_ctx * 20u + hf_ctx_2d(ctx, row, col)) * 2u));
            ns = SVT_AV2_SYMS_COEFF_BASE;
            sym = min_u32(level, 3u);
        }
        {
            const int st = emit_sym(enc, model, cdf, ns, sym);
            if (st != SVT_AV2_COEFF_OK) {
                return st;
            }
        }

        /* coeff_br: present when level > baseLevels, except LF chroma where the
         * base symbol saturates at LF_NUM_BASE_LEVELS + 1 and no coeff_br
         * follows (5.20.7.27 p.228). */
        const uint32_t base_levels =
            is_lf ? SVT_AV2_LF_NUM_BASE_LEVELS : SVT_AV2_NUM_BASE_LEVELS;
        if (level > base_levels && !(is_lf && plane > 0u)) {
            const uint32_t brc = br_ctx_2d(cb.lv, cb.bwl, row, col, plane, is_lf);
            const uint16_t* br_cdf;
            uint32_t br_model;
            if (plane > 0u) {
                br_cdf = svt_av2_cdf_coeff_br_uv_by_qctx[q_ctx] +
                         (size_t)brc * (SVT_AV2_SYMS_COEFF_BR - 1u);
                br_model = SVT_AV2_MODEL_BR_UV + (uint32_t)((size_t)brc);
            } else if (is_lf) {
                br_cdf = svt_av2_cdf_coeff_br_lf_by_qctx[q_ctx] +
                         (size_t)brc * (SVT_AV2_SYMS_COEFF_BR - 1u);
                br_model = SVT_AV2_MODEL_BR_LF + (uint32_t)((size_t)brc);
            } else {
                br_cdf = svt_av2_cdf_coeff_br_by_qctx[q_ctx] +
                         (size_t)brc * (SVT_AV2_SYMS_COEFF_BR - 1u);
                br_model = SVT_AV2_MODEL_BR + (uint32_t)((size_t)brc);
            }
            const int st = write_low_range(enc, br_model, br_cdf, level, is_lf, plane);
            if (st != SVT_AV2_COEFF_OK) {
                return st;
            }
        }
        cb.lv[lvl_idx((int)pos, cb.bwl)] =
            (uint8_t)((level > 255u) ? 255u : level);
    }

    /* --- 4. DC magnitude, c = 0 (5.20.7.27 p.228) ------------------------ */
    {
        const uint32_t c = 0u;
        const uint32_t pos = cb.scan[0];
        const int row = (int)(pos >> cb.bwl);
        const int col = (int)(pos - ((uint32_t)row << cb.bwl));
        const int is_eob = (cb.eob == 1u);
        const int is_lf = lf_limits_2d(row, col, plane);
        const int64_t qv = (int64_t)qcoeff[pos];
        const uint32_t level = (uint32_t)((qv < 0) ? -qv : qv);
        const uint32_t mag = nz_mag_2d(cb.lv, cb.bwl, row, col,
                                       (is_lf && is_eob) ? 3u : (is_lf ? 5u : 3u), plane);
        const uint32_t ctx = (mag + 1u) >> 1;

        uint32_t sym;
        const uint16_t* cdf;
        uint32_t ns, model;
        if (is_eob) {
            const uint32_t eob_ctx_idx = lower_levels_ctx_eob(cb.bwl, cb.height, c);
            if (plane > 0u) {
                if (is_lf) {
                    cdf = svt_av2_cdf_coeff_base_lf_eob_uv_by_qctx[q_ctx] +
                          (size_t)eob_ctx_idx *
                              (SVT_AV2_SYMS_COEFF_BASE_EOB_LF - 1u);
                    model = SVT_AV2_MODEL_LF_EOB_UV + (uint32_t)((size_t)eob_ctx_idx);
                    ns = SVT_AV2_SYMS_COEFF_BASE_EOB_LF;
                    sym = min_u32(level, SVT_AV2_LF_BASE_SYMBOLS - 1u) - 1u;
                } else {
                    cdf = svt_av2_cdf_coeff_base_eob_uv_by_qctx[q_ctx] +
                          (size_t)eob_ctx_idx *
                              (SVT_AV2_SYMS_COEFF_BASE_EOB - 1u);
                    model = SVT_AV2_MODEL_BASE_EOB_UV + (uint32_t)((size_t)eob_ctx_idx);
                    ns = SVT_AV2_SYMS_COEFF_BASE_EOB;
                    sym = min_u32(level, 3u) - 1u;
                }
            } else if (is_lf) {
                cdf = svt_av2_cdf_coeff_base_lf_eob_by_qctx[q_ctx] +
                      ((size_t)cb.txs_ctx * 4u + eob_ctx_idx) *
                          (SVT_AV2_SYMS_COEFF_BASE_EOB_LF - 1u);
                model = SVT_AV2_MODEL_LF_EOB + (uint32_t)(((size_t)cb.txs_ctx * 4u + eob_ctx_idx));
                ns = SVT_AV2_SYMS_COEFF_BASE_EOB_LF;
                sym = min_u32(level, SVT_AV2_LF_BASE_SYMBOLS - 1u) - 1u;
            } else {
                cdf = svt_av2_cdf_coeff_base_eob_by_qctx[q_ctx] +
                      ((size_t)cb.txs_ctx * 4u + eob_ctx_idx) *
                          (SVT_AV2_SYMS_COEFF_BASE_EOB - 1u);
                model = SVT_AV2_MODEL_BASE_EOB + (uint32_t)(((size_t)cb.txs_ctx * 4u + eob_ctx_idx));
                ns = SVT_AV2_SYMS_COEFF_BASE_EOB;
                sym = min_u32(level, 3u) - 1u;
            }
        } else if (plane > 0u) {
            if (is_lf) {
                cdf = svt_av2_cdf_coeff_base_lf_uv_by_qctx[q_ctx] +
                      (size_t)uv_ctx_2d(ctx, plane) *
                          (SVT_AV2_SYMS_COEFF_BASE_LF - 1u);
                model = SVT_AV2_MODEL_LF_UV + (uint32_t)((size_t)uv_ctx_2d(ctx, plane));
                ns = SVT_AV2_SYMS_COEFF_BASE_LF;
                sym = min_u32(level, SVT_AV2_LF_BASE_SYMBOLS - 1u);
            } else {
                cdf = svt_av2_cdf_coeff_base_uv_by_qctx[q_ctx] +
                      (size_t)uv_ctx_2d(ctx, plane) *
                          (SVT_AV2_SYMS_COEFF_BASE - 1u);
                model = SVT_AV2_MODEL_BASE_UV + (uint32_t)((size_t)uv_ctx_2d(ctx, plane));
                ns = SVT_AV2_SYMS_COEFF_BASE;
                sym = min_u32(level, 3u);
            }
        } else if (is_lf) {
            cdf = svt_av2_cdf_coeff_base_lf_by_qctx[q_ctx] +
                  (((size_t)cb.txs_ctx * 33u + lf_ctx_2d(ctx, c, row, col)) * 2u) *
                      (SVT_AV2_SYMS_COEFF_BASE_LF - 1u);
            model = SVT_AV2_MODEL_LF + (uint32_t)((((size_t)cb.txs_ctx * 33u + lf_ctx_2d(ctx, c, row, col)) * 2u));
            ns = SVT_AV2_SYMS_COEFF_BASE_LF;
            sym = min_u32(level, SVT_AV2_LF_BASE_SYMBOLS - 1u);
        } else {
            cdf = svt_av2_cdf_coeff_base_by_qctx[q_ctx] +
                  (((size_t)cb.txs_ctx * 20u + hf_ctx_2d(ctx, row, col)) * 2u) *
                      (SVT_AV2_SYMS_COEFF_BASE - 1u);
            model = SVT_AV2_MODEL_BASE + (uint32_t)((((size_t)cb.txs_ctx * 20u + hf_ctx_2d(ctx, row, col)) * 2u));
            ns = SVT_AV2_SYMS_COEFF_BASE;
            sym = min_u32(level, 3u);
        }
        {
            const int st = emit_sym(enc, model, cdf, ns, sym);
            if (st != SVT_AV2_COEFF_OK) {
                return st;
            }
        }
        const uint32_t base_levels =
            is_lf ? SVT_AV2_LF_NUM_BASE_LEVELS : SVT_AV2_NUM_BASE_LEVELS;
        if (level > base_levels && !(is_lf && plane > 0u)) {
            const uint32_t brc = br_ctx_2d(cb.lv, cb.bwl, row, col, plane, is_lf);
            const uint16_t* br_cdf;
            uint32_t br_model;
            if (plane > 0u) {
                br_cdf = svt_av2_cdf_coeff_br_uv_by_qctx[q_ctx] +
                         (size_t)brc * (SVT_AV2_SYMS_COEFF_BR - 1u);
                br_model = SVT_AV2_MODEL_BR_UV + (uint32_t)((size_t)brc);
            } else if (is_lf) {
                br_cdf = svt_av2_cdf_coeff_br_lf_by_qctx[q_ctx] +
                         (size_t)brc * (SVT_AV2_SYMS_COEFF_BR - 1u);
                br_model = SVT_AV2_MODEL_BR_LF + (uint32_t)((size_t)brc);
            } else {
                br_cdf = svt_av2_cdf_coeff_br_by_qctx[q_ctx] +
                         (size_t)brc * (SVT_AV2_SYMS_COEFF_BR - 1u);
                br_model = SVT_AV2_MODEL_BR + (uint32_t)((size_t)brc);
            }
            const int st = write_low_range(enc, br_model, br_cdf, level, is_lf, plane);
            if (st != SVT_AV2_COEFF_OK) {
                return st;
            }
        }
        cb.lv[lvl_idx((int)pos, cb.bwl)] =
            (uint8_t)((level > 255u) ? 255u : level);
    }

    /* --- 5. signs and read_quant(), scan order c = eob-1 down to 0 -------- */
    {
        int32_t hr_level_avg = 0;
        for (uint32_t c = cb.eob; c-- > 0u;) {
            const uint32_t pos = cb.scan[c];
            const int row = (int)(pos >> cb.bwl);
            const int col = (int)(pos - ((uint32_t)row << cb.bwl));
            const int is_lf = lf_limits_2d(row, col, plane);
            const int64_t qv = (int64_t)qcoeff[pos];
            const uint32_t level = (uint32_t)((qv < 0) ? -qv : qv);
            if (level == 0u) {
                continue;
            }

            /* sign: dc_sign for the luma DC (c == 0), raw sign_bit otherwise
             * (5.20.7.27 p.228). Only TX_CLASS_2D is reachable this round. */
            if (c == 0u && plane == 0u) {
                const uint32_t sctx = dc_sign_ctx_of(nb, pi);
                /* plane_type = 0 (luma), isHidden = 0. */
                const uint16_t* dsc = svt_av2_cdf_dc_sign_by_qctx[q_ctx] +
                                      ((size_t)0 * 2u + 0u) * 3u *
                                          (SVT_AV2_SYMS_DC_SIGN - 1u) +
                                      (size_t)sctx * (SVT_AV2_SYMS_DC_SIGN - 1u);
                const int st = emit_sym(enc, SVT_AV2_MODEL_DC_SIGN + sctx, dsc, SVT_AV2_SYMS_DC_SIGN,
                                        (qv < 0) ? 1u : 0u);
                if (st != SVT_AV2_COEFF_OK) {
                    return st;
                }
            } else {
                /* sign_bit L(1): raw, 1 = negative (5.20.7.27 p.228). */
                const int st = emit_lit(enc, (qv < 0) ? 1u : 0u, 1);
                if (st != SVT_AV2_COEFF_OK) {
                    return st;
                }
            }

            /* read_quant() high-range escape (5.20.7.28 p.231): reached when the
             * level saturates the base + coeff_br range, i.e.
             * level >= maxLevel - allowTcq with allowTcq = 0 (TCQ disabled).
             * x = level - maxLevel is the residual coded by the adaptive
             * truncated-Rice scheme. */
            const uint32_t max_level =
                is_lf ? (plane == 0u ? (SVT_AV2_LF_NUM_BASE_LEVELS +
                                        SVT_AV2_COEFF_BASE_RANGE + 1u)
                                     : (SVT_AV2_LF_NUM_BASE_LEVELS + 1u))
                      : (SVT_AV2_NUM_BASE_LEVELS + SVT_AV2_COEFF_BASE_RANGE + 1u);
            if (level > (uint32_t)svt_av2_coeff_max_level()) {
                return SVT_AV2_COEFF_ERR_LEVEL_RANGE;
            }
            if (level >= max_level) {
                const int st = write_high_range(enc, level - max_level,
                                                &hr_level_avg);
                if (st != SVT_AV2_COEFF_OK) {
                    return st;
                }
            }
        }
    }

    /* --- 6. neighbour write-back (5.20.7.27 p.230) ----------------------- */
    {
        uint32_t cul_level = 0u;
        int dc_val = 0;
        for (uint32_t c = 0; c < cb.eob; ++c) {
            const uint32_t pos = cb.scan[c];
            const int64_t qv = (int64_t)qcoeff[pos];
            const uint32_t level = (uint32_t)((qv < 0) ? -qv : qv);
            cul_level += level;
            if (pos == 0u) {
                dc_val = (int)qv;
            }
        }
        cul_level = min_u32(cul_level, 4u);
        const uint8_t dc_category =
            (dc_val < 0) ? 1u : ((dc_val > 0) ? 2u : 0u);
        const uint32_t w4 = cb.ctx_w >> 2;
        const uint32_t h4 = cb.ctx_h >> 2;
        for (uint32_t i = 0; i < w4 && pi->x4 + i < SVT_AV2_COEFF_CTX_UNITS; ++i) {
            nb->above_level[pi->x4 + i] = (uint8_t)cul_level;
            nb->above_dc[pi->x4 + i] = dc_category;
        }
        for (uint32_t i = 0; i < h4 && pi->y4 + i < SVT_AV2_COEFF_CTX_UNITS; ++i) {
            nb->left_level[pi->y4 + i] = (uint8_t)cul_level;
            nb->left_dc[pi->y4 + i] = dc_category;
        }
    }

    return SVT_AV2_COEFF_OK;
}

int svt_av2_coeff_write_block_transform(SvtAv2EntropyEncoder* enc,
    const SvtAv2CoeffPlaneInfo* pi, uint8_t eob_u_nonzero, const int32_t* qcoeff,
    SvtAv2CoeffNeighbours* nb, uint32_t q_ctx, uint32_t is_inter,
    uint32_t reduced_tx_set, uint32_t tx_type) {
    return svt_av2_coeff_write_block_transform_intra(enc, pi, eob_u_nonzero, qcoeff,
        nb, q_ctx, is_inter, reduced_tx_set, tx_type, 0u);
}

int svt_av2_coeff_write_block_qctx(SvtAv2EntropyEncoder* enc,
    const SvtAv2CoeffPlaneInfo* pi, uint8_t eob_u_nonzero,
    const int32_t* qcoeff, SvtAv2CoeffNeighbours* nb, uint32_t q_ctx) {
    return svt_av2_coeff_write_block_prediction(enc, pi, eob_u_nonzero, qcoeff, nb, q_ctx, 0u);
}

int svt_av2_coeff_write_block(SvtAv2EntropyEncoder* enc,
    const SvtAv2CoeffPlaneInfo* pi, uint8_t eob_u_nonzero,
    const int32_t* qcoeff, SvtAv2CoeffNeighbours* nb) {
    return svt_av2_coeff_write_block_qctx(enc, pi, eob_u_nonzero, qcoeff, nb, 1u);
}


int svt_av2_coeff_write_block_prediction(SvtAv2EntropyEncoder* enc,
    const SvtAv2CoeffPlaneInfo* pi, uint8_t eob_u_nonzero, const int32_t* qcoeff,
    SvtAv2CoeffNeighbours* nb, uint32_t q_ctx, uint32_t is_inter) {
    return svt_av2_coeff_write_block_transform(enc, pi, eob_u_nonzero, qcoeff, nb,
        q_ctx, is_inter, 2u, 0u);
}
