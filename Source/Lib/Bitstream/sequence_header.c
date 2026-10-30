/*
 * SVT-AV2 - minimal AV2 sequence header writer (T004).
 * See sequence_header.h for the frozen scope and spec references.
 */
#include "sequence_header.h"

/* AV2 valid level indices: 0..21 (levels 2.0..8.3), 22..30 reserved,
 * 31 = Maximum parameters (spec Table A.8 / AVM is_valid_seq_level_idx). */

/* Table A.8 level limits for indices 0..21: { max_picture_size, max_h_size,
 * max_v_size } as in avm/av2/common/level.c:36-260 (av2_level_defs). */
typedef struct SeqLevelLimits {
    uint32_t max_picture_size;
    uint32_t max_h_size;
    uint32_t max_v_size;
} SeqLevelLimits;

static const SeqLevelLimits k_seq_level_limits[SVT_AV2_SEQ_LEVEL_RESERVED_START] = {
    { 147456u, 640u, 640u },       /* 0: level 2.0 */
    { 278784u, 880u, 880u },       /* 1: level 2.1 */
    { 665856u, 1360u, 1360u },     /* 2: level 3.0 */
    { 1065024u, 1720u, 1720u },    /* 3: level 3.1 */
    { 2359296u, 2560u, 2560u },    /* 4: level 4.0 */
    { 2359296u, 2560u, 2560u },    /* 5: level 4.1 */
    { 8912896u, 4975u, 4975u },    /* 6: level 5.0 */
    { 8912896u, 4975u, 4975u },    /* 7: level 5.1 */
    { 8912896u, 4975u, 4975u },    /* 8: level 5.2 */
    { 8912896u, 4975u, 4975u },    /* 9: level 5.3 */
    { 35651584u, 9951u, 9951u },   /* 10: level 6.0 */
    { 35651584u, 9951u, 9951u },   /* 11: level 6.1 */
    { 35651584u, 9951u, 9951u },   /* 12: level 6.2 */
    { 35651584u, 9951u, 9951u },   /* 13: level 6.3 */
    { 35651584u, 9951u, 9951u },   /* 14: level 7.0 */
    { 35651584u, 9951u, 9951u },   /* 15: level 7.1 */
    { 35651584u, 9951u, 9951u },   /* 16: level 7.2 */
    { 35651584u, 9951u, 9951u },   /* 17: level 7.3 */
    { 35651584u, 9951u, 9951u },   /* 18: level 8.0 */
    { 35651584u, 9951u, 9951u },   /* 19: level 8.1 */
    { 35651584u, 9951u, 9951u },   /* 20: level 8.2 */
    { 35651584u, 9951u, 9951u },   /* 21: level 8.3 */
};

uint32_t svt_av2_sequence_header_min_level_for_size(uint32_t width,
                                                    uint32_t height) {
    const uint64_t samples = (uint64_t)width * (uint64_t)height;
    for (uint32_t level = 0; level < SVT_AV2_SEQ_LEVEL_RESERVED_START; ++level) {
        if (samples <= k_seq_level_limits[level].max_picture_size &&
            width <= k_seq_level_limits[level].max_h_size &&
            height <= k_seq_level_limits[level].max_v_size) {
            return level;
        }
    }
    return SVT_AV2_SEQ_LEVEL_NONE;
}

/* Bits needed to represent value, at least 1 (f(n) requires n >= 1 here). */
static uint32_t seq_bits_for_value(uint32_t value) {
    uint32_t bits = 1;
    while ((value >> bits) != 0u) {
        ++bits;
    }
    return bits;
}

static int seq_write_bit(SvtAv2BitWriter* bw, uint32_t bit) {
    return svt_av2_bit_writer_write_bit(bw, bit);
}

static int seq_write_bits(SvtAv2BitWriter* bw, uint32_t value, uint32_t num_bits) {
    return svt_av2_bit_writer_write_bits(bw, value, num_bits);
}

#define SVT_AV2_SEQ_MAX_SEQ_NUM 16u

void svt_av2_sequence_header_config_default(SvtAv2SequenceHeaderConfig* cfg,
                                            uint32_t width, uint32_t height) {
    if (cfg == NULL) {
        return;
    }
    cfg->seq_header_id = 0u;
    cfg->seq_profile_idc = SVT_AV2_SEQ_PROFILE_MAIN_420_10_IP0;
    cfg->seq_level_idx = 0u;
    cfg->max_frame_width = width;
    cfg->max_frame_height = height;
    cfg->use_256x256_superblock = 0u; /* 64x64 superblock */
    cfg->use_128x128_superblock = 0u;
    cfg->seq_tile_info_present_flag = 0u; /* single tile */
    cfg->low_delay_p = 0u;
    cfg->bit_depth = 8u;
    cfg->chroma_format = 1u;
    cfg->chroma_dct_only = 0u;
    cfg->enable_cfl_intra = 0u;
    cfg->enable_mrls = 0u;
    cfg->enable_mhccp = 0u;
    cfg->enable_ccso64 = 0u;
    cfg->enable_pc_wiener = 0u;
}

/* Shared body validation (everything the M1 subset checks except the
 * byte-alignment and NULL checks). Returns SVT_AV2_BW_OK on success, or the
 * first SvtAv2BitWriterStatus that rejects the configuration. Also reports the
 * derived dimension field widths through width_bits/height_bits. */
static int seq_validate_config(const SvtAv2SequenceHeaderConfig* cfg,
                              uint32_t* width_bits,
                              uint32_t* height_bits) {
    if (cfg->low_delay_p > 1u || cfg->chroma_dct_only > 1u || cfg->enable_cfl_intra > 1u || cfg->enable_mrls > 1u || cfg->enable_mhccp > 1u) return SVT_AV2_BW_ERROR_RANGE;
    if (cfg->enable_pc_wiener > 1u || (cfg->enable_pc_wiener &&
        ((cfg->max_frame_width & 7u) || (cfg->max_frame_height & 7u) ||
         cfg->max_frame_width > 4096u || cfg->max_frame_height > 2160u ||
         cfg->use_256x256_superblock || cfg->use_128x128_superblock)))
        return SVT_AV2_BW_ERROR_RANGE;
    if (cfg->enable_ccso64 > 1u || (cfg->enable_ccso64 &&
        ((cfg->max_frame_width & 7u) || (cfg->max_frame_height & 7u) ||
         cfg->max_frame_width > 4096u || cfg->max_frame_height > 2160u ||
         cfg->chroma_format != 1u || cfg->use_256x256_superblock || cfg->use_128x128_superblock)))
        return SVT_AV2_BW_ERROR_RANGE;
    if (cfg->seq_header_id >= SVT_AV2_SEQ_MAX_SEQ_NUM) {
        return SVT_AV2_BW_ERROR_RANGE;
    }
    if ((cfg->bit_depth != 8u && cfg->bit_depth != 10u) ||
        (cfg->chroma_format != 1u && !(cfg->bit_depth == 10u && cfg->chroma_format == 3u)))
        return SVT_AV2_BW_ERROR_UNSUPPORTED;
    if (cfg->seq_profile_idc > 31u) {
        return SVT_AV2_BW_ERROR_RANGE;
    }
    if (cfg->seq_profile_idc != (cfg->chroma_format == 3u ? 4u : 0u))
        return SVT_AV2_BW_ERROR_UNSUPPORTED;
    if (cfg->seq_level_idx >= SVT_AV2_SEQ_LEVEL_RESERVED_START &&
        cfg->seq_level_idx != SVT_AV2_SEQ_LEVEL_MAX) {
        return SVT_AV2_BW_ERROR_RANGE;
    }
    if (cfg->max_frame_width < 1u || cfg->max_frame_height < 1u) {
        return SVT_AV2_BW_ERROR_INVALID_ARG;
    }
    /* frame_width_bits_minus_1 / frame_height_bits_minus_1 are f(4). */
    const uint32_t wb = seq_bits_for_value(cfg->max_frame_width - 1u);
    const uint32_t hb = seq_bits_for_value(cfg->max_frame_height - 1u);
    if (wb > 16u || hb > 16u) {
        return SVT_AV2_BW_ERROR_RANGE;
    }
    if (cfg->seq_tile_info_present_flag != 0u) {
        /* tile_params() is out of the M1 subset. */
        return SVT_AV2_BW_ERROR_UNSUPPORTED;
    }
    *width_bits = wb;
    *height_bits = hb;
    return SVT_AV2_BW_OK;
}

int svt_av2_sequence_header_write_multi(SvtAv2BitWriter* bw,
                                       const SvtAv2SequenceHeaderConfig* cfg) {
    if (cfg != NULL && (cfg->enable_ccso64 || cfg->enable_pc_wiener)) return SVT_AV2_BW_ERROR_RANGE;
    if (bw == NULL || cfg == NULL) {
        return SVT_AV2_BW_ERROR_NULL;
    }
    if (!svt_av2_bit_writer_byte_aligned(bw)) {
        return SVT_AV2_BW_ERROR_UNALIGNED;
    }

    uint32_t width_bits = 0u;
    uint32_t height_bits = 0u;
    int status = seq_validate_config(cfg, &width_bits, &height_bits);
    if (status != SVT_AV2_BW_OK) {
        return status;
    }

    /* --- sequence_header_obu(), single_picture_header_flag = 0 (5.4.1) --- */
    if ((status = svt_av2_bit_writer_write_uvlc(bw, cfg->seq_header_id)) != SVT_AV2_BW_OK) {
        return status;
    }
    if ((status = seq_write_bits(bw, cfg->seq_profile_idc, 5u)) != SVT_AV2_BW_OK) {
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* single_picture_header_flag */
        return status;
    }
    if ((status = seq_write_bits(bw, cfg->seq_level_idx, 5u)) != SVT_AV2_BW_OK) {
        return status;
    }
    /* seq_tier: written only for level >= 4 on a multi-frame sequence
     * (spec 5.4.1; AVM obu.c:559-567). tier 0 = Main. */
    if (cfg->seq_level_idx >= SVT_AV2_SEQ_TIER_PRESENT_LEVEL) {
        if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) {
            return status;
        }
    }
    if ((status = svt_av2_bit_writer_write_uvlc(bw, cfg->chroma_format == 3u ? 2u : 0u)) != SVT_AV2_BW_OK) {
        return status;
    }
    if ((status = svt_av2_bit_writer_write_uvlc(bw, cfg->bit_depth == 10u ? 0u : 1u)) != SVT_AV2_BW_OK) {
        return status;
    }

    /* Multi-frame head fields (spec 5.4.1 p.56). max_mlayer_id == 0 keeps
     * seq_max_mlayer_cnt_minus_1 (f(CeilLog2(max_mlayer_id+1))) off the wire;
     * the whole-layer dependency flags are tied to max_mlayer_id / max_tlayer_id,
     * so both are 0 bits here too. */
    if ((status = seq_write_bits(bw, 0u, 3u)) != SVT_AV2_BW_OK) { /* seq_lcr_id */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* still_picture */
        return status;
    }
    if ((status = seq_write_bits(bw, 0u, 2u)) != SVT_AV2_BW_OK) { /* max_tlayer_id */
        return status;
    }
    if ((status = seq_write_bits(bw, 0u, 3u)) != SVT_AV2_BW_OK) { /* max_mlayer_id */
        return status;
    }
    /* max_mlayer_id == 0: no seq_max_mlayer_cnt_minus_1, no
     * mlayer_dependency_present_flag, no tlayer_dependency_present_flag. */
    if ((status = seq_write_bit(bw, 1u)) != SVT_AV2_BW_OK) { /* monotonic_output_order_flag */
        return status;
    }

    if ((status = seq_write_bits(bw, width_bits - 1u, 4u)) != SVT_AV2_BW_OK) {
        return status;
    }
    if ((status = seq_write_bits(bw, height_bits - 1u, 4u)) != SVT_AV2_BW_OK) {
        return status;
    }
    if ((status = seq_write_bits(bw, cfg->max_frame_width - 1u, width_bits)) != SVT_AV2_BW_OK) {
        return status;
    }
    if ((status = seq_write_bits(bw, cfg->max_frame_height - 1u, height_bits)) != SVT_AV2_BW_OK) {
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* seq_cropping_window_present_flag */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* seq_initial_display_delay_present_flag */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* decoder_model_info_present_flag */
        return status;
    }
    /* decoder_model_info_present_flag == 0: buffer_delay_length_minus_1,
     * num_units_in_decoding_tick, decoder_model_parameters() all absent. */

    /* --- sequence_partition_config() (spec 5.4.3) ------------------------ */
    if ((status = seq_write_bit(bw, cfg->use_256x256_superblock ? 1u : 0u)) != SVT_AV2_BW_OK) {
        return status;
    }
    if (!cfg->use_256x256_superblock) {
        if ((status = seq_write_bit(bw, cfg->use_128x128_superblock ? 1u : 0u)) != SVT_AV2_BW_OK) {
            return status;
        }
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_sdp */
        return status;
    }
    /* enable_sdp == 0: enable_extended_sdp is not written. */
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_ext_partitions */
        return status;
    }
    /* enable_ext_partitions == 0: enable_uneven_4way_partitions is not written. */
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* reduce_pb_aspect_ratio */
        return status;
    }

    /* --- sequence_segment_config() (spec 5.4.4) -------------------------- */
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_ext_seg */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* seq_seg_info_present_flag */
        return status;
    }

    /* --- sequence_intra_config() (spec 5.4.5) ---------------------------- */
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_dip */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_intra_edge_filter */
        return status;
    }
    if ((status = seq_write_bit(bw, cfg->enable_mrls)) != SVT_AV2_BW_OK) { /* enable_mrls */
        return status;
    }
    if ((status = seq_write_bit(bw, cfg->enable_cfl_intra)) != SVT_AV2_BW_OK) { /* enable_cfl_intra */
        return status;
    }
    if ((status = seq_write_bits(bw, 0u, 2u)) != SVT_AV2_BW_OK) { /* cfl_ds_filter_index */
        return status;
    }
    if ((status = seq_write_bit(bw, cfg->enable_mhccp)) != SVT_AV2_BW_OK) { /* enable_mhccp */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_ibp */
        return status;
    }

    /* --- sequence_inter_config() (spec 5.4.6, multi-frame branch) --------
     * Field order and conditional dependencies follow the decoder's actual
     * read sequence (AVM avm/av2/decoder/decodeframe.c:6005-6143), because the
     * PDF text layer does not contain this syntax body. In particular
     * enable_refmvbank / the enable_drl_reorder bit come AFTER the motion-mode
     * flags and order_hint_bits_minus_1 (decodeframe.c:6043-6048). */
    /* seq_enabled_motion_modes[mode] for mode = INTERINTRA..MOTION_MODES-1 (four modes).
     * All 0 => motion_mode_enabled == 0, so seq_frame_motion_modes_present_flag
     * and enable_six_param_warp_delta are not written. */
    for (uint32_t mode = 0; mode < SVT_AV2_SEQ_MOTION_MODE_FLAGS; ++mode) {
        if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) {
            return status;
        }
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_masked_compound */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_ref_frame_mvs */
        return status;
    }
    /* enable_ref_frame_mvs == 0: reduced_ref_frame_mvs_mode is not written. */
    if ((status = seq_write_bits(bw, 7u, 4u)) != SVT_AV2_BW_OK) { /* order_hint_bits_minus_1: eight bits */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_refmvbank */
        return status;
    }
    /* enable_drl_reorder: 1 bit; 1 => DRL_REORDER_DISABLED (what M1 uses). */
    if ((status = seq_write_bit(bw, 1u)) != SVT_AV2_BW_OK) {
        return status;
    }
    if ((status = seq_write_bit(bw, cfg->low_delay_p != 0u)) != SVT_AV2_BW_OK) { /* explicit_ref_frame_map */
        return status;
    }
    if ((status = seq_write_bit(bw, cfg->low_delay_p != 0u)) != SVT_AV2_BW_OK) { /* explicit_num_ref_frames */
        return status;
    }
    if (cfg->low_delay_p && (status = seq_write_bits(bw, 0u, 4u)) != SVT_AV2_BW_OK) return status;
    /* explicit_num_ref_frames == 0: NumRefFrames = 8, no
     * num_ref_frames_minus_1. */
    if ((status = seq_write_bits(bw, 0u, 3u)) != SVT_AV2_BW_OK) { /* long_term_frame_id_bits */
        return status;
    }
    /* seq_max_drl_bits_minus_1 uses ns(MAX_MAX_DRL_BITS - MIN_MAX_DRL_BITS + 1)
     * = ns(5) (AVM decodeframe.c:6067-6070, enums.h). The minimum value is 0. */
    if ((status = svt_av2_bit_writer_write_ns(bw, SVT_AV2_SEQ_MAX_DRL_BITS_MINUS_1_N, 0u)) != SVT_AV2_BW_OK) {
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* allow_frame_max_drl_bits */
        return status;
    }
    /* seq_max_bvp_drl_bits_minus_1 uses ns(3), minimum value 0. */
    if ((status = svt_av2_bit_writer_write_ns(bw, SVT_AV2_SEQ_MAX_IBC_DRL_BITS_MINUS_1_N, 0u)) != SVT_AV2_BW_OK) {
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* allow_frame_max_bvp_drl_bits */
        return status;
    }
    if ((status = seq_write_bits(bw, 0u, 2u)) != SVT_AV2_BW_OK) { /* num_same_ref_compound */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_tip */
        return status;
    }
    /* enable_tip == 0: no enable_tip_hole_fill. */
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_mv_traj */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_bawp */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_cwp */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_imp_msk_bld */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_lf_sub_pu */
        return status;
    }
    /* enable_lf_sub_pu == 0: no enable_tip_explicit_qp. */
    if ((status = seq_write_bits(bw, 0u, 2u)) != SVT_AV2_BW_OK) { /* enable_opfl_refine */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_refinemv */
        return status;
    }
    /* enable_tip == 0: no enable_tip_refinemv. */
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_bru */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_adaptive_mvd */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_mvd_sign_derive */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_flex_mvres */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_global_motion */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_short_refresh_frame_flags */
        return status;
    }

    /* --- sequence_scc_config() (spec 5.4.7, multi-frame branch) ---------- */
    if ((status = seq_write_bit(bw, 1u)) != SVT_AV2_BW_OK) { /* seq_choose_screen_content_tools */
        return status;
    }
    /* choose == 1 => seq_force_screen_content_tools = SELECT(2), not written. */
    if ((status = seq_write_bit(bw, 1u)) != SVT_AV2_BW_OK) { /* seq_choose_integer_mv */
        return status;
    }
    /* choose == 1 => seq_force_integer_mv = SELECT(2), not written. */

    /* --- sequence_transform_quant_entropy_config() (spec 5.4.8) ---------- */
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_fsc */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_idtx_intra (!fsc) */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_intra_ist */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_inter_ist */
        return status;
    }
    if ((status = seq_write_bit(bw, cfg->chroma_dct_only)) != SVT_AV2_BW_OK) { /* enable_chroma_dctonly */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_inter_ddt */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* reduced_tx_part_set */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_cctx */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_tcq */
        return status;
    }
    /* enable_tcq == 0: choose_tcq_per_frame is not written. */
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_parity_hiding */
        return status;
    }
    if ((status = seq_write_bit(bw, 1u)) != SVT_AV2_BW_OK) { /* enable_avg_cdf */
        return status;
    }
    if ((status = seq_write_bit(bw, 1u)) != SVT_AV2_BW_OK) { /* avg_cdf_type: per-tile CDF */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* separate_uv_delta_q */
        return status;
    }
    if ((status = seq_write_bit(bw, 1u)) != SVT_AV2_BW_OK) { /* equal_ac_dc_q */
        return status;
    }
    if ((status = seq_write_bits(bw, 23u, 5u)) != SVT_AV2_BW_OK) { /* base_uv_ac_delta_q: -23 + 23 = 0 */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* uv_ac_delta_q_enabled */
        return status;
    }

    /* --- sequence_filter_config() (spec 5.4.10) -------------------------- */
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* disable_loopfilters_across_tiles */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_cdef */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_gdf */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_restoration */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_ccso */
        return status;
    }
    /* Multi-frame branch: cdef_on_skip_txfm is moved from inferred to written.
     * always_on=0, disabled=0 => CDEF_ON_SKIP_TXFM_ADAPTIVE (M1's inferred
     * value), matching AVM decodeframe.c:6171-6218. */
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* cdef_on_skip_txfm_always_on */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* cdef_on_skip_txfm_disabled */
        return status;
    }
    if ((status = seq_write_bits(bw, 0u, 2u)) != SVT_AV2_BW_OK) { /* df_par_bits_minus_2 */
        return status;
    }

    /* --- sequence_tile_config() (spec 5.4.2) ----------------------------- */
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* seq_tile_info_present_flag */
        return status;
    }

    /* --- tail of sequence_header_obu() ---------------------------------- */
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* film_grain_params_present */
        return status;
    }
    return seq_write_bit(bw, 0u); /* obu_extension_flag */
}

int svt_av2_sequence_header_write(SvtAv2BitWriter* bw,
                                  const SvtAv2SequenceHeaderConfig* cfg) {
    if (bw == NULL || cfg == NULL) {
        return SVT_AV2_BW_ERROR_NULL;
    }
    if (!svt_av2_bit_writer_byte_aligned(bw)) {
        return SVT_AV2_BW_ERROR_UNALIGNED;
    }

    /* --- validation of the M1 subset (fail loudly, never guess) ---------- */
    uint32_t width_bits = 0u;
    uint32_t height_bits = 0u;
    int status = seq_validate_config(cfg, &width_bits, &height_bits);
    if (status != SVT_AV2_BW_OK) {
        return status;
    }

    /* --- sequence_header_obu() (spec 5.4.1) ------------------------------ */
    if ((status = svt_av2_bit_writer_write_uvlc(bw, cfg->seq_header_id)) != SVT_AV2_BW_OK) {
        return status;
    }
    if ((status = seq_write_bits(bw, cfg->seq_profile_idc, 5u)) != SVT_AV2_BW_OK) {
        return status;
    }
    if ((status = seq_write_bit(bw, 1u)) != SVT_AV2_BW_OK) { /* single_picture_header_flag */
        return status;
    }
    if ((status = seq_write_bits(bw, cfg->seq_level_idx, 5u)) != SVT_AV2_BW_OK) {
        return status;
    }
    /* single_picture_header_flag == 1: seq_tier is inferred 0 (not written). */

    if ((status = svt_av2_bit_writer_write_uvlc(bw, cfg->chroma_format == 3u ? 2u : 0u)) != SVT_AV2_BW_OK) {
        return status;
    }
    if ((status = svt_av2_bit_writer_write_uvlc(bw, cfg->bit_depth == 10u ? 0u : 1u)) != SVT_AV2_BW_OK) {
        return status;
    }

    /* single_picture_header_flag == 1: seq_lcr_id, still_picture, max_tlayer_id,
     * max_mlayer_id, SeqMaxMlayerCnt, monotonic_output_order_flag are inferred
     * and not written (spec 5.4.1). */

    if ((status = seq_write_bits(bw, width_bits - 1u, 4u)) != SVT_AV2_BW_OK) {
        return status;
    }
    if ((status = seq_write_bits(bw, height_bits - 1u, 4u)) != SVT_AV2_BW_OK) {
        return status;
    }
    if ((status = seq_write_bits(bw, cfg->max_frame_width - 1u, width_bits)) != SVT_AV2_BW_OK) {
        return status;
    }
    if ((status = seq_write_bits(bw, cfg->max_frame_height - 1u, height_bits)) != SVT_AV2_BW_OK) {
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* seq_cropping_window_present_flag */
        return status;
    }
    /* single_picture_header_flag == 1: no display delay / decoder model. */

    /* --- sequence_partition_config() (spec 5.4.3) ------------------------ */
    if ((status = seq_write_bit(bw, cfg->use_256x256_superblock ? 1u : 0u)) != SVT_AV2_BW_OK) {
        return status;
    }
    if (!cfg->use_256x256_superblock) {
        if ((status = seq_write_bit(bw, cfg->use_128x128_superblock ? 1u : 0u)) != SVT_AV2_BW_OK) {
            return status;
        }
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_sdp */
        return status;
    }
    /* single_picture_header_flag == 1: no enable_extended_sdp. */
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_ext_partitions */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* reduce_pb_aspect_ratio */
        return status;
    }

    /* --- sequence_segment_config() (spec 5.4.4) -------------------------- */
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_ext_seg */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* seq_seg_info_present_flag */
        return status;
    }

    /* --- sequence_intra_config() (spec 5.4.5) ---------------------------- */
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_dip */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_intra_edge_filter */
        return status;
    }
    if ((status = seq_write_bit(bw, cfg->enable_mrls)) != SVT_AV2_BW_OK) { /* enable_mrls */
        return status;
    }
    if ((status = seq_write_bit(bw, cfg->enable_cfl_intra)) != SVT_AV2_BW_OK) { /* enable_cfl_intra */
        return status;
    }
    if ((status = seq_write_bits(bw, 0u, 2u)) != SVT_AV2_BW_OK) { /* cfl_ds_filter_index */
        return status;
    }
    if ((status = seq_write_bit(bw, cfg->enable_mhccp)) != SVT_AV2_BW_OK) { /* enable_mhccp */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_ibp */
        return status;
    }

    /* --- sequence_inter_config() (spec 5.4.6, single_picture branch) ----- */
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_refmvbank */
        return status;
    }
    if ((status = seq_write_bit(bw, 1u)) != SVT_AV2_BW_OK) { /* disable_drl_reorder */
        return status;
    }
    /* n = MAX_REF_BV_STACK_SIZE - 1 = 3; M1 uses the minimum. */
    if ((status = svt_av2_bit_writer_write_ns(bw, SVT_AV2_SEQ_MAX_REF_BV_STACK_SIZE - 1u, 0u)) != SVT_AV2_BW_OK) {
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* allow_frame_max_bvp_drl_bits */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_bawp */
        return status;
    }

    /* --- sequence_scc_config() (spec 5.4.7): all inferred, no bits ------- */

    /* --- sequence_transform_quant_entropy_config() (spec 5.4.8) ---------- */
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_fsc */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_idtx_intra (!fsc) */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_intra_ist */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_inter_ist */
        return status;
    }
    if ((status = seq_write_bit(bw, cfg->chroma_dct_only)) != SVT_AV2_BW_OK) { /* enable_chroma_dctonly */
        return status;
    }
    /* single_picture_header_flag == 1: no enable_inter_ddt. */
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* reduced_tx_part_set */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_cctx */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_tcq */
        return status;
    }
    /* enable_tcq == 0 => enable_parity_hiding is written. */
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_parity_hiding */
        return status;
    }
    /* single_picture_header_flag == 1: enable_avg_cdf / avg_cdf_type inferred. */
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* separate_uv_delta_q */
        return status;
    }
    if ((status = seq_write_bit(bw, 1u)) != SVT_AV2_BW_OK) { /* equal_ac_dc_q */
        return status;
    }
    /* equal_ac_dc_q == 1: no base_y_dc_delta_q / base_uv_dc_delta_q.
     * Not monochrome: base_uv_ac_delta_q f(5) + uv_ac_delta_q_enabled f(1)
     * are always present (outside the equal_ac_dc_q condition). */
    if ((status = seq_write_bits(bw, 23u, 5u)) != SVT_AV2_BW_OK) { /* base_uv_ac_delta_q: -23 + 23 = 0 */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* uv_ac_delta_q_enabled */
        return status;
    }

    /* --- sequence_filter_config() (spec 5.4.10) -------------------------- */
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* disable_loopfilters_across_tiles */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_cdef */
        return status;
    }
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* enable_gdf */
        return status;
    }
    /* enable_gdf == 0: no gdf_unit_matches_sb_size. */
    if ((status = seq_write_bit(bw, cfg->enable_pc_wiener)) != SVT_AV2_BW_OK) { /* enable_restoration */
        return status;
    }
    if (cfg->enable_pc_wiener) {
        /* Enable Y PC-Wiener only; UV inherits and disables PC-Wiener. */
        if ((status = seq_write_bits(bw, 2u, 3u)) != SVT_AV2_BW_OK) return status;
    }
    if ((status = seq_write_bit(bw, cfg->enable_ccso64)) != SVT_AV2_BW_OK) { /* enable_ccso */
        return status;
    }
    if (cfg->enable_ccso64 && (status = seq_write_bit(bw, 1u)) != SVT_AV2_BW_OK) return status; /* match64SB */
    /* single_picture_header_flag == 1: no cdef_on_skip_txfm. */
    if ((status = seq_write_bits(bw, 0u, 2u)) != SVT_AV2_BW_OK) { /* df_par_bits_minus_2 */
        return status;
    }

    /* --- sequence_tile_config() (spec 5.4.2): present flag is always 0 --- */
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* seq_tile_info_present_flag */
        return status;
    }

    /* --- tail of sequence_header_obu() ---------------------------------- */
    if ((status = seq_write_bit(bw, 0u)) != SVT_AV2_BW_OK) { /* film_grain_params_present */
        return status;
    }
    /* open_bitstream_unit(): OBU_SEQUENCE_HEADER is extensible, so the payload
     * ends with obu_extension_flag (spec 5.2.1). The trailing bits are appended
     * by svt_av2_obu_payload_finalize() (T003). */
    return seq_write_bit(bw, 0u); /* obu_extension_flag */
}
