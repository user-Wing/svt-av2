#include "inter_frame.h"
#include "inter_prediction.h"
#include "intra_prediction.h"
#include <string.h>

typedef struct InterCore {
    const SvtAv2FrameBuffer* source;
    SvtAv2FrameBuffer* recon;
    const SvtAv2ReferenceFrame* reference;
    const SvtAv2InterFrameConfig* config;
    SvtAv2ReconScratch* scratch;
    SvtAv2FrameGeometry geometry;
    SvtAv2InterFrameStats stats;
    SvtAv2Error error;
} InterCore;
static uint64_t prediction_sad(const SvtAv2FrameBuffer* source, uint32_t plane,
    const SvtAv2BlockView* block, const uint16_t* prediction) {
    uint64_t cost = 0;
    const uint32_t step = source->bit_depth > 8 ? 2u : 1u;
    for (uint32_t y = 0; y < block->h; ++y) {
        const uint8_t* row = source->planes[plane] + (size_t)(block->y + y) * source->strides[plane] + (size_t)block->x * step;
        for (uint32_t x = 0; x < block->w; ++x) {
            const int difference = (int)svt_av2_sample_load(row + x * step, source->bit_depth) -
                prediction[(size_t)y * block->w + x];
            cost += (uint64_t)(difference < 0 ? -difference : difference);
        }
    }
    return cost;
}
static SvtAv2Error intra_prediction(InterCore* core, uint32_t plane,
    const SvtAv2BlockView* block, uint16_t* prediction, uint32_t* selected_mode) {
    const SvtAv2IntraBlock intra = { block->x, block->y, block->w, block->h, plane };
    SvtAv2IntraReference neighbors;
    SvtAv2Error error = svt_av2_intra_reference_build(core->recon, &intra, &neighbors);
    if (error != SVT_AV2_ERROR_NONE) return error;
    uint8_t bytes[64 * 64 * 2];
    const uint32_t step = core->recon->bit_depth > 8 ? 2u : 1u;
    error = svt_av2_intra_dc_predict(&neighbors, &intra, bytes, block->w * step);
    if (error != SVT_AV2_ERROR_NONE) return error;
    for (uint32_t i = 0; i < block->w * block->h; ++i)
        prediction[i] = svt_av2_sample_load(bytes + i * step, core->recon->bit_depth);
    if (plane == 0 && core->config->m5_select_intra) {
        uint64_t best_sad = prediction_sad(core->source, plane, block, prediction);
        uint16_t candidate[64 * 64];
        for (uint32_t mode = 1; mode <= 2; ++mode) {
            if ((mode == 1 && block->y == 0) || (mode == 2 && block->x == 0)) continue;
            error = svt_av2_intra_axis_predict(&neighbors, &intra,
                mode == 1 ? SVT_AV2_INTRA_AXIS_VERTICAL : SVT_AV2_INTRA_AXIS_HORIZONTAL,
                bytes, block->w * step);
            if (error != SVT_AV2_ERROR_NONE) return error;
            for (uint32_t i = 0; i < block->w * block->h; ++i)
                candidate[i] = svt_av2_sample_load(bytes + i * step, core->recon->bit_depth);
            const uint64_t sad = prediction_sad(core->source, plane, block, candidate);
            if (sad < best_sad) {
                best_sad = sad;
                *selected_mode = mode;
                memcpy(prediction, candidate, (size_t)block->w * block->h * sizeof(*prediction));
            }
        }
    }
    return SVT_AV2_ERROR_NONE;
}
static SvtAv2Error reconstruct_plane(InterCore* core, SvtAv2InterBlockResult* result,
    const uint16_t* prediction) {
    const uint32_t plane = result->plane, w = result->block.w, h = result->block.h;
    const uint32_t step = core->recon->bit_depth > 8 ? 2u : 1u;
    uint8_t* destination = core->recon->planes[plane] + (size_t)result->block.y * core->recon->strides[plane] +
        (size_t)result->block.x * step;
    for (uint32_t y = 0; y < h; ++y) {
        const uint8_t* source = core->source->planes[plane] + (size_t)(result->block.y + y) * core->source->strides[plane] +
            (size_t)result->block.x * step;
        for (uint32_t x = 0; x < w; ++x) {
            const uint16_t pred = prediction[(size_t)y * w + x];
            core->scratch->residual[(size_t)y * core->scratch->stride + x] =
                (int16_t)((int)svt_av2_sample_load(source + x * step, core->source->bit_depth) - pred);
            svt_av2_sample_store(destination + (size_t)y * core->recon->strides[plane] + x * step,
                core->recon->bit_depth, pred);
        }
    }
    SvtAv2Error error = core->config->transform.forward(core->config->transform.user, plane, w, h,
        core->scratch->residual, core->scratch->stride, core->scratch->coeff, core->scratch->qcoeff,
        core->scratch->dqcoeff, &result->all_zero);
    if (error != SVT_AV2_ERROR_NONE) return error;
    error = core->config->transform.inverse(core->config->transform.user, plane, w, h,
        core->scratch->dqcoeff, core->scratch->stride, core->scratch->inverse_residual);
    if (error != SVT_AV2_ERROR_NONE) return error;
    error = svt_av2_recon_write_block(core->recon, plane, &result->block, destination,
        core->recon->strides[plane], core->scratch->inverse_residual, core->scratch->stride);
    if (error != SVT_AV2_ERROR_NONE) return error;
    if (result->all_zero) ++core->stats.zero_planes;
    else ++core->stats.nonzero_planes;
    if (core->config->block_hook != NULL &&
        core->config->block_hook(core->config->block_hook_user, result, core->scratch) != 0)
        return SVT_AV2_ERROR_INVALID_STATE;
    return SVT_AV2_ERROR_NONE;
}
static int visit_luma(void* user, const SvtAv2BlockView* luma) {
    InterCore* core = (InterCore*)user;
    SvtAv2MotionSearchResult search;
    core->error = svt_av2_motion_estimate(core->source, &core->reference->committed, luma, &search);
    if (core->error != SVT_AV2_ERROR_NONE) return 1;
    SvtAv2BlockView blocks[3] = { *luma, { 0, 0, 0, 0, NULL }, { 0, 0, 0, 0, NULL } };
    core->error = svt_av2_chroma_view(&core->geometry, luma, &blocks[1]);
    if (core->error != SVT_AV2_ERROR_NONE) return 1;
    blocks[2] = blocks[1];
    uint16_t intra[3][64 * 64], inter[3][64 * 64];
    uint64_t intra_sad = 0, inter_sad = 0;
    uint32_t intra_mode = 0;
    for (uint32_t plane = 0; plane < 3; ++plane) {
        core->error = intra_prediction(core, plane, &blocks[plane], intra[plane], &intra_mode);
        if (core->error != SVT_AV2_ERROR_NONE) return 1;
        core->error = svt_av2_inter_predict(&core->reference->committed, plane, &blocks[plane], search.mv,
            inter[plane], blocks[plane].w);
        if (core->error != SVT_AV2_ERROR_NONE) return 1;
        intra_sad += prediction_sad(core->source, plane, &blocks[plane], intra[plane]);
        inter_sad += prediction_sad(core->source, plane, &blocks[plane], inter[plane]);
    }
    const SvtAv2BlockPredictionMode mode = inter_sad < intra_sad ? SVT_AV2_BLOCK_SINGLE_INTER : SVT_AV2_BLOCK_DC_INTRA;
    if (mode == SVT_AV2_BLOCK_SINGLE_INTER) ++core->stats.inter_blocks;
    else ++core->stats.intra_blocks;
    core->stats.selected_sad += mode == SVT_AV2_BLOCK_SINGLE_INTER ? inter_sad : intra_sad;
    core->stats.motion_candidates += search.candidates;
    for (uint32_t plane = 0; plane < 3; ++plane) {
        SvtAv2InterBlockResult result;
        result.plane = plane; result.block = blocks[plane]; result.luma_block = *luma;
        result.mode = mode; result.reference_frame_index = core->reference->frame_index;
        result.mv.mv_x = mode == SVT_AV2_BLOCK_SINGLE_INTER ? search.mv.mv_x : 0;
        result.mv.mv_y = mode == SVT_AV2_BLOCK_SINGLE_INTER ? search.mv.mv_y : 0;
        result.all_zero = 0;
        result.intra_mode = mode == SVT_AV2_BLOCK_SINGLE_INTER ? 0 : intra_mode;
        core->error = reconstruct_plane(core, &result, mode == SVT_AV2_BLOCK_SINGLE_INTER ? inter[plane] : intra[plane]);
        if (core->error != SVT_AV2_ERROR_NONE) return 1;
    }
    return 0;
}
SvtAv2Error svt_av2_inter_frame_encode(const SvtAv2FrameBuffer* source,
    SvtAv2FrameBuffer* reconstruction, const SvtAv2ReferenceFrame* reference,
    const SvtAv2InterFrameConfig* config, SvtAv2ReconScratch* scratch, SvtAv2InterFrameStats* stats) {
    if (stats != NULL) memset(stats, 0, sizeof(*stats));
    if (source == NULL || reconstruction == NULL || reference == NULL || config == NULL || scratch == NULL ||
        source->buffer == NULL || reconstruction->buffer == NULL || source == reconstruction ||
        source->buffer == reconstruction->buffer ||
        config->transform.forward == NULL || config->transform.inverse == NULL ||
        scratch->residual == NULL || scratch->inverse_residual == NULL || scratch->coeff == NULL ||
        scratch->qcoeff == NULL || scratch->dqcoeff == NULL || scratch->stride < 64 || scratch->capacity < 4096 || scratch->coeff_capacity < 1024)
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    const SvtAv2FrameBuffer* ref = svt_av2_reference_view(reference);
    if (ref == NULL) return SVT_AV2_ERROR_INVALID_STATE;
    if (source->visible_width != reconstruction->visible_width || source->visible_height != reconstruction->visible_height ||
        source->bit_depth != reconstruction->bit_depth || source->chroma_format != reconstruction->chroma_format ||
        source->visible_width != ref->visible_width || source->visible_height != ref->visible_height ||
        source->bit_depth != ref->bit_depth || source->chroma_format != ref->chroma_format ||
        source->buffer == ref->buffer || reconstruction->buffer == ref->buffer)
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    InterCore core;
    memset(&core, 0, sizeof(core));
    core.source = source; core.recon = reconstruction; core.reference = reference; core.config = config; core.scratch = scratch;
    core.error = svt_av2_frame_geometry_init_ex(&core.geometry, source->visible_width, source->visible_height, 64, source->chroma_format);
    if (core.error != SVT_AV2_ERROR_NONE) return core.error;
    core.error = svt_av2_recon_reset_frame(reconstruction, (uint16_t)(1u << (source->bit_depth - 1)));
    if (core.error != SVT_AV2_ERROR_NONE) return core.error;
    const SvtAv2Error traversal = svt_av2_partition_traverse_coded_plan(&core.geometry, config->partition_decisions,
        config->partition_decision_count, visit_luma, &core);
    if (traversal != SVT_AV2_ERROR_NONE) return core.error != SVT_AV2_ERROR_NONE ? core.error : traversal;
    if (stats != NULL) *stats = core.stats;
    return SVT_AV2_ERROR_NONE;
}
