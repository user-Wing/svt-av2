#ifndef SVT_AV2_DEBLOCK_LUMA_H
#define SVT_AV2_DEBLOCK_LUMA_H
#include "picture.h"
#include "tile_writer.h"
/* Single-picture, one tile, no lossless/segmentation/delta-Q/TX subdivision.
 * mode 1 V-only, 2 H-only, 3 combined V/H; UV is never filtered here. */
SvtAv2Error svt_av2_deblock_luma_apply(SvtAv2FrameBuffer* recon,
    const SvtAv2TileCoeffBlock* blocks,size_t count,uint32_t mode,int delta,int delta_h,
    uint32_t actual_qindex,uint64_t* edge_calls,uint64_t* changed_pixels);
#endif
