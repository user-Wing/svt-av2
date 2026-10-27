#include "motion_estimation.h"
#include <stdint.h>

static uint32_t edge_coordinate(int64_t coordinate, uint32_t extent) {
    if (coordinate < 0) return 0;
    if (coordinate >= extent) return extent - 1;
    return (uint32_t)coordinate;
}
/* AVM avm_scale/generic/yv12extend.c extends each edge sample and then each
 * edge row. Clamping each address has the same result without reading padding. */
static uint64_t candidate_sad(const SvtAv2FrameBuffer* source,
    const SvtAv2FrameBuffer* reference, const SvtAv2BlockView* block,
    SvtAv2MotionVector mv, uint64_t limit) {
    uint64_t cost = 0;
    const uint32_t step = source->bit_depth > 8 ? 2u : 1u;
    for (uint32_t y = 0; y < block->h; ++y) {
        const uint32_t ry = edge_coordinate((int64_t)block->y + y + mv.mv_y, reference->visible_height);
        const uint8_t* src = source->planes[0] + (size_t)(block->y + y) * source->strides[0] + (size_t)block->x * step;
        const uint8_t* ref = reference->planes[0] + (size_t)ry * reference->strides[0];
        for (uint32_t x = 0; x < block->w; ++x) {
            const uint32_t rx = edge_coordinate((int64_t)block->x + x + mv.mv_x, reference->visible_width);
            const int difference = (int)svt_av2_sample_load(src + x * step, source->bit_depth) -
                (int)svt_av2_sample_load(ref + (size_t)rx * step, reference->bit_depth);
            cost += (uint64_t)(difference < 0 ? -difference : difference);
        }
        /* A strictly larger partial SAD cannot win, including the tie rule. */
        if (cost > limit) break;
    }
    return cost;
}
static int magnitude(int32_t value) { return value < 0 ? -value : value; }
static int preferred(SvtAv2MotionVector candidate, SvtAv2MotionVector best) {
    const int a = magnitude(candidate.mv_x) + magnitude(candidate.mv_y);
    const int b = magnitude(best.mv_x) + magnitude(best.mv_y);
    return a < b || (a == b && (candidate.mv_y < best.mv_y ||
        (candidate.mv_y == best.mv_y && candidate.mv_x < best.mv_x)));
}
SvtAv2Error svt_av2_motion_estimate(const SvtAv2FrameBuffer* source,
    const SvtAv2FrameBuffer* reference, const SvtAv2BlockView* block,
    SvtAv2MotionSearchResult* result) {
    if (source == NULL || reference == NULL || block == NULL || result == NULL ||
        source->buffer == NULL || reference->buffer == NULL || source->planes[0] == NULL || reference->planes[0] == NULL)
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    if (source->visible_width != reference->visible_width || source->visible_height != reference->visible_height ||
        source->bit_depth != reference->bit_depth || source->chroma_format != reference->chroma_format ||
        block->w == 0 || block->h == 0 || block->x >= source->visible_width || block->y >= source->visible_height ||
        block->w > source->visible_width - block->x || block->h > source->visible_height - block->y)
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    SvtAv2MotionSearchResult best = { { 0, 0 }, 0, 1 };
    best.sad = candidate_sad(source, reference, block, best.mv, UINT64_MAX);
    for (int32_t y = -SVT_AV2_ME_SEARCH_RADIUS; y <= SVT_AV2_ME_SEARCH_RADIUS; ++y)
        for (int32_t x = -SVT_AV2_ME_SEARCH_RADIUS; x <= SVT_AV2_ME_SEARCH_RADIUS; ++x) {
            if (x == 0 && y == 0) continue;
            const SvtAv2MotionVector candidate = { x, y };
            const uint64_t sad = candidate_sad(source, reference, block, candidate, best.sad);
            ++best.candidates;
            if (sad < best.sad || (sad == best.sad && preferred(candidate, best.mv))) {
                best.sad = sad;
                best.mv = candidate;
            }
        }
    *result = best;
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_motion_estimate_list(const SvtAv2FrameBuffer* source,
    const SvtAv2FrameBuffer* reference, const SvtAv2BlockView* block,
    SvtAv2MotionSearchList* result) {
    if (source == NULL || reference == NULL || block == NULL || result == NULL ||
        source->buffer == NULL || reference->buffer == NULL || source->planes[0] == NULL || reference->planes[0] == NULL)
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    if (source->visible_width != reference->visible_width || source->visible_height != reference->visible_height ||
        source->bit_depth != reference->bit_depth || source->chroma_format != reference->chroma_format ||
        block->w == 0 || block->h == 0 || block->x >= source->visible_width || block->y >= source->visible_height ||
        block->w > source->visible_width - block->x || block->h > source->visible_height - block->y)
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    const SvtAv2MotionVector zero = {0, 0};
    const uint64_t zero_sad = candidate_sad(source, reference, block, zero, UINT64_MAX);
    result->count = 1u; result->candidates = 1u;
    result->mv[0] = zero; result->sad[0] = zero_sad;
    for (int32_t y = -SVT_AV2_ME_SEARCH_RADIUS; y <= SVT_AV2_ME_SEARCH_RADIUS; ++y)
        for (int32_t x = -SVT_AV2_ME_SEARCH_RADIUS; x <= SVT_AV2_ME_SEARCH_RADIUS; ++x) {
            if (x == 0 && y == 0) continue;
            const SvtAv2MotionVector mv = {x, y};
            const uint64_t limit = result->count < 3u ? UINT64_MAX : result->sad[2];
            const uint64_t sad = candidate_sad(source, reference, block, mv, limit);
            ++result->candidates;
            uint32_t slot = 0u;
            while (slot < result->count && (sad > result->sad[slot] ||
                (sad == result->sad[slot] && !preferred(mv, result->mv[slot])))) ++slot;
            if (slot >= 3u) continue;
            if (result->count < 3u) ++result->count;
            for (uint32_t i = result->count - 1u; i > slot; --i) {
                result->mv[i] = result->mv[i - 1u]; result->sad[i] = result->sad[i - 1u];
            }
            result->mv[slot] = mv; result->sad[slot] = sad;
        }
    int have_zero = 0;
    for (uint32_t i = 0u; i < result->count; ++i)
        if (result->mv[i].mv_x == 0 && result->mv[i].mv_y == 0) have_zero = 1;
    if (!have_zero) {
        result->mv[result->count] = zero; result->sad[result->count++] = zero_sad;
    }
    return SVT_AV2_ERROR_NONE;
}

static void keep_wide(SvtAv2MotionSearchList* result, SvtAv2MotionVector mv, uint64_t sad) {
    uint32_t slot = 0u;
    while (slot < result->count && (sad > result->sad[slot] ||
        (sad == result->sad[slot] && !preferred(mv, result->mv[slot])))) ++slot;
    if (slot >= 2u) return;
    if (result->count < 2u) ++result->count;
    for (uint32_t i = result->count - 1u; i > slot; --i) {
        result->mv[i] = result->mv[i - 1u]; result->sad[i] = result->sad[i - 1u];
    }
    result->mv[slot] = mv; result->sad[slot] = sad;
}

SvtAv2Error svt_av2_motion_estimate_wide(const SvtAv2FrameBuffer* source,
    const SvtAv2FrameBuffer* reference, const SvtAv2BlockView* block,
    SvtAv2MotionSearchList* result) {
    if (source == NULL || reference == NULL || block == NULL || result == NULL ||
        source->buffer == NULL || reference->buffer == NULL || source->planes[0] == NULL || reference->planes[0] == NULL)
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    if (source->visible_width != reference->visible_width || source->visible_height != reference->visible_height ||
        source->bit_depth != reference->bit_depth || source->chroma_format != reference->chroma_format ||
        block->w == 0 || block->h == 0 || block->x >= source->visible_width || block->y >= source->visible_height ||
        block->w > source->visible_width - block->x || block->h > source->visible_height - block->y)
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    uint8_t seen[33u * 33u] = {0};
    result->count = result->candidates = 0u;
    for (int32_t y = -16; y <= 16; y += 4)
        for (int32_t x = -16; x <= 16; x += 4) {
            if (x >= -8 && x <= 8 && y >= -8 && y <= 8) continue;
            const SvtAv2MotionVector mv = {x, y};
            const uint64_t limit = result->count < 2u ? UINT64_MAX : result->sad[1];
            const uint64_t sad = candidate_sad(source, reference, block, mv, limit);
            seen[(size_t)(y + 16) * 33u + (uint32_t)(x + 16)] = 1u;
            ++result->candidates; keep_wide(result, mv, sad);
        }
    const SvtAv2MotionVector center = result->mv[0];
    for (int32_t y = center.mv_y - 2; y <= center.mv_y + 2; ++y)
        for (int32_t x = center.mv_x - 2; x <= center.mv_x + 2; ++x) {
            if (x < -16 || x > 16 || y < -16 || y > 16 ||
                (x >= -8 && x <= 8 && y >= -8 && y <= 8)) continue;
            const size_t index = (size_t)(y + 16) * 33u + (uint32_t)(x + 16);
            if (seen[index]) continue;
            seen[index] = 1u;
            const SvtAv2MotionVector mv = {x, y};
            const uint64_t limit = result->count < 2u ? UINT64_MAX : result->sad[1];
            const uint64_t sad = candidate_sad(source, reference, block, mv, limit);
            ++result->candidates; keep_wide(result, mv, sad);
        }
    return SVT_AV2_ERROR_NONE;
}
