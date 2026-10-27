#ifndef SVT_AV2_RD_FRAME_H
#define SVT_AV2_RD_FRAME_H

#include "intra_frame.h"
#include "reference_frame.h"
#include "tile_writer.h"

/* Internal, bounded RD domain. Public configuration validation belongs to Codec.
 * off bypasses this module. Source/reference are read-only; references are never
 * committed here. See docs/m6-rd-contracts.md for units and candidate domains. */
typedef struct SvtAv2RdFrameConfig {
    SvtAv2IntraTransformPath transform;
    SvtAv2FrameHeaderConfig frame_header;
    uint32_t rd_mode; /* 1=exhaustive, 2=pruned, 3=fast still intra */
    uint32_t partition_selection; /* 4=small rectangles, 5=also uniform long rectangles, 6=also bounded 32-quadrant rectangles, 7=also bounded small-rectangle mixtures, 8=also uniform thin rectangles, 9=also bounded thin mixtures, 10=also uniform 8x64/64x8 */
    uint32_t motion_optimization; /* 1=legacy-range RD, 2=legacy-range RD plus bounded wide refinement */
    uint32_t angle_search; /* 48-angle Y-SAD shortlist plus exact refinement on the winning tree */
    uint32_t transform_selection; /* 1=binary DCT/ADST_ADST, 2=full syntax with four Y primary types */
    uint32_t coeff_optimization; /* 1=legacy tail trim, 2=single-picture level/EOB RD */
    uint32_t cfl_search; /* 1=classic explicit CfL, pixel-SSE top2/plane then exact SB RD */
    /* Internal audit seam; production callers leave these zero. The observer
     * receives every completed trial, then the replayed winner (committed=1). */
    void (*candidate_hook)(void* user, size_t sb_index, uint64_t candidate_id,
        uint64_t distortion, uint64_t rate_q12, uint64_t cost, int committed);
    void* candidate_hook_user;
    uint32_t reverse_candidates;
    uint32_t tree_finalists; /* 0=original winner, 2=best two basic trees, 4=also preserve two seeds before long rectangles, 8=global four plus two before long and thin rectangles, 10=also retain both 8x64/64x8 seeds */
    uint32_t rd_lambda_model; /* 0=original, 1=fixed AVM AI base in current distortion units */
    uint32_t mhccp_search; /* 1=three shared filter0 MHCCP directions, exact whole-SB RD. */
    uint32_t intra_joint; /* 1=bounded Y mode/TX joint RD; complete original route retained. */
} SvtAv2RdFrameConfig;

typedef struct SvtAv2RdFrameOutput {
    SvtAv2TileCoeffBlock* blocks;
    size_t block_capacity, block_count;
    SvtAv2TileBlockMode* modes;
    size_t mode_capacity, mode_count;
    int32_t* qcoeff;
    size_t coeff_capacity, coeff_count;
    uint16_t* decisions;
    size_t decision_capacity, decision_count;
} SvtAv2RdFrameOutput;

typedef struct SvtAv2RdFrameStats {
    uint64_t candidate_count, forward_calls, replay_checks;
    uint64_t distortion, rate_q12, cost, lambda_q8;
    uint64_t full_superblocks, partial_superblocks, motion_candidates;
    uint64_t winner_hash;
    uint64_t coeff_trials, coeff_accepts;
    uint64_t transform_trials, transform_accepts;
    uint64_t angle_trials, angle_accepts;
    uint64_t motion_trials, motion_accepts;
    uint64_t wide_trials, wide_accepts;
    uint64_t mrl_trials, mrl_accepts;
    uint64_t cfl_trials, cfl_accepts;
    uint64_t tree_finalist_trials, tree_finalist_accepts;
    uint64_t mhccp_trials, mhccp_accepts;
} SvtAv2RdFrameStats;

/* Cost helpers are also used by the independent numeric unit tests. qindex is
 * the actual 8/10-bit index, not the user-facing CQP number. */
SvtAv2Error svt_av2_rd_lambda(uint32_t qindex, uint32_t bit_depth,
    uint64_t* lambda_q8, uint32_t* qstep_qtx);
SvtAv2Error svt_av2_rd_lambda_ai(uint32_t qindex, uint32_t bit_depth,
    uint64_t* lambda_q8, uint32_t* qstep_qtx);
SvtAv2Error svt_av2_rd_cost(uint64_t distortion, uint64_t lambda_q8,
    uint64_t rate_q12, uint64_t* cost);
uint64_t svt_av2_rd_distortion(const uint64_t sse[3], uint32_t bit_depth,
    uint32_t chroma_format);

SvtAv2Error svt_av2_rd_frame_encode(const SvtAv2FrameBuffer* source,
    SvtAv2FrameBuffer* recon, const SvtAv2ReferenceFrame* reference,
    const SvtAv2RdFrameConfig* config, SvtAv2ReconScratch* scratch,
    SvtAv2RdFrameOutput* output, SvtAv2RdFrameStats* stats);

#endif
