#include "test_util.h"
#include "reference_frame.h"
#include "reconstruction.h"
#include <string.h>

int main(void) {
    const uint32_t depths[] = { 8, 10, 10 };
    const SvtAv2ChromaFormat formats[] = { SVT_AV2_CHROMA_420, SVT_AV2_CHROMA_420, SVT_AV2_CHROMA_444 };
    for (uint32_t f = 0; f < 3; ++f) {
        SvtAv2ReferenceFrame ref; SvtAv2FrameBuffer current;
        SVT_TEST_CHECK_EQ_INT(svt_av2_reference_init(&ref, 16, 16, depths[f], formats[f]), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK(svt_av2_reference_view(&ref) == NULL);
        SVT_TEST_CHECK_EQ_INT(svt_av2_reference_commit(&ref), SVT_AV2_ERROR_INVALID_STATE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&current, 16, 16, depths[f], formats[f]), SVT_AV2_ERROR_NONE);
        for (uint64_t index = 0; index < 3; ++index) {
            const uint16_t value = (uint16_t)(17 + index * 71);
            SVT_TEST_CHECK_EQ_INT(svt_av2_recon_reset_frame(&current, value), SVT_AV2_ERROR_NONE);
            SVT_TEST_CHECK_EQ_INT(svt_av2_reference_prepare(&ref, &current, index), SVT_AV2_ERROR_NONE);
            if (index != 0) {
                SVT_TEST_CHECK_EQ_INT(ref.frame_index, index - 1);
                SVT_TEST_CHECK_EQ_INT(svt_av2_sample_load(ref.committed.planes[0], depths[f]), 17 + (index - 1) * 71);
            } else SVT_TEST_CHECK(svt_av2_reference_view(&ref) == NULL);
            SVT_TEST_CHECK(ref.prepared.buffer != current.buffer);
            SVT_TEST_CHECK(ref.prepared.buffer != ref.committed.buffer);
            SVT_TEST_CHECK_EQ_INT(svt_av2_recon_reset_frame(&current, 0), SVT_AV2_ERROR_NONE);
            SVT_TEST_CHECK_EQ_INT(svt_av2_reference_commit(&ref), SVT_AV2_ERROR_NONE);
            SVT_TEST_CHECK_EQ_INT(ref.frame_index, index);
            for (uint32_t p = 0; p < 3; ++p)
                SVT_TEST_CHECK_EQ_INT(svt_av2_sample_load(ref.committed.planes[p], depths[f]), value);
            SVT_TEST_CHECK_EQ_INT(svt_av2_reference_commit(&ref), SVT_AV2_ERROR_INVALID_STATE);
        }
        SvtAv2FrameBuffer wrong;
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&wrong, 8, 8, depths[f], formats[f]), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_reference_prepare(&ref, &wrong, 3), SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(ref.frame_index, 2);
        SVT_TEST_CHECK_EQ_INT(svt_av2_sample_load(ref.committed.planes[0], depths[f]), 159);
        SVT_TEST_CHECK_EQ_INT(svt_av2_reference_commit(&ref), SVT_AV2_ERROR_INVALID_STATE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_reference_prepare(&ref, &current, 4), SVT_AV2_ERROR_INVALID_STATE);
        uint8_t odd[3][16 * 35 + 1];
        const uint32_t step = depths[f] == 8 ? 1u : 2u;
        for (uint32_t p = 0; p < 3; ++p) {
            current.planes[p] = odd[p] + 1; current.strides[p] = 35;
            for (uint32_t y = 0; y < svt_av2_picture_plane_height(&current, p); ++y)
                for (uint32_t x = 0; x < svt_av2_picture_plane_width(&current, p); ++x)
                    svt_av2_sample_store(current.planes[p] + y * 35 + x * step, depths[f], 100);
        }
        SVT_TEST_CHECK_EQ_INT(svt_av2_reference_prepare(&ref, &current, 3), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_reference_commit(&ref), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_sample_load(ref.committed.planes[2], depths[f]), 100);
        svt_av2_picture_free(&wrong); svt_av2_picture_free(&current);
        SVT_TEST_CHECK_EQ_INT(svt_av2_reference_release(&ref), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_reference_release(&ref), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK(svt_av2_reference_view(&ref) == NULL);
    }
    SVT_TEST_REPORT("test_reference_frame");
}
