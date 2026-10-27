/*
 * SVT-AV2 - M1 keyframe encoder entry point (T005). See keyframe.h.
 *
 * This is the gen.c harness logic promoted into the library, with the fixed M1
 * configuration and the input-plane interface the application needs. It reuses
 * only the public T003/T004/T005 writers; it never hand-assembles OBU bytes.
 *
 * Capacity policy (T013, M1->M2 round): no fixed-size stack buffer stands in
 * for the coded picture any more. The tile-group payload is built in a heap
 * scratch buffer whose size is derived from the caller's `out_cap`, and the
 * sequence header scratch is sized from the payload itself (the writer is run
 * once as a counting writer). The M1 code used a 32-byte stack array for the
 * tile payload and a 512-byte stack array for the whole stream; those constants
 * were only ever valid for the fixed 21-byte M1 stream. When a payload does not
 * fit, the writers report SVT_AV2_BW_ERROR_NO_SPACE instead of truncating.
 */
#include "keyframe.h"

#include <stdlib.h>
#include <string.h>

#include "bit_writer.h"
#include "frame_header.h"
#include "obu_writer.h"
#include "sequence_header.h"
#include "tile_writer.h"

#include "cdf_tables.h"      /* svt_av2_cdf_coeff_q_ctx */
#include "entropy_encoder.h" /* SVT_AV2_ENTROPY_ERROR_RANGE */

/* The three OBU headers plus the fixed 22-bit frame header cannot fit in less
 * than this, so a smaller request is a caller error rather than a "no space"
 * case; the public header documents the minimum. */
#define SVT_AV2_KEYFRAME_MIN_OUT_CAP 16u

/* Probe buffer for the sequence header payload. The M1 sequence header is a
 * handful of bytes and is a frozen structure, so this bound is checked (a
 * larger payload fails instead of being truncated) rather than assumed. */
#define SVT_AV2_KEYFRAME_SEQ_PROBE 64u

int svt_av2_encode_keyframe_64x64(const uint8_t* y, size_t y_stride,
                                  const uint8_t* u, size_t u_stride,
                                  const uint8_t* v, size_t v_stride,
                                  uint32_t base_q_idx, uint8_t* out,
                                  size_t out_cap, size_t* out_size) {
    (void)y;
    (void)y_stride;
    (void)u;
    (void)u_stride;
    (void)v;
    (void)v_stride;

    if (out == NULL || out_size == NULL) {
        return SVT_AV2_BW_ERROR_NULL;
    }
    *out_size = 0;
    if (out_cap < SVT_AV2_KEYFRAME_MIN_OUT_CAP) {
        return SVT_AV2_BW_ERROR_NO_SPACE;
    }
    if (base_q_idx == 0u || base_q_idx > 255u) {
        return SVT_AV2_BW_ERROR_RANGE;
    }
    /* The M1 all-zero CDFs are tabulated for COEFF_CDF_Q_CTXS index 1 only. */
    if (svt_av2_cdf_coeff_q_ctx(base_q_idx) != 1u) {
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    }

    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, out, out_cap);

    /* 1. temporal delimiter OBU (no payload). */
    int st = svt_av2_obu_write(&bw, SVT_AV2_OBU_TEMPORAL_DELIMITER, 0, NULL, 0);
    if (st != SVT_AV2_BW_OK) {
        return st;
    }

    /* 2. sequence header OBU (64x64, level 0, 8-bit 4:2:0).
     *
     * The writer is first run into a scratch buffer to learn the exact payload
     * size, then re-run into a buffer of exactly that size. This removes the
     * fixed 512-byte M1 scratch constant without inventing a bound for the
     * coded picture (the tile payload, the part that actually grows, is sized
     * from the caller's `out_cap` below). */
    SvtAv2BitWriter            sw;
    SvtAv2SequenceHeaderConfig scfg;
    svt_av2_sequence_header_config_default(&scfg, 64, 64);

    uint8_t seq_probe[SVT_AV2_KEYFRAME_SEQ_PROBE];
    svt_av2_bit_writer_init(&sw, seq_probe, sizeof(seq_probe));
    st = svt_av2_sequence_header_write(&sw, &scfg);
    if (st == SVT_AV2_BW_OK) {
        st = svt_av2_obu_payload_finalize(&sw);
    }
    if (st != SVT_AV2_BW_OK) {
        return st;
    }
    const size_t seq_size = svt_av2_bit_writer_bytes_written(&sw);
    if (seq_size > sizeof(seq_probe)) {
        return SVT_AV2_BW_ERROR_NO_SPACE;
    }

    uint8_t* seq = (uint8_t*)malloc(seq_size);
    if (seq == NULL) {
        return SVT_AV2_BW_ERROR_NO_SPACE;
    }
    svt_av2_bit_writer_init(&sw, seq, seq_size);
    st = svt_av2_sequence_header_write(&sw, &scfg);
    if (st == SVT_AV2_BW_OK) {
        st = svt_av2_obu_payload_finalize(&sw);
    }
    if (st != SVT_AV2_BW_OK) {
        free(seq);
        return st;
    }
    st = svt_av2_obu_write(&bw, SVT_AV2_OBU_SEQUENCE_HEADER, 0, seq, seq_size);
    free(seq);
    if (st != SVT_AV2_BW_OK) {
        return st;
    }

    /* 3. closed loop key OBU = tile_group_obu().
     *
     * The payload is built in its own scratch buffer and then handed to
     * svt_av2_obu_write(), which copies it. The scratch is sized from the
     * caller's remaining output capacity, so a caller that sized `out` from the
     * real payload is never defeated by a hidden fixed bound. */
    SvtAv2FrameHeaderConfig fcfg;
    svt_av2_frame_header_config_default(&fcfg, 64, 64);
    fcfg.base_q_idx = base_q_idx;

    const size_t header_bytes = svt_av2_bit_writer_bytes_written(&bw);
    const size_t framing_max =
        SVT_AV2_BIT_WRITER_MAX_LEB128_BYTES + svt_av2_obu_header_size(SVT_AV2_OBU_CLOSED_LOOP_KEY, 0);
    if (header_bytes + framing_max >= out_cap) {
        return SVT_AV2_BW_ERROR_NO_SPACE;
    }

    const size_t tile_scratch_cap = out_cap - header_bytes - framing_max;
    uint8_t*     tile = (uint8_t*)malloc(tile_scratch_cap);
    if (tile == NULL) {
        return SVT_AV2_BW_ERROR_NO_SPACE;
    }

    size_t tile_size = 0;
    st = svt_av2_tile_writer_write_closed_loop_key(&fcfg, tile, tile_scratch_cap, &tile_size);
    if (st != 0) {
        free(tile);
        return st;
    }
    if (tile_size == 0 || tile_size > tile_scratch_cap) {
        free(tile);
        return SVT_AV2_BW_ERROR_NO_SPACE;
    }

    st = svt_av2_obu_write(&bw, SVT_AV2_OBU_CLOSED_LOOP_KEY, 0, tile, tile_size);
    free(tile);
    if (st != SVT_AV2_BW_OK) {
        return st;
    }

    *out_size = svt_av2_bit_writer_bytes_written(&bw);
    return 0;
}

/* ==========================================================================
 * Multi-frame sequence (S4, T013)
 *
 * The pieces below are the multi-frame counterparts of the single-picture
 * path above. They deliberately reuse the same bit writers, frame header and
 * tile writer: the only differences are single_picture_header_flag (0) and the
 * fact that the sequence header is emitted once instead of per frame.
 * ========================================================================== */

/* Shared tail: emit one closed_loop_key OBU into `bw` for `width` x `height`.
 * `multi_frame` selects the frame-header variant. `out_cap` is only used to
 * bound the tile scratch; the OBU writer copies, so nothing is truncated. */
static int write_closed_loop_key_obu(SvtAv2BitWriter* bw, uint32_t width,
                                     uint32_t height, uint32_t base_q_idx,
                                     uint32_t multi_frame, size_t out_cap) {
    SvtAv2FrameHeaderConfig fcfg;
    svt_av2_frame_header_config_default(&fcfg, width, height);
    fcfg.base_q_idx = base_q_idx;
    fcfg.multi_frame = multi_frame;

    const size_t header_bytes = svt_av2_bit_writer_bytes_written(bw);
    const size_t framing_max =
        SVT_AV2_BIT_WRITER_MAX_LEB128_BYTES +
        svt_av2_obu_header_size(SVT_AV2_OBU_CLOSED_LOOP_KEY, 0);
    if (header_bytes + framing_max >= out_cap) {
        return SVT_AV2_BW_ERROR_NO_SPACE;
    }

    const size_t tile_scratch_cap = out_cap - header_bytes - framing_max;
    uint8_t*     tile = (uint8_t*)malloc(tile_scratch_cap);
    if (tile == NULL) {
        return SVT_AV2_BW_ERROR_NO_SPACE;
    }

    size_t tile_size = 0;
    int st = svt_av2_tile_writer_write_closed_loop_key(&fcfg, tile,
                                                       tile_scratch_cap, &tile_size);
    if (st != 0) {
        free(tile);
        return st;
    }
    if (tile_size == 0 || tile_size > tile_scratch_cap) {
        free(tile);
        return SVT_AV2_BW_ERROR_NO_SPACE;
    }

    st = svt_av2_obu_write(bw, SVT_AV2_OBU_CLOSED_LOOP_KEY, 0, tile, tile_size);
    free(tile);
    return st;
}

static int encode_sequence_header_ex(uint32_t width, uint32_t height,
                                         uint32_t bit_depth, uint32_t chroma_format, uint32_t low_delay_p,
                                         uint32_t still_picture, uint32_t chroma_dct_only, uint32_t enable_cfl_intra, uint32_t enable_mrls, uint32_t enable_mhccp, uint32_t enable_ccso64,
                                         uint8_t* out, size_t out_cap,
                                         size_t* out_size) {
    if (out == NULL || out_size == NULL) {
        return SVT_AV2_BW_ERROR_NULL;
    }
    *out_size = 0;
    if (width == 0u || height == 0u) {
        return SVT_AV2_BW_ERROR_RANGE;
    }

    SvtAv2SequenceHeaderConfig scfg;
    svt_av2_sequence_header_config_default(&scfg, width, height);
    scfg.bit_depth = bit_depth;
    scfg.chroma_format = chroma_format;
    scfg.low_delay_p = low_delay_p;
    scfg.chroma_dct_only = chroma_dct_only;
    scfg.enable_cfl_intra = enable_cfl_intra;
    scfg.enable_mrls = enable_mrls;
    scfg.enable_mhccp = enable_mhccp;
    scfg.enable_ccso64 = enable_ccso64;
    scfg.seq_profile_idc = chroma_format == 3u ? 4u : 0u;
    /* The caller must have verified a level exists; a size with no level is
     * refused rather than written with a bogus seq_level_idx. */
    const uint32_t level = svt_av2_sequence_header_min_level_for_size(width, height);
    if (level == SVT_AV2_SEQ_LEVEL_NONE) {
        return SVT_AV2_BW_ERROR_RANGE;
    }
    scfg.seq_level_idx = level;

    /* Probe the exact sequence-header size first, then write into a buffer of
     * exactly that size, exactly as the single-picture path does. */
    SvtAv2BitWriter sw;
    uint8_t         seq_probe[SVT_AV2_KEYFRAME_SEQ_PROBE];
    svt_av2_bit_writer_init(&sw, seq_probe, sizeof(seq_probe));
    int st = still_picture ? svt_av2_sequence_header_write(&sw, &scfg) :
                            svt_av2_sequence_header_write_multi(&sw, &scfg);
    if (st == SVT_AV2_BW_OK) {
        st = svt_av2_obu_payload_finalize(&sw);
    }
    if (st != SVT_AV2_BW_OK) {
        return st;
    }
    const size_t seq_size = svt_av2_bit_writer_bytes_written(&sw);
    if (seq_size > sizeof(seq_probe)) {
        return SVT_AV2_BW_ERROR_NO_SPACE;
    }

    uint8_t* seq = (uint8_t*)malloc(seq_size);
    if (seq == NULL) {
        return SVT_AV2_BW_ERROR_NO_SPACE;
    }
    svt_av2_bit_writer_init(&sw, seq, seq_size);
    st = still_picture ? svt_av2_sequence_header_write(&sw, &scfg) :
                        svt_av2_sequence_header_write_multi(&sw, &scfg);
    if (st == SVT_AV2_BW_OK) {
        st = svt_av2_obu_payload_finalize(&sw);
    }
    if (st != SVT_AV2_BW_OK) {
        free(seq);
        return st;
    }

    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, out, out_cap);
    st = svt_av2_obu_write(&bw, SVT_AV2_OBU_TEMPORAL_DELIMITER, 0, NULL, 0);
    if (st == SVT_AV2_BW_OK) {
        st = svt_av2_obu_write(&bw, SVT_AV2_OBU_SEQUENCE_HEADER, 0, seq, seq_size);
    }
    free(seq);
    if (st != SVT_AV2_BW_OK) {
        return st;
    }

    *out_size = svt_av2_bit_writer_bytes_written(&bw);
    return 0;
}

int svt_av2_encode_sequence_header_multi(uint32_t width, uint32_t height,
    uint8_t* out, size_t out_cap, size_t* out_size) {
    return encode_sequence_header_ex(width, height, 8u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, out, out_cap, out_size);
}

int svt_av2_encode_keyframe_64x64_multi(const uint8_t* y, size_t y_stride,
                                        const uint8_t* u, size_t u_stride,
                                        const uint8_t* v, size_t v_stride,
                                        uint32_t base_q_idx, uint8_t* out,
                                        size_t out_cap, size_t* out_size) {
    /* Same accepted-but-unused plane contract as the single-picture path: this
     * profile is All-Intra/DC_PRED with no residual coding yet. */
    (void)y;
    (void)y_stride;
    (void)u;
    (void)u_stride;
    (void)v;
    (void)v_stride;

    if (out == NULL || out_size == NULL) {
        return SVT_AV2_BW_ERROR_NULL;
    }
    *out_size = 0;
    if (out_cap < SVT_AV2_KEYFRAME_MIN_OUT_CAP) {
        return SVT_AV2_BW_ERROR_NO_SPACE;
    }
    if (base_q_idx == 0u || base_q_idx > 255u) {
        return SVT_AV2_BW_ERROR_RANGE;
    }
    if (svt_av2_cdf_coeff_q_ctx(base_q_idx) != 1u) {
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    }

    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, out, out_cap);

    /* No temporal delimiter per frame: the sequence's single leading delimiter
     * covers every frame in the temporal unit. */
    const int st = write_closed_loop_key_obu(&bw, 64u, 64u, base_q_idx, 1u, out_cap);
    if (st != SVT_AV2_BW_OK) {
        return st;
    }

    *out_size = svt_av2_bit_writer_bytes_written(&bw);
    return 0;
}

static int encode_packet_ccso64(uint32_t width, uint32_t height,
    uint32_t bit_depth, uint32_t chroma_format,
    uint32_t base_q_idx, uint32_t frame_index, const SvtAv2TileCoeffBlock* blocks,
    size_t block_count, const SvtAv2TileBlockMode* modes, size_t mode_count, uint32_t low_delay_p,
    uint32_t m5_intra_modes, const uint16_t* partition_decisions, size_t partition_decision_count,
    uint32_t entropy_adaptation, uint32_t still_picture, uint32_t transform_selection, uint32_t enable_mrls, uint32_t enable_mhccp,
    const SvtAv2Ccso64Plan* ccso64, uint32_t deblock_y, int32_t deblock_delta, int32_t deblock_delta_h, uint8_t* out, size_t out_cap, size_t* out_size) {
    if (out_size == NULL || out == NULL) return SVT_AV2_BW_ERROR_NULL;
    *out_size = 0;
    size_t prefix = 0;
    int st = 0;
    if (frame_index == 0u) {
        st = encode_sequence_header_ex(width, height, bit_depth, chroma_format, low_delay_p,
            still_picture, m5_intra_modes >= 2u, m5_intra_modes == 4u, enable_mrls, enable_mhccp, ccso64 != NULL, out, out_cap, &prefix);
        if (st != 0) return st;
    } else {
        SvtAv2BitWriter delimiter;
        svt_av2_bit_writer_init(&delimiter, out, out_cap);
        st = svt_av2_obu_write(&delimiter, SVT_AV2_OBU_TEMPORAL_DELIMITER, 0, NULL, 0);
        if (st != 0) return st;
        prefix = svt_av2_bit_writer_bytes_written(&delimiter);
    }
    if (out_cap <= prefix + 16u) return SVT_AV2_BW_ERROR_NO_SPACE;
    const size_t tile_cap = out_cap - prefix - 16u;
    uint8_t* tile = (uint8_t*)malloc(tile_cap);
    if (tile == NULL) return SVT_AV2_BW_ERROR_NO_SPACE;
    SvtAv2FrameHeaderConfig cfg;
    svt_av2_frame_header_config_default(&cfg, width, height);
    cfg.base_q_idx = base_q_idx;
    cfg.multi_frame = !still_picture;
    cfg.inter_frame = low_delay_p && frame_index != 0u;
    cfg.order_hint = frame_index & 255u;
    cfg.reduced_tx_set = transform_selection == 2u ? 0u : transform_selection ? 1u : 2u;
    cfg.bit_depth = bit_depth;
    cfg.chroma_format = chroma_format;
    cfg.m5_intra_modes = m5_intra_modes;
    cfg.entropy_adaptation = entropy_adaptation;
    cfg.enable_mrls = enable_mrls;
    cfg.enable_mhccp = enable_mhccp;
    cfg.ccso64 = ccso64;
    cfg.deblock_y = deblock_y;cfg.deblock_delta = deblock_delta;cfg.deblock_delta_h = deblock_delta_h;
    cfg.partition_decisions = partition_decisions;
    cfg.partition_decision_count = partition_decision_count;
    size_t tile_size = 0;
    st = cfg.inter_frame ? svt_av2_tile_writer_write_inter_coeffs(&cfg, blocks, block_count,
        modes, mode_count, tile, tile_cap, &tile_size) :
        svt_av2_tile_writer_write_closed_loop_key_coeffs(&cfg, blocks, block_count,
        tile, tile_cap, &tile_size, NULL);
    if (st == 0) {
        SvtAv2BitWriter bw;
        svt_av2_bit_writer_init(&bw, out + prefix, out_cap - prefix);
        st = svt_av2_obu_write(&bw, cfg.inter_frame ? SVT_AV2_OBU_REGULAR_TILE_GROUP : SVT_AV2_OBU_CLOSED_LOOP_KEY, 0, tile, tile_size);
        if (st == 0) *out_size = prefix + svt_av2_bit_writer_bytes_written(&bw);
    }
    free(tile);
    return st;
}

static int encode_packet(uint32_t width, uint32_t height,
    uint32_t bit_depth, uint32_t chroma_format, uint32_t base_q_idx,
    uint32_t frame_index, const SvtAv2TileCoeffBlock* blocks, size_t block_count,
    const SvtAv2TileBlockMode* modes, size_t mode_count, uint32_t low_delay_p,
    uint32_t intra_modes, const uint16_t* decisions, size_t decision_count,
    uint32_t entropy, uint32_t still, uint32_t transform, uint32_t mrl, uint32_t mh,
    uint8_t* out, size_t out_cap, size_t* out_size) {
    return encode_packet_ccso64(width,height,bit_depth,chroma_format,base_q_idx,
        frame_index,blocks,block_count,modes,mode_count,low_delay_p,intra_modes,
        decisions,decision_count,entropy,still,transform,mrl,mh,NULL,0u,0,0,out,out_cap,out_size);
}

int svt_av2_encode_intra_packet_ex(uint32_t width, uint32_t height,
    uint32_t bit_depth, uint32_t chroma_format, uint32_t base_q_idx,
    uint32_t frame_index, const SvtAv2TileCoeffBlock* blocks, size_t block_count,
    uint8_t* out, size_t out_cap, size_t* out_size) {
    return encode_packet(width,height,bit_depth,chroma_format,base_q_idx,frame_index,
        blocks,block_count,NULL,0u,0u,0u,NULL,0u,0u,0u,0u,0u,0u,out,out_cap,out_size);
}
int svt_av2_encode_intra_packet_modes(uint32_t width, uint32_t height,
    uint32_t bit_depth, uint32_t chroma_format, uint32_t base_q_idx,
    uint32_t frame_index, const SvtAv2TileCoeffBlock* blocks, size_t block_count,
    uint8_t* out, size_t out_cap, size_t* out_size) {
    return encode_packet(width,height,bit_depth,chroma_format,base_q_idx,frame_index,
        blocks,block_count,NULL,0u,0u,1u,NULL,0u,0u,0u,0u,0u,0u,out,out_cap,out_size);
}
int svt_av2_encode_low_delay_packet(uint32_t width, uint32_t height,
    uint32_t bit_depth, uint32_t chroma_format, uint32_t base_q_idx,
    uint32_t frame_index, const SvtAv2TileCoeffBlock* blocks, size_t block_count,
    const SvtAv2TileBlockMode* modes, size_t mode_count,
    uint8_t* out, size_t out_cap, size_t* out_size) {
    return encode_packet(width,height,bit_depth,chroma_format,base_q_idx,frame_index,
        blocks,block_count,modes,mode_count,1u,0u,NULL,0u,0u,0u,0u,0u,0u,out,out_cap,out_size);
}

int svt_av2_encode_low_delay_packet_modes(uint32_t width, uint32_t height,
    uint32_t bit_depth, uint32_t chroma_format, uint32_t base_q_idx,
    uint32_t frame_index, const SvtAv2TileCoeffBlock* blocks, size_t block_count,
    const SvtAv2TileBlockMode* modes, size_t mode_count, uint32_t intra_modes,
    uint8_t* out, size_t out_cap, size_t* out_size) {
    return encode_packet(width,height,bit_depth,chroma_format,base_q_idx,frame_index,
        blocks,block_count,modes,mode_count,1u,intra_modes,NULL,0u,0u,0u,0u,0u,0u,out,out_cap,out_size);
}

int svt_av2_encode_packet_tools(uint32_t width, uint32_t height,
    uint32_t bit_depth, uint32_t chroma_format, uint32_t base_q_idx,
    uint32_t frame_index, const SvtAv2TileCoeffBlock* blocks, size_t block_count,
    const SvtAv2TileBlockMode* modes, size_t mode_count, uint32_t low_delay_p,
    uint32_t intra_modes, const uint16_t* partition_decisions, size_t decision_count,
    uint8_t* out, size_t out_cap, size_t* out_size) {
    return encode_packet(width,height,bit_depth,chroma_format,base_q_idx,frame_index,
        blocks,block_count,modes,mode_count,low_delay_p,intra_modes,
        partition_decisions,decision_count,0u,0u,0u,0u,0u,out,out_cap,out_size);
}

int svt_av2_encode_packet_tools_entropy(uint32_t width, uint32_t height,
    uint32_t bit_depth, uint32_t chroma_format, uint32_t base_q_idx,
    uint32_t frame_index, const SvtAv2TileCoeffBlock* blocks, size_t block_count,
    const SvtAv2TileBlockMode* modes, size_t mode_count, uint32_t low_delay_p,
    uint32_t intra_modes, const uint16_t* partition_decisions, size_t decision_count,
    uint32_t entropy_adaptation, uint8_t* out, size_t out_cap, size_t* out_size) {
    return encode_packet(width,height,bit_depth,chroma_format,base_q_idx,frame_index,
        blocks,block_count,modes,mode_count,low_delay_p,intra_modes,
        partition_decisions,decision_count,entropy_adaptation,0u,0u,0u,0u,out,out_cap,out_size);
}

int svt_av2_encode_still_picture(uint32_t width, uint32_t height,
    uint32_t bit_depth, uint32_t chroma_format, uint32_t base_q_idx,
    const SvtAv2TileCoeffBlock* blocks, size_t block_count, uint32_t intra_modes,
    const uint16_t* partition_decisions, size_t decision_count, uint32_t entropy_adaptation,
    uint8_t* out, size_t out_cap, size_t* out_size) {
    return encode_packet(width,height,bit_depth,chroma_format,base_q_idx,0u,
        blocks,block_count,NULL,0u,0u,intra_modes,
        partition_decisions,decision_count,entropy_adaptation,1u,0u,0u,0u,out,out_cap,out_size);
}

int svt_av2_encode_still_picture_tx(uint32_t width, uint32_t height,
    uint32_t bit_depth, uint32_t chroma_format, uint32_t base_q_idx,
    const SvtAv2TileCoeffBlock* blocks, size_t block_count, uint32_t intra_modes,
    const uint16_t* partition_decisions, size_t decision_count, uint32_t entropy_adaptation,
    uint32_t transform_selection, uint8_t* out, size_t out_cap, size_t* out_size) {
    if (transform_selection > 2u || (transform_selection && intra_modes < 2u))
        return SVT_AV2_BW_ERROR_RANGE;
    return encode_packet(width,height,bit_depth,chroma_format,base_q_idx,0u,
        blocks,block_count,NULL,0u,0u,intra_modes,partition_decisions,decision_count,
        entropy_adaptation,1u,transform_selection,0u,0u,out,out_cap,out_size);
}

int svt_av2_encode_intra_packet(uint32_t width, uint32_t height,
    uint32_t base_q_idx, uint32_t frame_index, const SvtAv2TileCoeffBlock* blocks,
    size_t block_count, uint8_t* out, size_t out_cap, size_t* out_size) {
    return svt_av2_encode_intra_packet_ex(width, height, 8u, 1u, base_q_idx,
        frame_index, blocks, block_count, out, out_cap, out_size);
}

int svt_av2_encode_still_picture_mrls(uint32_t width, uint32_t height,
    uint32_t bit_depth, uint32_t chroma_format, uint32_t base_q_idx,
    const SvtAv2TileCoeffBlock* blocks, size_t block_count, uint32_t intra_modes,
    const uint16_t* partition_decisions, size_t decision_count, uint32_t entropy_adaptation,
    uint32_t transform_selection, uint32_t enable_mrls,
    uint8_t* out, size_t out_cap, size_t* out_size) {
    if (enable_mrls > 1u || transform_selection > 2u ||
        (transform_selection && intra_modes < 2u) || (enable_mrls && intra_modes < 3u))
        return SVT_AV2_BW_ERROR_RANGE;
    return encode_packet(width,height,bit_depth,chroma_format,base_q_idx,0u,
        blocks,block_count,NULL,0u,0u,intra_modes,partition_decisions,decision_count,
        entropy_adaptation,1u,transform_selection,enable_mrls,0u,out,out_cap,out_size);
}

int svt_av2_encode_still_picture_mhccp(uint32_t width, uint32_t height,
    uint32_t bit_depth, uint32_t chroma_format, uint32_t base_q_idx,
    const SvtAv2TileCoeffBlock* blocks, size_t block_count, uint32_t intra_modes,
    const uint16_t* partition_decisions, size_t decision_count, uint32_t entropy_adaptation,
    uint32_t transform_selection, uint32_t enable_mrls, uint32_t enable_mhccp,
    uint8_t* out, size_t out_cap, size_t* out_size) {
    if (enable_mhccp > 1u || (enable_mhccp && intra_modes != 4u) || enable_mrls > 1u ||
        transform_selection > 2u || (transform_selection && intra_modes < 2u) ||
        (enable_mrls && intra_modes < 3u)) return SVT_AV2_BW_ERROR_RANGE;
    return encode_packet(width,height,bit_depth,chroma_format,base_q_idx,0u,
        blocks,block_count,NULL,0u,0u,intra_modes,partition_decisions,decision_count,
        entropy_adaptation,1u,transform_selection,enable_mrls,enable_mhccp,out,out_cap,out_size);
}

int svt_av2_ccso64_header_size(const SvtAv2FrameHeaderConfig* cfg,SvtAv2Ccso64HeaderSize* size) {
    if (cfg==NULL || size==NULL) return SVT_AV2_BW_ERROR_NULL;
    memset(size,0,sizeof(*size));
    if (cfg->multi_frame || cfg->inter_frame || cfg->sb_shift!=4u) return SVT_AV2_BW_ERROR_RANGE;
    uint8_t data[256];size_t prefix=0u;
    int st=encode_sequence_header_ex(cfg->frame_width,cfg->frame_height,cfg->bit_depth,
        cfg->chroma_format,0u,1u,cfg->m5_intra_modes>=2u,cfg->m5_intra_modes==4u,
        cfg->enable_mrls,cfg->enable_mhccp,cfg->ccso64!=NULL,data,sizeof(data),&prefix);
    SvtAv2BitWriter bw;svt_av2_bit_writer_init(&bw,data,sizeof(data));
    if (st==0) st=svt_av2_bit_writer_write_bit(&bw,1u);
    if (st==0) st=svt_av2_frame_header_write(&bw,cfg);
    const size_t bits=svt_av2_bit_writer_bits_written(&bw);
    if (st==0) st=svt_av2_bit_writer_byte_alignment(&bw);
    if (st==0) {size->sequence_bytes=prefix;size->frame_bits=bits;size->frame_bytes=svt_av2_bit_writer_bytes_written(&bw);}
    return st;
}
int svt_av2_encode_still_picture_ccso64_packet(const SvtAv2FrameHeaderConfig* cfg,
    const SvtAv2TileCoeffBlock* blocks,size_t count,uint8_t* out,size_t capacity,size_t* bytes) {
    if (cfg==NULL) return SVT_AV2_BW_ERROR_NULL;
    if (cfg->multi_frame || cfg->inter_frame || cfg->sb_shift!=4u || cfg->reduced_tx_set>2u)
        return SVT_AV2_BW_ERROR_RANGE;
    const uint32_t transform=cfg->reduced_tx_set==0u?2u:cfg->reduced_tx_set==1u?1u:0u;
    return encode_packet_ccso64(cfg->frame_width,cfg->frame_height,cfg->bit_depth,cfg->chroma_format,
        cfg->base_q_idx,0u,blocks,count,NULL,0u,0u,cfg->m5_intra_modes,cfg->partition_decisions,
        cfg->partition_decision_count,cfg->entropy_adaptation,1u,transform,cfg->enable_mrls,
        cfg->enable_mhccp,cfg->ccso64,cfg->deblock_y,cfg->deblock_delta,cfg->deblock_delta_h,out,capacity,bytes);
}

int svt_av2_encode_still_picture_ccso64(uint32_t width, uint32_t height,
    uint32_t bit_depth, uint32_t chroma_format, uint32_t base_q_idx,
    const SvtAv2TileCoeffBlock* blocks, size_t block_count, uint32_t intra_modes,
    const uint16_t* decisions, size_t decision_count, uint32_t entropy,
    uint32_t transform, uint32_t mrl, uint32_t mh,
    const SvtAv2Ccso64Plan* ccso64, SvtAv2FrameBuffer* recon,
    uint8_t* out, size_t out_cap, size_t* out_size) {
    if (out_size == NULL) return SVT_AV2_BW_ERROR_NULL;
    *out_size = 0u;
    if (svt_av2_ccso64_validate(ccso64,width,height,bit_depth,chroma_format) != 0 ||
        svt_av2_ccso64_picture_validate(ccso64,recon) != 0 ||
        (ccso64 && (recon->bit_depth != bit_depth || recon->visible_width != width || recon->visible_height != height)) ||
        mh > 1u || (mh && intra_modes != 4u) || mrl > 1u || transform > 2u ||
        (transform && intra_modes < 2u) || (mrl && intra_modes < 3u)) return SVT_AV2_BW_ERROR_RANGE;
    const int st = encode_packet_ccso64(width,height,bit_depth,chroma_format,
        base_q_idx,0u,blocks,block_count,NULL,0u,0u,intra_modes,decisions,
        decision_count,entropy,1u,transform,mrl,mh,ccso64,0u,0,0,out,out_cap,out_size);
    if (st == 0) svt_av2_ccso64_apply(ccso64,recon);
    return st;
}
