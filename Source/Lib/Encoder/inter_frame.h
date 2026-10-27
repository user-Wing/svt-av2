#ifndef SVT_AV2_INTER_FRAME_H
#define SVT_AV2_INTER_FRAME_H
#include "intra_frame.h"
#include "reference_frame.h"
#include "motion_estimation.h"

typedef enum SvtAv2BlockPredictionMode {
    SVT_AV2_BLOCK_DC_INTRA = 0,
    SVT_AV2_BLOCK_SINGLE_INTER = 1
} SvtAv2BlockPredictionMode;
/* Each luma leaf chooses one mode and absolute MV, shared by Y/U/V hooks.
 * Scratch coefficients remain valid only during the hook. The consumer copies
 * min(w,32)*min(h,32) qcoeff into packet-owned storage before scratch reuse. */
typedef struct SvtAv2InterBlockResult {
    uint32_t plane;
    SvtAv2BlockView block;
    SvtAv2BlockView luma_block;
    SvtAv2BlockPredictionMode mode;
    SvtAv2MotionVector mv;
    uint64_t reference_frame_index;
    int all_zero;
    uint32_t intra_mode; /* Shared luma mode: 0 DC, 1 V, 2 H; inter uses 0. */
} SvtAv2InterBlockResult;
typedef int (*SvtAv2InterBlockHookFn)(void* user, const SvtAv2InterBlockResult* result,
    const SvtAv2ReconScratch* scratch);
typedef struct SvtAv2InterFrameConfig {
    SvtAv2IntraTransformPath transform;
    SvtAv2InterBlockHookFn block_hook;
    void* block_hook_user;
    uint32_t m5_select_intra; /* Zero preserves the M4 DC-only selection. */
    const uint16_t* partition_decisions; /* Immutable per-SB plan, NULL/0=frozen tree. */
    size_t partition_decision_count;
} SvtAv2InterFrameConfig;
typedef struct SvtAv2InterFrameStats {
    uint32_t intra_blocks;
    uint32_t inter_blocks;
    uint32_t zero_planes;
    uint32_t nonzero_planes;
    uint64_t selected_sad;
    uint64_t motion_candidates;
} SvtAv2InterFrameStats;
SvtAv2Error svt_av2_inter_frame_encode(const SvtAv2FrameBuffer* source,
    SvtAv2FrameBuffer* reconstruction, const SvtAv2ReferenceFrame* reference,
    const SvtAv2InterFrameConfig* config, SvtAv2ReconScratch* scratch,
    SvtAv2InterFrameStats* stats);
#endif
