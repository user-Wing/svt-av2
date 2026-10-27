#include "inter_prediction.h"

static int64_t floor_half(int64_t value) {
    return value >= 0 ? value / 2 : -((-value + 1) / 2);
}
static uint16_t reference_sample(const SvtAv2FrameBuffer* reference, uint32_t plane,
    int64_t x, int64_t y) {
    const uint32_t w = svt_av2_picture_plane_width(reference, plane);
    const uint32_t h = svt_av2_picture_plane_height(reference, plane);
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= w) x = w - 1;
    if (y >= h) y = h - 1;
    const uint32_t step = reference->bit_depth > 8 ? 2u : 1u;
    return svt_av2_sample_load(reference->planes[plane] + (size_t)y * reference->strides[plane] +
        (size_t)x * step, reference->bit_depth);
}
SvtAv2Error svt_av2_inter_predict(const SvtAv2FrameBuffer* reference, uint32_t plane,
    const SvtAv2BlockView* block, SvtAv2MotionVector mv, uint16_t* prediction,
    uint32_t prediction_stride) {
    if (reference == NULL || block == NULL || prediction == NULL || reference->buffer == NULL ||
        plane >= 3 || reference->planes[plane] == NULL || prediction_stride < block->w ||
        block->w == 0 || block->h == 0) return SVT_AV2_ERROR_INVALID_PARAMETER;
    const uint32_t width = svt_av2_picture_plane_width(reference, plane);
    const uint32_t height = svt_av2_picture_plane_height(reference, plane);
    if (block->x >= width || block->y >= height || block->w > width - block->x || block->h > height - block->y)
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    const int subsampled = plane != 0 && reference->chroma_format == SVT_AV2_CHROMA_420;
    const int64_t dx = subsampled ? floor_half(mv.mv_x) : mv.mv_x;
    const int64_t dy = subsampled ? floor_half(mv.mv_y) : mv.mv_y;
    const int half_x = subsampled && (int64_t)mv.mv_x - 2 * dx != 0;
    const int half_y = subsampled && (int64_t)mv.mv_y - 2 * dy != 0;
    /* AV2 spec 7.13.3.16 and interpolation process; AVM filter.h BILINEAR3,
     * convolve.c single-reference round0=3/round1=11. Half-phase coefficients
     * are64/64; first-stage rounding loses no bits. Preserve both axes' exact
     * sum until final rounding (rounding twice would differ for odd sums). */
    for (uint32_t y = 0; y < block->h; ++y) for (uint32_t x = 0; x < block->w; ++x) {
        const int64_t rx = (int64_t)block->x + x + dx, ry = (int64_t)block->y + y + dy;
        uint32_t sum = reference_sample(reference, plane, rx, ry);
        if (half_x) sum += reference_sample(reference, plane, rx + 1, ry);
        if (half_y) {
            sum += reference_sample(reference, plane, rx, ry + 1);
            if (half_x) sum += reference_sample(reference, plane, rx + 1, ry + 1);
        }
        const uint32_t shift = (uint32_t)(half_x + half_y);
        if (shift != 0) sum = (sum + (1u << (shift - 1))) >> shift;
        prediction[(size_t)y * prediction_stride + x] = (uint16_t)sum;
    }
    return SVT_AV2_ERROR_NONE;
}
