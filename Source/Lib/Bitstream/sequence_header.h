/*
 * SVT-AV2 - minimal AV2 sequence header writer (T004).
 *
 * Scope: the M1 single-frame path (8-bit 4:2:0, single picture header, fixed
 * size, no cropping, no inter tools) plus the H2 multi-frame head writer used
 * by the M2 multi-frame wiring. The writers emit exactly the bytes of
 * sequence_header_obu() for those configurations and hand them to T003's OBU
 * framing. They do not wrap the OBU itself and do not write frame headers.
 * Sizes are derived from the real max_frame_width/height, so 176x144,
 * 1280x720 and 1920x1080 work without changing the frozen 64x64 vectors.
 *
 * Spec references (AV2 Bitstream & Decoding Process Specification v1.0.0):
 *   - 5.4.1 sequence_header_obu(), 5.4.2..5.4.10 the sequence_*_config()
 *     sub-structures, 5.2.1 open_bitstream_unit() obu_extension_flag.
 *   - 4.11.3 uvlc(), 4.11.8 ns(n) - descriptor primitives owned by the T003
 *     BitWriter (declared in bit_writer.h), used by this module.
 *   - 6.2.3 trailing_bits semantics, 6.4.x sequence header semantics.
 *
 * Contract:
 *   - The caller builds the payload with this module, then calls
 *     svt_av2_obu_payload_finalize() (T003) to append the trailing bits, then
 *     svt_av2_obu_write() (T003) to prepend the leb128 size and OBU header.
 *   - Fixed M1 values: chroma_format_idc=0 (420), bit_depth_idc=1 (8-bit),
 *     single_picture_header_flag=1, profile 0, no cropping, no segmentation,
 *     no filter tools. The superblock size and seq_tile_info_present_flag are
 *     parameters so T004/T005 can freeze them without changing the writer.
 */
#ifndef SVT_AV2_SEQUENCE_HEADER_H
#define SVT_AV2_SEQUENCE_HEADER_H

#include <stddef.h>
#include <stdint.h>

#include "bit_writer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- M1-frozen constants (spec Tables 6.2 / 6.3) ------------------------- */

#define SVT_AV2_SEQ_CHROMA_FORMAT_420 0u /* chroma_format_idc */
#define SVT_AV2_SEQ_BIT_DEPTH_IDC_8   1u /* bit_depth_idc: 0=10bit, 1=8bit */

#define SVT_AV2_SEQ_PROFILE_MAIN_420_10_IP0 0u /* Table A.1 */

/* seq_tier is written only when seq_level_idx >= this and the stream is not a
 * single picture (spec 5.4.1; AVM avm/av2/decoder/obu.c:559-567). */
#define SVT_AV2_SEQ_TIER_PRESENT_LEVEL 4u

/* seq_enabled_motion_modes[] has one f(1) per mode from INTERINTRA up to
 * MOTION_MODES-1 (AVM avm/av2/decoder/decodeframe.c:6018-6028). With
 * SIMPLE_TRANSLATION=0, INTERINTRA=1, MOTION_MODES=5
 * (avm/av2/common/enums.h), that is 4 flags. The PDF text layer does not cover
 * this line, so the count is pinned from AVM. */
#define SVT_AV2_SEQ_MOTION_MODE_FLAGS 4u

/* seq_max_drl_bits_minus_1 uses ns(MAX_MAX_DRL_BITS - MIN_MAX_DRL_BITS + 1)
 * with MIN_MAX_DRL_BITS = 1 and MAX_MAX_DRL_BITS = MAX_REF_MV_STACK_SIZE - 1 = 5,
 * i.e. ns(5) (AVM decodeframe.c:6067-6070, enums.h). */
#define SVT_AV2_SEQ_MAX_DRL_BITS_MINUS_1_N 5u

/* seq_max_bvp_drl_bits_minus_1 uses ns(MAX_MAX_IBC_DRL_BITS -
 * MIN_MAX_IBC_DRL_BITS + 1) with MIN_MAX_IBC_DRL_BITS = 1 and
 * MAX_MAX_IBC_DRL_BITS = MAX_REF_BV_STACK_SIZE - 1 = 3, i.e. ns(3)
 * (AVM decodeframe.c:6073-6077). */
#define SVT_AV2_SEQ_MAX_REF_BV_STACK_SIZE 4u
#define SVT_AV2_SEQ_MAX_IBC_DRL_BITS_MINUS_1_N 3u

/* First reserved level index; 0..21 are levels 2.0..8.3, 22..30 reserved,
 * 31 = Maximum parameters (spec Table A.8; AVM is_valid_seq_level_idx). */
#define SVT_AV2_SEQ_LEVEL_RESERVED_START 22u
#define SVT_AV2_SEQ_LEVEL_MAX            31u

/* Lowest level that can hold the given frame size (spec Table A.8, "Level
 * limits": max_picture_size / max_h_size / max_v_size must all be satisfied;
 * AVM avm/av2/common/level.c:36-260, av2_level_defs). Returns
 * SVT_AV2_SEQ_LEVEL_NONE when no level in 0..21 admits the size.
 *
 * This is a size-only bound: a real encoder also has to satisfy the level's
 * decoder/display rate limits, which depend on the frame rate and are outside
 * this module's inputs. */
#define SVT_AV2_SEQ_LEVEL_NONE 0xFFFFFFFFu

uint32_t svt_av2_sequence_header_min_level_for_size(uint32_t width,
                                                    uint32_t height);

/* ---- encoder-side configuration ----------------------------------------- */

typedef struct SvtAv2SequenceHeaderConfig {
    uint32_t seq_header_id;              /* uvlc, < MAX_SEQ_NUM (16)        */
    uint32_t seq_profile_idc;            /* f(5), 0..31                     */
    uint32_t seq_level_idx;              /* f(5), Table A.8; <22 or 31      */
    uint32_t max_frame_width;            /* >= 1                            */
    uint32_t max_frame_height;           /* >= 1                            */
    /* 1 => 256x256 superblock, 0 => use_128x128_superblock decides. */
    uint32_t use_256x256_superblock;
    /* Only meaningful when use_256x256_superblock == 0. */
    uint32_t use_128x128_superblock;
    /* 0 => single tile (the decoder derives the tile grid from the frame). */
    uint32_t seq_tile_info_present_flag;
    uint32_t low_delay_p;               /* M4 one-slot explicit reference map */
    uint32_t bit_depth;                  /* 8 or 10 */
    uint32_t chroma_format;              /* 1=420, 3=444 (public enum) */
    uint32_t chroma_dct_only;            /* I1 UV modes require the implemented DCT path. */
    uint32_t enable_cfl_intra;           /* M6 implicit CfL, filter index zero. */
    uint32_t enable_mrls; /* Optional Y multi-reference-line syntax. */
    uint32_t enable_mhccp; /* Optional filter0 multi-parameter chroma-from-luma syntax. */
    uint32_t enable_ccso64; /* Internal forced64 BO-only; default0. */
} SvtAv2SequenceHeaderConfig;

/* Fills cfg with the frozen M1 defaults for the given frame size:
 * profile 0, single picture, 8-bit 4:2:0, 64x64 superblock, no tile info.
 * seq_header_id and seq_level_idx are set to 0; the caller may override them. */
void svt_av2_sequence_header_config_default(SvtAv2SequenceHeaderConfig* cfg,
                                            uint32_t width, uint32_t height);

/* Writes sequence_header_obu() into bw. bw must be byte aligned. Does NOT
 * append the obu_extension_flag or trailing bits (T003's payload_finalize
 * does). Returns SVT_AV2_BW_OK or a negative SvtAv2BitWriterStatus. */
int svt_av2_sequence_header_write(SvtAv2BitWriter* bw,
                                  const SvtAv2SequenceHeaderConfig* cfg);

/* Writes sequence_header_obu() for a multi-frame sequence (H2 wiring for
 * T013). Emits the same byte layout as svt_av2_sequence_header_write() except
 * that single_picture_header_flag is 0 and the fields the specification only
 * defines for that case are present: seq_lcr_id, still_picture,
 * max_tlayer_id, max_mlayer_id (spec 5.4.1) plus the sequence_inter_config()
 * / sequence_scc_config() / sequence_transform_quant_entropy_config() /
 * sequence_filter_config() fields that a single picture header infers.
 *
 * Fixed minimal multi-frame choices (justified field-by-field in
 * docs/t015-header-field-reference.md section 3.3):
 *   seq_lcr_id=0, still_picture=0, max_tlayer_id=0, max_mlayer_id=0 (so
 *   seq_max_mlayer_cnt_minus_1 is not written), monotonic_output_order_flag=1,
 *   seq_initial_display_delay_present_flag=0, decoder_model_info_present_flag=0,
 *   order_hint_bits_minus_1=0, explicit_ref_frame_map=0, enable_bru=0,
 *   enable_short_refresh_frame_flags=0, enable_avg_cdf=1 + avg_cdf_type=1,
 *   seq_choose_screen_content_tools=1 + seq_choose_integer_mv=1,
 *   cdef_on_skip_txfm_always_on=0 (=> ADAPTIVE).
 * Everything else stays at the frozen M1 value.
 *
 * cfg->seq_level_idx must already be valid for the size. bw must be byte
 * aligned. Same return contract as svt_av2_sequence_header_write(). */
int svt_av2_sequence_header_write_multi(SvtAv2BitWriter* bw,
                                       const SvtAv2SequenceHeaderConfig* cfg);

#ifdef __cplusplus
}
#endif

#endif /* SVT_AV2_SEQUENCE_HEADER_H */
