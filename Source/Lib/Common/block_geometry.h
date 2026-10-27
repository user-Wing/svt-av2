/*
 * SVT-AV2 block geometry (T007).
 *
 * Scope: the partition grid, the block views, the boundary rule and the chroma
 * mapping needed by T008 (neighbour extraction), T011 (partition syntax
 * placement), T012 (reconstruction) and T013 (frame plumbing). No syntax is
 * produced here and no pixels are predicted.
 *
 * Frozen contract: docs/t007-picture-block-geometry.md (+ the -v2 addendum for
 * the coordinate convention). Coordinate convention, one and only one:
 *   blk.x / blk.y are plane-relative pixel coordinates of the block's top-left
 *   corner; a block covers col in [x, x+w) and row in [y, y+h).
 *
 * Evidence for every rule (read-only AVM source, re-checkable):
 *   mi grid        avm\av2\decoder\decoder.c:97, inspection.c:31-34
 *   SB grid        avm\av2\common\tile_common.c:52-57, 84-87
 *   boundary rule  avm\av2\common\av2_common_int.h:4535-4593
 *   implied = no symbol
 *                  avm\av2\decoder\decodeframe.c:1741-1757 (early returns)
 *   chroma shape   avm\av2\common\common_data.h:709-743 (Subsampled_Size table)
 *   chroma <= 8x8  avm\av2\common\av2_common_int.h:4620-4622
 *   chroma coords  avm\av2\common\blockd.h:1466-1473
 */
#ifndef SVT_AV2_BLOCK_GEOMETRY_H
#define SVT_AV2_BLOCK_GEOMETRY_H

#include "picture.h"

#include <stddef.h>
#include <stdint.h>

/* Mode-info unit size: the AV2 partition grid works in 4x4 units
 * (avm\av2\common\enums.h:164). */
#define SVT_AV2_MI_SIZE 4u

/* Superblock sizes this module can describe. 64x64 is the frozen coding unit;
 * 128/256 are recognised so geometry queries can report them, but the partition
 * traversal does not claim the explicit boundary-split syntax that AVM emits for
 * frames wider/taller than one superblock (decodeframe.c:1818, 1885). */
#define SVT_AV2_SB_SIZE_64 64u
#define SVT_AV2_SB_SIZE_128 128u
#define SVT_AV2_SB_SIZE_256 256u

/* Minimum coding-block edge, in luma samples, that this module will traverse.
 * 8x8 is the chroma coding unit limit: the decoder forces PARTITION_NONE for a
 * CHROMA_PART block of size BLOCK_8X8 (av2_common_int.h:4620-4622), and a 4x4
 * luma block would collapse two source columns onto one chroma column. */
#define SVT_AV2_MIN_CODING_BLOCK_SIZE 8u

typedef enum SvtAv2PartitionType {
    SVT_AV2_PARTITION_NONE = 0, /* the area is one coding block */
    SVT_AV2_PARTITION_SPLIT,    /* four square sub-blocks */
    SVT_AV2_PARTITION_HORZ,     /* two stacked sub-blocks */
    SVT_AV2_PARTITION_VERT,     /* two side-by-side sub-blocks */
    SVT_AV2_PARTITION_INVALID
} SvtAv2PartitionType;

/* Frame-level partition geometry. Computed once per frame; cheap to hold by
 * value and free of allocations. */
typedef struct SvtAv2FrameGeometry {
    SvtAv2ChromaFormat chroma_format;
    uint32_t visible_width;
    uint32_t visible_height;

    uint32_t sb_size;      /* superblock edge in luma samples: 64, 128 or 256 */
    uint32_t mi_width;     /* mode-info units across the frame */
    uint32_t mi_height;    /* mode-info units down the frame */
    uint32_t sb_cols;      /* superblocks across the frame */
    uint32_t sb_rows;      /* superblocks down the frame */
    uint32_t chroma_width;  /* visible chroma width, 4:2:0 */
    uint32_t chroma_height; /* visible chroma height, 4:2:0 */

    /* Blocks at the right/bottom edge whose area extends past the visible
     * frame. sb_cols/sb_rows count them, so a full traversal visits every
     * visible sample exactly once. */
    uint32_t partial_sb_count;
} SvtAv2FrameGeometry;

/* A rectangular block view in one plane. `base` is the address of the
 * top-left sample inside the picture plane buffer, i.e.
 * `plane + y * stride + x`; it is NULL when the view was computed without a
 * picture (geometry-only use). */
typedef struct SvtAv2BlockView {
    uint32_t x; /* plane-relative pixel column of the left edge */
    uint32_t y; /* plane-relative pixel row of the top edge */
    uint32_t w; /* visible width of this block in this plane, >= 1 */
    uint32_t h; /* visible height of this block in this plane, >= 1 */
    uint8_t* base;
} SvtAv2BlockView;

/* Above/left neighbourhood of a block, read from the internal reconstruction.
 *
 * Availability only describes whether the samples exist. Choosing what to use
 * when they do not is T008's job and is NOT done here: the normative
 * unavailable-neighbour substitution depends on the prediction mode.
 *
 * above[0] / left[0] hold the top-left corner sample; above[1] / left[1] hold
 * the sample directly above / to the left of the block's left/top edge. When
 * the corresponding flag is 0 the samples are left as 0 but are not part of the
 * contract. */
typedef struct SvtAv2BlockNeighbors {
    uint32_t above[2];
    uint32_t left[2];
    int      above_valid;
    int      left_valid;
} SvtAv2BlockNeighbors;

/* --- frame geometry -------------------------------------------------------- */

/* Computes the partition grid for a frame.
 *
 * `sb_size` must be 64, 128 or 256. The visible size follows the same rule as
 * svt_av2_picture_alloc: non-zero multiples of 8.
 *
 * The superblock counts are derived from the FRAME's mi size rounded up to the
 * superblock size (avm tile_common.c:52-57). They are deliberately NOT derived
 * by rounding the frame size itself up to a multiple of the superblock: that
 * would make every traversal read and write rows/columns outside the picture,
 * which is exactly what the frozen contract forbids. The difference matters:
 * 1920x1080 with 64x64 superblocks is 8x5 superblocks of which the bottom row
 * is partial. */
SvtAv2Error svt_av2_frame_geometry_init(SvtAv2FrameGeometry* geometry,
                                        uint32_t width,
                                        uint32_t height,
                                        uint32_t sb_size);

SvtAv2Error svt_av2_frame_geometry_init_ex(SvtAv2FrameGeometry* geometry,
    uint32_t width, uint32_t height, uint32_t sb_size, SvtAv2ChromaFormat chroma_format);

/* --- block views ----------------------------------------------------------- */

/* Visible size in `plane` of the block at superblock (sb_col, sb_row), clipped
 * to the right/bottom edge of the frame. This is the only supported way to
 * build a coding-block view. */
SvtAv2Error svt_av2_block_from_sb(const SvtAv2FrameGeometry* geometry,
                                  uint32_t sb_col,
                                  uint32_t sb_row,
                                  SvtAv2BlockView* block);

/* Same, but with `base` filled in from `picture`. Returns
 * SVT_AV2_ERROR_INVALID_PARAMETER when the picture's visible size disagrees
 * with the geometry, or when the plane index is out of range. */
SvtAv2Error svt_av2_block_view(const SvtAv2FrameGeometry* geometry,
                               const SvtAv2FrameBuffer* picture,
                               uint32_t plane,
                               uint32_t sb_col,
                               uint32_t sb_row,
                               SvtAv2BlockView* block);

/* --- partition rules ------------------------------------------------------- */

/* True when the block at superblock (sb_col, sb_row) has no left / no top
 * neighbour inside the frame.
 *
 * A block has no top neighbour when its top edge is the frame's top edge; no
 * left neighbour when its left edge is the frame's left edge. The chroma
 * boundary is identical to the luma boundary: for an even luma coordinate x > 0
 * the previous chroma column covers luma [x-2, x) and the block's first chroma
 * column covers luma [x, x+2), so they are different columns. There is
 * therefore no separate chroma flag. */
SvtAv2Error svt_av2_block_boundary_flags(const SvtAv2FrameGeometry* geometry,
                                         uint32_t sb_col,
                                         uint32_t sb_row,
                                         int* has_left,
                                         int* has_above);

/* Partition forced by the frame boundary for the block at (sb_col, sb_row),
 * following av2_common_int.h:4535-4593:
 *   - fully inside            -> SVT_AV2_PARTITION_NONE
 *   - missing columns only    -> SVT_AV2_PARTITION_VERT
 *   - missing rows (or both)  -> SVT_AV2_PARTITION_HORZ
 *
 * This is what tells T011/T015 which blocks carry no partition syntax at all.
 * See svt_av2_partition_traverse for why such a split must not emit symbols. */
SvtAv2Error svt_av2_partition_implied_at_boundary(const SvtAv2FrameGeometry* geometry,
                                                  uint32_t sb_col,
                                                  uint32_t sb_row,
                                                  SvtAv2PartitionType* implied);

/* --- chroma mapping -------------------------------------------------------- */

/* Maps a luma block view to the chroma view it corresponds to, 4:2:0.
 *
 * Two rules that a naive `w/2 x h/2` gets wrong:
 *   - the chroma block shape is looked up per luma block shape (Subsampled_Size
 *     table), not computed by halving. For the shapes this module traverses the
 *     two agree, and the table keeps it correct if finer partitions land later.
 *   - the chroma origin is the truncated halving of the luma origin
 *     (`x >> 1`), never rounded up to a multiple of 8.
 *
 * `chroma_view->base` is left NULL: caller maps through the picture itself.
 * Returns SVT_AV2_ERROR_UNSUPPORTED when the block lies entirely outside the
 * chroma plane. */
SvtAv2Error svt_av2_chroma_view(const SvtAv2FrameGeometry* geometry,
                                const SvtAv2BlockView* block,
                                SvtAv2BlockView* chroma_view);

/* --- traversal ------------------------------------------------------------- */

/* Visited exactly once per coding block of the frame, in raster order of
 * superblocks. Returning a non-zero value from `visit` stops the traversal and
 * makes svt_av2_partition_traverse return SVT_AV2_ERROR_INVALID_STATE, so a
 * caller's failure cannot be mistaken for a completed walk. */
typedef int (*SvtAv2BlockVisitFn)(void* user, const SvtAv2BlockView* block);

/* Walks every coding block of the frame, in raster order.
 *
 * `visit` is called ONLY for blocks that carry partition syntax. A superblock
 * at the right/bottom edge is first split by the boundary-implicit rule above;
 * those sub-blocks carry no syntax and are visited directly without recursing
 * further. The walk therefore covers the whole visible frame with
 * non-overlapping blocks and never leaves the visible area.
 *
 * Emission order is the order the bitstream needs: sub-blocks of a split
 * superblock are produced before moving to the next superblock, i.e. recursion
 * order (left-to-right, top-to-bottom within the superblock).
 *
 * Geometry only: no symbol is written, no partition type is invented. */
SvtAv2Error svt_av2_partition_traverse(const SvtAv2FrameGeometry* geometry,
                                       SvtAv2BlockVisitFn visit,
                                       void* user);

/* M2 coding tree. Unlike visible block views, w/h are coded dimensions here.
 * Boundary-forced binary partitions follow AVM av2_common_int.h. Non-forced
 * rectangles split along their long edge; partial squares split horizontally.
 * Every leaf is a fully visible 8/16/32/64 square, with no clipped TX shape.
 * The tile writer and reconstruction use this same decision and leaf order. */
SvtAv2Error svt_av2_partition_coded_type(const SvtAv2FrameGeometry* geometry,
                                        const SvtAv2BlockView* block,
                                        SvtAv2PartitionType* partition,
                                        int* is_implied);
SvtAv2Error svt_av2_partition_traverse_coded(const SvtAv2FrameGeometry* geometry,
                                            SvtAv2BlockVisitFn visit,
                                            void* user);

/* M5.2 source-only proxy: one uint16_t entry per SB in raster order, 0=old tree,
 * 1=full 64 root HORZ followed by VERT children (four 32-square leaves).
 * Partial SBs are always 0. Selection compares exact quadrant-mean SSE gain
 * against a fixed 8-bit-domain 16/sample penalty, scaled by bit depth squared.
 * Capacity must cover sb_cols*sb_rows. Invalid input leaves decisions untouched. */
SvtAv2Error svt_av2_partition_select_quads(const SvtAv2FrameBuffer* source,
    const SvtAv2FrameGeometry* geometry, uint16_t* decisions, size_t capacity);
/* NULL/0 uses the frozen tree. A non-NULL plan must cover the complete grid,
 * use 0..3 for uniform 64/32/16/8 leaves, or 4+four base-3 digits for
 * quadrant depths 1..3 (32/16/8), or uniform rectangles: 85/86=16x8/8x16, 87/88=32x64/64x32,
 * 89/90=16x32/32x16, 91/92=16x64/64x16.
 * 93..173 use four base-3 quadrant digits: 0=32x32, 1=16x32, 2=32x16.
 * 174..254 use the same digits with 0=four 16x16 leaves.
 * 255..879 use four base-5 digits: 0=four16x16, 1=sixteen8x8,
 * 2=eight16x8, 3=eight8x16, 4=one32x32. Plans880/881 are uniform8x32/32x8.
 * 882+base7code adds shapes5=8x32 and6=32x8 per32x32 quadrant. The raw
 * range882..3282 accepts only1774 canonical new plans (first887,last3281).
 * 3283/3284 are full-SB uniform8x64/64x8 binary trees; their leaves
 * have implicit NONE. They do not add shapes to the base7 helper.
 * Codes using only old shapes or uniform thin shapes must use their old IDs.
 * The all-zero codes 93/174 are aliases of uniform plans 1/2.
 * Partial SBs must be 0. Invalid plans are rejected
 * before traversal callbacks. Both frame cores and the writer use this helper. */
/* Four base7 quadrant shape digits in0..2400 -> canonical plan; invalid
 * code returns UINT16_MAX. Existing selectors retain their own frozen mapping. */
uint16_t svt_av2_partition_mixed7_plan(uint32_t code);
SvtAv2Error svt_av2_partition_plan_validate(const SvtAv2FrameGeometry* geometry,
    const uint16_t* decisions, size_t count);
/* Validate just the changed entry of an already-valid private plan. */
SvtAv2Error svt_av2_partition_plan_validate_sb(const SvtAv2FrameGeometry* geometry,
    size_t sb_index, uint16_t decision);
/* Internal traversal helper: the caller has validated the complete plan and
 * must not modify it except through single-entry validation during traversal. */
SvtAv2Error svt_av2_partition_coded_type_plan_validated(const SvtAv2FrameGeometry* geometry,
    const SvtAv2BlockView* block, const uint16_t* decisions,
    SvtAv2PartitionType* partition, int* is_implied);
/* Square-depth plans 0..84 only, quadrant 0..3 in raster order. */
uint32_t svt_av2_partition_plan_depth(uint16_t decision, uint32_t quadrant);
SvtAv2Error svt_av2_partition_coded_type_plan(const SvtAv2FrameGeometry* geometry,
    const SvtAv2BlockView* block, const uint16_t* decisions, size_t count,
    SvtAv2PartitionType* partition, int* is_implied);
SvtAv2Error svt_av2_partition_traverse_coded_plan(const SvtAv2FrameGeometry* geometry,
    const uint16_t* decisions, size_t count, SvtAv2BlockVisitFn visit, void* user);
/* M6: the same coded traversal restricted to one raster-order superblock. */
SvtAv2Error svt_av2_partition_traverse_sb_plan(const SvtAv2FrameGeometry* geometry,
    size_t sb_index, const uint16_t* decisions, size_t count,
    SvtAv2BlockVisitFn visit, void* user);

/* --- neighbours ------------------------------------------------------------ */

/* Reads the available above/left samples of `block` from `picture` (plane
 * `plane`). Missing neighbours are reported through the validity flags; this
 * function never reads outside the picture.
 *
 * The samples come from whatever `picture` holds. For prediction that MUST be
 * the internal reconstruction, never the uncoded source (contract 2.2); passing
 * a source picture here is legal but is not a prediction neighbour. */
SvtAv2Error svt_av2_block_neighbors(const SvtAv2FrameBuffer* picture,
                                    uint32_t plane,
                                    const SvtAv2BlockView* block,
                                    SvtAv2BlockNeighbors* neighbors);

#endif /* SVT_AV2_BLOCK_GEOMETRY_H */
