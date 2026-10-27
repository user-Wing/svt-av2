/*
 * SVT-AV2 - minimal AV2 frame header writer (T005). See frame_header.h for the
 * frozen M1 bit layout and the spec references.
 */
#include "frame_header.h"

#define SVT_AV2_FRAME_DEFAULT_BASE_Q_IDX 100u

/* MAX_TILE_COLS / MAX_TILE_ROWS (spec 5.18.7.3; AVM avm/av2/common/enums.h:193-194). */
#define SVT_AV2_FRAME_MAX_TILE_COLS 64u
#define SVT_AV2_FRAME_MAX_TILE_ROWS 64u

/* tile_log2(blk, target) = smallest k with blk << k >= target (spec 5.18.7.3;
 * AVM avm/av2/common/tile_common.h:99-104). */
static uint32_t frame_tile_log2(uint32_t blk_size, uint32_t target) {
    uint32_t k = 0;
    while ((blk_size << k) < target) {
        ++k;
    }
    return k;
}

static uint32_t frame_min_u32(uint32_t a, uint32_t b) {
    return (a < b) ? a : b;
}

void svt_av2_frame_header_config_default(SvtAv2FrameHeaderConfig* cfg,
                                         uint32_t width, uint32_t height) {
    if (cfg == NULL) {
        return;
    }
    cfg->frame_width = width;
    cfg->frame_height = height;
    cfg->base_q_idx = SVT_AV2_FRAME_DEFAULT_BASE_Q_IDX;
    cfg->frame_width_bits_minus_1 = 0u; /* 0 => derive from frame_width */
    cfg->frame_height_bits_minus_1 = 0u;
    cfg->sb_shift = SVT_AV2_FRAME_SB_SHIFT_64;
    cfg->multi_frame = 0u;
    cfg->order_hint = 0u;
    cfg->reduced_tx_set = 0u;
    cfg->inter_frame = 0u;
    cfg->bit_depth = 8u;
    cfg->chroma_format = 1u;
    cfg->m5_intra_modes = 0u;
    cfg->partition_decisions = NULL;
    cfg->partition_decision_count = 0u;
    cfg->entropy_adaptation = 0u;
    cfg->enable_mrls = 0u;
    cfg->enable_mhccp = 0u;
    cfg->ccso64 = NULL;
    cfg->deblock_y = 0u;cfg->deblock_delta = 0;cfg->deblock_delta_h = 0;
}

/* ceil_log2(v) for v >= 1: the number of bits needed for v - 1. Mirrors
 * sequence_header.c's seq_bits_for_value(). */
static uint32_t frame_bits_for_value(uint32_t value) {
    uint32_t bits = 1;
    while ((value >> bits) != 0u) {
        ++bits;
    }
    return bits;
}

void svt_av2_frame_header_sb_grid(const SvtAv2FrameHeaderConfig* cfg,
                                  uint32_t* sb_cols,
                                  uint32_t* sb_rows) {
    if (cfg == NULL) {
        return;
    }
    /* miCols = 2 * ((frameWidth + 7) >> 3) = ceil(frameWidth / 4) in 4x4 units
     * (spec 5.18.7.3). */
    const uint32_t sb_size = 1u << cfg->sb_shift;
    const uint32_t mi_cols = 2u * ((cfg->frame_width + 7u) >> 3);
    const uint32_t mi_rows = 2u * ((cfg->frame_height + 7u) >> 3);
    if (sb_cols != NULL) {
        *sb_cols = (mi_cols + sb_size - 1u) >> cfg->sb_shift;
    }
    if (sb_rows != NULL) {
        *sb_rows = (mi_rows + sb_size - 1u) >> cfg->sb_shift;
    }
}

uint32_t svt_av2_frame_header_tile_params_bits(const SvtAv2FrameHeaderConfig* cfg) {
    if (cfg == NULL) {
        return SVT_AV2_FRAME_TILE_PARAMS_ERROR;
    }
    uint32_t sb_cols = 0;
    uint32_t sb_rows = 0;
    svt_av2_frame_header_sb_grid(cfg, &sb_cols, &sb_rows);
    if (sb_cols == 0u || sb_rows == 0u) {
        return SVT_AV2_FRAME_TILE_PARAMS_ERROR;
    }

    /* frame_width_bits_minus_1 / frame_height_bits_minus_1 hold "bits - 1"
     * (the field width is one more than the stored value). 0 means "derive
     * from the frame size": ceil_log2(size) - 1. Both are bounded by f(4), so
     * a stored value > 15 cannot be represented at all. */
    uint32_t width_bits_minus_1 = cfg->frame_width_bits_minus_1;
    if (width_bits_minus_1 == 0u) {
        width_bits_minus_1 = frame_bits_for_value(cfg->frame_width) - 1u;
    } else {
        width_bits_minus_1 -= 1u;
    }
    uint32_t height_bits_minus_1 = cfg->frame_height_bits_minus_1;
    if (height_bits_minus_1 == 0u) {
        height_bits_minus_1 = frame_bits_for_value(cfg->frame_height) - 1u;
    } else {
        height_bits_minus_1 -= 1u;
    }
    const uint32_t width_bits = width_bits_minus_1 + 1u;
    const uint32_t height_bits = height_bits_minus_1 + 1u;
    if (width_bits_minus_1 > 15u || height_bits_minus_1 > 15u ||
        cfg->frame_width == 0u || cfg->frame_height == 0u) {
        return SVT_AV2_FRAME_TILE_PARAMS_ERROR;
    }
    const uint32_t width_minus_1 = cfg->frame_width - 1u;
    const uint32_t height_minus_1 = cfg->frame_height - 1u;

    /* seq_level_idx != 31 path. seq_tier is 0 here (all M2 configurations use
     * tier 0): the header writer never emits seq_tier for level < 4 and writes
     * 0 for levels >= 4. */
    const uint32_t sb_size_log2 = cfg->sb_shift + 2u; /* MI_SIZE_LOG2 == 2 */
    const uint32_t max_tile_width_sb = (4u * 4096u) >> sb_size_log2;
    const uint32_t max_tile_area_sb = (4u * (4096u * 2304u)) >> (2u * sb_size_log2 + 2u);

    const uint32_t min_log2_cols = frame_tile_log2(max_tile_width_sb, sb_cols);
    const uint32_t max_log2_cols = frame_tile_log2(1u, frame_min_u32(sb_cols, SVT_AV2_FRAME_MAX_TILE_COLS));
    const uint32_t max_log2_rows = frame_tile_log2(1u, frame_min_u32(sb_rows, SVT_AV2_FRAME_MAX_TILE_ROWS));

    /* minLog2Tiles = Max(minLog2TileCols, tile_log2(maxTileAreaSb, sbRows*sbCols)). */
    uint32_t min_log2_tiles = frame_tile_log2(max_tile_area_sb, sb_rows * sb_cols);
    if (min_log2_cols > min_log2_tiles) {
        min_log2_tiles = min_log2_cols;
    }

    /* uniform_tile_spacing_flag = 1, single tile per axis: tileColsLog2 stops
     * at minLog2TileCols, then tileRowsLog2 = Min(minLog2Tiles - tileColsLog2,
     * maxLog2TileRows). */
    const uint32_t min_log2_rows = (min_log2_tiles > min_log2_cols)
                                       ? (min_log2_tiles - min_log2_cols)
                                       : 0u;
    const uint32_t col_stop = frame_min_u32(min_log2_rows, max_log2_rows);

    uint32_t bits = 1u; /* uniform_tile_spacing_flag */
    if (max_log2_cols > min_log2_cols) {
        bits += 1u; /* first zero terminates the unary increment loop */
    }
    if (max_log2_rows > col_stop) {
        bits += 1u;
    }

    /* The f(4) fields must be able to carry size - 1; sequence_header.c
     * already enforces this, so reaching here with a value that does not fit
     * means a caller built cfg by hand. */
    if (width_minus_1 >= (1u << width_bits) || height_minus_1 >= (1u << height_bits)) {
        return SVT_AV2_FRAME_TILE_PARAMS_ERROR;
    }
    return bits;
}

int svt_av2_frame_header_write(SvtAv2BitWriter* bw,
                               const SvtAv2FrameHeaderConfig* cfg) {
    if (bw == NULL || cfg == NULL) {
        return SVT_AV2_BW_ERROR_NULL;
    }
    if (cfg->deblock_y>3u || cfg->deblock_delta< -2 || cfg->deblock_delta>1 ||
        cfg->deblock_delta_h< -2 || cfg->deblock_delta_h>1 ||
        (cfg->deblock_y && (cfg->multi_frame || cfg->inter_frame || cfg->sb_shift!=4u)))
        return SVT_AV2_BW_ERROR_RANGE;
    if (svt_av2_ccso64_validate(cfg->ccso64, cfg->frame_width, cfg->frame_height,
        cfg->bit_depth, cfg->chroma_format) != 0 ||
        (cfg->ccso64 && (cfg->multi_frame || cfg->inter_frame || cfg->sb_shift != 4u)))
        return SVT_AV2_BW_ERROR_RANGE;
    if (cfg->inter_frame > 1u || (cfg->inter_frame && !cfg->multi_frame))
        return SVT_AV2_BW_ERROR_UNSUPPORTED;
    if ((cfg->bit_depth != 8u && cfg->bit_depth != 10u) ||
        (cfg->chroma_format != 1u && !(cfg->bit_depth == 10u && cfg->chroma_format == 3u)))
        return SVT_AV2_BW_ERROR_UNSUPPORTED;
    if (cfg->base_q_idx == 0u || cfg->base_q_idx > (cfg->bit_depth == 10u ? 511u : 255u)) {
        /* Nonzero actual qindex keeps coded_lossless false. The field is
         * eight bits at depth 8, nine bits at depth 10. */
        return SVT_AV2_BW_ERROR_RANGE;
    }
    if (cfg->entropy_adaptation > 1u || cfg->enable_mrls > 1u || cfg->enable_mhccp > 1u) return SVT_AV2_BW_ERROR_RANGE;
    const uint32_t tile_bits = svt_av2_frame_header_tile_params_bits(cfg);
    if (tile_bits == SVT_AV2_FRAME_TILE_PARAMS_ERROR) {
        return SVT_AV2_BW_ERROR_RANGE;
    }

    int st;

    /* cur_mfh_id, seq_header_id_in_frame_header: both uvlc()==0 -> single "1"
     * bit each. The frame uses the first sequence header (id 0). */
    if ((st = svt_av2_bit_writer_write_uvlc(bw, 0u)) != SVT_AV2_BW_OK) {
        return st;
    }
    if ((st = svt_av2_bit_writer_write_uvlc(bw, 0u)) != SVT_AV2_BW_OK) {
        return st;
    }

    if (cfg->inter_frame && (st = svt_av2_bit_writer_write_bit(bw, 1u)) != 0) return st;
    if (cfg->multi_frame != 0u) {
        /* single_picture_header_flag == 0: immediate_output_frame (OLK, not a
         * bridge frame => written) and frame_size_override_flag == 0 so the
         * frame size comes from the sequence header (spec 5.18.2 p.98; AVM
         * decodeframe.c:8100-8102, :8167). order_hint is written below using the sequence
         * header's eight-bit width. */
        if ((st = svt_av2_bit_writer_write_bit(bw, 1u)) != SVT_AV2_BW_OK) {
            return st;
        }
        if ((st = svt_av2_bit_writer_write_bit(bw, 0u)) != SVT_AV2_BW_OK) {
            return st;
        }
    }

    /* Multi-frame sequence signals eight order-hint bits. */
    if (cfg->multi_frame != 0u &&
        (st = svt_av2_bit_writer_write_bits(bw, cfg->order_hint, 8u)) != SVT_AV2_BW_OK) return st;

    if (cfg->inter_frame) {
        /* Explicit PRIMARY_REF_NONE, no cross-frame probability inheritance. */
        if ((st = svt_av2_bit_writer_write_bit(bw, 1u)) != 0) return st;
        if ((st = svt_av2_bit_writer_write_bit(bw, 1u)) != 0) return st;
        if ((st = svt_av2_bit_writer_write_bits(bw, 7u, 3u)) != 0) return st;
        if ((st = svt_av2_bit_writer_write_bit(bw, 1u)) != 0) return st; /* refresh slot0 */
        if ((st = svt_av2_bit_writer_write_bit(bw, 1u)) != 0) return st; /* explicit map */
        if ((st = svt_av2_bit_writer_write_bits(bw, 1u, 3u)) != 0) return st; /* actual reference count */
        /* slot index has zero bits because DPB has one slot. */
        if ((st = svt_av2_bit_writer_write_bit(bw, 1u)) != 0) return st; /* screen content */
        if ((st = svt_av2_bit_writer_write_bit(bw, 1u)) != 0) return st; /* integer MV */
        if ((st = svt_av2_bit_writer_write_bit(bw, 0u)) != 0) return st; /* intrabc */
        if ((st = svt_av2_bit_writer_write_bit(bw, 0u)) != 0) return st; /* fixed filter */
        if ((st = svt_av2_bit_writer_write_bits(bw, 3u, 2u)) != 0) return st; /* BILINEAR */
    } else {
    /* screen_content_params(): seq_force_screen_content_tools == SELECT, so one
     * bit; value 0 also forces force_integer_mv = 0 (no further bits). */
    if ((st = svt_av2_bit_writer_write_bit(bw, 0u)) != SVT_AV2_BW_OK) {
        return st;
    }
    /* intrabc_params(): allow_intrabc = 0. */
    if ((st = svt_av2_bit_writer_write_bit(bw, 0u)) != SVT_AV2_BW_OK) {
        return st;
    }
    }
    /* R7 frame-local adaptation; PRIMARY_REF_NONE still resets each frame. */
    if ((st = svt_av2_bit_writer_write_bit(bw, !cfg->entropy_adaptation)) != SVT_AV2_BW_OK) {
        return st;
    }

    /* Single tile: each available unary increment loop stops at its FIRST
     * zero. Its maximum number of iterations is not its encoded length. */
    if ((st = svt_av2_bit_writer_write_bit(bw, 1u)) != SVT_AV2_BW_OK) {
        return st;
    }
    for (uint32_t i = 1u; i < tile_bits; ++i) {
        if ((st = svt_av2_bit_writer_write_bit(bw, 0u)) != SVT_AV2_BW_OK) {
            return st;
        }
    }
    /* Single tile => TileCols*TileRows == 1 => no context_update_tile_id and no
     * tile_size_bytes_minus_1. */

    /* quantization_params(): base_q_idx is f(BitDepth == 8 ? 8 : 9). No delta_q
     * because y_dc_delta_q_enabled/uv_*_delta_q_enabled are all 0 (T004). */
    if ((st = svt_av2_bit_writer_write_bits(bw, cfg->base_q_idx, cfg->bit_depth == 10u ? 9u : 8u)) !=
        SVT_AV2_BW_OK) {
        return st;
    }
    /* segmentation_params(): segmentation_enabled = 0. */
    if ((st = svt_av2_bit_writer_write_bit(bw, 0u)) != SVT_AV2_BW_OK) {
        return st;
    }
    /* setup_qm_params(): using_qmatrix = 0. */
    if ((st = svt_av2_bit_writer_write_bit(bw, 0u)) != SVT_AV2_BW_OK) {
        return st;
    }
    /* delta_q_params(): base_q_idx > 0, delta_q_present = 0. */
    if ((st = svt_av2_bit_writer_write_bit(bw, 0u)) != SVT_AV2_BW_OK) {
        return st;
    }
    /* AVM Y direction flags, UV flags, V delta, then H delta/inheritance. */
    if ((st=svt_av2_bit_writer_write_bit(bw,(cfg->deblock_y&1u)!=0u))!=0) return st;
    if ((st=svt_av2_bit_writer_write_bit(bw,(cfg->deblock_y&2u)!=0u))!=0) return st;
    if (cfg->deblock_y) {
        if ((st=svt_av2_bit_writer_write_bits(bw,0u,2u))!=0) return st;
        if (cfg->deblock_y&1u) {
            if ((st=svt_av2_bit_writer_write_bit(bw,cfg->deblock_delta!=0))!=0) return st;
            if (cfg->deblock_delta && (st=svt_av2_bit_writer_write_bits(bw,
                (uint32_t)(cfg->deblock_delta+2),2u))!=0) return st;
        }
        if (cfg->deblock_y&2u) {
            const int previous=(cfg->deblock_y&1u)?cfg->deblock_delta:0;
            const int explicit_delta=cfg->deblock_delta_h!=previous;
            if ((st=svt_av2_bit_writer_write_bit(bw,(uint32_t)explicit_delta))!=0) return st;
            if (explicit_delta && (st=svt_av2_bit_writer_write_bits(bw,
                (uint32_t)(cfg->deblock_delta_h+2),2u))!=0) return st;
        }
    }
    if (cfg->ccso64 != NULL) { /* single-picture CCSO frame_flag is inferred1. */
        if ((st = svt_av2_bit_writer_write_bit(bw, 0u)) != 0) return st; /* Yoff */
        for (uint32_t p = 0u; p < 2u; ++p) {
            if ((st = svt_av2_bit_writer_write_bit(bw, cfg->ccso64->enabled[p])) != 0) return st;
            if (!cfg->ccso64->enabled[p]) continue;
            if ((st = svt_av2_bit_writer_write_bits(bw, 34u, 6u)) != 0) return st; /* BO1,scale00,band010 */
            for (uint32_t b = 0u; b < 4u; ++b) {
                const int offset = cfg->ccso64->offset[p][b];
                const uint32_t index = offset == 0 ? 0u : offset == 1 ? 1u : 2u;
                for (uint32_t i = 0u; i <= index; ++i)
                    if ((st = svt_av2_bit_writer_write_bit(bw, i != index)) != 0) return st;
            }
        }
    }
    /* read_tx_mode(): not CodedLossless, tx_mode_select = 0 -> TX_MODE_LARGEST. */
    if ((st = svt_av2_bit_writer_write_bit(bw, 0u)) != SVT_AV2_BW_OK) {
        return st;
    }
    if (cfg->inter_frame && (st = svt_av2_bit_writer_write_bit(bw, 0u)) != 0) return st; /* SINGLE_REFERENCE */
    /* AVM v1.0.0 permits skip mode even with a single reference. */
    if (cfg->inter_frame && (st = svt_av2_bit_writer_write_bit(bw, 0u)) != 0) return st; /* skip_mode_flag */
    /* reduced_tx_set: 2 implies intra DCT; inter small transforms signal DCT_IDTX. */
    if ((st = svt_av2_bit_writer_write_bits(bw, cfg->reduced_tx_set, 2u)) != SVT_AV2_BW_OK) {
        return st;
    }

    /* gdf_params()/cdef_params()/lr_params()/global_motion_params()
     * /film_grain_config() all emit
     * zero bits for this configuration. */
    return SVT_AV2_BW_OK;
}
