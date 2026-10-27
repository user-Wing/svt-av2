/*
 * T007 unit tests: frame geometry, partition traversal, chroma mapping and
 * neighbour extraction.
 *
 * Covers the frozen contract (docs/t007-picture-block-geometry.md):
 *   - the superblock grid covers every visible sample, with no overlap and no
 *     block reaching outside the frame (including 176x144, 720p and 1080p,
 *     where 1080/64 is not an integer)
 *   - the boundary-implicit partition rule for partial superblocks
 *   - chroma coordinates and chroma block shapes for every traversed block
 *   - the traversal emits no syntax for implicitly split superblocks
 *   - at least 100 fixed-seed random block/pattern cases against an independent
 *     expected model, via svt_av2_block_neighbors
 */

#include "block_geometry.h"
#include "picture.h"
#include "test_util.h"

#include <string.h>

/* --- deterministic pseudo random generator -------------------------------- */

/* Fixed-seed xorshift: the same sequence is produced on every platform and run,
 * so a failure is reproducible from the seed alone. Not used to generate any
 * expected value, only to pick coordinates and patterns. */
typedef struct Rng {
    uint32_t state;
} Rng;

static void rng_seed(Rng* rng, uint32_t seed) { rng->state = seed ? seed : 0x9E3779B9u; }

static uint32_t rng_next(Rng* rng) {
    uint32_t x = rng->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng->state = x;
    return x;
}

static uint32_t rng_below(Rng* rng, uint32_t bound) { return (bound == 0) ? 0 : (rng_next(rng) % bound); }

/* --- traversal recorder ---------------------------------------------------- */

typedef struct VisitRecord {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
} VisitRecord;

#define MAX_RECORDS 8192

typedef struct VisitLog {
    VisitRecord records[MAX_RECORDS];
    uint32_t    count;
    int         overflow;
    int         abort_after; /* -1 = never abort */
} VisitLog;

static int record_block(void* user, const SvtAv2BlockView* block) {
    VisitLog* log = (VisitLog*)user;
    if (log->count >= MAX_RECORDS) {
        log->overflow = 1;
        return 1;
    }
    log->records[log->count].x = block->x;
    log->records[log->count].y = block->y;
    log->records[log->count].w = block->w;
    log->records[log->count].h = block->h;
    ++log->count;
    if (log->abort_after >= 0 && log->count > (uint32_t)log->abort_after) {
        return 1;
    }
    return 0;
}

/* Coverage map: every visible sample must be claimed exactly once. */
#define COVER_W 1920u
#define COVER_H 1080u

static void check_coverage(const SvtAv2FrameGeometry* geometry, const VisitLog* log) {
    static uint8_t covered[COVER_W * COVER_H];
    memset(covered, 0, (size_t)geometry->visible_width * geometry->visible_height);

    int outside = 0;
    int double_covered = 0;
    size_t claimed = 0;

    for (uint32_t i = 0; i < log->count; ++i) {
        const VisitRecord* const r = &log->records[i];
        if (r->w == 0 || r->h == 0) {
            ++outside;
            continue;
        }
        if (r->x + r->w > geometry->visible_width || r->y + r->h > geometry->visible_height) {
            ++outside;
            continue;
        }
        for (uint32_t row = r->y; row < r->y + r->h; ++row) {
            for (uint32_t col = r->x; col < r->x + r->w; ++col) {
                uint8_t* const cell = &covered[(size_t)row * geometry->visible_width + col];
                if (*cell != 0) {
                    ++double_covered;
                }
                *cell = 1;
                ++claimed;
            }
        }
    }

    size_t uncovered = 0;
    const size_t total = (size_t)geometry->visible_width * geometry->visible_height;
    for (size_t i = 0; i < total; ++i) {
        if (covered[i] == 0) {
            ++uncovered;
        }
    }

    SVT_TEST_CHECK_EQ_INT(outside, 0);
    SVT_TEST_CHECK_EQ_INT(double_covered, 0);
    SVT_TEST_CHECK_EQ_INT(uncovered, 0);
    SVT_TEST_CHECK_EQ_INT(claimed, total);
}

static void expect_geometry(uint32_t width,
                            uint32_t height,
                            uint32_t sb_size,
                            uint32_t sb_cols,
                            uint32_t sb_rows,
                            uint32_t partial) {
    SvtAv2FrameGeometry geometry;
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&geometry, width, height, sb_size), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(geometry.sb_cols, sb_cols);
    SVT_TEST_CHECK_EQ_INT(geometry.sb_rows, sb_rows);
    SVT_TEST_CHECK_EQ_INT(geometry.mi_width, width >> 2);
    SVT_TEST_CHECK_EQ_INT(geometry.mi_height, height >> 2);
    SVT_TEST_CHECK_EQ_INT(geometry.chroma_width, width >> 1);
    SVT_TEST_CHECK_EQ_INT(geometry.chroma_height, height >> 1);
    SVT_TEST_CHECK_EQ_INT(geometry.partial_sb_count, partial);

    static VisitLog log;
    memset(&log, 0, sizeof(log));
    log.abort_after = -1;
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse(&geometry, record_block, &log), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK(!log.overflow);
    check_coverage(&geometry, &log);

    /* Every block must be at least the chroma coding unit. */
    int too_small = 0;
    for (uint32_t i = 0; i < log.count; ++i) {
        if (log.records[i].w < SVT_AV2_MIN_CODING_BLOCK_SIZE || log.records[i].h < SVT_AV2_MIN_CODING_BLOCK_SIZE) {
            ++too_small;
        }
    }
    SVT_TEST_CHECK_EQ_INT(too_small, 0);
}

static void test_geometry_rejection(void) {
    SvtAv2FrameGeometry geometry;

    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(NULL, 64, 64, 64), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&geometry, 0, 64, 64), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&geometry, 64, 0, 64), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&geometry, 63, 64, 64), SVT_AV2_ERROR_UNSUPPORTED);
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&geometry, 64, 65, 64), SVT_AV2_ERROR_UNSUPPORTED);
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&geometry, 64, 64, 32), SVT_AV2_ERROR_UNSUPPORTED);
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&geometry, 64, 64, 0), SVT_AV2_ERROR_UNSUPPORTED);
}

static void test_target_sizes(void) {
    /* 64x64: a single whole superblock, no partial block. */
    expect_geometry(64, 64, 64, 1, 1, 0);
    /* 176x144: 2.75 x 2.25 superblocks -> 3x3; the right column and the bottom
     * row reach past the edge, so 9 - (2*2) = 5 superblocks are partial. */
    expect_geometry(176, 144, 64, 3, 3, 5);
    /* 1280x720: 720/64 = 11.25 is not an integer, so the bottom superblock row
     * is partial: 180 mi rounds up to 12 rows of 16 mi, and all 20 superblocks
     * of that last row reach past the edge. */
    expect_geometry(1280, 720, 64, 20, 12, 20);
    /* 1080p: 1080/64 = 16.875 is NOT an integer, so the bottom superblock row
     * is partial. This is the case the task singles out. 270 mi rounds up to 17
     * rows of 16 mi, and all 30 superblocks of that last row are partial. */
    expect_geometry(1920, 1080, 64, 30, 17, 30);
    /* 1080p with a 128 superblock: 480 mi -> 15 whole superblocks per row;
     * 270 mi rounds up to 288 -> 9 rows, so the entire bottom row is partial. */
    expect_geometry(1920, 1080, 128, 15, 9, 15);
}

static void test_partition_boundary_rule(void) {
    SvtAv2FrameGeometry geometry;
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&geometry, 1920, 1080, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(geometry.sb_cols, 30);
    SVT_TEST_CHECK_EQ_INT(geometry.sb_rows, 17);

    SvtAv2PartitionType implied = SVT_AV2_PARTITION_INVALID;
    int has_left = -1;
    int has_above = -1;

    /* Fully interior superblock: no implied split, both neighbours exist. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_implied_at_boundary(&geometry, 3, 3, &implied), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(implied, SVT_AV2_PARTITION_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_block_boundary_flags(&geometry, 3, 3, &has_left, &has_above), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(has_left, 1);
    SVT_TEST_CHECK_EQ_INT(has_above, 1);

    /* Right-edge superblock sb_col=29: its mi extent is [464, 480) and the
     * frame is 480 mi wide, so mi_col + hbs = 464 + 8 = 472 < 480 -> has_cols.
     * AVM's is_partition_implied_at_boundary only asks whether the block's HALF
     * extent stays inside, so a superblock that ends exactly on the frame edge
     * is NOT at the boundary and carries no implied partition. sb_col=30 does
     * not exist (sb_cols == 30), so for 1920x1080 with 64x64 superblocks the
     * whole right edge is exact and every implied split comes from the bottom. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_implied_at_boundary(&geometry, 29, 3, &implied), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(implied, SVT_AV2_PARTITION_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_implied_at_boundary(&geometry, 29, 16, &implied), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(implied, SVT_AV2_PARTITION_NONE);

    /* A frame whose width is NOT a multiple of the superblock does produce
     * implied VERT splits. 176 has 2.75 superblocks of 64: mi width is 44, so
     * sb_col=2 spans mi [32, 48) and 32 + 8 = 40 < 44 -> still has_cols by the
     * AVM test, yet the block itself reaches 48 > 44. AVM reports NONE here too,
     * which is why the visible extent is clipped separately in
     * svt_av2_block_from_sb rather than being folded into the boundary test. */
    SvtAv2FrameGeometry narrow;
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&narrow, 176, 144, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_implied_at_boundary(&narrow, 2, 0, &implied), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(implied, SVT_AV2_PARTITION_NONE);

    /* Bottom edge: 1080/64 = 16.875, so sb_row=16 spans mi [256, 272) while the
     * frame is 270 mi tall. 256 + 8 = 264 < 270 -> has_rows is true, but the
     * block still reaches past the bottom. The implied split is driven by
     * hbs_h: for sb_row=16, mi_row + hbs_h = 264 < 270 -> has_rows true;
     * has_cols true as shown above; so AVM returns NONE and the clipping in
     * svt_av2_block_from_sb supplies the 16-row visible height. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_implied_at_boundary(&geometry, 3, 16, &implied), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(implied, SVT_AV2_PARTITION_NONE);

    /* The boundary rule does fire when a superblock's half extent is itself
     * outside the frame. Build that case explicitly: a frame 64 mi wide (256
     * samples) with 128x128 superblocks has sb_cols = align(64, 5) >> 5 = 2, and
     * sb_col=1 spans mi [32, 64): 32 + 16 = 48 < 64 -> has_cols. Shrink the
     * frame so the second superblock's half extent falls outside. */
    SvtAv2FrameGeometry half;
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&half, 192, 192, 128), SVT_AV2_ERROR_NONE);
    /* mi width = 48, 128 superblock = 32 mi: sb_cols = align(48,5)>>5 = 2.
     * sb_col=1 spans mi [32, 64); 32 + 16 = 48, and 48 < 48 is false -> no
     * columns. Rows are exact (48 mi = 1.5 sb -> sb_rows = 2; sb_row=0 spans
     * [0,32): 0 + 16 = 16 < 48 -> has_rows). has_rows && !has_cols -> VERT. */
    SVT_TEST_CHECK_EQ_INT(half.sb_cols, 2);
    SVT_TEST_CHECK_EQ_INT(half.sb_rows, 2);
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_implied_at_boundary(&half, 1, 0, &implied), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(implied, SVT_AV2_PARTITION_VERT);
    /* sb_row=1 spans mi [32, 64): 32 + 16 = 48, not < 48 -> no rows either ->
     * HORZ (square block, anything but "rows yes, cols no" gives HORZ). */
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_implied_at_boundary(&half, 1, 1, &implied), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(implied, SVT_AV2_PARTITION_HORZ);

    /* Spec truth table for a square block, from partition_implied_at_boundary:
     * only three of the four (hasRows, hasCols) combinations are reachable,
     * because (1,1) returns NONE first. Verify the remaining row explicitly:
     * hasRows=0 and hasCols=0 must give HORZ (the spec's `hasRows ? VERT : HORZ`
     * with hasRows false), not VERT and not NONE. */
    SvtAv2FrameGeometry tiny;
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&tiny, 192, 192, 64), SVT_AV2_ERROR_NONE);
    /* mi 48x48. A 64 superblock is 16 mi, so hbs = 8. sb_col=2 spans mi [32,48):
     * 32 + 8 = 40 < 48 -> hasCols true. sb_row=2 likewise hasRows true -> NONE.
     * Shrink to 128x128 (mi 32x32) so that sb_col=1 spans [16,32): 16 + 8 = 24 <
     * 32 -> still true. Use 160x160 (mi 40x40): sb_col=2 spans [32,48), 32+8=40
     * which is not < 40 -> hasCols false; sb_row=2 -> 32+8=40 not < 40 ->
     * hasRows false. Both false -> HORZ. */
    SvtAv2FrameGeometry both;
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&both, 160, 160, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_implied_at_boundary(&both, 2, 2, &implied), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(implied, SVT_AV2_PARTITION_HORZ);
    /* Same frame, right edge only (hasRows true, hasCols false) -> VERT. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_implied_at_boundary(&both, 2, 0, &implied), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(implied, SVT_AV2_PARTITION_VERT);
    /* Bottom edge only (hasRows false, hasCols true) -> HORZ. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_implied_at_boundary(&both, 0, 2, &implied), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(implied, SVT_AV2_PARTITION_HORZ);

    /* Top-left: no neighbours at all, and no implied split (the block is whole). */
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_implied_at_boundary(&geometry, 0, 0, &implied), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(implied, SVT_AV2_PARTITION_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_block_boundary_flags(&geometry, 0, 0, &has_left, &has_above), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(has_left, 0);
    SVT_TEST_CHECK_EQ_INT(has_above, 0);

    /* Out of range superblock indices are rejected. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_implied_at_boundary(&geometry, 30, 0, &implied),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_implied_at_boundary(&geometry, 0, 17, &implied),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
}

static void test_block_views_and_chroma(void) {
    SvtAv2FrameBuffer picture;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&picture, 176, 144), SVT_AV2_ERROR_NONE);

    SvtAv2FrameGeometry geometry;
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&geometry, 176, 144, 64), SVT_AV2_ERROR_NONE);

    SvtAv2BlockView block;

    /* Whole superblock. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_block_view(&geometry, &picture, SVT_AV2_PLANE_Y, 0, 0, &block),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(block.x, 0);
    SVT_TEST_CHECK_EQ_INT(block.y, 0);
    SVT_TEST_CHECK_EQ_INT(block.w, 64);
    SVT_TEST_CHECK_EQ_INT(block.h, 64);
    SVT_TEST_CHECK_EQ_INT(block.base, picture.planes[SVT_AV2_PLANE_Y]);

    /* Right edge: 176 - 128 = 48 visible columns. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_block_view(&geometry, &picture, SVT_AV2_PLANE_Y, 2, 0, &block),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(block.x, 128);
    SVT_TEST_CHECK_EQ_INT(block.w, 48);
    SVT_TEST_CHECK_EQ_INT(block.h, 64);
    SVT_TEST_CHECK_EQ_INT(block.base, picture.planes[SVT_AV2_PLANE_Y] + 128);

    /* Bottom edge: 144 - 128 = 16 visible rows. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_block_view(&geometry, &picture, SVT_AV2_PLANE_Y, 0, 2, &block),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(block.y, 128);
    SVT_TEST_CHECK_EQ_INT(block.h, 16);
    SVT_TEST_CHECK_EQ_INT(block.base, picture.planes[SVT_AV2_PLANE_Y] + 128u * picture.strides[SVT_AV2_PLANE_Y]);

    /* Bottom-right corner: 48x16. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_block_view(&geometry, &picture, SVT_AV2_PLANE_Y, 2, 2, &block),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(block.x, 128);
    SVT_TEST_CHECK_EQ_INT(block.y, 128);
    SVT_TEST_CHECK_EQ_INT(block.w, 48);
    SVT_TEST_CHECK_EQ_INT(block.h, 16);

    /* Chroma of the bottom-right block: 24x8 at (64,64). */
    SVT_TEST_CHECK_EQ_INT(svt_av2_block_view(&geometry, &picture, SVT_AV2_PLANE_U, 2, 2, &block),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(block.x, 64);
    SVT_TEST_CHECK_EQ_INT(block.y, 64);
    SVT_TEST_CHECK_EQ_INT(block.w, 24);
    SVT_TEST_CHECK_EQ_INT(block.h, 8);
    SVT_TEST_CHECK_EQ_INT(block.base,
                          picture.planes[SVT_AV2_PLANE_U] + 64u * picture.strides[SVT_AV2_PLANE_U] + 64u);

    /* The V plane must not alias U. */
    SvtAv2BlockView v_block;
    SVT_TEST_CHECK_EQ_INT(svt_av2_block_view(&geometry, &picture, SVT_AV2_PLANE_V, 2, 2, &v_block),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK(block.base != v_block.base);

    /* A picture whose size disagrees with the geometry must be rejected rather
     * than silently indexed with the wrong stride. */
    SvtAv2FrameBuffer other;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&other, 64, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_block_view(&geometry, &other, SVT_AV2_PLANE_Y, 0, 0, &block),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&other), SVT_AV2_ERROR_NONE);

    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&picture), SVT_AV2_ERROR_NONE);
}

static void test_chroma_shape_table(void) {
    SvtAv2FrameGeometry geometry;
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&geometry, 1920, 1080, 64), SVT_AV2_ERROR_NONE);

    struct Case {
        uint32_t luma_w;
        uint32_t luma_h;
        uint32_t chroma_w;
        uint32_t chroma_h;
    };
    /* Values read off the Subsampled_Size table (avm common_data.h:709-743),
     * column [bsize][1][1]. */
    const struct Case cases[] = {
        {8u, 8u, 4u, 4u},
        {16u, 16u, 8u, 8u},
        {32u, 32u, 16u, 16u},
        {64u, 64u, 32u, 32u},
        {64u, 32u, 32u, 16u},
        {32u, 64u, 16u, 32u},
    };

    for (size_t i = 0; i < SVT_AV2_ARRAY_SIZE(cases); ++i) {
        SvtAv2BlockView luma;
        memset(&luma, 0, sizeof(luma));
        luma.x = 64;
        luma.y = 64;
        luma.w = cases[i].luma_w;
        luma.h = cases[i].luma_h;

        SvtAv2BlockView chroma;
        memset(&chroma, 0, sizeof(chroma));
        SVT_TEST_CHECK_EQ_INT(svt_av2_chroma_view(&geometry, &luma, &chroma), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(chroma.x, 32);
        SVT_TEST_CHECK_EQ_INT(chroma.y, 32);
        SVT_TEST_CHECK_EQ_INT(chroma.w, cases[i].chroma_w);
        SVT_TEST_CHECK_EQ_INT(chroma.h, cases[i].chroma_h);
    }

    /* A 4x4 luma block has no chroma view of its own: it shares a chroma
     * reference with its parent. Refusing is better than guessing a window. */
    SvtAv2BlockView tiny;
    memset(&tiny, 0, sizeof(tiny));
    tiny.w = 4;
    tiny.h = 4;
    SvtAv2BlockView unused;
    SVT_TEST_CHECK_EQ_INT(svt_av2_chroma_view(&geometry, &tiny, &unused), SVT_AV2_ERROR_UNSUPPORTED);
}

static void test_traversal_abort(void) {
    SvtAv2FrameGeometry geometry;
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&geometry, 176, 144, 64), SVT_AV2_ERROR_NONE);

    /* A callback failure must be reported, not silently treated as a completed
     * walk. */
    VisitLog log;
    memset(&log, 0, sizeof(log));
    log.abort_after = 2;
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse(&geometry, record_block, &log), SVT_AV2_ERROR_INVALID_STATE);
    SVT_TEST_CHECK_EQ_INT(log.count, 3u);

    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse(&geometry, NULL, NULL), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse(NULL, record_block, &log), SVT_AV2_ERROR_INVALID_PARAMETER);
}

static void test_random_blocks_against_model(void) {
    /* At least 100 fixed-seed random cases, as required by the task. Each case
     * builds a picture with a random pattern, then checks the neighbour
     * extraction against an independently computed expectation. */
    const uint32_t kCaseCount = 128;
    Rng rng;
    rng_seed(&rng, 0x5A17C0DEu);

    static const uint32_t sizes[][2] = {
        {64u, 64u}, {176u, 144u}, {320u, 240u}, {1920u, 1080u},
    };

    uint32_t mismatch = 0;
    uint32_t cases_run = 0;

    for (uint32_t case_index = 0; case_index < kCaseCount; ++case_index) {
        const uint32_t* const size = sizes[rng_below(&rng, (uint32_t)SVT_AV2_ARRAY_SIZE(sizes))];
        const uint32_t width = size[0];
        const uint32_t height = size[1];

        SvtAv2FrameBuffer picture;
        if (svt_av2_picture_alloc(&picture, width, height) != SVT_AV2_ERROR_NONE) {
            continue;
        }
        ++cases_run;

        /* Random pattern, independently reproduced below from the same seed
         * stream would be fragile; instead the expected value is recomputed by
         * reading the picture back through the public accessors, and the
         * neighbour result is compared against a direct pointer read. That
         * keeps the check independent of the neighbour code path. */
        for (uint32_t plane = 0; plane < SVT_AV2_PICTURE_MAX_PLANES; ++plane) {
            uint8_t* const base = svt_av2_picture_plane(&picture, plane);
            const uint32_t pw = svt_av2_picture_plane_width(&picture, plane);
            const uint32_t ph = svt_av2_picture_plane_height(&picture, plane);
            const uint32_t stride = picture.strides[plane];
            const uint32_t seed = rng_next(&rng);
            for (uint32_t row = 0; row < ph; ++row) {
                for (uint32_t col = 0; col < pw; ++col) {
                    base[(size_t)row * stride + col] = (uint8_t)((seed + row * 31u + col * 17u) & 0xFFu);
                }
            }
        }

        const uint32_t sb_size = (rng_next(&rng) & 1u) ? 64u : 128u;
        SvtAv2FrameGeometry geometry;
        if (svt_av2_frame_geometry_init(&geometry, width, height, sb_size) != SVT_AV2_ERROR_NONE) {
            svt_av2_picture_free(&picture);
            continue;
        }

        const uint32_t plane = rng_below(&rng, SVT_AV2_PICTURE_MAX_PLANES);
        const uint32_t sb_col = rng_below(&rng, geometry.sb_cols);
        const uint32_t sb_row = rng_below(&rng, geometry.sb_rows);

        SvtAv2BlockView block;
        if (svt_av2_block_view(&geometry, &picture, plane, sb_col, sb_row, &block) != SVT_AV2_ERROR_NONE) {
            svt_av2_picture_free(&picture);
            continue;
        }

        SvtAv2BlockNeighbors neighbors;
        memset(&neighbors, 0xFF, sizeof(neighbors));
        if (svt_av2_block_neighbors(&picture, plane, &block, &neighbors) != SVT_AV2_ERROR_NONE) {
            ++mismatch;
            svt_av2_picture_free(&picture);
            continue;
        }

        /* Independent expectation, read straight out of the plane buffer. */
        const uint8_t* const base = svt_av2_picture_plane_const(&picture, plane);
        const uint32_t stride = picture.strides[plane];
        const int expect_left = (block.x > 0);
        const int expect_above = (block.y > 0);
        if (neighbors.left_valid != expect_left || neighbors.above_valid != expect_above) {
            ++mismatch;
        }
        if (expect_above && expect_left) {
            const uint8_t* const corner = base + (size_t)(block.y - 1u) * stride + (block.x - 1u);
            if (neighbors.above[0] != corner[0] || neighbors.above[1] != corner[1] ||
                neighbors.left[0] != corner[0] || neighbors.left[1] != corner[stride]) {
                ++mismatch;
            }
        } else if (expect_above) {
            const uint8_t* const above = base + (size_t)(block.y - 1u) * stride + block.x;
            if (neighbors.above[0] != above[0] || neighbors.above[1] != above[1]) {
                ++mismatch;
            }
        } else if (expect_left) {
            const uint8_t* const left = base + (size_t)block.y * stride + (block.x - 1u);
            if (neighbors.left[0] != left[0] || neighbors.left[1] != left[stride]) {
                ++mismatch;
            }
        }

        /* The neighbour window must never reach outside the plane, which is
         * guaranteed here by the validity flags plus the assertion that the
         * block itself is inside. */
        if (block.x + block.w > svt_av2_picture_plane_width(&picture, plane) ||
            block.y + block.h > svt_av2_picture_plane_height(&picture, plane)) {
            ++mismatch;
        }

        svt_av2_picture_free(&picture);
    }

    SVT_TEST_CHECK(cases_run >= 100u);
    SVT_TEST_CHECK_EQ_INT(mismatch, 0);
}

static void test_neighbors_rejection(void) {
    SvtAv2FrameBuffer picture;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&picture, 64, 64), SVT_AV2_ERROR_NONE);

    SvtAv2BlockView block;
    memset(&block, 0, sizeof(block));
    block.x = 0;
    block.y = 0;
    block.w = 64;
    block.h = 64;
    SvtAv2BlockNeighbors neighbors;

    SVT_TEST_CHECK_EQ_INT(svt_av2_block_neighbors(NULL, SVT_AV2_PLANE_Y, &block, &neighbors),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_block_neighbors(&picture, SVT_AV2_PLANE_Y, NULL, &neighbors),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_block_neighbors(&picture, SVT_AV2_PLANE_Y, &block, NULL),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_block_neighbors(&picture, 3, &block, &neighbors),
                          SVT_AV2_ERROR_INVALID_PARAMETER);

    /* A block running past the right/bottom edge would force an out-of-bounds
     * read, so it is rejected instead. */
    SvtAv2BlockView past;
    memset(&past, 0, sizeof(past));
    past.x = 32;
    past.y = 0;
    past.w = 64;
    past.h = 64;
    SVT_TEST_CHECK_EQ_INT(svt_av2_block_neighbors(&picture, SVT_AV2_PLANE_Y, &past, &neighbors),
                          SVT_AV2_ERROR_INVALID_PARAMETER);

    SvtAv2BlockView zero;
    memset(&zero, 0, sizeof(zero));
    SVT_TEST_CHECK_EQ_INT(svt_av2_block_neighbors(&picture, SVT_AV2_PLANE_Y, &zero, &neighbors),
                          SVT_AV2_ERROR_INVALID_PARAMETER);

    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&picture), SVT_AV2_ERROR_NONE);
}

static void test_coded_boundary_coverage(void) {
    static const uint32_t sizes[][2] = {
        {8u, 8u}, {64u, 64u}, {72u, 72u}, {176u, 144u},
        {1280u, 720u}, {1920u, 1080u}
    };
    for (size_t i = 0; i < SVT_AV2_ARRAY_SIZE(sizes); ++i) {
        SvtAv2FrameGeometry geometry;
        VisitLog log;
        memset(&log, 0, sizeof(log));
        log.abort_after = -1;
        SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&geometry, sizes[i][0],
                                                        sizes[i][1], 64u), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_coded(&geometry, record_block, &log),
                              SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(log.overflow, 0);
        check_coverage(&geometry, &log);
        for (uint32_t j = 0; j < log.count; ++j) {
            const VisitRecord* leaf = &log.records[j];
            SVT_TEST_CHECK(leaf->w == leaf->h);
            SVT_TEST_CHECK(leaf->w == 8u || leaf->w == 16u ||
                           leaf->w == 32u || leaf->w == 64u);
        }
        if (sizes[i][0] == 64u && sizes[i][1] == 64u) {
            SVT_TEST_CHECK_EQ_INT(log.count, 1);
            SVT_TEST_CHECK_EQ_INT(log.records[0].w, 64);
        }
        log.count = 0;
        log.abort_after = 0;
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_coded(&geometry, record_block, &log),
                              SVT_AV2_ERROR_INVALID_STATE);
    }
}

static void test_444_same_coded_tree_and_sample_addresses(void) {
    SvtAv2FrameGeometry g420, g444;
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&g420, 176, 144, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init_ex(&g444, 176, 144, 64, SVT_AV2_CHROMA_444), SVT_AV2_ERROR_NONE);
    VisitLog a, b; memset(&a, 0, sizeof(a)); memset(&b, 0, sizeof(b));
    a.abort_after = -1; b.abort_after = -1;
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_coded(&g420, record_block, &a), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_coded(&g444, record_block, &b), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(a.count, b.count);
    for (uint32_t i = 0; i < (uint32_t)a.count; ++i) {
        SVT_TEST_CHECK_EQ_INT(memcmp(&a.records[i], &b.records[i], sizeof(a.records[i])), 0);
        SvtAv2BlockView chroma;
        const SvtAv2BlockView block = { b.records[i].x, b.records[i].y, b.records[i].w, b.records[i].h, NULL };
        SVT_TEST_CHECK_EQ_INT(svt_av2_chroma_view(&g444, &block, &chroma), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(chroma.x, b.records[i].x); SVT_TEST_CHECK_EQ_INT(chroma.y, b.records[i].y);
        SVT_TEST_CHECK_EQ_INT(chroma.w, b.records[i].w); SVT_TEST_CHECK_EQ_INT(chroma.h, b.records[i].h);
    }
}

/* Independent SSE model: explicitly form global/quadrant means, then sum
 * squared pixel deviations; it does not reuse the selector's squared-sum score. */
static int model_quads(const SvtAv2FrameBuffer* source) {
    double global = 0.0, means[4] = {0.0, 0.0, 0.0, 0.0};
    const uint32_t step = source->bit_depth == 8u ? 1u : 2u;
    for (uint32_t y = 0; y < 64u; ++y) for (uint32_t x = 0; x < 64u; ++x) {
        const double value = svt_av2_sample_load(source->planes[0] + y * source->strides[0] + x * step, source->bit_depth);
        global += value; means[y / 32u * 2u + x / 32u] += value;
    }
    global /= 4096.0;
    for (uint32_t q = 0; q < 4u; ++q) means[q] /= 1024.0;
    double whole = 0.0, split = 0.0;
    for (uint32_t y = 0; y < 64u; ++y) for (uint32_t x = 0; x < 64u; ++x) {
        const double value = svt_av2_sample_load(source->planes[0] + y * source->strides[0] + x * step, source->bit_depth);
        const double a = value - global, b = value - means[y / 32u * 2u + x / 32u];
        whole += a * a; split += b * b;
    }
    const uint32_t scale = source->bit_depth == 8u ? 1u : 4u;
    return whole - split > 4096.0 * 16.0 * scale * scale;
}
static void test_content_selector(void) {
    uint8_t storage[64u * 131u + 1u];
    uint32_t seed = 0x82713245u;
    uint16_t equivalent[100];
    for (uint32_t f = 0; f < 3u; ++f) {
        const uint32_t bd = f == 0u ? 8u : 10u, step = bd == 8u ? 1u : 2u;
        const uint32_t scale = bd == 8u ? 1u : 4u;
        SvtAv2FrameGeometry geometry;
        SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init_ex(&geometry, 64, 64, 64,
            f == 2u ? SVT_AV2_CHROMA_444 : SVT_AV2_CHROMA_420), SVT_AV2_ERROR_NONE);
        SvtAv2FrameBuffer source; memset(&source, 0, sizeof(source));
        source.visible_width = source.visible_height = 64u; source.bit_depth = bd;
        source.chroma_format = geometry.chroma_format; source.planes[0] = storage + 1u;
        source.strides[0] = 131u; /* unaligned u16le, odd stride */
        for (uint32_t contrast = 3u; contrast <= 5u; ++contrast) {
            for (uint32_t y = 0; y < 64u; ++y) for (uint32_t x = 0; x < 64u; ++x)
                svt_av2_sample_store(source.planes[0] + y * 131u + x * step, bd,
                    (uint16_t)((x < 32u ? 128u - contrast : 128u + contrast) * scale));
            uint16_t plan = 0xA5u;
            SVT_TEST_CHECK_EQ_INT(svt_av2_partition_select_quads(&source, &geometry, &plan, 1u), SVT_AV2_ERROR_NONE);
            SVT_TEST_CHECK_EQ_INT(plan, contrast > 4u);
            SVT_TEST_CHECK_EQ_INT(plan, model_quads(&source));
        }
        seed = 0x82713245u;
        for (uint32_t trial = 0; trial < 100u; ++trial) {
            uint32_t base[4];
            for (uint32_t q = 0; q < 4u; ++q) { seed = seed * 1664525u + 1013904223u; base[q] = (seed >> 16) % 240u; }
            for (uint32_t y = 0; y < 64u; ++y) for (uint32_t x = 0; x < 64u; ++x) {
                seed = seed * 1664525u + 1013904223u;
                const uint16_t value = (uint16_t)((base[y / 32u * 2u + x / 32u] + (seed >> 28)) * scale);
                svt_av2_sample_store(source.planes[0] + y * 131u + x * step, bd, value);
            }
            uint16_t plan = 0xA5u;
            SVT_TEST_CHECK_EQ_INT(svt_av2_partition_select_quads(&source, &geometry, &plan, 1u), SVT_AV2_ERROR_NONE);
            SVT_TEST_CHECK_EQ_INT(plan, model_quads(&source));
            if (f == 0u) equivalent[trial] = plan;
            else SVT_TEST_CHECK_EQ_INT(plan, equivalent[trial]);
        }
        for (uint32_t kind = 0; kind < 2u; ++kind) {
            for (uint32_t y = 0; y < 64u; ++y) for (uint32_t x = 0; x < 64u; ++x)
                svt_av2_sample_store(source.planes[0] + y * 131u + x * step, bd,
                    (uint16_t)(kind && x < 32u ? 0u : (1u << bd) - 1u));
            uint16_t plan = 0xA5u;
            SVT_TEST_CHECK_EQ_INT(svt_av2_partition_select_quads(&source, &geometry, &plan, 1u), SVT_AV2_ERROR_NONE);
            SVT_TEST_CHECK_EQ_INT(plan, kind); /* maximum 10-bit sums/squares */
        }
        uint16_t sentinel = 0xA5u;
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_select_quads(&source, &geometry, &sentinel, 0u), SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(sentinel, 0xA5u);
        source.visible_width = 72u;
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_select_quads(&source, &geometry, &sentinel, 1u), SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(sentinel, 0xA5u);
    }
}
static void test_content_plan_geometry(void) {
    const uint32_t sizes[][2] = {{64,64},{72,72},{176,144},{1280,720},{1920,1080}};
    for (uint32_t f = 0; f < 2u; ++f) for (size_t i = 0; i < SVT_AV2_ARRAY_SIZE(sizes); ++i) {
        SvtAv2FrameGeometry geometry;
        svt_av2_frame_geometry_init_ex(&geometry, sizes[i][0], sizes[i][1], 64,
            f ? SVT_AV2_CHROMA_444 : SVT_AV2_CHROMA_420);
        uint16_t plan[1024] = {0};
        const size_t count = (size_t)geometry.sb_cols * geometry.sb_rows;
        VisitLog old, off, on; memset(&old, 0, sizeof(old)); memset(&off, 0, sizeof(off)); memset(&on, 0, sizeof(on));
        old.abort_after = off.abort_after = on.abort_after = -1;
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_coded(&geometry, record_block, &old), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_coded_plan(&geometry, plan, count, record_block, &off), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(old.count, off.count);
        SVT_TEST_CHECK_EQ_INT(memcmp(old.records, off.records, old.count * sizeof(old.records[0])), 0);
        SvtAv2FrameBuffer source;
        const uint32_t bd = f ? 10u : 8u, step = f ? 2u : 1u;
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&source, sizes[i][0], sizes[i][1], bd, geometry.chroma_format), SVT_AV2_ERROR_NONE);
        for (uint32_t y = 0; y < source.visible_height; ++y) for (uint32_t x = 0; x < source.visible_width; ++x)
            svt_av2_sample_store(source.planes[0] + (size_t)y * source.strides[0] + x * step, bd,
                (uint16_t)(x / 32u % 2u ? (1u << bd) - 1u : 0u));
        memset(plan, 0xA5, sizeof(plan));
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_select_quads(&source, &geometry, plan, count), SVT_AV2_ERROR_NONE);
        if (count < SVT_AV2_ARRAY_SIZE(plan)) SVT_TEST_CHECK_EQ_INT(plan[count], 0xA5A5u);
        uint32_t full = 0;
        for (uint32_t row = 0; row < geometry.sb_rows; ++row) for (uint32_t col = 0; col < geometry.sb_cols; ++col) {
            const int complete = geometry.visible_width - col * 64u >= 64u && geometry.visible_height - row * 64u >= 64u;
            SVT_TEST_CHECK_EQ_INT(plan[(size_t)row * geometry.sb_cols + col], complete);
            if (complete) ++full;
        }
        svt_av2_picture_free(&source);
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_coded_plan(&geometry, plan, count, record_block, &on), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(on.count, old.count + 3u * full); check_coverage(&geometry, &on);
        for (uint32_t leaf = 0; leaf < on.count; ++leaf) {
            const VisitRecord* block = &on.records[leaf];
            const int split = plan[(size_t)(block->y / 64u) * geometry.sb_cols + block->x / 64u];
            if (split) { SVT_TEST_CHECK_EQ_INT(block->w, 32); SVT_TEST_CHECK_EQ_INT(block->h, 32); }
            else {
                int found = 0;
                for (uint32_t n = 0; n < old.count; ++n)
                    if (memcmp(block, &old.records[n], sizeof(*block)) == 0) found = 1;
                SVT_TEST_CHECK(found); /* edge leaves byte-for-byte unchanged */
            }
        }
        if (sizes[i][0] == 64u) {
            SVT_TEST_CHECK_EQ_INT(on.records[0].x, 0); SVT_TEST_CHECK_EQ_INT(on.records[0].y, 0);
            SVT_TEST_CHECK_EQ_INT(on.records[1].x, 32); SVT_TEST_CHECK_EQ_INT(on.records[1].y, 0);
            SVT_TEST_CHECK_EQ_INT(on.records[2].x, 0); SVT_TEST_CHECK_EQ_INT(on.records[2].y, 32);
            SVT_TEST_CHECK_EQ_INT(on.records[3].x, 32); SVT_TEST_CHECK_EQ_INT(on.records[3].y, 32);
        }
        for (size_t sb = 0u; sb < count; ++sb) if (plan[sb]) plan[sb] = 2u;
        on.count = 0;
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_coded_plan(&geometry, plan, count, record_block, &on), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(on.count, old.count + 15u * full); check_coverage(&geometry, &on);
        for (uint32_t leaf = 0u; leaf < on.count; ++leaf) {
            const VisitRecord* block = &on.records[leaf];
            if (plan[(size_t)(block->y / 64u) * geometry.sb_cols + block->x / 64u]) {
                SVT_TEST_CHECK_EQ_INT(block->w, 16u); SVT_TEST_CHECK_EQ_INT(block->h, 16u);
            }
        }
        on.count = 0; plan[0] = 882u;
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_coded_plan(&geometry, plan, count, record_block, &on), SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(on.count, 0); plan[0] = 1u;
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_coded_plan(&geometry, plan, count - 1u, record_block, &on), SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(on.count, 0);
        if (geometry.partial_sb_count) {
            plan[count - 1u] = 1u;
            SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_coded_plan(&geometry, plan, count, record_block, &on), SVT_AV2_ERROR_INVALID_PARAMETER);
            SVT_TEST_CHECK_EQ_INT(on.count, 0);
        }
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_plan_validate(&geometry, NULL, 1u), SVT_AV2_ERROR_INVALID_PARAMETER);
    }
}

static void test_single_sb_traversal(void) {
    SvtAv2FrameGeometry geometry;
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init_ex(&geometry, 136u, 72u, 64u,
        SVT_AV2_CHROMA_420), SVT_AV2_ERROR_NONE);
    uint16_t plan[6] = {1, 2, 0, 0, 0, 0};
    VisitLog whole, separate;
    memset(&whole, 0, sizeof(whole)); memset(&separate, 0, sizeof(separate));
    whole.abort_after = separate.abort_after = -1;
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_coded_plan(&geometry, plan, 6u,
        record_block, &whole), SVT_AV2_ERROR_NONE);
    for (size_t i = 0; i < 6u; ++i)
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_sb_plan(&geometry, i, plan, 6u,
            record_block, &separate), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(whole.count, separate.count);
    SVT_TEST_CHECK(memcmp(whole.records, separate.records, whole.count * sizeof(VisitRecord)) == 0);
    const uint32_t before = separate.count;
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_sb_plan(&geometry, 6u, plan, 6u,
        record_block, &separate), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_sb_plan(NULL, 0u, NULL, 0u,
        record_block, &separate), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(separate.count, before);
}

static void test_mixed_plan_geometry(void) {
    for (uint32_t f = 0u; f < 2u; ++f) {
        SvtAv2FrameGeometry geometry;
        SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init_ex(&geometry, 64u, 64u, 64u,
            f ? SVT_AV2_CHROMA_444 : SVT_AV2_CHROMA_420), SVT_AV2_ERROR_NONE);
        for (uint16_t plan = 3u; plan <= 84u; ++plan) {
            VisitLog log; memset(&log, 0, sizeof(log)); log.abort_after = -1;
            SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_coded_plan(&geometry, &plan, 1u,
                record_block, &log), SVT_AV2_ERROR_NONE);
            uint32_t expected = 0u, digits = plan >= 4u ? plan - 4u : 80u;
            for (uint32_t q = 0u; q < 4u; ++q) {
                const uint32_t d = digits % 3u; digits /= 3u;
                expected += 1u << (2u * d);
            }
            SVT_TEST_CHECK_EQ_INT(log.count, expected);
            check_coverage(&geometry, &log);
            for (uint32_t i = 0u; i < log.count; ++i) {
                const VisitRecord* b = log.records + i;
                const uint32_t quadrant = b->y / 32u * 2u + b->x / 32u;
                uint32_t code = plan >= 4u ? plan - 4u : 80u;
                for (uint32_t q = 0u; q < quadrant; ++q) code /= 3u;
                SVT_TEST_CHECK_EQ_INT(b->w, 32u >> (code % 3u));
                SVT_TEST_CHECK_EQ_INT(b->h, b->w);
            }
        }
    }
}
int main(void) {
    test_mixed_plan_geometry();
    test_single_sb_traversal();
    test_content_selector();
    test_content_plan_geometry();
    test_444_same_coded_tree_and_sample_addresses();
    test_geometry_rejection();
    test_target_sizes();
    test_partition_boundary_rule();
    test_block_views_and_chroma();
    test_chroma_shape_table();
    test_traversal_abort();
    test_neighbors_rejection();
    test_random_blocks_against_model();
    test_coded_boundary_coverage();
    SVT_TEST_REPORT("test_block_geometry");
}
