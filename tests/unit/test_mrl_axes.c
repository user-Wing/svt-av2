#include "intra_prediction.h"
#include "test_util.h"

static uint16_t sample(uint32_t x, uint32_t y, uint32_t depth) {
    return (uint16_t)((x * 7u + y * 13u + x * y % 19u) & ((1u << depth) - 1u));
}

int main(void) {
    for (uint32_t format = 0u; format < 3u; ++format) {
        const uint32_t depth = format ? 10u : 8u, step = depth > 8u ? 2u : 1u;
        SvtAv2FrameBuffer recon = {0};
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&recon, 128u, 128u, depth,
            format == 2u ? SVT_AV2_CHROMA_444 : SVT_AV2_CHROMA_420), 0);
        for (uint32_t y = 0u; y < 128u; ++y)
            for (uint32_t x = 0u; x < 128u; ++x)
                svt_av2_sample_store(recon.planes[0] + (size_t)y * recon.strides[0] + x * step,
                    depth, sample(x, y, depth));
        for (uint32_t side = 8u; side <= 64u; side *= 2u)
        for (uint32_t mode = 1u; mode <= 2u; ++mode)
        for (uint32_t multi = 0u; multi < 2u; ++multi) {
            const SvtAv2IntraBlock block = {16u, 16u, side, side, 0u};
            uint8_t prediction[8192];
            /* MRL index 3 has no angle correction: these are exactly 90/180
             * degrees, where no directional derivative may be looked up. */
            const SvtAv2Error error = svt_av2_intra_mrls_predict(&recon, &block, mode,
                3u + 4u * multi, 0u, 0u, prediction, side * step);
            SVT_TEST_CHECK_EQ_INT(error, 0);
            if (error != 0) continue;
            for (uint32_t y = 0u; y < side; ++y)
            for (uint32_t x = 0u; x < side; ++x) {
                uint32_t expected = mode == 1u ? sample(16u + x, 12u, depth) :
                    sample(12u, 16u + y, depth);
                if (multi) expected = (expected + (mode == 1u ? sample(16u + x, 15u, depth) :
                    sample(15u, 16u + y, depth)) + 1u) >> 1u;
                SVT_TEST_CHECK_EQ_INT(svt_av2_sample_load(prediction + ((size_t)y * side + x) * step,
                    depth), expected);
            }
        }
        svt_av2_picture_free(&recon);
    }
    SVT_TEST_REPORT("test_mrl_axes");
}
