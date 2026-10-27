/*
 * SVT-AV2 - M1 closed loop key frame writer (T005).
 *
 * Scope: assembles the single tile-group OBU of the M1 single-frame stream,
 * i.e. tile_group_obu() for a 64x64 CLOSED_LOOP_KEY picture with one 64x64
 * superblock and a single tile:
 *
 *   f(1) is_first_tile_group           = 1
 *   frame_header()                     (T005 frame_header.c, 22 bits)
 *   byte_alignment()                   (spec 5.2.4)
 *   tile_group_payload():
 *     <symbol decoder bytes>           (T005 block_writer + entropy_encoder)
 *
 * Because NumTiles == 1 there is no tile_start_and_end_present_flag and no
 * tile_size field, and there is no per-tile header byte to skip. The symbol
 * decoder bytes are produced by the range encoder's final stage, which already
 * emits the trailing bits required by exit_symbol(): the caller must NOT append
 * svt_av2_obu_payload_finalize() trailing bits on top of them.
 *
 * Spec references (AV2 Bitstream & Decoding Process Specification v1.0.0):
 *   - 5.2.6 tile_group_obu()
 *   - 5.2.4 byte_alignment()
 *   - 6.18 tile group semantics
 *   - 6.17 frame_header_info()
 */
#ifndef SVT_AV2_TILE_WRITER_H
#define SVT_AV2_TILE_WRITER_H

#include <stddef.h>
#include <stdint.h>

#include "frame_header.h"
#include "motion_syntax.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Payload size bound for the M1 64x64 configuration (is_first_tile_group +
 * 22-bit frame header + alignment + a few entropy bytes). Kept as the M1
 * regression constant; size non-M1 frames with
 * svt_av2_tile_writer_max_payload_bytes() instead. The tile writer no longer
 * allocates any buffer from this value: entropy scratch is sized from the
 * caller's `out_cap`. */
#define SVT_AV2_TILE_PAYLOAD_MAX_BYTES 32u

/* Upper bound on the tile_group_obu() payload length, in bytes, for one frame
 * of `width` x `height` luma samples with a single tile and one superblock per
 * tile. The bound is derived from the sample count, not hard-coded to a pixel
 * size, so callers (T013) can size their packet buffer from the geometry they
 * already have.
 *
 * The bound is deliberately conservative: it is the header allowance plus one
 * coded byte per luma sample, which dominates any coefficient payload this
 * configuration can produce. */
size_t svt_av2_tile_writer_max_payload_bytes(uint32_t width, uint32_t height);

/* Optional caller-provided scratch for the entropy coder. Both regions are
 * written only while the range coder is running and are never read after
 * svt_av2_entropy_encoder_done() returns; supplying them removes the fixed
 * stack buffers from svt_av2_tile_writer_write_closed_loop_key() so that large
 * frames do not need a proportional amount of stack.
 *
 * `coded`  : one byte per emitted payload byte.
 * `precarry`: one uint16_t per emitted payload byte plus 2 entries of carry
 *             slack (see entropy_encoder.h).
 */
typedef struct SvtAv2TileWriterScratch {
    uint8_t*  coded;
    size_t    coded_cap;
    uint16_t* precarry;
    size_t    precarry_cap;
} SvtAv2TileWriterScratch;

/* M2 coding-block order: luma followed immediately by its U and V blocks.
 * Coordinates and transform dimensions are in plane samples. qcoeff is a
 * borrowed packed raster grid, min(width,32) * min(height,32) int32 values,
 * valid until this synchronous call returns. */
typedef struct SvtAv2TileCoeffBlock {
    uint32_t plane, x, y, width, height;
    const int32_t* qcoeff;
    uint8_t intra_mode; /* 0=DC,1=V,2=H,4=smooth,5=smoothV,6=smoothH,7=paeth; U/V share one mode. */
    uint8_t tx_type; /* 0=DCT_DCT, 1=ADST_DCT, 2=DCT_ADST, 3=tuned ADST_ADST; Y 4/8/16 squares or full-syntax 8x16/16x8. */
    uint8_t mrl; /* Y: index in low two bits; bit2 averages nearest line. */
    /* In UV mode14, the Y block owns both UV parameters; ignored otherwise.
     * flag0 retains implicit CfL/index1.
     * flag1 uses classic index0; idx=(abs(U)-1)<<3 | (abs(V)-1),
     * with zero-plane magnitude bits zero and joint_sign=3*signU+signV-1. */
    uint8_t cfl_explicit, cfl_alpha_idx, cfl_alpha_signs;
    uint8_t mhccp; /* UV mode14: 0=old CfL, 1..3=MHCCP shared direction+1. */
} SvtAv2TileCoeffBlock;

/* M6 rate session. One frame, raster SB order. Trials share the actual tile
 * syntax writer but do not emit bytes. Every noncommitted/error trial restores
 * all neighbour/motion state. Single-picture adaptive trials also copy the
 * committed CDF bank into independent scratch and commit only a successful
 * winner replay. R uses 4096 units/estimated bit, excludes the
 * fixed frame/OBU headers and range-coder finalization. No per-trial allocation.
 * Blocks/modes are borrowed only for the call. partition is the current SB's
 * validated canonical plan decision (raw0..3282). commit advances exactly one SB on success. */
typedef struct SvtAv2TileRateSession SvtAv2TileRateSession;
int svt_av2_tile_rate_create(const SvtAv2FrameHeaderConfig* cfg,
                             SvtAv2TileRateSession** session);
void svt_av2_tile_rate_free(SvtAv2TileRateSession* session);
int svt_av2_tile_rate_trial(SvtAv2TileRateSession* session, size_t sb_index,
    uint16_t partition, const SvtAv2TileCoeffBlock* blocks, size_t block_count,
    const SvtAv2TileBlockMode* modes, size_t mode_count, int commit,
    uint64_t* rate_q12);

typedef struct SvtAv2TileRatePrefix SvtAv2TileRatePrefix;
int svt_av2_tile_rate_prefix_create(SvtAv2TileRatePrefix** prefix);
void svt_av2_tile_rate_prefix_free(SvtAv2TileRatePrefix* prefix);
/* Current cumulative inlet rate of an active internal prefix transaction. */
uint64_t svt_av2_tile_rate_prefix_current_rate(const SvtAv2TileRatePrefix* prefix);
int svt_av2_tile_rate_prefix_begin(SvtAv2TileRatePrefix* prefix,
    const SvtAv2TileRateSession* session, size_t sb_index, uint16_t partition,
    size_t* leaf_count);
int svt_av2_tile_rate_prefix_trial(SvtAv2TileRatePrefix* prefix, size_t leaf,
    const SvtAv2TileCoeffBlock blocks[3], int commit, uint64_t* rate_q12);
/* Scratch-only cumulative prefix + current Y mode/MRL/coefficients; no UV. */
int svt_av2_tile_rate_prefix_trial_y(SvtAv2TileRatePrefix* prefix,size_t leaf,
    const SvtAv2TileCoeffBlock* y,uint64_t* rate_q12);
int svt_av2_tile_rate_prefix_finish(const SvtAv2TileRatePrefix* prefix, uint64_t* rate_q12);

/* Independent full-frame replay through the same writer for tests/audits. */
int svt_av2_tile_rate_frame(const SvtAv2FrameHeaderConfig* cfg,
    const SvtAv2TileCoeffBlock* blocks, size_t block_count,
    const SvtAv2TileBlockMode* modes, size_t mode_count, uint64_t* rate_q12);

int svt_av2_tile_writer_write_closed_loop_key_coeffs(
    const SvtAv2FrameHeaderConfig* cfg, const SvtAv2TileCoeffBlock* blocks,
    size_t block_count, uint8_t* out, size_t out_cap, size_t* out_size,
    const SvtAv2TileWriterScratch* scratch);

int svt_av2_tile_writer_write_inter_coeffs(
    const SvtAv2FrameHeaderConfig* cfg, const SvtAv2TileCoeffBlock* blocks,
    size_t block_count, const SvtAv2TileBlockMode* modes, size_t mode_count,
    uint8_t* out, size_t out_cap, size_t* out_size);

/* Builds the tile_group_obu() payload for the M1 64x64 CLOSED_LOOP_KEY picture
 * into out (capacity out_cap). On success *out_size holds the payload length.
 *
 * cfg carries the frozen M1 frame parameters; see frame_header.h. The entropy
 * CDF context is derived from cfg->base_q_idx.
 *
 * On failure out is left untouched and *out_size is set to the buffer size the
 * payload would have needed, so a caller can retry with a larger buffer
 * instead of guessing. Nothing is ever truncated silently.
 *
 * Returns 0 on success, or a negative status:
 *   - SvtAv2BitWriterStatus for NULL/size/alignment problems,
 *   - SvtAv2EntropyStatus for encoder errors. */
int svt_av2_tile_writer_write_closed_loop_key(const SvtAv2FrameHeaderConfig* cfg,
                                              uint8_t* out,
                                              size_t   out_cap,
                                              size_t*  out_size);

/* As svt_av2_tile_writer_write_closed_loop_key(), but the entropy scratch is
 * caller-owned (see SvtAv2TileWriterScratch). This avoids the scratch
 * allocation the default entry performs; it has the same output bytes, same
 * failure semantics, and the same *out_size-on-failure contract.
 *
 * The default entry sizes its scratch from `out_cap`, so it is never limited by
 * a fixed constant; use this variant only to avoid the allocation. */
int svt_av2_tile_writer_write_closed_loop_key_scratch(
    const SvtAv2FrameHeaderConfig* cfg, uint8_t* out, size_t out_cap,
    size_t* out_size, const SvtAv2TileWriterScratch* scratch);

#ifdef __cplusplus
}
#endif

#endif /* SVT_AV2_TILE_WRITER_H */
