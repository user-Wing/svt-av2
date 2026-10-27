/*
 * SVT-AV2 - coefficient (transform block) syntax for M2 (T011).
 *
 * Scope: everything below the transform-block boundary that the coefficient
 * coding path owns:
 *   - scan order for DCT_DCT (TX_CLASS_2D),
 *   - EOB / last position (the spec's eob_pt_* / eob_extra encoding),
 *   - base level + high-range level (coeff_base / coeff_base_eob / coeff_br and
 *     the read_quant() escape),
 *   - signs (dc_sign for luma DC, sign_bit / L(1) for everything else),
 *   - the real 8.3.2 context derivation (all_zero ctx, eobCtx, lfCtx/hfCtx/uvCtx,
 *     br ctx, dc_sign ctx) and the above/left neighbour state lifecycle.
 *
 * It does NOT own the range coder: it writes through SvtAv2EntropyEncoder from
 * EntropyCoding/entropy_encoder.h (the single coder in the tree; a second one is
 * forbidden by docs/m1-m2-contracts.md section 5). CDF values come from
 * cdf_tables_coeff.h, machine-derived from AVM's default tables.
 *
 * Every symbol and context rule is justified clause by clause in
 * docs/coeff-syntax.md; that file is the normative reference for this module.
 *
 * Frozen configuration (docs/m1-m2-contracts.md section 0):
 *   - 8-bit 4:2:0, single thread, All-Intra.
 *   - Explicit q-context variants accept all four coefficient CDF categories;
 *     the original wrapper retains category 1 for frozen QP100 callers.
 *   - DCT_DCT only (TX_CLASS_2D).
 *   - TCQ, parity hiding, FSC/IDTX, IST, CCTX, qmatrix all disabled.
 *
 * Spec references (AV2 Bitstream & Decoding Process Specification v1.0.0):
 *   - 5.20.7.27 coeffs(), 5.20.7.28 read_quant(), 5.20.7.30 get_scan()
 *   - 6.19.7.23 coefficients semantics
 *   - 8.3.2 cdf_selection_process()
 */
#ifndef SVT_AV2_COEFF_SYNTAX_H
#define SVT_AV2_COEFF_SYNTAX_H

#include <stddef.h>
#include <stdint.h>

#include "entropy_encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- frozen geometry limits ------------------------------------------------ */

/* Largest square transform this module handles (TX_64X64). */
#define SVT_AV2_COEFF_MAX_TX_DIM 64
#define SVT_AV2_COEFF_MAX_TX_AREA (SVT_AV2_COEFF_MAX_TX_DIM * SVT_AV2_COEFF_MAX_TX_DIM)

/* Per-plane 4x4-unit neighbour context extent for the M2 maximum 1920x1080
 * picture. Absolute x4/y4 indexing preserves neighbours across superblocks. */
#define SVT_AV2_COEFF_CTX_UNITS 480

/* Status: OK plus the encoder statuses. Errors are explicit; a block is never
 * partially encoded and then reported as success. */
typedef enum SvtAv2CoeffStatus {
    SVT_AV2_COEFF_OK = 0,
    SVT_AV2_COEFF_ERR_NULL = -1,
    /* Unsupported configuration, or a syntax element this module's coder
     * interface cannot emit (see svt_av2_coeff_literals_supported()). */
    SVT_AV2_COEFF_ERR_UNSUPPORTED = -2,
    /* The level cannot be represented at this configuration. */
    SVT_AV2_COEFF_ERR_LEVEL_RANGE = -3,
    /* The range coder ran out of output/precarry space. */
    SVT_AV2_COEFF_ERR_NO_SPACE = -4
} SvtAv2CoeffStatus;

/* --- plane / transform description ----------------------------------------- */

/* Square and small-rectangle IDs remain frozen; larger DCT sizes append. */
typedef enum SvtAv2TxSize {
    SVT_AV2_TX_4X4 = 0,
    SVT_AV2_TX_8X8 = 1,
    SVT_AV2_TX_16X16 = 2,
    SVT_AV2_TX_32X32 = 3,
    SVT_AV2_TX_64X64 = 4,
    SVT_AV2_TX_4X8 = 5,
    SVT_AV2_TX_8X4 = 6,
    SVT_AV2_TX_8X16 = 7,
    SVT_AV2_TX_16X8 = 8,
    SVT_AV2_TX_32X64 = 9,
    SVT_AV2_TX_64X32 = 10,
    SVT_AV2_TX_16X32 = 11,
    SVT_AV2_TX_32X16 = 12,
    SVT_AV2_TX_16X64 = 13,
    SVT_AV2_TX_64X16 = 14,
    SVT_AV2_TX_8X32 = 15,
    SVT_AV2_TX_32X8 = 16,
    SVT_AV2_TX_4X16 = 17,
    SVT_AV2_TX_16X4 = 18,
    SVT_AV2_TX_8X64 = 19,
    SVT_AV2_TX_64X8 = 20,
    SVT_AV2_TX_4X32 = 21,
    SVT_AV2_TX_32X4 = 22,
    SVT_AV2_TX_SIZE_COUNT = 23
} SvtAv2TxSize;

int svt_av2_coeff_tx_size_from_dims(uint32_t width, uint32_t height, SvtAv2TxSize* tx_size);

/* txSzCtx used by the coefficient CDF selection:
 *   txSzCtx = (Tx_Size_Sqr[txSz] + Tx_Size_Sqr_Up[txSz] + 1) >> 1
 * Square transforms use their original index; 2:1 rectangles use the larger
 * square index; 4:1 rectangles use the intermediate square index. */
uint32_t svt_av2_coeff_tx_sz_ctx(SvtAv2TxSize tx_size);

/* Transform-block geometry in samples (unadjusted; a 64x64 transform reports
 * 64 here). The coded coefficient grid is clamped to 32x32 by
 * svt_av2_coeff_coeff_count(); see docs/coeff-syntax.md section 8. */
uint32_t svt_av2_coeff_tx_width(SvtAv2TxSize tx_size);
uint32_t svt_av2_coeff_tx_height(SvtAv2TxSize tx_size);
/* log2(width) of the unadjusted transform; the get_txb_bwl()
 * equivalent is derived from the adjusted (<=32) size. */
uint32_t svt_av2_coeff_tx_bwl(SvtAv2TxSize tx_size);

/* Number of coded coefficients: av2_get_max_eob() = min(w,32)*min(h,32), so a
 * TX_64X64 block codes 1024 coefficients and its scan is the 32x32 order. */
uint32_t svt_av2_coeff_coeff_count(SvtAv2TxSize tx_size);

/* --- scan order ------------------------------------------------------------ */

/* Fills `scan` (one entry per coded coefficient, raster positions in scan
 * order) for DCT_DCT at tx_size. Returns the number of coefficients written, or
 * 0 for an unsupported size. `scan` must have room for at least
 * svt_av2_coeff_coeff_count(tx_size) entries.
 *
 * The order is the anti-diagonal (DCT_DCT / TX_CLASS_2D) scan over the coded
 * grid, which is clamped to 32x32: TX_64X64 yields the 1024-entry 32x32 order
 * (AVM reuses default_scan_32x32 for it). Verified against AVM's
 * default_scan_4x4/8x8/16x16/32x32 tables. */
size_t svt_av2_coeff_scan_dct_dct(SvtAv2TxSize tx_size, uint16_t* scan);

/* --- EOB / last position --------------------------------------------------- */

/* Returns the scan index one past the last non-zero coefficient, i.e. `eob` in
 * the spec's sense: 0 for an all-zero block. */
uint32_t svt_av2_coeff_find_eob(const int32_t* qcoeff, const uint16_t* scan,
                                size_t count);

/* --- neighbour context state ----------------------------------------------- */

/*
 * Per-plane coefficient neighbour state, i.e. the spec's
 * AboveLevelContext / LeftLevelContext / AboveDcContext / LeftDcContext, at a
 * 4x4-unit granularity. One instance per plane, spanning the whole picture.
 *
 * These are NOT the transform-block level buffer (that lives inside
 * svt_av2_coeff_write_block); they carry `culLevel` and the DC sign category
 * across blocks, exactly as coeffs() writes them back at the end.
 */
typedef struct SvtAv2CoeffNeighbours {
    uint8_t above_level[SVT_AV2_COEFF_CTX_UNITS];
    uint8_t left_level[SVT_AV2_COEFF_CTX_UNITS];
    uint8_t above_dc[SVT_AV2_COEFF_CTX_UNITS];
    uint8_t left_dc[SVT_AV2_COEFF_CTX_UNITS];
} SvtAv2CoeffNeighbours;

/* Zeroes one plane's neighbour state. Must be called at the start of a tile for
 * every plane: with M2's single-tile stream that means per frame. */
void svt_av2_coeff_neighbours_reset(SvtAv2CoeffNeighbours* nb);

/* --- block description ----------------------------------------------------- */

/*
 * Everything below the transform-block boundary that the context derivation
 * needs and that the coefficient module does not own.
 *
 *   plane 0 = Y, 1 = U, 2 = V.
 *   tx_size    supported transform size of this block.
 *   x4, y4     block origin in 4x4 units within the plane.
 *   units_w/h  plane width/height in 4x4 units (MiCols/MiRows equivalents).
 *              Neighbour units at or beyond these are treated as absent.
 *   bw, bh     the *residual block* size in samples (the spec's `bw`/`bh`), used
 *              only by the all_zero "block size == transform size" shortcut and
 *              by the chroma V ctx += 3 rule.
 */
typedef struct SvtAv2CoeffPlaneInfo {
    uint32_t    plane;
    SvtAv2TxSize tx_size;
    uint32_t    x4;
    uint32_t    y4;
    uint32_t    units_w;
    uint32_t    units_h;
    uint32_t    bw;
    uint32_t    bh;
} SvtAv2CoeffPlaneInfo;

/* --- block writer ---------------------------------------------------------- */

/*
 * Writes the coefficient syntax of one transform block.
 *
 *   enc            range encoder bound to the caller's buffers (not owned).
 *   plane_info     plane / transform / position description (see above).
 *   eob_u_nonzero  non-zero when the U plane of the same luma block had
 *                  coefficients; only used for plane 2 (spec 8.3.2 p.599).
 *                  Pass 0 when U has not been coded yet.
 *   qcoeff         raster-order quantized coefficients of the coded grid
 *                  (av2_get_max_eob(), i.e. min(w,32)*min(h,32) values: 1024 for
 *                  both TX_64X64 and TX_32X32). Every index touched is
 *                  `scan[c] < svt_av2_coeff_coeff_count(tx_size)`, so a caller
 *                  that sizes the buffer to the coded count is safe; a 4096-entry
 *                  64-wide buffer is NOT what this module reads. Owned by T010.
 *   nb             neighbourhood state for this plane; updated in place with
 *                  this block's culLevel / DC category.
 *
 * Returns SVT_AV2_COEFF_OK on success. On error nothing beyond an already
 * committed prefix of the block may be assumed; callers must treat a failed
 * block as fatal for the frame (a range coder has no rollback).
 */
/* q_ctx is 0..3, derived from actual qindex by svt_av2_coeff_q_context(). */
int svt_av2_coeff_write_block_prediction(SvtAv2EntropyEncoder* enc,
    const SvtAv2CoeffPlaneInfo* pi, uint8_t eob_u_nonzero,
    const int32_t* qcoeff, SvtAv2CoeffNeighbours* nb, uint32_t q_ctx, uint32_t is_inter);
int svt_av2_coeff_write_block_qctx(SvtAv2EntropyEncoder* enc,
    const SvtAv2CoeffPlaneInfo* pi, uint8_t eob_u_nonzero,
    const int32_t* qcoeff, SvtAv2CoeffNeighbours* nb, uint32_t q_ctx);

int svt_av2_coeff_write_block(SvtAv2EntropyEncoder* enc,
                              const SvtAv2CoeffPlaneInfo* plane_info,
                              uint8_t eob_u_nonzero, const int32_t* qcoeff,
                              SvtAv2CoeffNeighbours* nb);

/* Reports the maximum level this module can encode. Levels above it make
 * svt_av2_coeff_write_block() fail with ERR_LEVEL_RANGE rather than emit a
 * stream the decoder would reject. */
/* Fixed AVM entropy.c: thresholds apply to actual qindex, without bit-depth offset removal. */
uint32_t svt_av2_coeff_q_context(uint32_t base_qindex);
int32_t svt_av2_coeff_max_level(void);

/*
 * Non-zero when the entropy coder bound by the caller can emit the raw L(n)
 * fields coeffs() interleaves with its entropy symbols (eob_pt_*_extra,
 * eob_extra_bit, sign_bit, read_quant()).
 *
 * The current entropy coder supports bypass literals, including the adaptive
 * Rice/Exp-Golomb escape up to the 20-bit coefficient ceiling.
 */
int svt_av2_coeff_literals_supported(void);

int svt_av2_coeff_write_block_transform_intra(SvtAv2EntropyEncoder* enc,
    const SvtAv2CoeffPlaneInfo* pi, uint8_t eob_u_nonzero, const int32_t* qcoeff,
    SvtAv2CoeffNeighbours* nb, uint32_t q_ctx, uint32_t is_inter,
    uint32_t reduced_tx_set, uint32_t tx_type, uint32_t intra_mode);

int svt_av2_coeff_write_block_transform(SvtAv2EntropyEncoder* enc,
    const SvtAv2CoeffPlaneInfo* pi, uint8_t eob_u_nonzero, const int32_t* qcoeff,
    SvtAv2CoeffNeighbours* nb, uint32_t q_ctx, uint32_t is_inter,
    uint32_t reduced_tx_set, uint32_t tx_type);

#ifdef __cplusplus
}
#endif

#endif /* SVT_AV2_COEFF_SYNTAX_H */
