#ifndef SVT_AV2_CCSO64_H
#define SVT_AV2_CCSO64_H
#include "bit_writer.h"
#include "picture.h"
#include "entropy_encoder.h"

/* Forced-only 64x64 units; Yoff, UV BO-only, scale0, four bands. */
typedef struct SvtAv2Ccso64Plan {
    uint8_t enabled[2], block_flag[2];
    int8_t offset[2][4];
    /* Borrowed immutable row-major [unit][U,V] flags. NULL retains one-unit input. */
    const uint8_t (*unit_flags)[2];
    size_t unit_count;
} SvtAv2Ccso64Plan;

/* Canonical CCSO rows are private, inline, and copied with each transaction. */
void svt_av2_ccso64_cdf_init(uint16_t cdf[2][4][6]);
uint32_t svt_av2_ccso64_context(const SvtAv2Ccso64Plan* plan,
    size_t unit, uint32_t cols, uint32_t plane);
int svt_av2_ccso64_symbol(SvtAv2EntropyEncoder* enc,uint16_t state[6],
    uint32_t flag,int adaptive);

int svt_av2_ccso64_validate(const SvtAv2Ccso64Plan* plan,
    uint32_t width, uint32_t height, uint32_t bit_depth, uint32_t chroma_format);
int svt_av2_ccso64_picture_validate(const SvtAv2Ccso64Plan* plan,
    const SvtAv2FrameBuffer* picture);
/* Call only after all prediction/reconstruction, with validated plan/picture. */
void svt_av2_ccso64_apply(const SvtAv2Ccso64Plan* plan, SvtAv2FrameBuffer* picture);
#endif
