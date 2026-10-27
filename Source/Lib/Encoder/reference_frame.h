#ifndef SVT_AV2_REFERENCE_FRAME_H
#define SVT_AV2_REFERENCE_FRAME_H
#include "picture.h"

/* Prepared pixels remain private until commit. Both buffers are allocated at init,
 * so committing a serialized packet cannot fail due to allocation. */
typedef struct SvtAv2ReferenceFrame {
    SvtAv2FrameBuffer committed;
    SvtAv2FrameBuffer prepared;
    uint64_t frame_index;
    uint64_t prepared_index;
    int valid;
    int prepared_valid;
} SvtAv2ReferenceFrame;
SvtAv2Error svt_av2_reference_init(SvtAv2ReferenceFrame* reference, uint32_t width,
    uint32_t height, uint32_t bit_depth, SvtAv2ChromaFormat chroma_format);
SvtAv2Error svt_av2_reference_prepare(SvtAv2ReferenceFrame* reference,
    const SvtAv2FrameBuffer* reconstruction, uint64_t frame_index);
SvtAv2Error svt_av2_reference_commit(SvtAv2ReferenceFrame* reference);
const SvtAv2FrameBuffer* svt_av2_reference_view(const SvtAv2ReferenceFrame* reference);
SvtAv2Error svt_av2_reference_release(SvtAv2ReferenceFrame* reference);
#endif
