/*
 * SVT-AV2 internal codec definitions.
 *
 * M0 scope: encoder context skeleton and the state machine that guards the
 * public API. No AV2 syntax is produced in this milestone.
 */
#ifndef SVT_AV2_CODEC_H
#define SVT_AV2_CODEC_H

#include "common.h"
#include "svt_av2.h"          /* public API: SvtAv2ReconFrame, SvtAv2Handle   */
#include "picture.h"          /* SvtAv2FrameBuffer, SvtAv2BorrowedView (T007) */
#include "reconstruction.h"   /* SvtAv2ReconScratch, recon export (T012)      */
#include "tile_writer.h"
#include "reference_frame.h"
#include "transform_path.h"   /* SvtAv2TransformPathState, init (T012)        */

typedef enum SvtAv2EncoderState {
    SVT_AV2_STATE_UNINITIALIZED = 0, /* handle allocated, not configured */
    SVT_AV2_STATE_CONFIGURED,        /* parameters validated */
    SVT_AV2_STATE_INITIALIZED,       /* encoder ready to receive pictures */
    SVT_AV2_STATE_CLOSED
} SvtAv2EncoderState;

typedef struct SvtAv2EncoderContext {
    uint32_t           magic;
    SvtAv2EncoderState state;
    SvtAv2EncoderConfig config;
    uint32_t           parameter_mask; /* bitmask of SvtAv2Parameter already set */
    uint32_t           pictures_sent;
    uint32_t           pictures_encoded;
    int                fatal_error;

    /* --- M1->M2 (T013) synchronous single-frame pipeline state -------------
     *
     * The encoder is strictly synchronous and single threaded: at most one
     * picture is "in flight" between send_picture() and get_packet(). A second
     * send_picture() before the pending packet is taken is refused with
     * SVT_AV2_ERROR_INVALID_STATE rather than silently overwriting the frame.
     *
     * `frame_source` is the encoder-owned copy of the submitted picture, in an
     * ALLOCATED SvtAv2FrameBuffer layout (padded rows, stride align_up(w,32)),
     * because the M2 encoder core consumes allocated pictures through T007's
     * adaptation entry (svt_av2_picture_view/copy_from_view). The caller's
     * planes are only read during send_picture() (contract section 1.2).
     * `packet` is the encoded stream for that frame and stays valid until the
     * next API call that produces a new one (section 7). */
    SvtAv2FrameBuffer frame_source; /* pending input picture, owned; allocated */
    uint32_t frame_width;
    uint32_t frame_height;
    int32_t  frame_pts;
    int      frame_pending;   /* 1 = a picture awaits get_packet() */
    int      eos;             /* 1 = send_picture(NULL) / flush was called */
    int      eos_reported;    /* 1 = the EOS "no packet" was already returned */
    uint8_t* packet;          /* last encoded stream, owned */
    size_t   packet_size;
    size_t   packet_capacity;
    int      packet_pending;  /* 1 = packet holds an untaken stream */

    /* M2 uses one frame core for coefficients and internal reconstruction.
     * Hook copies retain the packed qcoeff until this frame's tile is written.
     * Buffers are owned by the context and reused for the configured geometry. */
    SvtAv2FrameBuffer   recon;        /* owned; one per encoder, per size */
    SvtAv2ReconScratch  scratch;      /* owned; reused across frames */
    SvtAv2TransformPathState path_state; /* owned; reused across frames */
    int                 recon_ready;  /* 1 = recon + scratch are live */
    SvtAv2TileCoeffBlock* coded_blocks;
    SvtAv2TileBlockMode* coded_modes;
    uint32_t m5_last_intra_mode;
    SvtAv2RdStats rd_stats;
    uint16_t* partition_decisions;
    size_t partition_decision_capacity;
    size_t partition_decision_count;
    SvtAv2ReferenceFrame reference;
    int32_t* coded_coeffs;
    size_t coded_block_capacity, coded_coeff_capacity;
    size_t coded_block_count, coded_coeff_count;
    uint32_t            recon_frame_index; /* index of the exported recon frame */
} SvtAv2EncoderContext;

/* Releases every buffer owned by the context (pending frame and packet) and
 * resets the pipeline flags. Used by deinit() and by init() on reconfigure;
 * safe to call more than once. */
void svt_av2_context_release_pipeline(SvtAv2EncoderContext* context);

/* Internal helpers shared between the API layer and the app. */

/* Applies default values for every optional field. */
void svt_av2_config_set_defaults(SvtAv2EncoderConfig* config);

/* Validates the configuration; returns SVT_AV2_ERROR_NONE when usable. */
SvtAv2Error svt_av2_config_validate(const SvtAv2EncoderConfig* config);

/* Builds the M0 placeholder "bitstream" payload for one picture.
 *
 * This is NOT AV2 syntax. It is a documented placeholder that exists only so
 * that the pipeline (app -> library -> output file -> verification tooling) can
 * be exercised end to end while M1's bit writer does not exist yet. It is
 * intentionally shaped like a container so that a decoder-side sanity check can
 * reject it: the payload starts with the ASCII magic "SVT2M0" and contains the
 * frame index, geometry and a payload length.
 *
 * Owner: caller supplied buffer. Lifetime: valid until the caller frees it.
 *
 * M1 onwards: the encoder no longer uses this. send_picture()/get_packet() now
 * produce real AV2 syntax (see encoder.c and Bitstream/keyframe.c). The helper
 * is kept only because tests/unit/test_api.c and the M0 record reference it;
 * it is not reachable from the encoder pipeline. */
SvtAv2Error svt_av2_m0_build_placeholder_payload(const SvtAv2EncoderConfig* config,
                                                 uint32_t frame_index,
                                                 uint8_t* out_buffer,
                                                 size_t out_capacity,
                                                 size_t* out_size);

/* Records the result of encoding one picture through the real AV2 path.
 *
 * Called by the API layer once keyframe.c has produced `stream`. It takes
 * ownership of a private copy of the bytes, so the caller's buffer can be
 * reused immediately. Returns SVT_AV2_ERROR_OUT_OF_MEMORY when the copy cannot
 * be made; in that case no packet is published and the frame stays pending, so
 * the caller cannot observe a partially stored packet. */
SvtAv2Error svt_av2_context_store_packet(SvtAv2EncoderContext* context,
                                         const uint8_t* stream,
                                         size_t stream_size,
                                         int32_t pts);

/* All-Intra video packets share one sequence header; each begins a temporal
 * unit. There is no one-frame still-picture cap on the production API. */
#define SVT_AV2_MAX_ENCODABLE_FRAMES UINT32_MAX

/* --- packet delivery (T013) ------------------------------------------------
 *
 * `SvtAv2Packet` and `svt_av2_enc_get_packet_ex()` are declared in the public
 * header include/svt_av2.h (applied by 00 on 2026-10-07; see contracts
 * section 7). The old `svt_av2_enc_get_packet(handle, void*, int32_t*)`
 * signature has no capacity input and no length output, so it cannot deliver
 * bytes without risking a caller buffer overrun and cannot report "space too
 * small". It is retained for the M0 call sequence but now only reports
 * availability. */

#define SVT_AV2_M0_PLACEHOLDER_MAGIC "SVT2M0"
#define SVT_AV2_M0_PLACEHOLDER_MAGIC_SIZE 6
#define SVT_AV2_M0_PLACEHOLDER_HEADER_SIZE 24

/* --- reconstruction export (T013, M2) --------------------------------------
 *
 * SvtAv2ReconFrame and svt_av2_enc_get_recon() are declared in the public
 * header include/svt_av2.h (landed by 00 on 2026-10-07, this milestone's
 * design). The accessor hands the caller the visible samples of the
 * reconstruction of the most recently taken frame, in the tight 8-bit 4:2:0
 * layout that tools/compare_recon.py reads. The bytes come from T012's single
 * export primitive svt_av2_recon_export_frame(), so this project keeps exactly
 * one packing implementation (contract section 6).
 *
 * Structure and semantics intentionally mirror SvtAv2Packet: caller-owned
 * buffer, `capacity` as input, and a too-small buffer reports the required size
 * without writing. The definition below is the accessor's body, kept in this
 * translation unit; the public prototype lives in the public header. */

#endif /* SVT_AV2_CODEC_H */
