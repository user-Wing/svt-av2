/*
 * SVT-AV2 internal reconstruction (M2, T012).
 *
 * Scope: the reconstruction side of the frozen block chain and the per-frame
 * scratch/lifecycle plumbing the frame core needs. It owns
 *
 *   - the clipped write of `prediction + inverse residual` into the internal
 *     reconstruction picture (which is also what makes the block available as
 *     a neighbour for the blocks that follow),
 *   - the frame-level scratch pool, allocated once per frame and reused for
 *     every block (contract 6: no per-block malloc),
 *   - the per-frame reset of the intra neighbour state,
 *   - the export descriptor that ties tightly packed raw bytes to a visible
 *     area and a frame index.
 *
 * This module contains NO prediction, transform, quantisation or syntax. It
 * consumes an already computed inverse residual, so it is independent of
 * T009/T010 and can be verified before they land.
 *
 * Frozen contract: docs/m1-m2-contracts.md section 6 (owner T012).
 *
 * THE ONE LEGAL CHAIN (contract 6, do not add branches):
 *
 *   source -> prediction -> residual -> coeff -> qcoeff -> dqcoeff
 *          -> inverse residual -> clipped recon
 *
 * `svt_av2_recon_write_block()` performs the final arrow only. Everything
 * before it is the frame core's job (Encoder/intra_frame.*) and the transform
 * path's. In particular there is deliberately no "lossless shortcut" here that
 * copies source into recon: writing source is forbidden (contract 9.2/9.3).
 *
 * 8-bit only. Clipping is to [0, 255] (contract 6). 10-bit is M3.
 *
 * Ownership / lifetime (contract 6):
 *   - The reconstruction picture is owned by the caller (T013's encoder owns
 *     it for the frame's lifetime) and must outlive any prediction that reads
 *     it as a neighbour.
 *   - The scratch pool owns its buffers; free with svt_av2_recon_pool_free.
 *     Buffers stay valid until that call, so a caller may move the pool
 *     between frames (contract 6: scratch is reused across frames).
 */
#ifndef SVT_AV2_RECONSTRUCTION_H
#define SVT_AV2_RECONSTRUCTION_H

#include "block_geometry.h"
#include "common.h"
#include "picture.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 8-bit clipping bounds (contract 6: clip to [0, 255]). */
#define SVT_AV2_RECON_MIN_SAMPLE 0
#define SVT_AV2_RECON_MAX_SAMPLE 255

/* --- scratch pool ---------------------------------------------------------- */

/*
 * Per-frame working buffers, sized once from the superblock geometry and then
 * reused for every block of every frame. Everything the frame core needs for
 * one block lives here, so no block allocates.
 *
 * Layout contract, one block at a time, block-local (matching T008's residual
 * layout, intra_prediction.h section "residual"):
 *   - `residual[i * stride + j]` holds block offset (i, j) with i in [0, h)
 *     and j in [0, w).
 *   - `stride` is in SAMPLES, not bytes, and is checked against the BLOCK
 *     width. It is padded up to a multiple of SVT_AV2_RECON_SCRATCH_STRIDE_ALIGN
 *     so row addressing stays cheap and identical for every block size.
 *   - Only rows [0, h) and columns [0, w) of a buffer are meaningful; the rest
 *     is unspecified and must not be read. `capacity` is the number of samples
 *     reserved in each SAMPLE buffer (prediction / residual / inverse_residual),
 *     which is stride * max_block_height.
 *   - The three coefficient arrays (`coeff` / `qcoeff` / `dqcoeff`) do NOT use
 *     `stride`: per contract section 3 their row stride is `min(w,32)` and their
 *     length is `min(w,32) * min(h,32)` (see the struct fields below), so
 *     TX_64X64 carries 1024 entries, not 4096.
 *
 * `max_block_width`/`max_block_height` are the largest coding block the caller
 * will pass. For the frozen 64x64 superblock configuration that is 64x64; a
 * caller driving 128/256 superblocks passes the corresponding size. Both must
 * be non-zero.
 */
#define SVT_AV2_RECON_SCRATCH_STRIDE_ALIGN 16u

typedef struct SvtAv2ReconScratch {
    /* Block-local prediction buffer, [0, 255], uint8_t, for callers that want a
     * standalone prediction instead of predicting in place. The frame core
     * (Encoder/intra_frame.c) predicts straight into the recon picture, the way
     * AVM does, and therefore does not use this buffer; the reconstruction unit
     * tests and any block-local consumer do. It never aliases the recon
     * picture. */
    uint8_t* prediction;
    /* Signed residual = source - prediction (T008 contract 2.3). */
    int16_t* residual;
    /* Inverse residual for the current block, i.e. the samples the inverse
     * transform produced BEFORE adding the prediction. int16_t because the
     * inverse transform output can leave [0, 255] before clipping. */
    int16_t* inverse_residual;

    /* Forward-path coefficient arrays, int32_t per contract 3/4. Allocated
     * here so the frame core never allocates per block. `coeff` is T009's
     * output, `qcoeff` what T011 writes syntax from, `dqcoeff` what the
     * inverse path consumes. They must never alias (contract 4).
     *
     * Layout (contract section 3): row stride `min(block_w, 32)` and length
     * `min(block_w,32) * min(block_h,32)` -- for TX_64X64 that is a 32x32 grid
     * of 1024 entries, NOT a 64-wide raster of 4096. `coeff_capacity` below is
     * that length; T011's scan is built on the same 32-wide grid. */
    int32_t* coeff;
    int32_t* qcoeff;
    int32_t* dqcoeff;

    uint32_t max_block_width;
    uint32_t max_block_height;
    uint32_t stride;    /* samples per row for the block-local buffers */
    size_t capacity;    /* samples reserved in each of the three sample buffers
                         * (prediction / residual / inverse_residual) */
    size_t coeff_capacity; /* entries reserved in each coefficient array */
    int      allocated; /* non-zero once init succeeded */
} SvtAv2ReconScratch;

/* Allocates the pool for blocks up to `max_block_width` x `max_block_height`.
 *
 * All buffers start zeroed. On failure `scratch` is left zeroed and safe to
 * pass to svt_av2_recon_pool_free. Overflow in the size arithmetic is checked
 * before the multiplication (contract 1.4).
 */
SvtAv2Error svt_av2_recon_pool_init(SvtAv2ReconScratch* scratch,
                                    uint32_t max_block_width,
                                    uint32_t max_block_height);

/* Releases every buffer and zeroes the descriptor. Idempotent. */
SvtAv2Error svt_av2_recon_pool_free(SvtAv2ReconScratch* scratch);

/* True when `block_w` x `block_h` fits the pool. */
int svt_av2_recon_pool_fits(const SvtAv2ReconScratch* scratch,
                            uint32_t block_w,
                            uint32_t block_h);

/* --- the final chain step -------------------------------------------------- */

/*
 * Writes `prediction + inverse_residual`, clipped to [0, 255], into `recon`'s
 * plane `plane` for `block`.
 *
 * This is the ONLY function in the project that turns an inverse residual into
 * reconstructed samples, and it is deliberately narrow so the chain cannot
 * grow a second branch:
 *
 *   recon_sample = Clip3(0, 255, prediction_sample + inverse_residual_sample)
 *
 * `prediction` and `inverse_residual` are block-local, `stride` samples per
 * row, indexed `[i * stride + j]` for i in [0, h), j in [0, w). The two
 * buffers may be the same stride but must not be the same pointer.
 *
 * Because the samples land inside `recon`'s visible area at
 * `plane + (y + i) * stride + (x + j)`, this write IS the neighbour update the
 * next block's prediction reads (contract 2.2: neighbours come from the
 * internal reconstruction, never from source). There is no separate
 * "publish" step and callers must not add one that writes elsewhere.
 *
 * `block` is a T007 `SvtAv2BlockView` in plane-relative pixel coordinates: it
 * covers col in [x, x+w) and row in [y, y+h), and it must lie entirely inside
 * the visible area of `plane`. Only x/y/w/h are read; `block->base` is not used
 * (the plane is selected by the `plane` argument, and `svt_av2_partition_traverse`
 * leaves `base` NULL by design -- the geometry layer holds no picture, see T007's
 * frozen-interface correction of 2026-10-07). The function never writes
 * padding and never writes outside the visible area, so an edge block cannot
 * corrupt another plane.
 *
 * Strides are in SAMPLES and `prediction_stride` may differ from
 * `inverse_residual_stride`:
 *
 *   - Block-local buffers: pass the scratch stride (see
 *     SvtAv2ReconScratch) for both, and only rows [0, h) / columns [0, w) are
 *     read.
 *   - In-place prediction: the frame core lets T008's predictor write the
 *     prediction straight into the recon plane and then calls this function
 *     with `prediction` = the plane address of the block's top-left sample and
 *     `prediction_stride` = the plane's stride. That is the ordering AVM uses
 *     (predict into the reconstruction buffer, then add the residual) and it
 *     avoids a second prediction buffer. Reading a sample and then writing the
 *     same sample is well defined here because each output sample depends only
 *     on the input at the same offset.
 *
 * `inverse_residual_stride` must be >= block->w, and `prediction_stride` must
 * be >= block->w.
 *
 * Returns SVT_AV2_ERROR_INVALID_PARAMETER for NULL pointers, a zero-sized
 * block, an out-of-range plane, a stride smaller than the block width, or a
 * block outside the visible area. On failure nothing is written.
 */
SvtAv2Error svt_av2_recon_write_block(SvtAv2FrameBuffer* recon,
                                      uint32_t plane,
                                      const SvtAv2BlockView* block,
                                      const uint8_t* prediction,
                                      uint32_t prediction_stride,
                                      const int16_t* inverse_residual,
                                      uint32_t inverse_residual_stride);

/*
 * Clip helper, exposed so tests can pin the boundary behaviour instead of
 * trusting a macro. Returns Clip3(0, 255, value).
 */
uint8_t svt_av2_recon_clip_sample(int32_t value);

/* --- per-frame lifecycle --------------------------------------------------- */

/*
 * Resets the frame's intra neighbour state.
 *
 * All-Intra has no inter reference, but the intra neighbour state is still
 * per-frame: the first block of a frame must not read samples reconstructed
 * for the previous frame. This function fills the whole allocation of every
 * plane (visible area AND padding) with `fill_value`, so
 *
 *   - any sample a buggy neighbour read touches is deterministic instead of
 *     whatever the previous frame left behind, and
 *   - a test can pass a value that no legal reconstruction can produce
 *     (e.g. a byte pattern chosen to be distinguishable) and then assert that
 *     the frame's output does not depend on it.
 *
 * It does NOT define the first block's prediction: the normative
 * unavailable-neighbour rule (T008, AV2 section 7.13.2.1) handles that, and
 * filling with a prediction-like constant would hide a missing-flag bug. Call
 * this once at the start of every frame, before the first block. The frame
 * core does it for its callers.
 *
 * Returns SVT_AV2_ERROR_INVALID_PARAMETER for a NULL or unallocated picture.
 */
SvtAv2Error svt_av2_recon_reset_frame(SvtAv2FrameBuffer* recon, uint16_t fill_value);

/* --- export --------------------------------------------------------------- */

/*
 * Descriptor for one exported frame of tightly packed 8-bit 4:2:0 raw.
 *
 * The bytes themselves are produced by T007's single export primitive,
 * svt_av2_picture_export_raw(); this project must not contain a second copy of
 * that packing (contract 6 and picture.h). This descriptor exists because the
 * raw stream carries no frame index and no visible dimensions of its own, and
 * T014 needs both to compare frames one by one.
 */
typedef struct SvtAv2ReconExport {
    uint32_t bit_depth;
    SvtAv2ChromaFormat chroma_format;
    uint32_t frame_index;     /* caller-assigned, starts at 0, increments by 1 */
    uint32_t visible_width;   /* luma samples */
    uint32_t visible_height;  /* luma rows */
    uint32_t chroma_width;    /* visible_width >> 1  (4:2:0) */
    uint32_t chroma_height;   /* visible_height >> 1 (4:2:0) */
    size_t   byte_count;      /* bytes written: W*H + 2*(W/2)*(H/2) */
} SvtAv2ReconExport;

/*
 * Exports the visible samples of `recon` as tightly packed 8-bit 4:2:0 raw and
 * fills `meta` with the frame index and visible geometry.
 *
 * `out_capacity` must be at least the size reported by
 * svt_av2_picture_raw_size(); a smaller buffer fails without writing anything
 * and without touching `meta`. `padding is never emitted`: this is what makes
 * the bytes comparable with tools/compare_recon.py.
 *
 * `frame_index` is recorded, not derived: the caller knows which frame it is
 * exporting (picture.h's export primitive makes the same point).
 *
 * Returns SVT_AV2_ERROR_INVALID_PARAMETER on NULL/unallocated inputs or
 * insufficient capacity.
 */
SvtAv2Error svt_av2_recon_export_frame(const SvtAv2FrameBuffer* recon,
                                       uint32_t frame_index,
                                       uint8_t* out,
                                       size_t out_capacity,
                                       SvtAv2ReconExport* meta);

#ifdef __cplusplus
}
#endif

#endif /* SVT_AV2_RECONSTRUCTION_H */
