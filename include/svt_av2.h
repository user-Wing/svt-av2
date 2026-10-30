/*
 * SVT-AV2 - independent AV2 production encoder implementation
 *
 * M4: synchronous scalar All-Intra / low-delay P encoding with real coefficients
 * and caller-owned coded packet/reconstruction buffers. Supported: 8-bit 4:2:0,
 * 10-bit 4:2:0 / 4:4:4, user QP 100, one thread, dimensions divisible by eight.
 */
#ifndef SVT_AV2_H
#define SVT_AV2_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#if defined(SVT_AV2_BUILD_SHARED)
#define SVT_AV2_API __declspec(dllexport)
#elif defined(SVT_AV2_USE_SHARED)
#define SVT_AV2_API __declspec(dllimport)
#else
#define SVT_AV2_API
#endif
#else
#define SVT_AV2_API __attribute__((visibility("default")))
#endif

/* --- version -------------------------------------------------------------- */

#define SVT_AV2_VERSION_MAJOR 0
#define SVT_AV2_VERSION_MINOR 0
#define SVT_AV2_VERSION_PATCH 5
#define SVT_AV2_VERSION_STRING "0.0.5-m5"

/* --- error codes ---------------------------------------------------------- */

typedef enum SvtAv2Error {
    SVT_AV2_ERROR_NONE = 0,
    SVT_AV2_ERROR_INVALID_HANDLE,
    SVT_AV2_ERROR_INVALID_PARAMETER,
    SVT_AV2_ERROR_INVALID_STATE,
    SVT_AV2_ERROR_NOT_IMPLEMENTED,
    SVT_AV2_ERROR_OUT_OF_MEMORY,
    SVT_AV2_ERROR_IO,
    SVT_AV2_ERROR_UNSUPPORTED,
    SVT_AV2_ERROR_INTERNAL
} SvtAv2Error;

SVT_AV2_API const char* svt_av2_error_to_string(SvtAv2Error error);

/* --- basic enums ---------------------------------------------------------- */

typedef enum SvtAv2ChromaFormat {
    SVT_AV2_CHROMA_400 = 0,
    SVT_AV2_CHROMA_420 = 1,
    SVT_AV2_CHROMA_422 = 2,
    SVT_AV2_CHROMA_444 = 3
} SvtAv2ChromaFormat;

typedef enum SvtAv2BitDepth {
    SVT_AV2_BIT_DEPTH_8 = 8,
    SVT_AV2_BIT_DEPTH_10 = 10,
    SVT_AV2_BIT_DEPTH_12 = 12
} SvtAv2BitDepth;

/* Prediction structure; M4 adds synchronous single-reference low-delay P.
 * ALL_INTRA remains the default so existing encoded streams are unchanged. */
typedef enum SvtAv2PredictionStructure {
    SVT_AV2_PRED_ALL_INTRA = 0,
    SVT_AV2_PRED_LOW_DELAY_P = 1
} SvtAv2PredictionStructure;

typedef enum SvtAv2Preset {
    SVT_AV2_PRESET_FASTEST = 12,
    SVT_AV2_PRESET_FAST = 8,
    SVT_AV2_PRESET_SLOW = 4,
    SVT_AV2_PRESET_PLACEBO = 0
} SvtAv2Preset;

/* Optional externally supplied content hint. This is context state, not a
 * member of SvtAv2EncoderConfig; that public structure retains its ABI.
 * No detector is run and no lambda/QP is changed. Only the isolated compiled
 * animation PCW guard may use the hint, in its documented narrow domain. */
typedef enum SvtAv2ContentType {
    SVT_AV2_CONTENT_NONE = 0,
    SVT_AV2_CONTENT_ANIMATION = 1,
    SVT_AV2_CONTENT_REAL = 2
} SvtAv2ContentType;

/* --- opaque handle -------------------------------------------------------- */

typedef struct SvtAv2EncoderContext* SvtAv2Handle;

/* --- configuration -------------------------------------------------------- */

typedef struct SvtAv2EncoderConfig {
    /* Required: source properties. */
    uint32_t           width;
    uint32_t           height;
    SvtAv2ChromaFormat chroma_format;
    SvtAv2BitDepth     bit_depth;
    uint32_t           frame_rate_num;
    uint32_t           frame_rate_den;

    /* M0 keeps the tuning surface deliberately tiny (plan section 4.3). */
    int32_t      qp;      /* Non-lossless CQP 1..255; 10-bit actual qindex = qp+48. */
    SvtAv2Preset preset;
    uint32_t     threads; /* 0 = auto, 1 = single thread */
    int32_t      deterministic;

    /* Output. */
    const char* output_path;

    /* M4: appended configuration field; callers must be rebuilt. */
    SvtAv2PredictionStructure prediction_structure;
    uint32_t intra_mode_selection; /* 0=DC, 1=M5 DC/V/H, 2=basic Y/UV, 3=directional Y + basic UV, 4=directional Y + implicit CfL UV (still + RD). */
    uint32_t partition_selection; /* 0=fixed, 1=64/32, 2=64/32/16, 3=mixed 32/16/8 quadrants, 4=also 16x8/8x16, 5=also six larger rectangles, 6=also mixed 32-quadrant rectangles, 7=also mixed small rectangles, 8=also uniform 8x32/32x8, 9=also mixed thin rectangles, 10=also uniform 8x64/64x8 (2..10 require still + basic/directional + RD) */
    uint32_t rd_mode; /* M6 experimental: 0=off, 1=bounded exhaustive, 2=pruned, 3=fast still intra. */
    uint32_t coeff_optimization; /* Experimental: 0=off, 1=legacy tail RD, 2=bounded level/EOB RDOQ (still + basic/directional/CfL + RD). */
    uint32_t motion_optimization; /* 0=off, 1=integer MV RD, 2=bounded wide RD; excludes coefficient RD. */
    uint32_t entropy_adaptation; /* Frame-local CDF updates; RD combination requires still_picture, motion off and coeff 0/2. */
    uint32_t still_picture; /* Single-picture header; exactly one input picture, All-Intra only. */
    uint32_t transform_selection; /* Appended: 0=DCT-only, 1=binary Y DCT/ADST_ADST, 2=full syntax four-type Y subset; still + basic/directional + RD. */
    uint32_t angle_search; /* Appended: nonzero Y deltas with inherited UV V/H; still + directional/CfL + RD. */
    uint32_t mrl_search; /* Appended: bounded square/rectangle Y multiple-reference-line RD, still + directional/CfL + RD. */
    uint32_t tree_refinement; /* Appended: 1=refine best two basic trees, 2=also preserve seeds before long rectangles; with partition>=8, global four plus two before long and two before thin rectangles. */
    uint32_t rd_lambda_model; /* Appended: 0=original, 1=AVM AI with compile-time scale, 2=AVM AI base x6/4; still + RD. */
    uint32_t cfl_search; /* Appended: 0=implicit only, 1=also bounded classic explicit alpha RD; still + cfl + RD. */
    uint32_t mhccp_search; /* Appended: 0=off, 1=also causal multi-hypothesis cross-component RD; still + cfl + RD. */
    uint32_t intra_joint; /* Appended: 0=off, 1=bounded Y mode/TX joint RD; still + directional/cfl + RD + full TX. */
} SvtAv2EncoderConfig;

typedef struct SvtAv2EncoderInfo {
    uint32_t version_major;
    uint32_t version_minor;
    uint32_t version_patch;
    char     version_string[32];
    char     build_type[32];
    char     compiler[64];
    char     milestone[16];
} SvtAv2EncoderInfo;

/* Last completed frame's M6 instrumentation; all zero when RD is off. */
typedef struct SvtAv2RdStats {
    uint64_t candidate_count, forward_calls, replay_checks;
    uint64_t distortion, rate_q12, cost, lambda_q8;
    uint64_t full_superblocks, partial_superblocks, motion_candidates;
    uint64_t winner_hash;
    uint64_t coeff_trials, coeff_accepts;
    uint64_t motion_trials, motion_accepts;
    uint64_t wide_trials, wide_accepts;
} SvtAv2RdStats;

/* --- input picture ------------------------------------------------------- */

/* A single input picture handed to svt_av2_enc_send_picture().
 *
 * The caller owns all buffers; the encoder only reads them for the duration of
 * the call and must not retain pointers. Plane order is Y, U, V: planes[0] is
 * luma, planes[1] is Cb (U), planes[2] is Cr (V). For 4:0:0 only planes[0] is
 * used and planes[1..2] may be NULL. strides[] are in BYTES per row (for the
 * 8-bit, tightly packed sources this equals the plane width). 10-bit samples
 * are unsigned little-endian 16-bit values with only the low ten bits valid;
 * strides must cover two bytes per visible sample. Unaligned planes are valid.
 *
 * The scalar profile supports single-threaded All-Intra, 8-bit 4:2:0 and
 * 10-bit 4:2:0 / 4:4:4 at user QP 1..255, with non-zero dimensions divisible by
 * eight, at most 1920x1080, and one picture in flight. Other combinations
 * are rejected by the encoder, not silently converted. */
typedef struct SvtAv2Picture {
    const uint8_t*     planes[3];
    uint32_t           strides[3];
    uint32_t           width;
    uint32_t           height;
    SvtAv2ChromaFormat chroma_format;
    SvtAv2BitDepth     bit_depth;
    int32_t            pts;
} SvtAv2Picture;

/* --- M0 parameter surface ------------------------------------------------ */

typedef enum SvtAv2Parameter {
    SVT_AV2_PARAM_WIDTH = 0,
    SVT_AV2_PARAM_HEIGHT,
    SVT_AV2_PARAM_CHROMA_FORMAT,
    SVT_AV2_PARAM_BIT_DEPTH,
    SVT_AV2_PARAM_FRAME_RATE_NUM,
    SVT_AV2_PARAM_FRAME_RATE_DEN,
    SVT_AV2_PARAM_QP,
    SVT_AV2_PARAM_PRESET,
    SVT_AV2_PARAM_THREADS,
    SVT_AV2_PARAM_DETERMINISTIC,
    SVT_AV2_PARAM_OUTPUT_PATH,
    SVT_AV2_PARAM_PREDICTION_STRUCTURE,
    SVT_AV2_PARAM_INTRA_MODE_SELECTION,
    SVT_AV2_PARAM_PARTITION_SELECTION,
    SVT_AV2_PARAM_RD_MODE,
    SVT_AV2_PARAM_COEFF_OPTIMIZATION,
    SVT_AV2_PARAM_MOTION_OPTIMIZATION,
    SVT_AV2_PARAM_ENTROPY_ADAPTATION,
    SVT_AV2_PARAM_STILL_PICTURE,
    SVT_AV2_PARAM_TRANSFORM_SELECTION,
    SVT_AV2_PARAM_ANGLE_SEARCH,
    SVT_AV2_PARAM_MRL_SEARCH,
    SVT_AV2_PARAM_TREE_REFINEMENT,
    SVT_AV2_PARAM_RD_LAMBDA_MODEL,
    SVT_AV2_PARAM_CFL_SEARCH,
    SVT_AV2_PARAM_MHCCP_SEARCH,
    SVT_AV2_PARAM_INTRA_JOINT,
    /* value: const SvtAv2ContentType*. Optional, default NONE; set before init.
     * Unknown values return INVALID_PARAMETER, initialized handles return
     * INVALID_STATE. Stored privately; get_config() retains its old layout.
     * Macro-off builds accept the hint but do not change encoding decisions. */
    SVT_AV2_PARAM_CONTENT_TYPE,
    SVT_AV2_PARAM_COUNT
} SvtAv2Parameter;

/* --- output packet -------------------------------------------------------- */

/* A coded frame handed back by the encoder.
 *
 * Frozen contract: docs/m1-m2-contracts.md section 7. The buffer belongs to the
 * caller and stays valid only until the next API call. `data` and `capacity`
 * are inputs (caller supplies the storage); `size` and `pts` are outputs filled
 * by the encoder. When the stream does not fit in `capacity`, the encoder
 * reports SVT_AV2_ERROR_OUT_OF_MEMORY, sets `size` to the required byte count
 * and writes nothing -- it never truncates silently. */
typedef struct SvtAv2Packet {
    void*   data;     /* caller-owned output buffer (required)                */
    size_t  capacity; /* bytes available at data (in)                         */
    size_t  size;     /* bytes written, or required size when too small (out) */
    int32_t pts;      /* presentation timestamp of the packet (out)           */
} SvtAv2Packet;

/* --- API (plan section 4.3) --------------------------------------------- */

SVT_AV2_API SvtAv2Error svt_av2_enc_init_handle(SvtAv2Handle* handle, const SvtAv2EncoderInfo** info);
SVT_AV2_API SvtAv2Error svt_av2_enc_set_parameter(SvtAv2Handle handle, SvtAv2Parameter param, const void* value);
SVT_AV2_API SvtAv2Error svt_av2_enc_init(SvtAv2Handle handle);
SVT_AV2_API SvtAv2Error svt_av2_enc_send_picture(SvtAv2Handle handle, const void* picture);
SVT_AV2_API SvtAv2Error svt_av2_enc_get_packet(SvtAv2Handle handle, void* packet, int32_t* has_packet);

/* Delivers the coded packet for the frame handed to send_picture().
 *
 * Semantics (contract section 7):
 *   - success: SVT_AV2_ERROR_NONE, size = bytes written, pts = frame pts
 *   - no output yet / after EOS: SVT_AV2_ERROR_NONE with size = 0 (this is how
 *     "nothing available" stays distinguishable from a failure)
 *   - buffer too small: SVT_AV2_ERROR_OUT_OF_MEMORY, size = required bytes, and
 *     no partial data is written
 *   - send_picture() before the pending packet is taken:
 *     SVT_AV2_ERROR_INVALID_STATE (one picture in flight, never overwritten)
 *   - send_picture(NULL) signals EOS; send_picture() after EOS returns
 *     SVT_AV2_ERROR_INVALID_STATE */
SVT_AV2_API SvtAv2Error svt_av2_enc_get_packet_ex(SvtAv2Handle handle, SvtAv2Packet* packet);

SVT_AV2_API SvtAv2Error svt_av2_enc_deinit(SvtAv2Handle* handle);
SVT_AV2_API SvtAv2Error svt_av2_enc_get_rd_stats(SvtAv2Handle handle, SvtAv2RdStats* stats);

/* --- reconstruction export (M2, T013) ------------------------------------- */

/* Descriptor filled by svt_av2_enc_get_recon(). Layout matches
 * SvtAv2ReconExport in Source/Lib/Reconstruction/reconstruction.h (contract
 * docs/m2-m3-contracts.md section 7): the bytes are produced by the single export primitive
 * svt_av2_picture_export_raw(); this project contains no second copy of that
 * packing. */
typedef struct SvtAv2ReconFrame {
    void*    data;           /* caller-owned output buffer (required)          */
    size_t   capacity;       /* bytes available at data (in)                   */
    size_t   byte_count;     /* bytes written, or required size when too small */
    uint32_t frame_index;    /* frame this recon belongs to (out)              */
    uint32_t visible_width;  /* luma samples (out)                             */
    uint32_t visible_height; /* luma rows (out)                                */
    uint32_t chroma_width;   /* visible chroma samples per row (out)          */
    uint32_t chroma_height;  /* visible chroma rows (out)                     */
    SvtAv2BitDepth bit_depth;          /* sample bit depth (out)              */
    SvtAv2ChromaFormat chroma_format;  /* plane geometry (out)                */
} SvtAv2ReconFrame;

/* Exports the visible samples of the reconstruction of the frame most recently
 * taken by svt_av2_enc_get_packet_ex(), as tightly packed Y, U, V raw planes
 * without padding. 8-bit uses one byte per sample; 10-bit uses two bytes in
 * unsigned little-endian order, low ten bits valid. Plane dimensions and
 * sample representation are returned in the metadata fields.
 *
 * Semantics (mirrors SvtAv2Packet, contract section 7.2):
 *   - success: SVT_AV2_ERROR_NONE, byte_count = bytes written, all meta
 *     fields filled
 *   - buffer too small: SVT_AV2_ERROR_OUT_OF_MEMORY, byte_count = required
 *     bytes, and no partial data is written and no meta field is changed
 *   - no usable recon (nothing encoded yet, after EOS, invalid handle):
 *     SVT_AV2_ERROR_INVALID_STATE
 *   - no internal pointer is returned and no I/O happens; `data` stays valid
 *     only until the next API call that produces a new reconstruction. */
SVT_AV2_API SvtAv2Error svt_av2_enc_get_recon(SvtAv2Handle handle, SvtAv2ReconFrame* recon);

/* --- informational ------------------------------------------------------- */

/* Reads the configuration currently held by the handle. M0 helper used by the
 * app and by tests; not part of the long term public contract. */
SVT_AV2_API SvtAv2Error svt_av2_enc_get_config(SvtAv2Handle handle, SvtAv2EncoderConfig* config);

/* Returns a pointer to a static, immutable build information record. */
SVT_AV2_API const SvtAv2EncoderInfo* svt_av2_enc_get_build_info(void);

#ifdef __cplusplus
}
#endif

#endif /* SVT_AV2_H */
