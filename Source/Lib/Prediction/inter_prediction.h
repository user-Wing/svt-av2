#ifndef SVT_AV2_INTER_PREDICTION_H
#define SVT_AV2_INTER_PREDICTION_H
#include "motion_estimation.h"
/* Fixed AV2 BILINEAR frame filter. Destination stride is uint16 samples. */
SvtAv2Error svt_av2_inter_predict(const SvtAv2FrameBuffer* reference, uint32_t plane,
    const SvtAv2BlockView* block, SvtAv2MotionVector mv, uint16_t* prediction,
    uint32_t prediction_stride);
#endif
