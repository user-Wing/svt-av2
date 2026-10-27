#include "test_util.h"
#include "motion_estimation.h"
#include "reconstruction.h"
#include <stdint.h>

static uint16_t sample(const SvtAv2FrameBuffer* p, int x, int y) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= (int)p->visible_width) x = (int)p->visible_width - 1;
    if (y >= (int)p->visible_height) y = (int)p->visible_height - 1;
    const uint8_t* q = p->planes[0] + (size_t)y * p->strides[0] + (size_t)x * (p->bit_depth == 8 ? 1u : 2u);
    return p->bit_depth == 8 ? q[0] : (uint16_t)(q[0] + 256u * q[1]);
}
static int absv(int n) { return n < 0 ? -n : n; }
static SvtAv2MotionSearchResult model(const SvtAv2FrameBuffer* src, const SvtAv2FrameBuffer* ref,
    const SvtAv2BlockView* block) {
    SvtAv2MotionSearchResult best = { { 0, 0 }, UINT64_MAX, 0 };
    for (int y = -8; y <= 8; ++y) for (int x = -8; x <= 8; ++x) {
        uint64_t sad = 0;
        for (uint32_t j = 0; j < block->h; ++j) for (uint32_t i = 0; i < block->w; ++i)
            sad += (uint64_t)absv((int)sample(src, (int)(block->x + i), (int)(block->y + j)) -
                (int)sample(ref, (int)(block->x + i) + x, (int)(block->y + j) + y));
        const int norm = absv(x) + absv(y), old = absv(best.mv.mv_x) + absv(best.mv.mv_y);
        if (sad < best.sad || (sad == best.sad && (norm < old || (norm == old &&
            (y < best.mv.mv_y || (y == best.mv.mv_y && x < best.mv.mv_x)))))) {
            best.sad = sad; best.mv.mv_x = x; best.mv.mv_y = y;
        }
        ++best.candidates;
    }
    return best;
}
int main(void) {
    uint32_t seed = 0x31415926u;
    for (uint32_t f = 0; f < 3; ++f) {
        const uint32_t bd = f == 0 ? 8 : 10, step = bd == 8 ? 1u : 2u;
        SvtAv2FrameBuffer src, ref;
        const SvtAv2ChromaFormat format = f == 2 ? SVT_AV2_CHROMA_444 : SVT_AV2_CHROMA_420;
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&src, 32, 32, bd, format), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&ref, 32, 32, bd, format), SVT_AV2_ERROR_NONE);
        for (uint32_t y = 0; y < 32; ++y) for (uint32_t x = 0; x < 32; ++x) {
            seed = seed * 1664525u + 1013904223u;
            svt_av2_sample_store(ref.planes[0] + y * ref.strides[0] + x * step, bd, (uint16_t)((seed >> 12) & ((1u << bd) - 1u)));
        }
        for (uint32_t trial = 0; trial < 100; ++trial) {
            const int dx = (int)(trial % 17) - 8, dy = (int)((trial * 7) % 17) - 8;
            for (uint32_t y = 0; y < 32; ++y) for (uint32_t x = 0; x < 32; ++x)
                svt_av2_sample_store(src.planes[0] + y * src.strides[0] + x * step, bd, sample(&ref, (int)x + dx, (int)y + dy));
            const SvtAv2BlockView block = { trial % 3 == 0 ? 0u : trial % 3 == 1 ? 24u : 12u,
                trial % 3 == 0 ? 24u : trial % 3 == 1 ? 0u : 12u, 8, 8, NULL };
            SvtAv2MotionSearchResult actual;
            const SvtAv2MotionSearchResult expected = model(&src, &ref, &block);
            SVT_TEST_CHECK_EQ_INT(svt_av2_motion_estimate(&src, &ref, &block, &actual), SVT_AV2_ERROR_NONE);
            SVT_TEST_CHECK_EQ_INT(actual.sad, expected.sad);
            SVT_TEST_CHECK_EQ_INT(actual.mv.mv_x, expected.mv.mv_x);
            SVT_TEST_CHECK_EQ_INT(actual.mv.mv_y, expected.mv.mv_y);
            SVT_TEST_CHECK_EQ_INT(actual.candidates, 289);
        }
        svt_av2_recon_reset_frame(&src, 17); svt_av2_recon_reset_frame(&ref, 17);
        const SvtAv2BlockView flat = { 0, 0, 16, 16, NULL }; SvtAv2MotionSearchResult result;
        SVT_TEST_CHECK_EQ_INT(svt_av2_motion_estimate(&src, &ref, &flat, &result), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(result.mv.mv_x, 0); SVT_TEST_CHECK_EQ_INT(result.mv.mv_y, 0);
        svt_av2_picture_free(&src); svt_av2_picture_free(&ref);
    }
    SVT_TEST_REPORT("test_motion_estimation");
}
