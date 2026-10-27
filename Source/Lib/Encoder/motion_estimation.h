#ifndef SVT_AV2_MOTION_ESTIMATION_H
#define SVT_AV2_MOTION_ESTIMATION_H
#include "block_geometry.h"
#define SVT_AV2_ME_SEARCH_RADIUS 8
/* Absolute displacement in integer luma samples; reference=current+MV. */
typedef struct SvtAv2MotionVector { int32_t mv_x; int32_t mv_y; } SvtAv2MotionVector;
typedef struct SvtAv2MotionSearchResult {
    SvtAv2MotionVector mv;
    uint64_t sad;
    uint32_t candidates;
} SvtAv2MotionSearchResult;
SvtAv2Error svt_av2_motion_estimate(const SvtAv2FrameBuffer* source,
    const SvtAv2FrameBuffer* reference, const SvtAv2BlockView* luma_block,
    SvtAv2MotionSearchResult* result);
/* Exact top-three SAD candidates followed by zero MV when not already kept.
 * Slot zero uses the same tie rule as the legacy single-winner entry point. */
typedef struct SvtAv2MotionSearchList {
    SvtAv2MotionVector mv[4];
    uint64_t sad[4];
    uint32_t count, candidates;
} SvtAv2MotionSearchList;
SvtAv2Error svt_av2_motion_estimate_list(const SvtAv2FrameBuffer* source,
    const SvtAv2FrameBuffer* reference, const SvtAv2BlockView* luma_block,
    SvtAv2MotionSearchList* result);
/* Fixed outer-grid search, then one fixed-center refinement; <=80 positions.
 * Returns the best two distinct visited MVs outside the legacy +/-8 square. */
SvtAv2Error svt_av2_motion_estimate_wide(const SvtAv2FrameBuffer* source,
    const SvtAv2FrameBuffer* reference, const SvtAv2BlockView* luma_block,
    SvtAv2MotionSearchList* result);
#endif
