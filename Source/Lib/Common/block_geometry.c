#ifndef M6_FINE16
#define M6_FINE16 0
#endif
#ifndef M6_FINE16_PAIR
#define M6_FINE16_PAIR 0
#endif
#if M6_FINE16_PAIR && !M6_FINE16
#error M6_FINE16_PAIR_requires_M6_FINE16
#endif
#ifndef M6_FINE16_RETAIN_OLD
#define M6_FINE16_RETAIN_OLD 0
#endif
#ifndef M6_FINE16_REVERSE_CHOICES
#define M6_FINE16_REVERSE_CHOICES 0
#endif
#ifndef M6_ROOT_ASYM
#define M6_ROOT_ASYM 0
#endif

/* SVT-AV2 block geometry (T007). See block_geometry.h for the contract and for
 * the AVM file:line evidence behind each rule. */

#include "block_geometry.h"

#include <string.h>

/* --- small helpers --------------------------------------------------------- */

static uint32_t align_up(uint32_t value, uint32_t log2_align) {
    const uint32_t mask = (1u << log2_align) - 1u;
    return (value + mask) & ~mask;
}

static uint32_t log2_of_power_of_two(uint32_t value) {
    uint32_t log2 = 0;
    while ((1u << log2) < value) {
        ++log2;
    }
    return log2;
}

static int is_supported_sb_size(uint32_t sb_size) {
    return sb_size == SVT_AV2_SB_SIZE_64 || sb_size == SVT_AV2_SB_SIZE_128 ||
           sb_size == SVT_AV2_SB_SIZE_256;
}

static uint32_t min_u32(uint32_t a, uint32_t b) { return (a < b) ? a : b; }

/* --- chroma shape table ---------------------------------------------------- */

/* The block shapes this module can produce while traversing. The chroma shape is
 * taken from the Subsampled_Size table (avm\av2\common\common_data.h:709-743)
 * rather than computed by halving, because the table is the normative source.
 *
 * For 4:2:0 the relevant column of the table is [bsize][1][1]:
 *   BLOCK_64X64 -> BLOCK_32X32   64x64 -> 32x32
 *   BLOCK_32X32 -> BLOCK_16X16   32x32 -> 16x16
 *   BLOCK_16X16 -> BLOCK_8X8     16x16 ->  8x8
 *   BLOCK_8X8   -> BLOCK_4X4      8x8  ->  4x4
 *   BLOCK_64X32 -> BLOCK_32X16   64x32 -> 32x16
 *   BLOCK_32X64 -> BLOCK_16X32   32x64 -> 16x32
 */
typedef struct SvtAv2ChromaShape {
    uint32_t luma_width;
    uint32_t luma_height;
    uint32_t chroma_width;
    uint32_t chroma_height;
} SvtAv2ChromaShape;

static const SvtAv2ChromaShape g_svt_av2_chroma_shapes[] = {
    {8u, 8u, 4u, 4u},   {16u, 16u, 8u, 8u},   {32u, 32u, 16u, 16u}, {64u, 64u, 32u, 32u},
    {64u, 32u, 32u, 16u}, {32u, 64u, 16u, 32u},
    {16u, 8u, 8u, 4u}, {8u, 16u, 4u, 8u},
    {16u, 32u, 8u, 16u}, {32u, 16u, 16u, 8u},
    {16u, 64u, 8u, 32u}, {64u, 16u, 32u, 8u},
    {8u, 32u, 4u, 16u}, {32u, 8u, 16u, 4u},
    {8u, 64u, 4u, 32u}, {64u, 8u, 32u, 4u},
};

static const SvtAv2ChromaShape* lookup_chroma_shape(uint32_t width, uint32_t height) {
    for (size_t i = 0; i < SVT_AV2_ARRAY_SIZE(g_svt_av2_chroma_shapes); ++i) {
        if (g_svt_av2_chroma_shapes[i].luma_width == width && g_svt_av2_chroma_shapes[i].luma_height == height) {
            return &g_svt_av2_chroma_shapes[i];
        }
    }
    return NULL;
}

/* --- frame geometry -------------------------------------------------------- */

SvtAv2Error svt_av2_frame_geometry_init(SvtAv2FrameGeometry* geometry,
                                        uint32_t width,
                                        uint32_t height,
                                        uint32_t sb_size) {
    return svt_av2_frame_geometry_init_ex(geometry, width, height, sb_size, SVT_AV2_CHROMA_420);
}
SvtAv2Error svt_av2_frame_geometry_init_ex(SvtAv2FrameGeometry* geometry,
    uint32_t width, uint32_t height, uint32_t sb_size, SvtAv2ChromaFormat chroma_format) {
    if (geometry == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    memset(geometry, 0, sizeof(*geometry));

    if (width == 0 || height == 0) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (!is_supported_sb_size(sb_size)) {
        return SVT_AV2_ERROR_UNSUPPORTED;
    }
    /* Same rule as the picture allocation: the mi grid and the chroma reference
     * blocks are 8x8 granular. */
    if ((width & 7u) != 0 || (height & 7u) != 0) {
        return SVT_AV2_ERROR_UNSUPPORTED;
    }

    if (chroma_format != SVT_AV2_CHROMA_420 && chroma_format != SVT_AV2_CHROMA_444) return SVT_AV2_ERROR_UNSUPPORTED;
    geometry->chroma_format = chroma_format;
    geometry->visible_width = width;
    geometry->visible_height = height;
    geometry->sb_size = sb_size;
    geometry->mi_width = width >> 2;  /* == ALIGN_POWER_OF_TWO(width, 3) >> 2 */
    geometry->mi_height = height >> 2;
    geometry->chroma_width = chroma_format == SVT_AV2_CHROMA_444 ? width : width >> 1;
    geometry->chroma_height = chroma_format == SVT_AV2_CHROMA_444 ? height : height >> 1;

    /* Superblock counts come from the FRAME's mi size rounded up to the
     * superblock size (tile_common.c:52-57), which is what makes partial
     * superblocks at the right/bottom edge reachable by the traversal. */
    const uint32_t mib_log2 = log2_of_power_of_two(sb_size) - 2u;
    geometry->sb_cols = align_up(geometry->mi_width, mib_log2) >> mib_log2;
    geometry->sb_rows = align_up(geometry->mi_height, mib_log2) >> mib_log2;

    const uint32_t full_sb_cols = geometry->mi_width >> mib_log2;
    const uint32_t full_sb_rows = geometry->mi_height >> mib_log2;
    geometry->partial_sb_count = (geometry->sb_cols * geometry->sb_rows) - (full_sb_cols * full_sb_rows);
    return SVT_AV2_ERROR_NONE;
}

/* --- block views ----------------------------------------------------------- */

SvtAv2Error svt_av2_block_from_sb(const SvtAv2FrameGeometry* geometry,
                                  uint32_t sb_col,
                                  uint32_t sb_row,
                                  SvtAv2BlockView* block) {
    if (geometry == NULL || block == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (sb_col >= geometry->sb_cols || sb_row >= geometry->sb_rows) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    memset(block, 0, sizeof(*block));
    block->x = sb_col * geometry->sb_size;
    block->y = sb_row * geometry->sb_size;
    /* Clipped to the visible area: a partial superblock is only as large as the
     * samples that actually exist. */
    block->w = min_u32(geometry->sb_size, geometry->visible_width - block->x);
    block->h = min_u32(geometry->sb_size, geometry->visible_height - block->y);
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_block_view(const SvtAv2FrameGeometry* geometry,
                               const SvtAv2FrameBuffer* picture,
                               uint32_t plane,
                               uint32_t sb_col,
                               uint32_t sb_row,
                               SvtAv2BlockView* block) {
    if (geometry == NULL || picture == NULL || block == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (geometry->visible_width != picture->visible_width ||
        geometry->visible_height != picture->visible_height) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (plane >= SVT_AV2_PICTURE_MAX_PLANES) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    /* Build the luma block first: the superblock grid is defined on luma, and
     * both chroma planes share its geometry. */
    SvtAv2BlockView luma;
    const SvtAv2Error err = svt_av2_block_from_sb(geometry, sb_col, sb_row, &luma);
    if (err != SVT_AV2_ERROR_NONE) {
        return err;
    }

    if (plane == SVT_AV2_PLANE_Y) {
        *block = luma;
    } else {
        const SvtAv2Error chroma_err = svt_av2_chroma_view(geometry, &luma, block);
        if (chroma_err != SVT_AV2_ERROR_NONE) {
            return chroma_err;
        }
    }

    uint8_t* const base = svt_av2_picture_plane((SvtAv2FrameBuffer*)picture, plane);
    if (base == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    const uint32_t stride = picture->strides[plane];
    block->base = base + (size_t)block->y * stride + block->x * (picture->bit_depth > 8 ? 2u : 1u);
    return SVT_AV2_ERROR_NONE;
}

/* --- partition rules ------------------------------------------------------- */

SvtAv2Error svt_av2_block_boundary_flags(const SvtAv2FrameGeometry* geometry,
                                         uint32_t sb_col,
                                         uint32_t sb_row,
                                         int* has_left,
                                         int* has_above) {
    SvtAv2BlockView block;
    const SvtAv2Error err = svt_av2_block_from_sb(geometry, sb_col, sb_row, &block);
    if (err != SVT_AV2_ERROR_NONE) {
        return err;
    }
    if (has_left != NULL) {
        *has_left = (block.x > 0) ? 1 : 0;
    }
    if (has_above != NULL) {
        *has_above = (block.y > 0) ? 1 : 0;
    }
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_partition_implied_at_boundary(const SvtAv2FrameGeometry* geometry,
                                                  uint32_t sb_col,
                                                  uint32_t sb_row,
                                                  SvtAv2PartitionType* implied) {
    if (implied == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    SvtAv2BlockView block;
    const SvtAv2Error err = svt_av2_block_from_sb(geometry, sb_col, sb_row, &block);
    if (err != SVT_AV2_ERROR_NONE) {
        return err;
    }

    /* AV2 spec v1.0.0 5.20.3.2, partition_implied_at_boundary(r, c, bSize):
     *
     *     numWide4x4 = Num_4x4_Blocks_Wide[ bSize ]
     *     numHigh4x4 = Num_4x4_Blocks_High[ bSize ]
     *     hasRows = ( r + (numHigh4x4 >> 1) ) < MiRows
     *     hasCols = ( c + (numWide4x4 >> 1) ) < MiCols
     *     if ( hasRows && hasCols ) return (0, PARTITION_NONE)
     *     if ( numWide4x4 == numHigh4x4 ) implied = hasRows ? VERT : HORZ
     *
     * `numX4x4 >> 1` is exactly half the block in mode-info units, i.e. hbs, and
     * Num_4x4_Blocks_Wide[BLOCK_64X64] == 16 so hbs == 8 mi for a 64 superblock.
     *
     * The comparison uses the CODED superblock extent in mi units, NOT the
     * visible (clipped) block: the spec measures bSize, so a partial superblock
     * is evaluated as if it were whole. Measuring the clipped extent instead
     * would wrongly report partial superblocks as interior.
     *
     * This module only traverses square superblocks, so only the
     * numWide4x4 == numHigh4x4 branch applies. Note that the enclosing
     * partition_implied(r, c, bSize) has an earlier guard
     * `if (bSize == BLOCK_4X4 || bSize >= BLOCK_4X32) return (1, NONE)` which
     * does NOT fire here: BLOCK_64X64 precedes BLOCK_4X32 in the block size
     * enum (avm av2/common/enums.h:398 vs :411).
     */
    const uint32_t sb_mi = geometry->sb_size / SVT_AV2_MI_SIZE;
    const uint32_t hbs_mi = sb_mi / 2u;
    const uint32_t mi_col = sb_col * sb_mi;
    const uint32_t mi_row = sb_row * sb_mi;

    const int has_rows = (mi_row + hbs_mi) < geometry->mi_height;
    const int has_cols = (mi_col + hbs_mi) < geometry->mi_width;

    if (has_rows && has_cols) {
        *implied = SVT_AV2_PARTITION_NONE;
        return SVT_AV2_ERROR_NONE;
    }
    /* Square block: missing columns only forces VERT, anything else forces
     * HORZ. (AVM asserts !has_rows || !has_cols, which holds because hbs_w ==
     * hbs_h for a square.) */
    *implied = (has_rows && !has_cols) ? SVT_AV2_PARTITION_VERT : SVT_AV2_PARTITION_HORZ;
    return SVT_AV2_ERROR_NONE;
}

/* --- chroma mapping -------------------------------------------------------- */

SvtAv2Error svt_av2_chroma_view(const SvtAv2FrameGeometry* geometry,
                                const SvtAv2BlockView* block,
                                SvtAv2BlockView* chroma_view) {
    if (geometry == NULL || block == NULL || chroma_view == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (block->w == 0 || block->h == 0) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    if (geometry->chroma_format == SVT_AV2_CHROMA_444) {
        *chroma_view = *block;
        chroma_view->base = NULL;
        return SVT_AV2_ERROR_NONE;
    }

    /* Truncated halving, matching mi_to_pixel_loc (blockd.h:1466-1473) and the
     * CCSO unit arithmetic (decodemv.c:174). Never rounded up to 8: that would
     * shift a block's chroma window onto the wrong samples. */
    const uint32_t x = block->x >> 1;
    const uint32_t y = block->y >> 1;

    /* The chroma block SHAPE comes from the coded block size, not from the
     * visible extent: a right/bottom superblock is clipped to the samples that
     * exist, and 48x16 is not a coded block shape at all.
     *
     * Only blocks BELOW the minimum coding size are rounded back up, and only
     * for the reason that they can be a clipped superblock. A block whose
     * smaller edge is already at least the minimum coding size but whose shape
     * is not in the table is a genuinely odd shape and must be rejected rather
     * than widened, because widening would silently change which chroma samples
     * the block owns. */
    const uint32_t max_edge = (block->w > block->h) ? block->w : block->h;
    const SvtAv2ChromaShape* shape = lookup_chroma_shape(block->w, block->h);
    if (shape == NULL && max_edge >= SVT_AV2_MIN_CODING_BLOCK_SIZE) {
        /* Recover the square shape the superblock was coded with. */
        for (uint32_t candidate = SVT_AV2_MIN_CODING_BLOCK_SIZE; candidate <= geometry->sb_size;
             candidate <<= 1) {
            if (candidate < max_edge) {
                continue;
            }
            const SvtAv2ChromaShape* const square = lookup_chroma_shape(candidate, candidate);
            if (square != NULL) {
                shape = square;
                break;
            }
        }
    }
    if (shape == NULL) {
        /* Shapes this traversal never produces (e.g. a 4x4 luma block, which the
         * decoder forces into a shared chroma reference). Reject instead of
         * guessing: a wrong chroma window is worse than a clear failure. */
        return SVT_AV2_ERROR_UNSUPPORTED;
    }

    uint32_t w = shape->chroma_width;
    uint32_t h = shape->chroma_height;

    if (x >= geometry->chroma_width || y >= geometry->chroma_height) {
        return SVT_AV2_ERROR_UNSUPPORTED;
    }

    memset(chroma_view, 0, sizeof(*chroma_view));
    chroma_view->x = x;
    chroma_view->y = y;
    chroma_view->w = min_u32(w, geometry->chroma_width - x);
    chroma_view->h = min_u32(h, geometry->chroma_height - y);
    return SVT_AV2_ERROR_NONE;
}

/* --- traversal ------------------------------------------------------------- */

/* Splits a boundary superblock following the implied rule and calls `visit` on
 * each resulting sub-block. These blocks carry no syntax, so they are reported
 * directly and never recursed into again.
 *
 * The area covered is exactly the visible part of the superblock, so the walk
 * stays inside the picture by construction. */
static SvtAv2Error visit_boundary_superblock(const SvtAv2BlockView* block,
                                             SvtAv2PartitionType implied,
                                             SvtAv2BlockVisitFn visit,
                                             void* user) {
    uint32_t sub_w = block->w;
    uint32_t sub_h = block->h;
    uint32_t sub_cols = 1;
    uint32_t sub_rows = 1;
    if (implied == SVT_AV2_PARTITION_VERT) {
        sub_cols = 2;
        sub_w = block->w / 2u;
    } else {
        sub_rows = 2;
        sub_h = block->h / 2u;
    }
    if (sub_w == 0 || sub_h == 0) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    for (uint32_t row = 0; row < sub_rows; ++row) {
        for (uint32_t col = 0; col < sub_cols; ++col) {
            SvtAv2BlockView sub;
            memset(&sub, 0, sizeof(sub));
            sub.x = block->x + col * sub_w;
            sub.y = block->y + row * sub_h;
            /* The last sub-block absorbs the remainder so the union is exactly
             * the visible superblock area, with no gap and no overlap. */
            sub.w = ((col + 1u) == sub_cols) ? (block->w - col * sub_w) : sub_w;
            sub.h = ((row + 1u) == sub_rows) ? (block->h - row * sub_h) : sub_h;
            sub.base = NULL; /* geometry only; callers map planes themselves */
            if (visit(user, &sub) != 0) {
                return SVT_AV2_ERROR_INVALID_STATE;
            }
        }
    }
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_partition_traverse(const SvtAv2FrameGeometry* geometry,
                                       SvtAv2BlockVisitFn visit,
                                       void* user) {
    if (geometry == NULL || visit == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (geometry->visible_width == 0 || geometry->visible_height == 0) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    for (uint32_t sb_row = 0; sb_row < geometry->sb_rows; ++sb_row) {
        for (uint32_t sb_col = 0; sb_col < geometry->sb_cols; ++sb_col) {
            SvtAv2BlockView block;
            const SvtAv2Error err = svt_av2_block_from_sb(geometry, sb_col, sb_row, &block);
            if (err != SVT_AV2_ERROR_NONE) {
                return err;
            }

            SvtAv2PartitionType implied = SVT_AV2_PARTITION_NONE;
            const SvtAv2Error implied_err =
                svt_av2_partition_implied_at_boundary(geometry, sb_col, sb_row, &implied);
            if (implied_err != SVT_AV2_ERROR_NONE) {
                return implied_err;
            }

            if (implied != SVT_AV2_PARTITION_NONE) {
                const SvtAv2Error split_err = visit_boundary_superblock(&block, implied, visit, user);
                if (split_err != SVT_AV2_ERROR_NONE) {
                    return split_err;
                }
                continue;
            }

            if (visit(user, &block) != 0) {
                return SVT_AV2_ERROR_INVALID_STATE;
            }
        }
    }
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_partition_coded_type(const SvtAv2FrameGeometry* geometry,
                                        const SvtAv2BlockView* block,
                                        SvtAv2PartitionType* partition,
                                        int* is_implied) {
    if (geometry == NULL || block == NULL || partition == NULL || is_implied == NULL ||
        block->w < 8u || block->h < 8u || block->x >= geometry->visible_width ||
        block->y >= geometry->visible_height) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    /* Fixed v1 is_partition_point(): 8:1 leaves are implicit NONE. */
    if ((block->w == 8u && block->h == 64u) || (block->w == 64u && block->h == 8u)) {
        *partition = SVT_AV2_PARTITION_NONE;
        *is_implied = 1;
        return SVT_AV2_ERROR_NONE;
    }
    const int has_rows = block->y + block->h / 2u < geometry->visible_height;
    const int has_cols = block->x + block->w / 2u < geometry->visible_width;
    *is_implied = 0;
    if (!has_rows || !has_cols) {
        if (block->w == block->h) {
            *partition = has_rows ? SVT_AV2_PARTITION_VERT : SVT_AV2_PARTITION_HORZ;
            *is_implied = 1;
        } else if (block->h > block->w) {
            if (!has_rows || (block->w >= 16u &&
                             block->x + block->w / 4u >= geometry->visible_width)) {
                *partition = SVT_AV2_PARTITION_HORZ;
                *is_implied = 1;
            }
        } else if (!has_cols || (block->h >= 16u &&
                                block->y + block->h / 4u >= geometry->visible_height)) {
            *partition = SVT_AV2_PARTITION_VERT;
            *is_implied = 1;
        }
    }
    if (!*is_implied) {
        if (block->w > block->h) {
            *partition = SVT_AV2_PARTITION_VERT;
        } else if (block->h > block->w) {
            *partition = SVT_AV2_PARTITION_HORZ;
        } else if (block->w > geometry->visible_width - block->x ||
                   block->h > geometry->visible_height - block->y) {
            *partition = SVT_AV2_PARTITION_HORZ;
        } else {
            *partition = SVT_AV2_PARTITION_NONE;
        }
    }
    return SVT_AV2_ERROR_NONE;
}

static int valid_plan_geometry(const SvtAv2FrameGeometry* geometry) {
    return geometry != NULL && geometry->sb_size == 64u &&
        geometry->visible_width != 0u && geometry->visible_height != 0u &&
        (geometry->visible_width & 7u) == 0u && (geometry->visible_height & 7u) == 0u &&
        geometry->sb_cols == (geometry->visible_width - 1u) / 64u + 1u &&
        geometry->sb_rows == (geometry->visible_height - 1u) / 64u + 1u;
}
uint16_t svt_av2_partition_mixed7_plan(uint32_t code) {
    static const uint16_t uniform[7] = {2u,3u,85u,86u,1u,880u,881u};
    if (code >= 2401u) return UINT16_MAX;
    uint32_t digits = code, old_code = 0u, old_power = 1u, square_code = 4u, power = 1u;
    const uint32_t first = code % 7u;
    int old = 1, squares = 1, same = 1;
    for (uint32_t q = 0u; q < 4u; ++q) {
        const uint32_t shape = digits % 7u; digits /= 7u;
        const uint32_t depth = shape == 4u ? 1u : shape == 0u ? 2u : 3u;
        if (shape >= 5u) old = 0;
        if (shape == 2u || shape == 3u) squares = 0;
        if (shape != first) same = 0;
        old_code += shape * old_power; old_power *= 5u;
        square_code += (depth - 1u) * power; power *= 3u;
    }
    if (same) return uniform[first];
    if (old) return (uint16_t)(squares ? square_code : 255u + old_code);
    return (uint16_t)(882u + code);
}
SvtAv2Error svt_av2_partition_plan_validate_sb(const SvtAv2FrameGeometry* geometry,
    size_t sb_index, uint16_t decision) {
    if (!valid_plan_geometry(geometry) ||
        sb_index >= (size_t)geometry->sb_cols * geometry->sb_rows || (decision > (M6_ROOT_ASYM ? 3288u : 3284u) && (!M6_FINE16 || decision<4096u || decision>11095u) && (!M6_FINE16_PAIR || decision<11096u || decision>40495u)) ||
        (decision >= 882u && decision <= 3282u &&
         svt_av2_partition_mixed7_plan((uint32_t)decision - 882u) != decision))
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    const uint32_t col = (uint32_t)(sb_index % geometry->sb_cols);
    const uint32_t row = (uint32_t)(sb_index / geometry->sb_cols);
    if (decision && (geometry->visible_width - col * 64u < 64u ||
        geometry->visible_height - row * 64u < 64u)) return SVT_AV2_ERROR_INVALID_PARAMETER;
    return SVT_AV2_ERROR_NONE;
}
SvtAv2Error svt_av2_partition_plan_validate(const SvtAv2FrameGeometry* geometry,
    const uint16_t* decisions, size_t count) {
    if (!valid_plan_geometry(geometry)) return SVT_AV2_ERROR_INVALID_PARAMETER;
    if (decisions == NULL) return count == 0u ? SVT_AV2_ERROR_NONE : SVT_AV2_ERROR_INVALID_PARAMETER;
    const size_t sb_count = (size_t)geometry->sb_cols * geometry->sb_rows;
    if (count < sb_count) return SVT_AV2_ERROR_INVALID_PARAMETER;
    for (size_t sb = 0u; sb < sb_count; ++sb)
        if (svt_av2_partition_plan_validate_sb(geometry, sb, decisions[sb]) != SVT_AV2_ERROR_NONE)
            return SVT_AV2_ERROR_INVALID_PARAMETER;
    return SVT_AV2_ERROR_NONE;
}
SvtAv2Error svt_av2_partition_select_quads(const SvtAv2FrameBuffer* source,
    const SvtAv2FrameGeometry* geometry, uint16_t* decisions, size_t capacity) {
    if (!valid_plan_geometry(geometry) || source == NULL || decisions == NULL ||
        source->planes[0] == NULL || source->visible_width != geometry->visible_width ||
        source->visible_height != geometry->visible_height || source->chroma_format != geometry->chroma_format ||
        (source->bit_depth != 8u && source->bit_depth != 10u) ||
        (source->chroma_format != SVT_AV2_CHROMA_420 && source->chroma_format != SVT_AV2_CHROMA_444) ||
        capacity < (size_t)geometry->sb_cols * geometry->sb_rows)
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    const uint32_t step = source->bit_depth > 8u ? 2u : 1u;
    if ((uint64_t)source->strides[0] < (uint64_t)source->visible_width * step)
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    const uint64_t penalty = (UINT64_C(4096) * 4096u * 16u) << (2u * (source->bit_depth - 8u));
    for (uint32_t row = 0; row < geometry->sb_rows; ++row)
        for (uint32_t col = 0; col < geometry->sb_cols; ++col) {
            const size_t index = (size_t)row * geometry->sb_cols + col;
            decisions[index] = 0u;
            if (geometry->visible_width - col * 64u < 64u || geometry->visible_height - row * 64u < 64u)
                continue;
            uint64_t sums[4] = {0u, 0u, 0u, 0u};
            for (uint32_t y = 0; y < 64u; ++y) {
                const uint8_t* samples = source->planes[0] + (size_t)(row * 64u + y) * source->strides[0] +
                    (size_t)col * 64u * step;
                for (uint32_t x = 0; x < 64u; ++x)
                    sums[(y / 32u) * 2u + x / 32u] +=
                        svt_av2_sample_load(samples + x * step, source->bit_depth);
            }
            uint64_t sum = 0u, squares = 0u;
            for (uint32_t i = 0; i < 4u; ++i) { sum += sums[i]; squares += sums[i] * sums[i]; }
            decisions[index] = (uint8_t)(4u * squares - sum * sum > penalty);
        }
    return SVT_AV2_ERROR_NONE;
}
uint32_t svt_av2_partition_plan_depth(uint16_t decision, uint32_t quadrant) {
    static const uint32_t powers[4] = {1u, 3u, 9u, 27u};
    return decision <= 3u ? decision : 1u + ((uint32_t)decision - 4u) / powers[quadrant] % 3u;
}
SvtAv2Error svt_av2_partition_coded_type_plan(const SvtAv2FrameGeometry* geometry,
    const SvtAv2BlockView* block, const uint16_t* decisions, size_t count,
    SvtAv2PartitionType* partition, int* is_implied) {
    SvtAv2Error error = svt_av2_partition_plan_validate(geometry, decisions, count);
    if (error != SVT_AV2_ERROR_NONE) return error;
    return svt_av2_partition_coded_type_plan_validated(geometry, block, decisions, partition, is_implied);
}

SvtAv2Error svt_av2_partition_coded_type_plan_validated(const SvtAv2FrameGeometry* geometry,
    const SvtAv2BlockView* block, const uint16_t* decisions,
    SvtAv2PartitionType* partition, int* is_implied) {
    SvtAv2Error error = svt_av2_partition_coded_type(geometry, block, partition, is_implied);
    if (error != SVT_AV2_ERROR_NONE) return error;
    const uint16_t decision = decisions == NULL ? 0u :
        decisions[(size_t)(block->y / 64u) * geometry->sb_cols + block->x / 64u];
#if M6_FINE16_PAIR
    if (decision>=11096u && decision<=40495u) {
        static const uint32_t pairs[6][2]={{0u,1u},{0u,2u},{0u,3u},{1u,2u},{1u,3u},{2u,3u}};
        const uint32_t code=(uint32_t)decision-11096u;
        const uint32_t pair=code/(25u*14u*14u),lo=pairs[pair][0],hi=pairs[pair][1];
        const uint32_t low_mask=code/(25u*14u)%14u+1u,high_mask=code/25u%14u+1u;
        const uint32_t q=(block->y%64u/32u)*2u+block->x%64u/32u;
        if (block->w>32u || block->h>32u) {
            *partition=block->w>block->h?SVT_AV2_PARTITION_VERT:SVT_AV2_PARTITION_HORZ;
        } else if (q==lo || q==hi) {
            if (block->w>16u || block->h>16u) {
                *partition=block->w>block->h?SVT_AV2_PARTITION_VERT:SVT_AV2_PARTITION_HORZ;
            } else {
                const uint32_t cell=(block->y%32u/16u)*2u+block->x%32u/16u;
                const uint32_t mask=q==lo?low_mask:high_mask;
                const uint32_t size=(mask&(1u<<cell))?8u:16u;
                if (block->w==size && block->h==size) *partition=SVT_AV2_PARTITION_NONE;
                else if (block->w<size || block->h<size) return SVT_AV2_ERROR_INVALID_PARAMETER;
                else *partition=block->w>block->h?SVT_AV2_PARTITION_VERT:SVT_AV2_PARTITION_HORZ;
            }
        } else {
            static const uint32_t widths[5]={16u,8u,16u,8u,32u};
            static const uint32_t heights[5]={16u,8u,8u,16u,32u};
            const uint32_t slot=q-(q>lo)-(q>hi);
            const uint32_t shape=code%25u/(slot?5u:1u)%5u;
            const uint32_t w=widths[shape],h=heights[shape];
            if (block->w==w && block->h==h) *partition=SVT_AV2_PARTITION_NONE;
            else if (block->w==w) *partition=SVT_AV2_PARTITION_HORZ;
            else if (block->h==h) *partition=SVT_AV2_PARTITION_VERT;
            else *partition=block->w>block->h?SVT_AV2_PARTITION_VERT:SVT_AV2_PARTITION_HORZ;
        }
        *is_implied=0;return SVT_AV2_ERROR_NONE;
    }
#endif
#if M6_FINE16
    if (decision>=4096u && decision<=11095u) {
        const uint32_t code=(uint32_t)decision-4096u;
        const uint32_t special=(code/125u)/14u;
        const uint32_t mask=(code/125u)%14u+1u;
        const uint32_t q=(block->y%64u/32u)*2u+block->x%64u/32u;
        if (block->w>32u || block->h>32u) {
            *partition=block->w>block->h?SVT_AV2_PARTITION_VERT:SVT_AV2_PARTITION_HORZ;
        } else if (q==special) {
            if (block->w>16u || block->h>16u) {
                *partition=block->w>block->h?SVT_AV2_PARTITION_VERT:SVT_AV2_PARTITION_HORZ;
            } else {
                const uint32_t cell=(block->y%32u/16u)*2u+block->x%32u/16u;
                const uint32_t size=(mask&(1u<<cell))?8u:16u;
                if (block->w==size && block->h==size) *partition=SVT_AV2_PARTITION_NONE;
                else if (block->w<size || block->h<size) return SVT_AV2_ERROR_INVALID_PARAMETER;
                else *partition=block->w>block->h?SVT_AV2_PARTITION_VERT:SVT_AV2_PARTITION_HORZ;
            }
        } else {
            static const uint32_t widths[5]={16u,8u,16u,8u,32u};
            static const uint32_t heights[5]={16u,8u,8u,16u,32u};
            static const uint32_t powers[3]={1u,5u,25u};
            const uint32_t slot=q-(q>special);
            const uint32_t shape=(code%125u)/powers[slot]%5u;
            const uint32_t w=widths[shape],h=heights[shape];
            if (block->w==w && block->h==h) *partition=SVT_AV2_PARTITION_NONE;
            else if (block->w==w) *partition=SVT_AV2_PARTITION_HORZ;
            else if (block->h==h) *partition=SVT_AV2_PARTITION_VERT;
            else *partition=block->w>block->h?SVT_AV2_PARTITION_VERT:SVT_AV2_PARTITION_HORZ;
        }
        *is_implied=0;
        return SVT_AV2_ERROR_NONE;
    }
#endif

#if M6_ROOT_ASYM
    if (decision >= 3285u) {
        const uint32_t vertical = ((uint32_t)decision - 3285u) / 2u;
        const uint32_t keep_half = ((uint32_t)decision - 3285u) % 2u;
        if (block->w == 64u && block->h == 64u)
            *partition = vertical ? SVT_AV2_PARTITION_VERT : SVT_AV2_PARTITION_HORZ;
        else if ((!vertical && block->w == 64u && block->h == 32u) ||
                 (vertical && block->w == 32u && block->h == 64u)) {
            const uint32_t half = vertical ? block->x % 64u / 32u : block->y % 64u / 32u;
            *partition = half == keep_half ? SVT_AV2_PARTITION_NONE :
                vertical ? SVT_AV2_PARTITION_HORZ : SVT_AV2_PARTITION_VERT;
        } else if (block->w == 32u && block->h == 32u)
            *partition = SVT_AV2_PARTITION_NONE;
        else return SVT_AV2_ERROR_INVALID_PARAMETER;
        *is_implied = 0;
        return SVT_AV2_ERROR_NONE;
    }
#endif
    if (decision >= 3283u) {
        const uint32_t target_w = decision == 3283u ? 8u : 64u;
        const uint32_t target_h = decision == 3283u ? 64u : 8u;
        const int leaf = block->w == target_w && block->h == target_h;
        *partition = leaf ? SVT_AV2_PARTITION_NONE :
            decision == 3283u ? SVT_AV2_PARTITION_VERT : SVT_AV2_PARTITION_HORZ;
        *is_implied = leaf;
        return SVT_AV2_ERROR_NONE;
    }
    if (decision >= 882u) {
        static const uint32_t powers[4] = {1u,7u,49u,343u};
        static const uint32_t widths[7] = {16u,8u,16u,8u,32u,8u,32u};
        static const uint32_t heights[7] = {16u,8u,8u,16u,32u,32u,8u};
        const uint32_t quadrant = (block->y % 64u / 32u) * 2u + block->x % 64u / 32u;
        const uint32_t shape = ((uint32_t)decision - 882u) / powers[quadrant] % 7u;
        const uint32_t target_w = widths[shape], target_h = heights[shape];
        if (block->w > 32u || block->h > 32u)
            *partition = block->w > block->h ? SVT_AV2_PARTITION_VERT : SVT_AV2_PARTITION_HORZ;
        else if (block->w == target_w && block->h == target_h) *partition = SVT_AV2_PARTITION_NONE;
        else if (block->w == target_w) *partition = SVT_AV2_PARTITION_HORZ;
        else if (block->h == target_h) *partition = SVT_AV2_PARTITION_VERT;
        else *partition = block->w > block->h ? SVT_AV2_PARTITION_VERT : SVT_AV2_PARTITION_HORZ;
        *is_implied = 0;
        return SVT_AV2_ERROR_NONE;
    }
    if (decision >= 880u) {
        const uint32_t target_w = decision == 880u ? 8u : 32u;
        const uint32_t target_h = decision == 880u ? 32u : 8u;
        if (block->w == target_w && block->h == target_h) *partition = SVT_AV2_PARTITION_NONE;
        else if (block->w == target_w) *partition = SVT_AV2_PARTITION_HORZ;
        else if (block->h == target_h) *partition = SVT_AV2_PARTITION_VERT;
        else *partition = block->w > block->h ? SVT_AV2_PARTITION_VERT : SVT_AV2_PARTITION_HORZ;
        *is_implied = 0;
        return SVT_AV2_ERROR_NONE;
    }
    if (decision >= 255u) {
        static const uint32_t powers[4] = {1u,5u,25u,125u};
        static const uint32_t widths[5] = {16u,8u,16u,8u,32u};
        static const uint32_t heights[5] = {16u,8u,8u,16u,32u};
        const uint32_t quadrant = (block->y % 64u / 32u) * 2u + block->x % 64u / 32u;
        const uint32_t shape = ((uint32_t)decision - 255u) / powers[quadrant] % 5u;
        const uint32_t target_w = widths[shape], target_h = heights[shape];
        if (block->w > 32u || block->h > 32u)
            *partition = block->w > block->h ? SVT_AV2_PARTITION_VERT : SVT_AV2_PARTITION_HORZ;
        else if (block->w == target_w && block->h == target_h) *partition = SVT_AV2_PARTITION_NONE;
        else if (block->w == target_w) *partition = SVT_AV2_PARTITION_HORZ;
        else if (block->h == target_h) *partition = SVT_AV2_PARTITION_VERT;
        else *partition = block->w > block->h ? SVT_AV2_PARTITION_VERT : SVT_AV2_PARTITION_HORZ;
        *is_implied = 0;
        return SVT_AV2_ERROR_NONE;
    }
    if (decision >= 93u) {
        static const uint32_t powers[4] = {1u, 3u, 9u, 27u};
        const uint32_t quadrant = (block->y % 64u / 32u) * 2u + block->x % 64u / 32u;
        const uint32_t code = decision < 174u ? (uint32_t)decision - 93u : (uint32_t)decision - 174u;
        const uint32_t shape = code / powers[quadrant] % 3u;
        if (block->w > 32u || block->h > 32u)
            *partition = block->w > block->h ? SVT_AV2_PARTITION_VERT : SVT_AV2_PARTITION_HORZ;
        else if (block->w == 32u && block->h == 32u)
            *partition = shape == 1u ? SVT_AV2_PARTITION_VERT :
                shape == 2u || decision >= 174u ? SVT_AV2_PARTITION_HORZ : SVT_AV2_PARTITION_NONE;
        else if (shape == 0u && block->w != block->h)
            *partition = SVT_AV2_PARTITION_VERT;
        else *partition = SVT_AV2_PARTITION_NONE;
        *is_implied = 0;
        return SVT_AV2_ERROR_NONE;
    }
    if (decision >= 85u) {
        static const uint8_t widths[8] = {16u,8u,32u,64u,16u,32u,16u,64u};
        static const uint8_t heights[8] = {8u,16u,64u,32u,32u,16u,64u,16u};
        const uint32_t target_w = widths[decision - 85u];
        const uint32_t target_h = heights[decision - 85u];
        if (block->w == target_w && block->h == target_h) *partition = SVT_AV2_PARTITION_NONE;
        else if (block->w == target_w) *partition = SVT_AV2_PARTITION_HORZ;
        else if (block->h == target_h) *partition = SVT_AV2_PARTITION_VERT;
        else *partition = block->w > block->h ? SVT_AV2_PARTITION_VERT : SVT_AV2_PARTITION_HORZ;
        *is_implied = 0;
        return SVT_AV2_ERROR_NONE;
    }
    const uint32_t quadrant = (block->y % 64u / 32u) * 2u + block->x % 64u / 32u;
    if (decisions != NULL && block->w == block->h &&
        block->w > (64u >> svt_av2_partition_plan_depth(
            decision, quadrant))) {
        *partition = SVT_AV2_PARTITION_HORZ;
        *is_implied = 0;
    }
    return SVT_AV2_ERROR_NONE;
}

static SvtAv2Error visit_coded_block(const SvtAv2FrameGeometry* geometry,
                                    const SvtAv2BlockView* block,
                                    const uint16_t* decisions,
                                    SvtAv2BlockVisitFn visit,
                                    void* user) {
    if (block->x >= geometry->visible_width || block->y >= geometry->visible_height) {
        return SVT_AV2_ERROR_NONE;
    }
    SvtAv2PartitionType partition;
    int is_implied;
    SvtAv2Error err = svt_av2_partition_coded_type_plan_validated(geometry, block, decisions, &partition, &is_implied);
    if (err != SVT_AV2_ERROR_NONE) {
        return err;
    }
    if (partition == SVT_AV2_PARTITION_NONE) {
        return visit(user, block) == 0 ? SVT_AV2_ERROR_NONE : SVT_AV2_ERROR_INVALID_STATE;
    }
    const uint32_t cols = partition == SVT_AV2_PARTITION_HORZ ? 1u : 2u;
    const uint32_t rows = partition == SVT_AV2_PARTITION_VERT ? 1u : 2u;
    for (uint32_t row = 0; row < rows; ++row) {
        for (uint32_t col = 0; col < cols; ++col) {
            SvtAv2BlockView child = *block;
            child.w /= cols;
            child.h /= rows;
            child.x += col * child.w;
            child.y += row * child.h;
            err = visit_coded_block(geometry, &child, decisions, visit, user);
            if (err != SVT_AV2_ERROR_NONE) {
                return err;
            }
        }
    }
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_partition_traverse_coded(const SvtAv2FrameGeometry* geometry,
                                            SvtAv2BlockVisitFn visit,
                                            void* user) {
    return svt_av2_partition_traverse_coded_plan(geometry, NULL, 0u, visit, user);
}
SvtAv2Error svt_av2_partition_traverse_coded_plan(const SvtAv2FrameGeometry* geometry,
    const uint16_t* decisions, size_t count, SvtAv2BlockVisitFn visit, void* user) {
    if (visit == NULL) return SVT_AV2_ERROR_INVALID_PARAMETER;
    const SvtAv2Error validation = svt_av2_partition_plan_validate(geometry, decisions, count);
    if (validation != SVT_AV2_ERROR_NONE) return validation;
    for (uint32_t row = 0; row < geometry->sb_rows; ++row) {
        for (uint32_t col = 0; col < geometry->sb_cols; ++col) {
            SvtAv2BlockView block = {col * 64u, row * 64u, 64u, 64u, NULL};
            const SvtAv2Error err = visit_coded_block(geometry, &block, decisions, visit, user);
            if (err != SVT_AV2_ERROR_NONE) {
                return err;
            }
        }
    }
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_partition_traverse_sb_plan(const SvtAv2FrameGeometry* geometry,
    size_t sb_index, const uint16_t* decisions, size_t count,
    SvtAv2BlockVisitFn visit, void* user) {
    if (visit == NULL) return SVT_AV2_ERROR_INVALID_PARAMETER;
    const SvtAv2Error validation = svt_av2_partition_plan_validate(geometry, decisions, count);
    if (validation != SVT_AV2_ERROR_NONE) return validation;
    if (sb_index >= (size_t)geometry->sb_cols * geometry->sb_rows)
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    SvtAv2BlockView block = {(uint32_t)(sb_index % geometry->sb_cols) * 64u,
        (uint32_t)(sb_index / geometry->sb_cols) * 64u, 64u, 64u, NULL};
    return visit_coded_block(geometry, &block, decisions, visit, user);
}

/* --- neighbours ------------------------------------------------------------ */

SvtAv2Error svt_av2_block_neighbors(const SvtAv2FrameBuffer* picture,
                                    uint32_t plane,
                                    const SvtAv2BlockView* block,
                                    SvtAv2BlockNeighbors* neighbors) {
    if (picture == NULL || block == NULL || neighbors == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (plane >= SVT_AV2_PICTURE_MAX_PLANES || block->w == 0 || block->h == 0) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    const uint8_t* const base = svt_av2_picture_plane_const(picture, plane);
    if (base == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    const uint32_t plane_width = svt_av2_picture_plane_width(picture, plane);
    const uint32_t plane_height = svt_av2_picture_plane_height(picture, plane);
    if (block->x + block->w > plane_width || block->y + block->h > plane_height) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    const uint32_t stride = picture->strides[plane];
    const uint32_t step = picture->bit_depth > 8 ? 2u : 1u;
    memset(neighbors, 0, sizeof(*neighbors));

    /* Availability is a property of the frame boundary only. For an even luma
     * coordinate x > 0 the previous chroma column (luma [x-2, x)) and this
     * block's first chroma column (luma [x, x+2)) are distinct, so chroma needs
     * no separate flag. */
    neighbors->above_valid = (block->y > 0) ? 1 : 0;
    neighbors->left_valid = (block->x > 0) ? 1 : 0;

    if (neighbors->above_valid && neighbors->left_valid) {
        const uint8_t* const corner = base + (size_t)(block->y - 1u) * stride + (block->x - 1u) * step;
        neighbors->above[0] = svt_av2_sample_load(corner, picture->bit_depth);
        neighbors->above[1] = svt_av2_sample_load(corner + step, picture->bit_depth);
        neighbors->left[0] = svt_av2_sample_load(corner, picture->bit_depth);
        neighbors->left[1] = svt_av2_sample_load(corner + stride, picture->bit_depth);
    } else if (neighbors->above_valid) {
        /* No top-left corner exists: only the row directly above is available. */
        const uint8_t* const above = base + (size_t)(block->y - 1u) * stride + block->x * step;
        neighbors->above[0] = svt_av2_sample_load(above, picture->bit_depth);
        neighbors->above[1] = svt_av2_sample_load(above + step, picture->bit_depth);
    } else if (neighbors->left_valid) {
        /* No top-left corner exists: only the column directly to the left. */
        const uint8_t* const left = base + (size_t)block->y * stride + (block->x - 1u) * step;
        neighbors->left[0] = svt_av2_sample_load(left, picture->bit_depth);
        neighbors->left[1] = svt_av2_sample_load(left + stride, picture->bit_depth);
    }

    /* Both unavailable is a legal input state, not an error: the caller applies
     * the normative unavailable-neighbour rule for its prediction mode. */
    return SVT_AV2_ERROR_NONE;
}
