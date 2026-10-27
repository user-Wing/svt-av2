/*
 * SVT-AV2 intra frame core (M2, T012).
 *
 * Scope: assemble the frozen per-block chain over a whole frame,
 *
 *   source -> prediction -> residual -> coeff -> qcoeff -> dqcoeff
 *          -> inverse residual -> clipped recon
 *
 * for ALL-INTRA, single thread, 8-bit 4:2:0, and produce the encoder's own
 * internal reconstruction BEFORE any decoder runs.
 *
 * What this file owns and what it does NOT:
 *
 *   - It owns the traversal (through T007's svt_av2_partition_traverse, so the
 *     block order and the edge geometry are T007's rules, not a second copy),
 *     the intra block plumbing (T008 reference/predict/residual), and the
 *     reconstruction write (T012's svt_av2_recon_write_block).
 *   - It does NOT own coefficients: the forward transform + quantisation and
 *     the inverse transform + dequantisation are T009/T010. They are reached
 *     through `SvtAv2IntraTransformPath` below.
 *   - It does NOT own syntax: writing qcoeff to the bitstream is T011, which
 *     reads the SAME `scratch->qcoeff` array this core leaves valid for the
 *     block it just processed (contract 4: one qcoeff, one source of truth).
 *
 * Why the transform step is a function-pointer seam
 * -------------------------------------------------
 * T009's Transform/ module does not exist yet. Per the team rules
 * (m1-m2-contracts.md section 10.1/10.3) this core must neither invent a fake
 * transform nor collapse the chain into a lossless copy of the source. So the
 * seam is explicit and FAILS CLOSED:
 *
 *   - Production callers pass a path built from the real T009/T010 modules.
 *   - Passing NULL (the state until those land) returns
 *     SVT_AV2_ERROR_UNSUPPORTED with a clear reason string; it never silently
 *     writes source into the reconstruction.
 *   - Only tests supply a path of their own, and such a path is test
 *     scaffolding, not a claimed implementation of T009 or T010.
 *
 * Frozen contract: docs/m1-m2-contracts.md section 6 (owner T012).
 */
#ifndef SVT_AV2_INTRA_FRAME_H
#define SVT_AV2_INTRA_FRAME_H

#include "common.h"
#include "block_geometry.h"
#include "picture.h"
#include "reconstruction.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- transform path seam (T009 + T010 boundary) --------------------------- */

/*
 * Everything the frame core needs from the transform/quantisation pair for one
 * transform block. Both entry points must be internally consistent: the
 * coefficients written to `qcoeff` by `forward` and the samples produced by
 * `inverse` must come from the same dqcoeff (contract 4: "the qcoeff written to
 * syntax and the coefficients used by the internal reconstruction must have one
 * source").
 *
 * The sample buffers (`residual` in, `inverse_residual` out) receive the
 * block-local stride used for every SAMPLE buffer in this core (samples per row,
 * `scratch->stride`), so a transform of any supported size can index its rows
 * without the core repacking.
 *
 * The coefficient arrays (`coeff` / `qcoeff` / `dqcoeff`) do NOT use that
 * stride: contract section 3 fixes their row stride at `min(w,32)` and their
 * length at `min(w,32) * min(h,32)` (TX_64X64 -> a 32x32 grid of 1024 entries,
 * not a 64-wide raster). A path must write them in that packed layout, which is
 * the same grid T011's scan indexes.
 *
 * `block_w`/`block_h` are the actual block dimensions, which at the frame edge
 * can be smaller than a transform size; the path is responsible for whatever
 * legal padding/crop rule the frozen transform contract specifies. The core
 * does not read or write outside the block.
 *
 * `plane` is SVT_AV2_PLANE_Y/U/V so a path can apply chroma scaling.
 *
 * Return SVT_AV2_ERROR_NONE on success. Any failure aborts the frame; the core
 * never treats a failed transform as a zero residual.
 */
typedef SvtAv2Error (*SvtAv2ForwardTransformFn)(void* user,
                                                uint32_t plane,
                                                uint32_t block_w,
                                                uint32_t block_h,
                                                const int16_t* residual,
                                                uint32_t stride,
                                                int32_t* coeff,
                                                int32_t* qcoeff,
                                                int32_t* dqcoeff,
                                                int* all_zero_out);

typedef SvtAv2Error (*SvtAv2InverseTransformFn)(void* user,
                                                uint32_t plane,
                                                uint32_t block_w,
                                                uint32_t block_h,
                                                const int32_t* dqcoeff,
                                                uint32_t stride,
                                                int16_t* inverse_residual);

typedef SvtAv2Error (*SvtAv2ForwardTransformTypeFn)(void* user, uint32_t plane,
    uint32_t block_w, uint32_t block_h, const int16_t* residual, uint32_t stride,
    int32_t* coeff, int32_t* qcoeff, int32_t* dqcoeff, int* all_zero_out, uint32_t tx_type);
typedef SvtAv2Error (*SvtAv2InverseTransformTypeFn)(void* user, uint32_t plane,
    uint32_t block_w, uint32_t block_h, const int32_t* dqcoeff, uint32_t stride,
    int16_t* inverse_residual, uint32_t tx_type);

typedef struct SvtAv2IntraTransformPath {
    void* user;
    SvtAv2ForwardTransformFn forward;
    SvtAv2InverseTransformFn inverse;
    /* Human-readable identity of this path, copied into the frame report. For
     * the production path this must name the T009/T010 modules and their frozen
     * transform size/scale; a test path must say so. NULL is treated as
     * "unnamed". */
    const char* name;
    SvtAv2ForwardTransformTypeFn forward_type;
    SvtAv2InverseTransformTypeFn inverse_type;
} SvtAv2IntraTransformPath;

/* --- per-block hook (for T011 syntax and for tests) ----------------------- */

/*
 * Called once per reconstructed block, AFTER the block is in the recon picture
 * and while `scratch->qcoeff` / `scratch->dqcoeff` still hold that block.
 *
 * This is how T011 gets its coefficients without the frame core knowing any
 * syntax: the writer copies/serialises qcoeff here. It is also the hook a test
 * uses to observe the block sequence.
 *
 * `plane`, `block` (plane-relative, visible w/h) and `all_zero` describe the
 * block. Returning non-zero aborts the frame with
 * SVT_AV2_ERROR_INVALID_STATE.
 */
typedef int (*SvtAv2IntraBlockHookFn)(void* user,
                                      uint32_t plane,
                                      const SvtAv2BlockView* block,
                                      const SvtAv2ReconScratch* scratch,
                                      int all_zero);

/* --- frame configuration --------------------------------------------------- */

typedef struct SvtAv2IntraFrameConfig {
    /* Frame quantiser, frozen to 100 for M2 (contract section 0). Passed
     * through to the transform path; the core does not interpret it. */
    uint32_t base_q_idx;

    /* Superblock size in luma samples: 64 (frozen), 128 or 256. */
    uint32_t sb_size;

    /* Transform path. NULL means "not wired yet" and fails closed. */
    SvtAv2IntraTransformPath transform;

    /* Optional per-block hook; NULL is allowed. */
    SvtAv2IntraBlockHookFn block_hook;
    void* block_hook_user;

    /* Value the reconstruction allocation is filled with before the first
     * block (svt_av2_recon_reset_frame). All-Intra has no inter reference, but
     * the intra neighbour state is per-frame; see reconstruction.h. */
    uint16_t frame_reset_value;
    uint32_t m5_select_intra; /* 0=frozen DC; 1=adaptive AI mode selection */
    int (*m5_mode_hook)(void* user, uint32_t mode);
    const uint16_t* partition_decisions; /* Immutable per-SB plan, NULL/0=frozen tree. */
    size_t partition_decision_count;
} SvtAv2IntraFrameConfig;

/* --- frame result ---------------------------------------------------------- */

typedef struct SvtAv2IntraFrameStats {
    uint32_t luma_blocks;    /* coding blocks whose luma plane was reconstructed */
    uint32_t chroma_blocks;  /* chroma blocks (2 per luma block: U and V) */
    uint32_t zero_blocks;    /* blocks whose qcoeff was all zero */
    uint32_t nonzero_blocks; /* blocks with at least one non-zero qcoeff */
} SvtAv2IntraFrameStats;

/* --- frame core ------------------------------------------------------------ */

/*
 * Encodes one frame's internal reconstruction.
 *
 * Writes the reconstruction into `recon` (visible area only; padding is left at
 * the reset value) and leaves the recon picture complete before returning. No
 * decoder is involved: this is the encoder's own reconstruction, and it is the
 * ONLY thing a later decoder run is allowed to be compared against
 * (contract 6).
 *
 * `source` must be a distinct picture (contract 1.2: source and recon are two
 * distinguishable buffers); passing the same object for both returns
 * SVT_AV2_ERROR_INVALID_PARAMETER.
 *
 * `scratch` must be initialised with a capacity covering the largest block the
 * traversal can produce (sb_size x sb_size is sufficient; edge sub-blocks are
 * smaller). `config->transform.forward`/`.inverse` must both be non-NULL.
 *
 * Neighbour availability (frozen with T008, see the T008 READY_FOR_INTEGRATION
 * note sections 1 and 3):
 *   - The frame starts with svt_av2_recon_reset_frame(recon,
 *     config->frame_reset_value), so nothing from a previous frame can be read
 *     as this frame's neighbour.
 *   - Blocks are visited in T007's raster order, so the above/left neighbours of
 *     every block except the frame's first row/column have already been
 *     reconstructed in this frame.
 *   - The core therefore passes the FRAME-BOUNDARY flags (have_left = x > 0,
 *     have_above = y > 0) to T008's svt_av2_intra_dc_predict_flags, matching
 *     the reference it built with svt_av2_intra_reference_build.
 *
 * HOWEVER — and this is a hard precondition, not a remark — that pairing is
 * only valid while the coding structure makes every in-picture neighbour
 * available as soon as it is raster-order-preceding. That holds for the frozen
 * configuration (one tile, one superblock row-major walk, no wavefront/bridge
 * frame). It stops holding the moment T011 introduces a tile boundary, a
 * dependent tile, or any coding boundary that leaves a raster-preceding block
 * unavailable: the decoder then substitutes a reference value while this core
 * would read a real sample, and the reconstruction silently diverges.
 *
 * T008's report section 5 and the T008 interface note section 3 require
 * T011/T012/T015 to re-check this jointly. Until then:
 *   - the core exposes the seam it needs (T008's *_flags entry points are
 *     already the ones being called, so switching to tile-aware flags is a
 *     local change here, not an API change);
 *   - the unit test test_neighbour_availability_is_raster_order asserts the
 *     precondition by checking, for every visited block, that
 *     block->x > 0 <=> the left neighbour was already reconstructed. If a
 *     future coding structure breaks it, that test fails loudly instead of the
 *     reconstruction diverging quietly.
 * A caller that supplies a coding structure with in-picture unavailability must
 * therefore extend this core (pass real flags) before wiring T011 to it.
 *
 * Returns:
 *   SVT_AV2_ERROR_NONE             on success
 *   SVT_AV2_ERROR_UNSUPPORTED      when the transform path is not wired
 *   SVT_AV2_ERROR_INVALID_PARAMETER on bad inputs
 *   SVT_AV2_ERROR_INVALID_STATE    when a hook or the transform path failed
 *   SVT_AV2_ERROR_OUT_OF_MEMORY    when the geometry is unusable
 * `stats` may be NULL. On failure `recon` may hold a partially reconstructed
 * frame and must not be exported as a complete result.
 */
SvtAv2Error svt_av2_intra_frame_encode(const SvtAv2FrameBuffer* source,
                                       SvtAv2FrameBuffer* recon,
                                       const SvtAv2IntraFrameConfig* config,
                                       SvtAv2ReconScratch* scratch,
                                       SvtAv2IntraFrameStats* stats);

#ifdef __cplusplus
}
#endif

#endif /* SVT_AV2_INTRA_FRAME_H */
