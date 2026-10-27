#include "reference_frame.h"
#include <string.h>

SvtAv2Error svt_av2_reference_init(SvtAv2ReferenceFrame* reference, uint32_t width,
    uint32_t height, uint32_t bit_depth, SvtAv2ChromaFormat chroma_format) {
    if (reference == NULL) return SVT_AV2_ERROR_INVALID_PARAMETER;
    memset(reference, 0, sizeof(*reference));
    SvtAv2Error error = svt_av2_picture_alloc_ex(&reference->committed, width, height, bit_depth, chroma_format);
    if (error != SVT_AV2_ERROR_NONE) return error;
    error = svt_av2_picture_alloc_ex(&reference->prepared, width, height, bit_depth, chroma_format);
    if (error != SVT_AV2_ERROR_NONE) svt_av2_reference_release(reference);
    return error;
}
SvtAv2Error svt_av2_reference_prepare(SvtAv2ReferenceFrame* reference,
    const SvtAv2FrameBuffer* reconstruction, uint64_t frame_index) {
    if (reference == NULL) return SVT_AV2_ERROR_INVALID_PARAMETER;
    reference->prepared_valid = 0;
    if ((reference->valid && (reference->frame_index == UINT64_MAX || frame_index != reference->frame_index + 1)) ||
        (!reference->valid && frame_index != 0)) return SVT_AV2_ERROR_INVALID_STATE;
    const SvtAv2Error error = svt_av2_picture_copy_in(&reference->prepared, reconstruction);
    if (error != SVT_AV2_ERROR_NONE) return error;
    reference->prepared_index = frame_index;
    reference->prepared_valid = 1;
    return SVT_AV2_ERROR_NONE;
}
SvtAv2Error svt_av2_reference_commit(SvtAv2ReferenceFrame* reference) {
    if (reference == NULL) return SVT_AV2_ERROR_INVALID_PARAMETER;
    if (!reference->prepared_valid) return SVT_AV2_ERROR_INVALID_STATE;
    const SvtAv2FrameBuffer previous = reference->committed;
    reference->committed = reference->prepared;
    reference->prepared = previous;
    reference->frame_index = reference->prepared_index;
    reference->valid = 1;
    reference->prepared_valid = 0;
    return SVT_AV2_ERROR_NONE;
}
const SvtAv2FrameBuffer* svt_av2_reference_view(const SvtAv2ReferenceFrame* reference) {
    return reference != NULL && reference->valid ? &reference->committed : NULL;
}
SvtAv2Error svt_av2_reference_release(SvtAv2ReferenceFrame* reference) {
    if (reference == NULL) return SVT_AV2_ERROR_INVALID_PARAMETER;
    svt_av2_picture_free(&reference->committed);
    svt_av2_picture_free(&reference->prepared);
    memset(reference, 0, sizeof(*reference));
    return SVT_AV2_ERROR_NONE;
}
