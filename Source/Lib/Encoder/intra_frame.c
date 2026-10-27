/* SVT-AV2 intra frame core (M2, T012).
 *
 * Assembles the frozen per-block chain for one ALL-INTRA frame. See
 * intra_frame.h for the contract and for why the transform step is an explicit
 * function-pointer seam that fails closed.
 *
 * Spec references (AV2 Bitstream & Decoding Process Specification v1.0.0):
 *   - section 7.13.2.1   intra prediction per transform block, AboveRow/LeftCol
 *   - section 7.13.2.10  DC intra prediction
 *   Reconstruction is the decoder's `Clip1(prediction + residual)`; the exact
 *   clause number has NOT been re-extracted here, so this file does not cite
 *   one. The [0, 255] bound for 8-bit follows Clip1 from section 4.8, which
 *   T008 verified (see docs/task-reports/T008.md).
 * Cross-check (read-only AVM source, not copied):
 *   - avm\av2\decoder\decodeframe.c  predict_intra / recon_intra ordering
 *   - avm\avm_dsp\intrapred.c:175-236 DC predictors
 */

#include "intra_frame.h"

#include <string.h>

#include "intra_prediction.h"

/* One coding block of one plane, in plane-relative coordinates. */
typedef struct FrameCore {
    const SvtAv2FrameBuffer* source;
    SvtAv2FrameBuffer* recon;
    const SvtAv2IntraFrameConfig* config;
    SvtAv2ReconScratch* scratch;
    SvtAv2FrameGeometry geometry;
    SvtAv2IntraFrameStats stats;
    int failed; /* set when a hook or the transform path reported failure */
} FrameCore;

/* Pixel SAD over a predicted block, with byte strides and 8/10-bit loads. */
static uint64_t m5_prediction_sad(const SvtAv2FrameBuffer* source,
    const SvtAv2FrameBuffer* recon, uint32_t plane, const SvtAv2BlockView* block) {
    const uint32_t step = source->bit_depth > 8u ? 2u : 1u;
    uint64_t sad = 0u;
    for (uint32_t y = 0; y < block->h; ++y) {
        const uint8_t* src = source->planes[plane] +
            (size_t)(block->y + y) * source->strides[plane] + block->x * step;
        const uint8_t* pred = recon->planes[plane] +
            (size_t)(block->y + y) * recon->strides[plane] + block->x * step;
        for (uint32_t x = 0; x < block->w; ++x) {
            const int diff = (int)svt_av2_sample_load(src + x * step, source->bit_depth) -
                (int)svt_av2_sample_load(pred + x * step, source->bit_depth);
            sad += (uint64_t)(diff < 0 ? -diff : diff);
        }
    }
    return sad;
}

/* Reconstructs one block of one plane through the frozen chain, ending in the
 * clipped write into the recon picture. */
static SvtAv2Error reconstruct_block(FrameCore* core,
                                     uint32_t plane,
                                     const SvtAv2BlockView* block) {
    /* A block-local view for T008: same geometry, plane made explicit. */
    SvtAv2IntraBlock intra_block;
    intra_block.x = block->x;
    intra_block.y = block->y;
    intra_block.w = block->w;
    intra_block.h = block->h;
    intra_block.plane = plane;

    uint8_t* const plane_base = svt_av2_picture_plane(core->recon, plane);
    if (plane_base == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    const uint32_t plane_stride = core->recon->strides[plane];
    /* The block address is derived here from the plane base and the block's
     * x/y. `block->base` is deliberately NOT read: blocks arriving from
     * svt_av2_partition_traverse carry base == NULL by design (T007's frozen
     * interface: the geometry layer holds no picture). Deriving the address is
     * the documented usage for a consumer that needs pixels.
     *
     * Prediction goes straight into the recon picture at the block's position,
     * which is where AVM predicts too. It is transient: the clipped write at
     * step 4 replaces it with `prediction + inverse residual`, and only after
     * that is the block a valid neighbour for the blocks that follow. */
    uint8_t* const pred_dst = plane_base + (size_t)block->y * plane_stride + block->x * (core->recon->bit_depth > 8 ? 2u : 1u);

    /* 1. prediction from the internal reconstruction.
     *
     * Availability comes from T007's single source of truth
     * (svt_av2_block_neighbors: above_valid = y > 0, left_valid = x > 0) rather
     * than a locally re-derived `x > 0` rule, so the frame-boundary policy
     * cannot drift from the geometry module when a coding boundary is added.
     * See the note in intra_frame.h: this is only correct while a raster-order
     * predecessor is genuinely available, which test_intra_frame asserts. */
    SvtAv2BlockNeighbors neighbors;
    {
        const SvtAv2Error nb_err = svt_av2_block_neighbors(core->recon, plane, block, &neighbors);
        if (nb_err != SVT_AV2_ERROR_NONE) {
            return nb_err;
        }
    }

    SvtAv2IntraReference reference;
    SvtAv2Error err = svt_av2_intra_reference_build(core->recon, &intra_block, &reference);
    if (err != SVT_AV2_ERROR_NONE) {
        return err;
    }
    err = svt_av2_intra_dc_predict_flags(&reference,
                                         &intra_block,
                                         neighbors.left_valid,
                                         neighbors.above_valid,
                                         pred_dst,
                                         plane_stride);
    if (err != SVT_AV2_ERROR_NONE) {
        return err;
    }

    uint32_t m5_mode = 0u;
    if (plane == SVT_AV2_PLANE_Y && core->config->m5_select_intra) {
        uint64_t best_sad = m5_prediction_sad(core->source, core->recon, plane, block);
        for (uint32_t candidate = 1u; candidate <= 2u; ++candidate) {
            if ((candidate == 1u && !neighbors.above_valid) ||
                (candidate == 2u && !neighbors.left_valid))
                continue;
            const SvtAv2IntraAxis axis = candidate == 1u
                ? SVT_AV2_INTRA_AXIS_VERTICAL : SVT_AV2_INTRA_AXIS_HORIZONTAL;
            err = svt_av2_intra_axis_predict(&reference, &intra_block, axis,
                                             pred_dst, plane_stride);
            if (err != SVT_AV2_ERROR_NONE) return err;
            const uint64_t cost = m5_prediction_sad(core->source, core->recon, plane, block);
            if (cost < best_sad) {
                best_sad = cost;
                m5_mode = candidate;
            }
        }
        if (m5_mode == 0u) {
            err = svt_av2_intra_dc_predict_flags(&reference, &intra_block,
                neighbors.left_valid, neighbors.above_valid, pred_dst, plane_stride);
        } else {
            err = svt_av2_intra_axis_predict(&reference, &intra_block,
                m5_mode == 1u ? SVT_AV2_INTRA_AXIS_VERTICAL : SVT_AV2_INTRA_AXIS_HORIZONTAL,
                pred_dst, plane_stride);
        }
        if (err != SVT_AV2_ERROR_NONE) return err;
    }

    /* 2. signed residual = source - prediction (int16_t, contract 2.3). The
     * prediction passed to T008 lives in the recon picture; T008 reads only the
     * block's own region, which at this point holds the prediction, not source
     * and not a previous frame's reconstruction. */
    err = svt_av2_residual_compute(core->source,
                                   core->recon,
                                   &intra_block,
                                   core->scratch->residual,
                                   core->scratch->stride);
    if (err != SVT_AV2_ERROR_NONE) {
        return err;
    }

    /* 3. forward transform + quantisation, then inverse (T009/T010 seam). The
     * path only reads rows [0, h) / columns [0, w) of the block-local buffers
     * and leaves qcoeff valid for T011. */
    int all_zero = 0;
    err = core->config->transform.forward(core->config->transform.user,
                                          plane,
                                          block->w,
                                          block->h,
                                          core->scratch->residual,
                                          core->scratch->stride,
                                          core->scratch->coeff,
                                          core->scratch->qcoeff,
                                          core->scratch->dqcoeff,
                                          &all_zero);
    if (err != SVT_AV2_ERROR_NONE) {
        return err;
    }

    err = core->config->transform.inverse(core->config->transform.user,
                                          plane,
                                          block->w,
                                          block->h,
                                          core->scratch->dqcoeff,
                                          core->scratch->stride,
                                          core->scratch->inverse_residual);
    if (err != SVT_AV2_ERROR_NONE) {
        return err;
    }

    /* 4. clipped reconstruction, which is also the neighbour update. The
     * prediction is read in place from the recon plane. */
    err = svt_av2_recon_write_block(core->recon,
                                    plane,
                                    block,
                                    pred_dst,
                                    plane_stride,
                                    core->scratch->inverse_residual,
                                    core->scratch->stride);
    if (err != SVT_AV2_ERROR_NONE) {
        return err;
    }

    if (all_zero) {
        ++core->stats.zero_blocks;
    } else {
        ++core->stats.nonzero_blocks;
    }

    if (plane == SVT_AV2_PLANE_Y && core->config->m5_mode_hook != NULL &&
        core->config->m5_mode_hook(core->config->block_hook_user, m5_mode) != 0)
        return SVT_AV2_ERROR_INVALID_STATE;
    if (core->config->block_hook != NULL) {
        if (core->config->block_hook(core->config->block_hook_user,
                                     plane,
                                     block,
                                     core->scratch,
                                     all_zero) != 0) {
            core->failed = 1;
            return SVT_AV2_ERROR_INVALID_STATE;
        }
    }
    return SVT_AV2_ERROR_NONE;
}

/* Visits one luma coding block and its two chroma blocks. */
static int visit_block(void* user, const SvtAv2BlockView* luma_block) {
    FrameCore* core = (FrameCore*)user;

    const SvtAv2Error luma_err = reconstruct_block(core, SVT_AV2_PLANE_Y, luma_block);
    if (luma_err != SVT_AV2_ERROR_NONE) {
        return 1;
    }
    ++core->stats.luma_blocks;

    /* Chroma geometry comes from T007's mapping (truncated halving of the luma
     * origin, Subsampled_Size shape lookup), never a local w/2 x h/2. */
    SvtAv2BlockView chroma;
    memset(&chroma, 0, sizeof(chroma));
    const SvtAv2Error chroma_err = svt_av2_chroma_view(&core->geometry, luma_block, &chroma);
    if (chroma_err != SVT_AV2_ERROR_NONE) {
        /* A luma block entirely outside the chroma plane cannot happen for a
         * minimum 8x8 coding block, but treat it as a hard failure rather than
         * silently skipping chroma. */
        return 1;
    }

    for (uint32_t plane = SVT_AV2_PLANE_U; plane <= SVT_AV2_PLANE_V; ++plane) {
        if (reconstruct_block(core, plane, &chroma) != SVT_AV2_ERROR_NONE) {
            return 1;
        }
        ++core->stats.chroma_blocks;
    }
    return 0;
}

SvtAv2Error svt_av2_intra_frame_encode(const SvtAv2FrameBuffer* source,
                                       SvtAv2FrameBuffer* recon,
                                       const SvtAv2IntraFrameConfig* config,
                                       SvtAv2ReconScratch* scratch,
                                       SvtAv2IntraFrameStats* stats) {
    if (stats != NULL) {
        memset(stats, 0, sizeof(*stats));
    }
    if (source == NULL || recon == NULL || config == NULL || scratch == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (source == recon) {
        /* Source and reconstruction must be two distinguishable buffers
         * (contract 1.2); writing source into recon is forbidden (contract 9). */
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (source->visible_width != recon->visible_width ||
        source->visible_height != recon->visible_height || source->bit_depth != recon->bit_depth ||
        source->chroma_format != recon->chroma_format) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (!scratch->allocated) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (source->visible_width == 0 || source->visible_height == 0) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    /* The transform path is the T009/T010 boundary. Until it is wired this
     * fails closed instead of emitting a lossless (identity) reconstruction. */
    if (config->transform.forward == NULL || config->transform.inverse == NULL) {
        return SVT_AV2_ERROR_UNSUPPORTED;
    }

    FrameCore core;
    memset(&core, 0, sizeof(core));
    core.source = source;
    core.recon = recon;
    core.config = config;
    core.scratch = scratch;

    SvtAv2Error err = svt_av2_frame_geometry_init_ex(&core.geometry,
                                                  recon->visible_width,
                                                  recon->visible_height,
                                                  config->sb_size, recon->chroma_format);
    if (err != SVT_AV2_ERROR_NONE) {
        /* Pass T007's own verdict through rather than flattening it. T007
         * (block_geometry.c:79-85) returns UNSUPPORTED for both an unsupported
         * sb_size (not 64/128/256) and a frame size that is not a multiple of
         * 8; INVALID_PARAMETER is reserved for a null geometry or a zero
         * dimension. Forwarding the code keeps the failing input identifiable. */
        return err;
    }

    /* Reject a configuration the traversal could overflow the scratch with,
     * rather than discovering it mid-frame. */
    const uint32_t max_side = SVT_AV2_MAX(config->sb_size, SVT_AV2_MIN_CODING_BLOCK_SIZE);
    if (!svt_av2_recon_pool_fits(scratch, max_side, max_side)) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    /* Per-frame intra neighbour reset: nothing from the previous frame may be
     * read as this frame's neighbour. */
    err = svt_av2_recon_reset_frame(recon, config->frame_reset_value);
    if (err != SVT_AV2_ERROR_NONE) {
        return err;
    }

    err = svt_av2_partition_traverse_coded_plan(&core.geometry, config->partition_decisions,
        config->partition_decision_count, visit_block, &core);
    if (err != SVT_AV2_ERROR_NONE) {
        return err;
    }
    if (core.failed) {
        return SVT_AV2_ERROR_INVALID_STATE;
    }

    if (stats != NULL) {
        *stats = core.stats;
    }
    return SVT_AV2_ERROR_NONE;
}
