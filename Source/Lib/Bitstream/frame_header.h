/*
 * SVT-AV2 - minimal AV2 frame header + tile group writer (T005, M1).
 *
 * Scope: the M1 single-frame path only (8-bit 4:2:0, single picture header,
 * fixed size, 64x64 superblock, single tile, ALL-INTRA/CLOSED_LOOP_KEY,
 * non-lossless). The writer emits exactly the bits produced by
 * frame_header_info() for that configuration into a caller-owned BitWriter.
 * It does not write the sequence header (T004) and does not implement entropy
 * coding (see EntropyCoding/).
 *
 * Spec references (AV2 Bitstream & Decoding Process Specification v1.0.0):
 *   - 5.2.6 tile_group_obu(): is_first_tile_group, frame_header_present_flag...
 *   - 6.17 frame_header_info()/tile_info()/quantization_params() and the
 *     frame-header sub-structures it calls.
 *   - 5.18.6 screen_content_params(), 5.18.7 intrabc_params().
 *   - 4.11.2 f(n), 4.11.3 uvlc() - descriptor primitives owned by T003.
 *
 * Bit layout produced for the frozen M1 config (single_picture_header_flag=1,
 * obu_type=OBU_CLOSED_LOOP_KEY, use_64x64_superblock, base_q_idx>0):
 *
 *   uvlc() cur_mfh_id                              = 0        (1 bit, "1")
 *   uvlc() seq_header_id_in_frame_header           = 0        (1 bit, "1")
 *   f(1)   allow_screen_content_tools              = 0
 *   f(1)   allow_intrabc                           = 0
 *   f(1)   disable_cdf_update                      = 1
 *   f(1)   uniform_tile_spacing_flag               = 1   (tile_params)
 *   f(1)*N increment_tile_cols_log2 / increment_tile_rows_log2 = 0
 *          (N = tile_log2 terms in 5.18.7.3; 0 for 64x64; 2 for 176x144,
 *           1280x720 and 1920x1080 (first zero stops each axis) -- see the reference table)
 *   f(8)   base_q_idx                              = cfg->base_q_idx
 *   f(1)   segmentation_enabled                    = 0
 *   f(1)   using_qmatrix                           = 0
 *   f(1)   delta_q_present                         = 0   (base_q_idx>0 only)
 *   f(1)   apply_deblocking_filter[0]              = 0
 *   f(1)   apply_deblocking_filter[1]              = 0
 *   f(1)   tx_mode_select                          = 0   (TX_MODE_LARGEST)
 *   f(2)   reduced_tx_set                          = 0
 *   => 22 bits at 64x64; 24 bits for the other M2 target sizes.
 *
 * For cfg->multi_frame (single_picture_header_flag = 0) ten extra bits are
 * written before the screen-content flags, matching the AVM read order:
 *   f(1)   immediate_output_frame                   = 1
 *   f(1)   frame_size_override_flag                 = 0
 *   f(8)   order_hint                              = cfg->order_hint
 * The frame stays all-intra CLOSED_LOOP_KEY; refresh flags are inferred.
 *
 * Every other frame-header field is zero-bit for this configuration; the
 * per-field justification is in docs/bitstream-frame-minimal.md section 3.3
 * and docs/t015-header-field-reference.md sections 5 and 6.
 */
#ifndef SVT_AV2_FRAME_HEADER_H
#define SVT_AV2_FRAME_HEADER_H

#include <stddef.h>
#include <stdint.h>

#include "bit_writer.h"
#include "ccso64.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Number of bits emitted by svt_av2_frame_header_write() for the M1 config.
 * Exposed so callers/tests can assert the exact header length. */
#define SVT_AV2_FRAME_HEADER_M1_BITS 22u

/* Bit depth idc for 8-bit (spec Table 6.3). */
#define SVT_AV2_FRAME_BIT_DEPTH_IDC_8 1u

typedef struct SvtAv2FrameHeaderConfig {
    /* Frame dimensions (informational for M1; the decoder derives them from
     * the sequence header because frame_size_override_flag is 0). */
    uint32_t frame_width;
    uint32_t frame_height;
    /* frame-level quantizer index, f(8) for 8-bit, range [0,255]. M1 requires
     * > 0 to stay non-lossless (see docs/bitstream-frame-minimal.md 2.1). */
    uint32_t base_q_idx;
    /* f(4) frame_width_bits_minus_1 / frame_height_bits_minus_1 as written in
     * the sequence header (ceil_log2(size) - 1). Only used to derive sbCols.
     * 0 means "derive from frame_width/frame_height", which is what
     * svt_av2_frame_header_config_default() sets. */
    uint32_t frame_width_bits_minus_1;
    uint32_t frame_height_bits_minus_1;
    /* sbShift = Mi_Width_Log2[sbSize]: 4 for 64x64, 5 for 128x128, 6 for
     * 256x256 (spec 5.18.7.3; AVM decodeframe.c:4026-4043). */
    uint32_t sb_shift;
    /* Multi-frame stream (single_picture_header_flag == 0). Adds the
     * immediate_output_frame and frame_size_override_flag bits that a single
     * picture header derives (spec 5.18.2 p.98). order_hint occupies eight bits
     * because the multi-frame sequence header writes order_hint_bits_minus_1=7. */
    uint32_t multi_frame;
    uint32_t order_hint; /* eight-bit multi-frame order, ignored for still pictures */
    uint32_t reduced_tx_set; /* M2=2 forces DCT_DCT; M1 regression=0 */
    uint32_t inter_frame;               /* M4 OBU_REGULAR_TILE_GROUP, DPB size 1 */
    uint32_t bit_depth;
    uint32_t chroma_format;
    uint32_t m5_intra_modes; /* private selector, no frame header bits */
    const uint16_t* partition_decisions; /* Borrowed per-SB uniform/mixed depth plan; NULL keeps fixed tree. */
    size_t partition_decision_count;
    uint32_t entropy_adaptation;
    uint32_t enable_mrls; /* Private sequence-matching selector, default off. */
    uint32_t enable_mhccp; /* Optional filter0 multi-parameter chroma-from-luma syntax. */
    const SvtAv2Ccso64Plan* ccso64; /* Borrowed immutable 64-unit plan; NULL is oldoff. */
    uint32_t deblock_y; /* bit0 vertical, bit1 horizontal. UV off. */
    int32_t deblock_delta; /* vertical, df_par_bits=2: -2..1, scale 8. */
    int32_t deblock_delta_h; /* horizontal, inherits V when not signaled. */
} SvtAv2FrameHeaderConfig;

/* Superblock shift values for the three AV2 superblock sizes. */
#define SVT_AV2_FRAME_SB_SHIFT_64  4u
#define SVT_AV2_FRAME_SB_SHIFT_128 5u
#define SVT_AV2_FRAME_SB_SHIFT_256 6u

/* Fills cfg with the frozen M1 defaults for the given frame size:
 * base_q_idx = 100 (non-lossless), 64x64 assumed to be a single superblock. */
void svt_av2_frame_header_config_default(SvtAv2FrameHeaderConfig* cfg,
                                         uint32_t width, uint32_t height);

/* sbCols/sbRows: the frame's superblock grid (spec 5.18.7.3 tile_params();
 * AVM avm/av2/common/tile_common.c:43-57). Exposed so the frame-header writer
 * and its tests derive the tile-loop bit count from one place. */
void svt_av2_frame_header_sb_grid(const SvtAv2FrameHeaderConfig* cfg,
                                  uint32_t* sb_cols,
                                  uint32_t* sb_rows);

/* Number of bits tile_params() emits for the frozen configuration
 * (uniform_tile_spacing_flag = 1, single tile per axis, so every
 * increment_tile_cols_log2 / increment_tile_rows_log2 is 0). Returns
 * SVT_AV2_FRAME_TILE_PARAMS_ERROR (0xFFFFFFFF) for a null cfg. */
uint32_t svt_av2_frame_header_tile_params_bits(const SvtAv2FrameHeaderConfig* cfg);

#define SVT_AV2_FRAME_TILE_PARAMS_ERROR 0xFFFFFFFFu

/* Writes frame_header_info() for the frozen M1 configuration into bw. May be
 * called at any bit position: tile_group_obu() writes it right after the
 * is_first_tile_group bit. Returns SVT_AV2_BW_OK or a negative
 * SvtAv2BitWriterStatus. */
int svt_av2_frame_header_write(SvtAv2BitWriter* bw,
                               const SvtAv2FrameHeaderConfig* cfg);

#ifdef __cplusplus
}
#endif

#endif /* SVT_AV2_FRAME_HEADER_H */
