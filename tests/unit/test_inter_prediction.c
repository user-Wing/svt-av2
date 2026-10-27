#include "test_util.h"
#include "inter_prediction.h"
#include <stdint.h>
#include <string.h>

static int floor_two(int n) { return n >= 0 ? n / 2 : (n - 1) / 2; }
static uint32_t edge_sample(const SvtAv2FrameBuffer* ref, uint32_t p, int x, int y) {
    const int width = (int)svt_av2_picture_plane_width(ref, p), height = (int)svt_av2_picture_plane_height(ref, p);
    x = x < 0 ? 0 : x >= width ? width - 1 : x;
    y = y < 0 ? 0 : y >= height ? height - 1 : y;
    const uint8_t* q = ref->planes[p] + (size_t)y * ref->strides[p] + (size_t)x * (ref->bit_depth == 8 ? 1u : 2u);
    return ref->bit_depth == 8 ? q[0] : (uint32_t)q[0] + 256u * q[1];
}
/* Independent full 8-tap separable filter with AV2 single-reference offsets,
 * round0=3, round1=11 and postround0. Half-phase64/64, integerphase128. */
static uint16_t filter_model(const SvtAv2FrameBuffer* ref, uint32_t p, int x, int y, int mx, int my) {
    const int ss = p != 0 && ref->chroma_format == SVT_AV2_CHROMA_420;
    const int dx = ss ? floor_two(mx) : mx, dy = ss ? floor_two(my) : my;
    const int hx = ss && mx - 2 * dx, hy = ss && my - 2 * dy;
    int hfilter[8] = { 0 }, vfilter[8] = { 0 };
    hfilter[3] = hx ? 64 : 128; hfilter[4] = hx ? 64 : 0;
    vfilter[3] = hy ? 64 : 128; vfilter[4] = hy ? 64 : 0;
    const uint32_t offset_bits = ref->bit_depth + 11;
    int64_t vertical = (int64_t)1 << offset_bits;
    for (int j = 0; j < 8; ++j) {
        int64_t horizontal = (int64_t)1 << (ref->bit_depth + 6);
        for (int i = 0; i < 8; ++i)
            horizontal += (int64_t)hfilter[i] * edge_sample(ref, p, x + dx + i - 3, y + dy + j - 3);
        horizontal = (horizontal + 4) >> 3;
        vertical += (int64_t)vfilter[j] * horizontal;
    }
    const int64_t value = ((vertical + 1024) >> 11) -
        (((int64_t)1 << (offset_bits - 11)) + ((int64_t)1 << (offset_bits - 12)));
    return (uint16_t)value;
}
int main(void) {
    uint32_t seed = 0x31415926u;
    for (uint32_t f = 0; f < 3; ++f) {
        const uint32_t bd = f == 0 ? 8 : 10, step = bd == 8 ? 1u : 2u;
        const SvtAv2ChromaFormat format = f == 2 ? SVT_AV2_CHROMA_444 : SVT_AV2_CHROMA_420;
        SvtAv2FrameBuffer ref;
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&ref, 32, 32, bd, format), SVT_AV2_ERROR_NONE);
        uint8_t bytes[3][32 * 65 + 1];
        for (uint32_t p = 0; p < 3; ++p) {
            ref.planes[p] = bytes[p] + 1; ref.strides[p] = 65;
            for (uint32_t y = 0; y < svt_av2_picture_plane_height(&ref, p); ++y)
                for (uint32_t x = 0; x < svt_av2_picture_plane_width(&ref, p); ++x) {
                    seed = seed * 1664525u + 1013904223u;
                    const uint16_t value = (uint16_t)((seed >> 12) & ((1u << bd) - 1u));
                    svt_av2_sample_store(ref.planes[p] + y * 65 + x * step, bd, value);
                }
            for (int my = -8; my <= 8; ++my) for (int mx = -8; mx <= 8; ++mx) {
                const uint32_t width = svt_av2_picture_plane_width(&ref, p), height = svt_av2_picture_plane_height(&ref, p);
                const SvtAv2BlockView block = { 0, 0, width, height, NULL };
                const SvtAv2MotionVector mv = { mx, my };
                uint16_t prediction[32 * 33];
                for (uint32_t i = 0; i < 32 * 33; ++i) prediction[i] = 0xA5A5;
                SVT_TEST_CHECK_EQ_INT(svt_av2_inter_predict(&ref, p, &block, mv, prediction, 33), SVT_AV2_ERROR_NONE);
                for (uint32_t y = 0; y < height; ++y) {
                    for (uint32_t x = 0; x < width; ++x)
                        SVT_TEST_CHECK_EQ_INT(prediction[y * 33 + x], filter_model(&ref, p, (int)x, (int)y, mx, my));
                    SVT_TEST_CHECK_EQ_INT(prediction[y * 33 + width], 0xA5A5);
                }
            }
        }
        svt_av2_picture_free(&ref);
    }
    SVT_TEST_REPORT("test_inter_prediction");
}
