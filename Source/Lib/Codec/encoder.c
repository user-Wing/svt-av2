#ifndef M6_SATD_ANGLES
#define M6_SATD_ANGLES 0
#endif
#ifndef M6_DEBLOCK
#define M6_DEBLOCK 0
#endif
#ifndef M6_DEBLOCK_TEST
#define M6_DEBLOCK_TEST 0
#endif
#ifndef M6_PC_WIENER
#define M6_PC_WIENER 0
#endif
#ifndef M6_CONTENT_GUARD
#define M6_CONTENT_GUARD 0
#endif
#ifndef M6_PC_WIENER_TEST
#define M6_PC_WIENER_TEST 0
#endif

/*
 * SVT-AV2 encoder API implementation.
 *
 * M0 implemented only the handle lifecycle, parameter storage and the state
 * machine. M1 onwards (this file, owned by T013) additionally runs the real
 * encode path: send_picture() validates and copies the caller's planes,
 * get_packet() returns the AV2 elementary stream produced by Bitstream/
 * keyframe.c. The CLI does not have a second encode path any more; it drives
 * this API (contract section 7).
 *
 * Threading note: the encoder is single threaded and synchronous by definition
 * (plan section 4.2 / 12.1). No internal worker threads are created, so no
 * locking is required. This is a deliberate limitation, not an oversight.
 */

#include "codec.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "keyframe.h"
#include "rd_frame.h"
#include "deblock_luma.h"
#if M6_PC_WIENER
#include "../Restoration/pc_wiener.h"
#if M6_CONTENT_GUARD
#include "content_filter_guard.h"
#endif
#endif
#include "inter_frame.h"
#include "bit_writer.h" /* SVT_AV2_BW_ERROR_NO_SPACE */
#include "intra_frame.h" /* svt_av2_intra_frame_encode (T012), M2 recon */

#define SVT_AV2_CONTEXT_MAGIC 0x53564132u /* "SVA2" */

/* Runs T012's intra frame core for the pending frame, filling the context's
 * reconstruction picture. Defined after encode_pending_frame(); declared here
 * so that call site reads top-down. */
static SvtAv2Error encode_reconstruction(SvtAv2EncoderContext* context,
                                         uint32_t width,
                                         uint32_t height);

/* QP / base_q_idx validation range.
 *
 * M0 used [0,63] as a placeholder. The M0-M1 contract section 6 (frozen by
 * T004 against AV2 v1.0.0) records base_q_idx for 8-bit as f(8), i.e. the legal
 * range is [0,255] (MaxQ = MAXQ_8_BITS), NOT AV1's [0,63]. The API now accepts
 * the full legal range. This only widens what is accepted; it does not decide
 * any encoding behaviour - the M1 fixed QP value is chosen by T005. */
#define SVT_AV2_QP_MIN 0
#define SVT_AV2_QP_MAX 255

/* --- defaults ------------------------------------------------------------- */

void svt_av2_config_set_defaults(SvtAv2EncoderConfig* config) {
    if (config == NULL) {
        return;
    }
    memset(config, 0, sizeof(*config));
    config->chroma_format = SVT_AV2_CHROMA_420;
    config->bit_depth = SVT_AV2_BIT_DEPTH_8;
    config->frame_rate_num = 25;
    config->frame_rate_den = 1;
    config->qp = 100;
    config->preset = SVT_AV2_PRESET_SLOW;
    config->threads = 1;
    config->deterministic = 1;
    config->output_path = NULL;
    config->prediction_structure = SVT_AV2_PRED_ALL_INTRA;
    config->intra_mode_selection = 0u;
    config->partition_selection = 0u;
    config->rd_mode = 0u;
    config->coeff_optimization = 0u;
    config->motion_optimization = 0u;
    config->entropy_adaptation = 0u;
    config->still_picture = 0u;
    config->transform_selection = 0u;
    config->angle_search = 0u;
    config->mrl_search = 0u;
    config->tree_refinement = 0u;
    config->rd_lambda_model = 0u;
    config->cfl_search = 0u;
    config->mhccp_search = 0u;
    config->intra_joint = 0u;
}

/* --- validation ----------------------------------------------------------- */

static int is_valid_chroma_format(SvtAv2ChromaFormat format) {
    return format == SVT_AV2_CHROMA_400 || format == SVT_AV2_CHROMA_420 || format == SVT_AV2_CHROMA_422 ||
           format == SVT_AV2_CHROMA_444;
}

static int is_valid_bit_depth(SvtAv2BitDepth bit_depth) {
    return bit_depth == SVT_AV2_BIT_DEPTH_8 || bit_depth == SVT_AV2_BIT_DEPTH_10 || bit_depth == SVT_AV2_BIT_DEPTH_12;
}

static int is_valid_preset(SvtAv2Preset preset) {
    return preset == SVT_AV2_PRESET_FASTEST || preset == SVT_AV2_PRESET_FAST || preset == SVT_AV2_PRESET_SLOW ||
           preset == SVT_AV2_PRESET_PLACEBO;
}

SvtAv2Error svt_av2_config_validate(const SvtAv2EncoderConfig* config) {
    if (config == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (config->width == 0 || config->height == 0) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (!is_valid_chroma_format(config->chroma_format)) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (!is_valid_bit_depth(config->bit_depth)) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (config->frame_rate_num == 0 || config->frame_rate_den == 0) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (config->qp < SVT_AV2_QP_MIN || config->qp > SVT_AV2_QP_MAX) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (!is_valid_preset(config->preset)) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (config->output_path == NULL || config->output_path[0] == '\0') {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    if (config->intra_mode_selection > 4u) return SVT_AV2_ERROR_INVALID_PARAMETER;
    if (config->intra_mode_selection >= 2u && (!config->still_picture || !config->rd_mode ||
        config->coeff_optimization == 1u || config->motion_optimization))
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    if (config->partition_selection > 10u || (config->partition_selection >= 2u &&
        config->intra_mode_selection < 2u)) return SVT_AV2_ERROR_INVALID_PARAMETER;
    if (config->rd_mode > 3u) return SVT_AV2_ERROR_INVALID_PARAMETER;
    if (config->rd_mode==3u && (!config->still_picture || config->intra_mode_selection<3u ||
        config->partition_selection>7u || config->intra_joint || config->coeff_optimization==1u ||
        config->motion_optimization)) return SVT_AV2_ERROR_INVALID_PARAMETER;
    if (config->rd_lambda_model > 2u || (config->rd_lambda_model &&
        (!config->still_picture || !config->rd_mode))) return SVT_AV2_ERROR_INVALID_PARAMETER;
    if (config->cfl_search > 1u || (config->cfl_search &&
        (!config->still_picture || !config->rd_mode || config->intra_mode_selection != 4u)))
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    if (config->mhccp_search > 1u || (config->mhccp_search &&
        (!config->still_picture || !config->rd_mode || config->intra_mode_selection != 4u)))
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    if (config->intra_joint > 1u || (config->intra_joint &&
        (config->intra_mode_selection < 3u || config->transform_selection != 2u)))
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    if (config->angle_search > 1u || (config->angle_search && (!config->still_picture ||
        !config->rd_mode || config->intra_mode_selection < 3u || config->motion_optimization)))
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    if (config->tree_refinement > 2u || (config->tree_refinement && (!config->still_picture ||
        !config->rd_mode || config->intra_mode_selection < 2u || config->partition_selection < 2u)))
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    if (config->mrl_search > 1u || (config->mrl_search && (!config->still_picture ||
        !config->rd_mode || config->intra_mode_selection < 3u || config->motion_optimization)))
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    if (config->transform_selection > 2u || (config->transform_selection &&
        (!config->still_picture || !config->rd_mode || config->intra_mode_selection < 2u ||
            config->coeff_optimization == 1u || config->motion_optimization)))
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    if (config->coeff_optimization > 2u || (config->coeff_optimization && !config->rd_mode) ||
        (config->coeff_optimization == 2u && (!config->still_picture || config->intra_mode_selection < 2u)))
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    if (config->motion_optimization > 2u ||
        (config->motion_optimization && (!config->rd_mode || config->coeff_optimization)))
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    if (config->entropy_adaptation > 1u || (config->entropy_adaptation && config->rd_mode &&
        (!config->still_picture || config->coeff_optimization == 1u || config->motion_optimization)))
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    if (config->still_picture > 1u || (config->still_picture &&
        config->prediction_structure != SVT_AV2_PRED_ALL_INTRA))
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    if (config->prediction_structure != SVT_AV2_PRED_ALL_INTRA &&
        config->prediction_structure != SVT_AV2_PRED_LOW_DELAY_P)
        return SVT_AV2_ERROR_INVALID_PARAMETER;

    /* M0 guard rails: reject geometry that would overflow the sample counter or
     * that no realistic source provides. Keeps later milestones honest. */
    uint32_t lw = 0;
    uint32_t lh = 0;
    uint32_t cw = 0;
    uint32_t ch = 0;
    if (svt_av2_compute_plane_dimensions(config->width, config->height, config->chroma_format, &lw, &lh, &cw, &ch) !=
        SVT_AV2_ERROR_NONE) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    size_t samples = 0;
    if (svt_av2_compute_frame_sample_count(config->width, config->height, config->chroma_format, &samples) !=
        SVT_AV2_ERROR_NONE) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    return SVT_AV2_ERROR_NONE;
}

/* --- pipeline buffers (T013) ---------------------------------------------- */

void svt_av2_context_release_pipeline(SvtAv2EncoderContext* context) {
    if (context == NULL) {
        return;
    }
    /* The pending source picture is an allocated SvtAv2FrameBuffer; its free
     * leaves the struct zeroed and safe for a later alloc. */
    svt_av2_picture_free(&context->frame_source);
    context->frame_width = 0;
    context->frame_height = 0;
    context->frame_pts = 0;
    context->frame_pending = 0;
    context->eos = 0;
    context->eos_reported = 0;

    svt_av2_reference_release(&context->reference);
    free(context->partition_decisions); context->partition_decisions = NULL;
    context->partition_decision_capacity = context->partition_decision_count = 0;
    free(context->coded_modes); context->coded_modes = NULL;
    free(context->coded_blocks);
    free(context->coded_coeffs);
    context->coded_blocks = NULL;
    context->coded_coeffs = NULL;
    context->coded_block_capacity = context->coded_coeff_capacity = 0;
    free(context->packet);
    context->packet = NULL;
    context->packet_size = 0;
    context->packet_capacity = 0;
    context->packet_pending = 0;

    /* The recon picture and the block pool are owned by the context. Both
     * teardown calls are safe on a zeroed (never-initialised) member. */
    if (context->recon_ready) {
        svt_av2_recon_pool_free(&context->scratch);
        svt_av2_picture_free(&context->recon);
        context->recon_ready = 0;
    }
}

SvtAv2Error svt_av2_context_store_packet(SvtAv2EncoderContext* context,
                                         const uint8_t* stream,
                                         size_t stream_size,
                                         int32_t pts) {
    (void)pts;
    if (context == NULL || stream == NULL || stream_size == 0) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    /* Grow the packet buffer geometrically so a multi-frame run does not
     * reallocate on every frame, while never trusting a fixed capacity. */
    if (context->packet_capacity < stream_size) {
        uint8_t* grown = (uint8_t*)realloc(context->packet, stream_size);
        if (grown == NULL) {
            return SVT_AV2_ERROR_OUT_OF_MEMORY;
        }
        context->packet = grown;
        context->packet_capacity = stream_size;
    }
    memcpy(context->packet, stream, stream_size);
    context->packet_size = stream_size;
    context->packet_pending = 1;
    return SVT_AV2_ERROR_NONE;
}

/* --- input picture validation (contract section 7) ------------------------ */

/* Validates a submitted picture against the frozen M1 configuration.
 *
 * This does NOT accept an arbitrary void* and pretend to check it. It verifies
 * every field the contract requires (section 7): the picture metadata must
 * match the configured stream, the planes must be non-NULL and their strides
 * must be able to hold the visible plane width. Stride is in BYTES per row.
 *
 * The returned state describes which of Y/U/V are required so the caller can
 * report the exact plane that failed. */
static SvtAv2Error validate_picture(const SvtAv2EncoderContext* context,
                                    const SvtAv2Picture* picture,
                                    uint32_t* chroma_width,
                                    uint32_t* chroma_height) {
    const SvtAv2EncoderConfig* config = &context->config;

    if (picture->width == 0 || picture->height == 0) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    /* The picture must describe the same stream the handle was configured for.
     * M1 is fixed-format, so a mismatch is a request for something unsupported,
     * not something to silently convert. */
    if (picture->width != config->width || picture->height != config->height) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (picture->chroma_format != config->chroma_format) {
        return SVT_AV2_ERROR_UNSUPPORTED;
    }
    if (picture->bit_depth != config->bit_depth) {
        return SVT_AV2_ERROR_UNSUPPORTED;
    }

    if (!((picture->bit_depth == SVT_AV2_BIT_DEPTH_8 && picture->chroma_format == SVT_AV2_CHROMA_420) ||
          (picture->bit_depth == SVT_AV2_BIT_DEPTH_10 &&
           (picture->chroma_format == SVT_AV2_CHROMA_420 || picture->chroma_format == SVT_AV2_CHROMA_444))))
        return SVT_AV2_ERROR_UNSUPPORTED;

    SvtAv2ChromaFormat format = picture->chroma_format;
    uint32_t           lw = 0, lh = 0, cw = 0, ch = 0;
    if (svt_av2_compute_plane_dimensions(picture->width, picture->height, format, &lw, &lh, &cw, &ch) !=
        SVT_AV2_ERROR_NONE) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    (void)lw;
    (void)lh;
    *chroma_width = cw;
    *chroma_height = ch;

    /* Every plane must be present and readable for its visible width. A stride
     * smaller than the visible width would make the encoder read outside the
     * caller's rows; the contract forbids "detecting" capacity, so anything we
     * cannot prove is rejected here. */
    const uint32_t bytes = picture->bit_depth == SVT_AV2_BIT_DEPTH_10 ? 2u : 1u;
    if (picture->planes[0] == NULL || picture->strides[0] < picture->width * bytes) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (picture->planes[1] == NULL || picture->strides[1] < cw * bytes) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (picture->planes[2] == NULL || picture->strides[2] < cw * bytes) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    if (bytes == 2u) {
        for (uint32_t plane = 0; plane < 3u; ++plane) {
            const uint32_t w = plane == 0u ? picture->width : cw;
            const uint32_t h = plane == 0u ? picture->height : ch;
            for (uint32_t y = 0; y < h; ++y) {
                const uint8_t* row = picture->planes[plane] + (size_t)y * picture->strides[plane];
                for (uint32_t x = 0; x < w; ++x)
                    if (((uint32_t)row[2u*x] | ((uint32_t)row[2u*x+1u] << 8u)) > 1023u)
                        return SVT_AV2_ERROR_INVALID_PARAMETER;
            }
        }
    }

    /* Reject geometry whose tightly packed size would overflow. */
    size_t samples = 0;
    if (svt_av2_compute_frame_sample_count(picture->width, picture->height, format, &samples) !=
        SVT_AV2_ERROR_NONE) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    return SVT_AV2_ERROR_NONE;
}

/* --- error strings -------------------------------------------------------- */

const char* svt_av2_error_to_string(SvtAv2Error error) {
    switch (error) {
    case SVT_AV2_ERROR_NONE: return "SVT_AV2_ERROR_NONE";
    case SVT_AV2_ERROR_INVALID_HANDLE: return "SVT_AV2_ERROR_INVALID_HANDLE";
    case SVT_AV2_ERROR_INVALID_PARAMETER: return "SVT_AV2_ERROR_INVALID_PARAMETER";
    case SVT_AV2_ERROR_INVALID_STATE: return "SVT_AV2_ERROR_INVALID_STATE";
    case SVT_AV2_ERROR_NOT_IMPLEMENTED: return "SVT_AV2_ERROR_NOT_IMPLEMENTED";
    case SVT_AV2_ERROR_OUT_OF_MEMORY: return "SVT_AV2_ERROR_OUT_OF_MEMORY";
    case SVT_AV2_ERROR_IO: return "SVT_AV2_ERROR_IO";
    case SVT_AV2_ERROR_UNSUPPORTED: return "SVT_AV2_ERROR_UNSUPPORTED";
    case SVT_AV2_ERROR_INTERNAL: return "SVT_AV2_ERROR_INTERNAL";
    default: return "SVT_AV2_ERROR_UNKNOWN";
    }
}

/* --- handle helpers ------------------------------------------------------- */

static SvtAv2EncoderContext* context_from_handle(SvtAv2Handle handle) {
    if (handle == NULL) {
        return NULL;
    }
    if (handle->magic != SVT_AV2_CONTEXT_MAGIC) {
        return NULL;
    }
    if (handle->state == SVT_AV2_STATE_CLOSED) {
        return NULL;
    }
    return handle;
}

static void copy_string(char* destination, size_t capacity, const char* source) {
    if (capacity == 0) {
        return;
    }
    if (source == NULL) {
        destination[0] = '\0';
        return;
    }
    const size_t length = strlen(source);
    const size_t copy = (length < capacity - 1) ? length : capacity - 1;
    memcpy(destination, source, copy);
    destination[copy] = '\0';
}

/* --- API ------------------------------------------------------------------ */

SvtAv2Error svt_av2_enc_init_handle(SvtAv2Handle* handle, const SvtAv2EncoderInfo** info) {
    if (handle == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    *handle = NULL;

    SvtAv2EncoderContext* context = (SvtAv2EncoderContext*)calloc(1, sizeof(SvtAv2EncoderContext));
    if (context == NULL) {
        return SVT_AV2_ERROR_OUT_OF_MEMORY;
    }
    context->magic = SVT_AV2_CONTEXT_MAGIC;
    context->state = SVT_AV2_STATE_UNINITIALIZED;
    context->parameter_mask = 0;
    context->pictures_sent = 0;
    context->pictures_encoded = 0;
    context->fatal_error = 0;
    context->frame_source = (SvtAv2FrameBuffer){0}; /* zeroed; alloc'd on demand */
    context->frame_width = 0;
    context->frame_height = 0;
    context->frame_pts = 0;
    context->frame_pending = 0;
    context->eos = 0;
    context->eos_reported = 0;
    context->packet = NULL;
    context->packet_size = 0;
    context->packet_capacity = 0;
    context->packet_pending = 0;
    svt_av2_config_set_defaults(&context->config);

    if (info != NULL) {
        *info = svt_av2_enc_get_build_info();
    }
    *handle = context;
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_enc_set_parameter(SvtAv2Handle handle, SvtAv2Parameter param, const void* value) {
    SvtAv2EncoderContext* context = context_from_handle(handle);
    if (context == NULL) {
        return SVT_AV2_ERROR_INVALID_HANDLE;
    }
    if (context->state != SVT_AV2_STATE_UNINITIALIZED) {
        return SVT_AV2_ERROR_INVALID_STATE;
    }
    if (value == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if ((int)param < 0 || param >= SVT_AV2_PARAM_COUNT) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    switch (param) {
    case SVT_AV2_PARAM_WIDTH: context->config.width = *(const uint32_t*)value; break;
    case SVT_AV2_PARAM_HEIGHT: context->config.height = *(const uint32_t*)value; break;
    case SVT_AV2_PARAM_CHROMA_FORMAT: context->config.chroma_format = *(const SvtAv2ChromaFormat*)value; break;
    case SVT_AV2_PARAM_BIT_DEPTH: context->config.bit_depth = *(const SvtAv2BitDepth*)value; break;
    case SVT_AV2_PARAM_FRAME_RATE_NUM: context->config.frame_rate_num = *(const uint32_t*)value; break;
    case SVT_AV2_PARAM_FRAME_RATE_DEN: context->config.frame_rate_den = *(const uint32_t*)value; break;
    case SVT_AV2_PARAM_QP: context->config.qp = *(const int32_t*)value; break;
    case SVT_AV2_PARAM_PRESET: context->config.preset = *(const SvtAv2Preset*)value; break;
    case SVT_AV2_PARAM_THREADS: context->config.threads = *(const uint32_t*)value; break;
    case SVT_AV2_PARAM_DETERMINISTIC: context->config.deterministic = *(const int32_t*)value; break;
    case SVT_AV2_PARAM_OUTPUT_PATH: {
        const char* path = *(const char* const*)value;
        if (path == NULL || path[0] == '\0') {
            return SVT_AV2_ERROR_INVALID_PARAMETER;
        }
        const size_t length = strlen(path);
        char*        copy = (char*)malloc(length + 1);
        if (copy == NULL) {
            return SVT_AV2_ERROR_OUT_OF_MEMORY;
        }
        memcpy(copy, path, length + 1);
        if (context->config.output_path != NULL) {
            free((void*)context->config.output_path);
        }
        context->config.output_path = copy;
        break;
    }
    case SVT_AV2_PARAM_PREDICTION_STRUCTURE: context->config.prediction_structure = *(const SvtAv2PredictionStructure*)value; break;
    case SVT_AV2_PARAM_INTRA_MODE_SELECTION:
        if (*(const uint32_t*)value > 4u) return SVT_AV2_ERROR_INVALID_PARAMETER;
        context->config.intra_mode_selection = *(const uint32_t*)value;
        break;
    case SVT_AV2_PARAM_PARTITION_SELECTION:
        if (*(const uint32_t*)value > 10u) return SVT_AV2_ERROR_INVALID_PARAMETER;
        context->config.partition_selection = *(const uint32_t*)value;
        break;
    case SVT_AV2_PARAM_RD_MODE:
        if (*(const uint32_t*)value > 3u) return SVT_AV2_ERROR_INVALID_PARAMETER;
        context->config.rd_mode = *(const uint32_t*)value;
        break;
    case SVT_AV2_PARAM_COEFF_OPTIMIZATION:
        if (*(const uint32_t*)value > 2u) return SVT_AV2_ERROR_INVALID_PARAMETER;
        context->config.coeff_optimization = *(const uint32_t*)value;
        break;
    case SVT_AV2_PARAM_MOTION_OPTIMIZATION:
        if (*(const uint32_t*)value > 2u) return SVT_AV2_ERROR_INVALID_PARAMETER;
        context->config.motion_optimization = *(const uint32_t*)value;
        break;
    case SVT_AV2_PARAM_ENTROPY_ADAPTATION:
        if (*(const uint32_t*)value > 1u) return SVT_AV2_ERROR_INVALID_PARAMETER;
        context->config.entropy_adaptation = *(const uint32_t*)value;
        break;
    case SVT_AV2_PARAM_STILL_PICTURE:
        if (*(const uint32_t*)value > 1u) return SVT_AV2_ERROR_INVALID_PARAMETER;
        context->config.still_picture = *(const uint32_t*)value;
        break;
    case SVT_AV2_PARAM_TRANSFORM_SELECTION:
        if (*(const uint32_t*)value > 2u) return SVT_AV2_ERROR_INVALID_PARAMETER;
        context->config.transform_selection = *(const uint32_t*)value;
        break;
    case SVT_AV2_PARAM_TREE_REFINEMENT:
        if (*(const uint32_t*)value > 2u) return SVT_AV2_ERROR_INVALID_PARAMETER;
        context->config.tree_refinement = *(const uint32_t*)value;
        break;
    case SVT_AV2_PARAM_RD_LAMBDA_MODEL:
        if (*(const uint32_t*)value > 2u) return SVT_AV2_ERROR_INVALID_PARAMETER;
        context->config.rd_lambda_model = *(const uint32_t*)value;
        break;
    case SVT_AV2_PARAM_CFL_SEARCH:
        if (*(const uint32_t*)value > 1u) return SVT_AV2_ERROR_INVALID_PARAMETER;
        context->config.cfl_search = *(const uint32_t*)value;
        break;
    case SVT_AV2_PARAM_MHCCP_SEARCH:
        if (*(const uint32_t*)value > 1u) return SVT_AV2_ERROR_INVALID_PARAMETER;
        context->config.mhccp_search = *(const uint32_t*)value;
        break;
    case SVT_AV2_PARAM_INTRA_JOINT:
        if (*(const uint32_t*)value > 1u) return SVT_AV2_ERROR_INVALID_PARAMETER;
        context->config.intra_joint = *(const uint32_t*)value;
        break;
    case SVT_AV2_PARAM_MRL_SEARCH:
        if (*(const uint32_t*)value > 1u) return SVT_AV2_ERROR_INVALID_PARAMETER;
        context->config.mrl_search = *(const uint32_t*)value;
        break;
    case SVT_AV2_PARAM_ANGLE_SEARCH:
        if (*(const uint32_t*)value > 1u) return SVT_AV2_ERROR_INVALID_PARAMETER;
        context->config.angle_search = *(const uint32_t*)value;
        break;
    case SVT_AV2_PARAM_CONTENT_TYPE: {
        const SvtAv2ContentType type=*(const SvtAv2ContentType*)value;
        if (type!=SVT_AV2_CONTENT_NONE && type!=SVT_AV2_CONTENT_ANIMATION &&
            type!=SVT_AV2_CONTENT_REAL) return SVT_AV2_ERROR_INVALID_PARAMETER;
        context->content_type=type;
        break;
    }
    case SVT_AV2_PARAM_COUNT:
    default: return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    context->parameter_mask |= (1u << (uint32_t)param);
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_enc_init(SvtAv2Handle handle) {
    SvtAv2EncoderContext* context = context_from_handle(handle);
    if (context == NULL) {
        return SVT_AV2_ERROR_INVALID_HANDLE;
    }
    if (context->state != SVT_AV2_STATE_UNINITIALIZED) {
        return SVT_AV2_ERROR_INVALID_STATE;
    }

    const SvtAv2Error err = svt_av2_config_validate(&context->config);
    if (err != SVT_AV2_ERROR_NONE) {
        return err;
    }
    if (context->config.width % 8u != 0 || context->config.height % 8u != 0 ||
        context->config.width > 4096u || context->config.height > 2160u ||
        !((context->config.bit_depth == SVT_AV2_BIT_DEPTH_8 && context->config.chroma_format == SVT_AV2_CHROMA_420) ||
          (context->config.bit_depth == SVT_AV2_BIT_DEPTH_10 &&
           (context->config.chroma_format == SVT_AV2_CHROMA_420 || context->config.chroma_format == SVT_AV2_CHROMA_444))) ||
        context->config.qp == 0)
        return SVT_AV2_ERROR_UNSUPPORTED;
    if (context->config.threads > 1) {
        /* M0 has no threading implementation. Fail loudly instead of silently
         * encoding single threaded (plan section 0.4). */
        return SVT_AV2_ERROR_UNSUPPORTED;
    }

    context->state = SVT_AV2_STATE_INITIALIZED;
    context->pictures_sent = 0;
    context->pictures_encoded = 0;
    svt_av2_context_release_pipeline(context);
    return SVT_AV2_ERROR_NONE;
}

/* True when the owned source picture is already allocated at this geometry and
 * can be reused for the next frame without reallocation. */
static int frame_source_fits(const SvtAv2EncoderContext* context,
                             uint32_t width,
                             uint32_t height) {
    return context->frame_source.visible_width == width &&
           context->frame_source.visible_height == height &&
           context->frame_source.planes[0] != NULL;
}

/* Copies the caller's picture into the encoder-owned allocated source picture,
 * so the caller may free or reuse its planes as soon as send_picture() returns
 * (contract section 1.2).
 *
 * The copy goes through T007's adaptation entry: svt_av2_picture_view() builds a
 * borrowed view over the caller's planes (which are NOT an allocation) and
 * svt_av2_picture_copy_from_view() copies it into the allocated destination,
 * stripping row padding and retaining no caller pointer. This is the single
 * sanctioned bridge from the public SvtAv2Picture to an internal frame buffer;
 * no buffer pointer is faked. */
static SvtAv2Error copy_picture(const SvtAv2Picture* picture,
                                         SvtAv2FrameBuffer* destination) {
    SvtAv2BorrowedView view;
    SvtAv2Error        status = svt_av2_picture_view_ex(&view,
                                                     picture->planes,
                                                     picture->strides,
                                                     picture->width,
                                                     picture->height,
                                                     (uint32_t)picture->bit_depth, picture->chroma_format);
    if (status != SVT_AV2_ERROR_NONE) {
        return status;
    }
    return svt_av2_picture_copy_from_view(destination, &view);
}

/* Reuses the frame reconstruction and 64x64 block scratch across frames. */
static SvtAv2Error ensure_recon_buffers(SvtAv2EncoderContext* context,
                                        uint32_t width,
                                        uint32_t height) {
    if (context->recon_ready && context->recon.visible_width == width &&
        context->recon.visible_height == height) {
        return SVT_AV2_ERROR_NONE;
    }

    if (context->recon_ready) {
        svt_av2_recon_pool_free(&context->scratch);
        svt_av2_picture_free(&context->recon);
        context->recon_ready = 0;
    }

    SvtAv2Error status = svt_av2_picture_alloc_ex(&context->recon, width, height,
        (uint32_t)context->config.bit_depth, context->config.chroma_format);
    if (status != SVT_AV2_ERROR_NONE) {
        return status;
    }
    /* The pool must cover the largest block the traversal can hand the core:
     * one superblock, 64x64 for the frozen M2 configuration. */
    status = svt_av2_recon_pool_init(&context->scratch, 64u, 64u);
    if (status != SVT_AV2_ERROR_NONE) {
        svt_av2_picture_free(&context->recon);
        return status;
    }
    if (context->config.prediction_structure == SVT_AV2_PRED_LOW_DELAY_P) {
        status = svt_av2_reference_init(&context->reference, width, height,
            (uint32_t)context->config.bit_depth, context->config.chroma_format);
        if (status != SVT_AV2_ERROR_NONE) {
            svt_av2_recon_pool_free(&context->scratch);
            svt_av2_picture_free(&context->recon);
            return status;
        }
    }
    context->recon_ready = 1;
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_enc_send_picture(SvtAv2Handle handle, const void* picture) {
    SvtAv2EncoderContext* context = context_from_handle(handle);
    if (context == NULL) {
        return SVT_AV2_ERROR_INVALID_HANDLE;
    }
    if (context->state != SVT_AV2_STATE_INITIALIZED) {
        return SVT_AV2_ERROR_INVALID_STATE;
    }

    /* send_picture(NULL) is EOS / flush (contract section 7). It is only
     * meaningful once the pipeline is drained; asking for EOS while a packet is
     * still untaken is a state error, not a silent discard. */
    if (picture == NULL) {
        if (context->frame_pending || context->packet_pending) {
            return SVT_AV2_ERROR_INVALID_STATE;
        }
        context->eos = 1;
        return SVT_AV2_ERROR_NONE;
    }

    /* At most one picture may be in flight: the encoder is synchronous and the
     * packet is returned by the very next get_packet() call. A second picture
     * before that would silently drop the first frame's stream. */
    if (context->frame_pending || context->packet_pending) {
        return SVT_AV2_ERROR_INVALID_STATE;
    }
    if (context->eos) {
        /* EOS already signalled: no further pictures are accepted. */
        return SVT_AV2_ERROR_INVALID_STATE;
    }
    if (context->config.still_picture && context->pictures_sent != 0u)
        return SVT_AV2_ERROR_INVALID_STATE;

    const SvtAv2Picture* input = (const SvtAv2Picture*)picture;
    uint32_t             chroma_width = 0;
    uint32_t             chroma_height = 0;
    const SvtAv2Error    validation = validate_picture(context, input, &chroma_width, &chroma_height);
    if (validation != SVT_AV2_ERROR_NONE) {
        return validation;
    }

    size_t samples = 0;
    if (svt_av2_compute_frame_sample_count(input->width, input->height, input->chroma_format, &samples) !=
        SVT_AV2_ERROR_NONE) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    (void)samples; /* the allocated layout is sized by geometry, not by this count */

    /* Allocate (or reuse) the source picture in the allocated layout the M2
     * encoder core consumes. svt_av2_picture_alloc() computes the padded rows
     * and rejects a size change, so a later frame of a different size is a
     * clean reallocation rather than an overrun. */
    if (!frame_source_fits(context, input->width, input->height)) {
        svt_av2_picture_free(&context->frame_source);
        const SvtAv2Error alloc_err =
            svt_av2_picture_alloc_ex(&context->frame_source, input->width, input->height,
                (uint32_t)input->bit_depth, input->chroma_format);
        if (alloc_err != SVT_AV2_ERROR_NONE) {
            return alloc_err;
        }
    }

    const SvtAv2Error copy_err = copy_picture(input, &context->frame_source);
    if (copy_err != SVT_AV2_ERROR_NONE) {
        return copy_err;
    }

    /* Source, reconstruction and scratch remain distinct owned buffers. */
    const SvtAv2Error recon_err = ensure_recon_buffers(context, input->width, input->height);
    if (recon_err != SVT_AV2_ERROR_NONE) {
        return recon_err;
    }

    context->frame_width = input->width;
    context->frame_height = input->height;
    context->frame_pts = input->pts;
    context->frame_pending = 1;
    context->pictures_sent++;
    return SVT_AV2_ERROR_NONE;
}

/* Encodes the pending frame into context->packet, if a frame is waiting and no
 * packet has been produced for it yet. This is the single encode path used by
 * both get_packet() entry points; the CLI reaches it through the same API, so
 * there is no second, divergent encoder (contract section 7).
 *
 * Returns SVT_AV2_ERROR_NONE when a packet is ready or when there is simply
 * nothing to do; a real failure is returned and latched in fatal_error. */

/* Private experiment: unchanged r25 RD, then one bounded BO4 packet proposal. */
#define CCSO_AUTO_UNITS (64u * 34u)
typedef struct CcsoAuto {
    uint64_t table[CCSO_AUTO_UNITS][2][4][3],raw[3],post[3],flag_rate[2],estimate;
    uint8_t map[CCSO_AUTO_UNITS][2],best_flags[2][CCSO_AUTO_UNITS];
    uint32_t cols,units,lut[2],mask,writers,rewrites,no_space;
    SvtAv2Ccso64Plan plan;
    SvtAv2FrameHeaderConfig header;
    SvtAv2Ccso64HeaderSize headers[4];
    clock_t scan_ticks,select_ticks,pack_ticks,apply_ticks;
} CcsoAuto;
static int ccso_auto_domain(const SvtAv2EncoderContext* c) {
    const SvtAv2EncoderConfig* q=&c->config;
    return q->still_picture && q->prediction_structure==SVT_AV2_PRED_ALL_INTRA && q->rd_mode==3u &&
        q->intra_mode_selection==4u && (q->partition_selection==5u || q->partition_selection==7u) && q->tree_refinement==2u &&
        q->entropy_adaptation==1u && (q->rd_lambda_model==1u || q->rd_lambda_model==2u) && q->transform_selection==2u &&
        q->coeff_optimization==2u && q->angle_search==1u && q->mrl_search==1u &&
        q->cfl_search==1u && q->mhccp_search==1u && !q->intra_joint && !q->motion_optimization &&
        q->chroma_format==SVT_AV2_CHROMA_420 && (q->bit_depth==8u || q->bit_depth==10u) &&
        c->frame_width>=8u && c->frame_width<=4096u && c->frame_height>=8u && c->frame_height<=2160u &&
        !(c->frame_width&7u) && !(c->frame_height&7u);
}
static void ccso_auto_lut(uint32_t id,int8_t offsets[4]) {
    for (uint32_t b=0u;b<4u;++b) {const uint32_t digit=id%3u;id/=3u;offsets[b]=(int8_t)(digit==1u?-1:digit==2u?1:0);}
}
static SvtAv2Error ccso_auto_cost(const SvtAv2EncoderContext* c,const uint64_t raw[3],uint64_t rate,uint64_t* cost) {
    return svt_av2_rd_cost(svt_av2_rd_distortion(raw,(uint32_t)c->config.bit_depth,SVT_AV2_CHROMA_420),c->rd_stats.lambda_q8,rate,cost);
}
static uint64_t ccso_auto_hash(const uint8_t* data,size_t size) {
    uint64_t hash=UINT64_C(14695981039346656037);
    for (size_t i=0u;i<size;++i) hash=(hash^data[i])*UINT64_C(1099511628211);
    return hash; /* Diagnostic checksum; external closure also compares full SHA/bytes. */
}
static void ccso_auto_subset(CcsoAuto* w,uint32_t mask) {
    for (uint32_t p=0u;p<2u;++p) {
        w->plan.enabled[p]=(uint8_t)((mask>>p)&1u);ccso_auto_lut(w->lut[p],w->plan.offset[p]);
        for (uint32_t u=0u;u<w->units;++u) w->map[u][p]=w->plan.enabled[p]?w->best_flags[p][u]:0u;
    }
    w->header.ccso64=mask?&w->plan:NULL;
}
static SvtAv2Error ccso_auto_prepare(SvtAv2EncoderContext* c,CcsoAuto** output,int reverse,uint32_t db,int delta,int delta_h) {
    *output=NULL;
    CcsoAuto* w=(CcsoAuto*)calloc(1u,sizeof(*w));
    if (!w) return SVT_AV2_ERROR_OUT_OF_MEMORY;
    const uint32_t bd=(uint32_t)c->config.bit_depth,step=bd==10u?2u:1u,width=c->frame_width,height=c->frame_height;
    const int maximum=(1<<bd)-1;
    w->cols=(width+63u)/64u;w->units=w->cols*((height+63u)/64u);
    w->plan.unit_flags=(const uint8_t (*)[2])w->map;w->plan.unit_count=w->units;
    svt_av2_frame_header_config_default(&w->header,width,height);
    w->header.deblock_y=db;w->header.deblock_delta=delta;w->header.deblock_delta_h=delta_h;
    w->header.base_q_idx=(uint32_t)c->config.qp+(bd==10u?48u:0u);
    w->header.bit_depth=bd;w->header.chroma_format=1u;w->header.m5_intra_modes=4u;
    w->header.entropy_adaptation=1u;w->header.reduced_tx_set=0u;w->header.enable_mrls=w->header.enable_mhccp=1u;
    w->header.partition_decisions=c->partition_decisions;w->header.partition_decision_count=c->partition_decision_count;
    clock_t start=clock();
    for (uint32_t p=0u;p<3u;++p) for (uint32_t y=0u;y<(height>>(p!=0u));++y)
        for (uint32_t x=0u;x<(width>>(p!=0u));++x) {
            const int src=svt_av2_sample_load(c->frame_source.planes[p]+(size_t)y*c->frame_source.strides[p]+x*step,bd);
            const int rec=svt_av2_sample_load(c->recon.planes[p]+(size_t)y*c->recon.strides[p]+x*step,bd);
            if (!p) {const int d=src-rec;w->raw[0]+=(uint64_t)(d*d);continue;}
            const uint32_t luma=svt_av2_sample_load(c->recon.planes[0]+(size_t)(2u*y)*c->recon.strides[0]+2u*x*step,bd);
            const uint32_t band=luma>>(bd-2u),unit=(y/32u)*w->cols+x/32u;
            for (uint32_t k=0u;k<3u;++k) {
                int v=rec+(k==1u?-1:k==2u?1:0);v=v<0?0:v>maximum?maximum:v;
                const int d=src-v;w->table[unit][p-1u][band][k]+=(uint64_t)(d*d);
            }
        }
    for (uint32_t u=0u;u<w->units;++u) for (uint32_t p=0u;p<2u;++p)
        for (uint32_t b=0u;b<4u;++b) w->raw[p+1u]+=w->table[u][p][b][0];
    w->scan_ticks=clock()-start;start=clock();
    uint64_t plane_sse[2]={0u},best_cost[2]={UINT64_MAX,UINT64_MAX};
    SvtAv2Error error=SVT_AV2_ERROR_INVALID_STATE;
    for (uint32_t p=0u;p<2u;++p) for (uint32_t n=0u;n<81u;++n) {
        const uint32_t id=reverse?80u-n:n;
        memset(w->map,0,sizeof(w->map));w->plan.enabled[0]=(uint8_t)(p==0u);w->plan.enabled[1]=(uint8_t)(p==1u);
        ccso_auto_lut(id,w->plan.offset[p]);w->header.ccso64=&w->plan;
        SvtAv2Ccso64HeaderSize h;
        if (svt_av2_ccso64_header_size(&w->header,&h)!=0) goto fail;
        const uint64_t base_rate=c->rd_stats.rate_q12+(uint64_t)(h.sequence_bytes+h.frame_bytes)*32768u;
        uint64_t raw[3]={w->raw[0],w->raw[1],w->raw[2]},rate=0u,cost=0u;
        uint16_t cdf[2][4][6];svt_av2_ccso64_cdf_init(cdf);
        for (uint32_t u=0u;u<w->units;++u) {
            uint64_t off=0u,on=0u;
            for (uint32_t b=0u;b<4u;++b) {
                const int offset=w->plan.offset[p][b];off+=w->table[u][p][b][0];
                on+=w->table[u][p][b][offset<0?1u:offset>0?2u:0u];
            }
            const uint32_t ctx=svt_av2_ccso64_context(&w->plan,u,w->cols,p);
            uint64_t chosen=UINT64_MAX,chosen_rate=0u,chosen_sse=0u;uint16_t chosen_row[6];uint32_t flag=0u;
            for (uint32_t f=0u;f<2u;++f) {
                uint16_t row[6];memcpy(row,cdf[p][ctx],sizeof(row));
                SvtAv2EntropyEncoder enc;svt_av2_entropy_rate_init(&enc);
                if (svt_av2_ccso64_symbol(&enc,row,f,1)!=0) goto fail;
                uint64_t candidate[3]={raw[0],raw[1],raw[2]};candidate[p+1u]=raw[p+1u]-off+(f?on:off);
                if (ccso_auto_cost(c,candidate,base_rate+rate+enc.rate_q12,&cost)!=0) goto fail;
                if (f==0u || cost<chosen) {chosen=cost;chosen_rate=enc.rate_q12;chosen_sse=candidate[p+1u];flag=f;memcpy(chosen_row,row,sizeof(row));}
            }
            w->map[u][p]=(uint8_t)flag;raw[p+1u]=chosen_sse;rate+=chosen_rate;
            memcpy(cdf[p][ctx],chosen_row,sizeof(chosen_row));
        }
        if (ccso_auto_cost(c,raw,base_rate+rate,&cost)!=0) goto fail;
        if (cost<best_cost[p] || (cost==best_cost[p] && id<w->lut[p])) {
            best_cost[p]=cost;w->lut[p]=id;w->flag_rate[p]=rate;plane_sse[p]=raw[p+1u];
            for (uint32_t u=0u;u<w->units;++u) w->best_flags[p][u]=w->map[u][p];
        }
    }
    w->estimate=UINT64_MAX;
    for (uint32_t mask=0u;mask<4u;++mask) {
        ccso_auto_subset(w,mask);
        if (svt_av2_ccso64_header_size(&w->header,&w->headers[mask])!=0) goto fail;
        const uint64_t raw[3]={w->raw[0],mask&1u?plane_sse[0]:w->raw[1],mask&2u?plane_sse[1]:w->raw[2]};
        const uint64_t rate=c->rd_stats.rate_q12+(uint64_t)(w->headers[mask].sequence_bytes+w->headers[mask].frame_bytes)*32768u+
            (mask&1u?w->flag_rate[0]:0u)+(mask&2u?w->flag_rate[1]:0u);
        uint64_t cost;if (ccso_auto_cost(c,raw,rate,&cost)!=0) goto fail;
        if (cost<w->estimate) {w->estimate=cost;w->mask=mask;memcpy(w->post,raw,sizeof(raw));}
    }
    ccso_auto_subset(w,w->mask);w->select_ticks=clock()-start;*output=w;return SVT_AV2_ERROR_NONE;
fail:
    free(w);return error;
}
static SvtAv2Error ccso_auto_packet(SvtAv2EncoderContext* c,CcsoAuto* w,int on,size_t* bytes) {
    size_t capacity=svt_av2_tile_writer_max_payload_bytes(c->frame_width,c->frame_height)+256u;
    if (capacity<4096u) capacity=4096u;
    if (capacity<c->packet_capacity) capacity=c->packet_capacity;
    for (;;) {
        if (c->packet_capacity<capacity) {
            uint8_t* grown=(uint8_t*)realloc(c->packet,capacity);
            if (!grown) return SVT_AV2_ERROR_OUT_OF_MEMORY;
            c->packet=grown;c->packet_capacity=capacity;
        }
        ++w->writers;
        SvtAv2FrameHeaderConfig header=w->header;
        if (!on) header.ccso64=NULL;
        const int st=svt_av2_encode_still_picture_ccso64_packet(&header,c->coded_blocks,
            c->coded_block_count,c->packet,c->packet_capacity,bytes);
        if (st!=SVT_AV2_BW_ERROR_NO_SPACE) return st==0 && *bytes?SVT_AV2_ERROR_NONE:SVT_AV2_ERROR_INTERNAL;
        ++w->no_space;if (capacity>SIZE_MAX/2u) return SVT_AV2_ERROR_OUT_OF_MEMORY;capacity*=2u;
    }
}
#if M6_PC_WIENER
#if M6_CONTENT_GUARD
static int content_guard_domain(const SvtAv2EncoderContext* c) {
    return c->content_type==SVT_AV2_CONTENT_ANIMATION &&
        c->config.bit_depth==SVT_AV2_BIT_DEPTH_10 &&
        c->config.chroma_format==SVT_AV2_CHROMA_420 &&
        c->config.still_picture==1u &&
        c->config.prediction_structure==SVT_AV2_PRED_ALL_INTRA &&
        ((c->frame_width==1920u && c->frame_height==1080u) ||
         (c->frame_width==3840u && c->frame_height==2160u));
}
#endif
static SvtAv2Error pc_wiener_try(SvtAv2EncoderContext* c,CcsoAuto* w,int ccso_on,size_t* bytes) {
    const uint32_t width=c->frame_width,height=c->frame_height,cols=width/4u;
    const uint32_t bd=(uint32_t)c->config.bit_depth,step=bd==10u?2u:1u;
    const size_t pixels=(size_t)width*height,off_bytes=*bytes;
    uint16_t* src=(uint16_t*)malloc(pixels*sizeof(*src));
    uint16_t* dst=(uint16_t*)malloc(pixels*sizeof(*dst));
    uint8_t* skip=(uint8_t*)calloc(pixels/16u,1u);
    uint8_t* packet=(uint8_t*)malloc(off_bytes);
    SvtAv2Error error=SVT_AV2_ERROR_OUT_OF_MEMORY;
    if (!src || !dst || !skip || !packet) goto done;
    memcpy(packet,c->packet,off_bytes);
    for (size_t i=0u;i<c->coded_block_count;++i) {
        const SvtAv2TileCoeffBlock* b=c->coded_blocks+i;
        if (b->plane) continue;
        uint8_t zero=1u;
        const uint32_t cw=b->width<32u?b->width:32u,ch=b->height<32u?b->height:32u;
        for (uint32_t j=0u;j<cw*ch;++j) if (b->qcoeff[j]) {zero=0u;break;}
        for (uint32_t y=b->y/4u;y<(b->y+b->height)/4u && y<height/4u;++y)
            for (uint32_t x=b->x/4u;x<(b->x+b->width)/4u && x<cols;++x)
                skip[(size_t)y*cols+x]=zero;
    }
    for (uint32_t y=0u;y<height;++y) for (uint32_t x=0u;x<width;++x)
        src[(size_t)y*width+x]=svt_av2_sample_load(c->recon.planes[0]+(size_t)y*c->recon.strides[0]+x*step,bd);
    error=SVT_AV2_ERROR_INVALID_STATE;
    if (svt_av2_pcwiener_filter_y(src,width,dst,width,width,height,bd,
        w->header.base_q_idx,skip,cols)!=0) goto done;
    uint64_t raw[3];memcpy(raw,ccso_on?w->post:w->raw,sizeof(raw));
    const uint64_t old_y=raw[0];raw[0]=0u;
    int forced=-1;
#if M6_PC_WIENER_TEST
    const char* value=getenv("SVT_AV2_TEST_PC_WIENER");
    if (value) {
        if (strcmp(value,"0") && strcmp(value,"1") && strcmp(value,"2")) {error=SVT_AV2_ERROR_INVALID_PARAMETER;goto done;}
        forced=value[0]-'0';
    }
#endif
    const uint32_t ru_cols=(width+128u)/256u ? (width+128u)/256u : 1u;
    const uint32_t ru_rows=(height+128u)/256u ? (height+128u)/256u : 1u;
    uint16_t cdf[6]={19969,0,0,1,3,4};
    uint64_t none_mask[2]={0u,0u},check_y=0u;
#if M6_CONTENT_GUARD
    /* Existing test-only forced PCW behavior stays separate from automatic
     * selection. Production activation is exclusively this API hint/domain. */
    const uint32_t content_guard_active=(uint32_t)(content_guard_domain(c) && forced<0);
    uint32_t content_guard_considered=0u,content_guard_vetoes=0u;
    uint64_t content_edge_samples=0u,content_flat_samples=0u;
#endif
    for (uint32_t r=0u;r<ru_rows;++r) for (uint32_t col=0u;col<ru_cols;++col) {
        const uint32_t left=col*256u,right=col+1u==ru_cols?width:left+256u;
        const uint32_t top=r?r*256u-8u:0u,bottom=r+1u==ru_rows?height:(r+1u)*256u-8u;
        uint64_t sse[2]={0u,0u},cost[2]={0u,0u};
        uint16_t states[2][6];
        for (uint32_t y=top;y<bottom;++y) for (uint32_t x=left;x<right;++x) {
            const int target=(int)svt_av2_sample_load(c->frame_source.planes[0]+(size_t)y*c->frame_source.strides[0]+x*step,bd);
            const int d0=target-(int)src[(size_t)y*width+x],d1=target-(int)dst[(size_t)y*width+x];
            sse[0]+=(uint64_t)(d0*d0);sse[1]+=(uint64_t)(d1*d1);
        }
        for (uint32_t flag=0u;flag<2u;++flag) {
            SvtAv2EntropyEncoder rate;svt_av2_entropy_rate_init(&rate);
            memcpy(states[flag],cdf,sizeof(cdf));
            if (svt_av2_ccso64_symbol(&rate,states[flag],flag,1)!=0) goto done;
            const uint64_t region[3]={sse[flag],0u,0u};
            if (ccso_auto_cost(c,region,rate.rate_q12,&cost[flag])!=0) goto done;
        }
        const uint32_t unit=r*ru_cols+col;
        uint32_t on=forced>=0 ? (forced==2 ? (r+col)&1u : 1u) : cost[1]<cost[0];
#if M6_CONTENT_GUARD
        if (content_guard_active && on) {
            if (right<=left || bottom<=top || right>width || bottom>height ||
                right-left>384u || bottom-top>384u ||
                c->frame_source.strides[0]<(size_t)width*step) {
                error=SVT_AV2_ERROR_INVALID_STATE;goto done;
            }
            M6ContentFilterGuardStats guard_stats;
            ++content_guard_considered;
            const int allow=m6_content_filter_accept(c->frame_source.planes[0],
                c->frame_source.strides[0],bd,src,dst,width,height,left,top,
                right-left,bottom-top,&guard_stats);
            content_edge_samples+=guard_stats.edge_samples;
            content_flat_samples+=guard_stats.flat_samples;
            if (!allow) {on=0u;++content_guard_vetoes;}
        }
#endif
        memcpy(cdf,states[on],sizeof(cdf));
        check_y+=sse[0];raw[0]+=sse[on];
        if (!on) {
            none_mask[unit/64u]|=UINT64_C(1)<<(unit&63u);
            for (uint32_t y=top;y<bottom;++y)
                memcpy(dst+(size_t)y*width+left,src+(size_t)y*width+left,(right-left)*sizeof(*dst));
        }
    }
    if (check_y!=old_y) goto done;
    uint64_t old_j=0u,new_j=0u;
    if (ccso_auto_cost(c,ccso_on?w->post:w->raw,(uint64_t)off_bytes*32768u,&old_j)!=0) goto done;
    w->header.enable_pc_wiener=1u;
    memcpy(w->header.pc_wiener_none_mask,none_mask,sizeof(none_mask));
    size_t new_bytes=0u;
    error=ccso_auto_packet(c,w,ccso_on,&new_bytes);
    w->header.enable_pc_wiener=0u;
    memset(w->header.pc_wiener_none_mask,0,sizeof(w->header.pc_wiener_none_mask));
    if (error!=0) goto done;
    error=ccso_auto_cost(c,raw,(uint64_t)new_bytes*32768u,&new_j);
    if (error!=0) goto done;
    int accepted=new_j<old_j;
    if (forced>=0) accepted=forced!=0;
    if (accepted) {
        *bytes=new_bytes;
        for (uint32_t y=0u;y<height;++y) for (uint32_t x=0u;x<width;++x)
            svt_av2_sample_store(c->recon.planes[0]+(size_t)y*c->recon.strides[0]+x*step,bd,dst[(size_t)y*width+x]);
    } else memcpy(c->packet,packet,off_bytes);
    fprintf(stderr,"PC_WIENER accepted=%d bytes=%zu/%zu SSE=%llu/%llu J=%llu/%llu none_mask=%llu none_mask_hi=%llu\n",
        accepted,off_bytes,new_bytes,(unsigned long long)old_y,(unsigned long long)raw[0],
        (unsigned long long)old_j,(unsigned long long)new_j,(unsigned long long)none_mask[0],(unsigned long long)none_mask[1]);
#if M6_CONTENT_GUARD
    fprintf(stderr,"CONTENT_GUARD type=%s active=%u considered=%u vetoes=%u edge_samples=%llu flat_samples=%llu forced=%d accepted=%d\n",
        c->content_type==SVT_AV2_CONTENT_ANIMATION?"animation":c->content_type==SVT_AV2_CONTENT_REAL?"real":"none",
        content_guard_active,content_guard_considered,content_guard_vetoes,
        (unsigned long long)content_edge_samples,(unsigned long long)content_flat_samples,forced,accepted);
#endif
done:
    free(src);free(dst);free(skip);free(packet);return error;
}
#endif

static SvtAv2Error ccso_auto_encode_base(SvtAv2EncoderContext* c,size_t* bytes,uint32_t db,int delta,int delta_h) {
    CcsoAuto* w=NULL;SvtAv2Error error=ccso_auto_prepare(c,&w,0,db,delta,delta_h);if (error!=0) return error;
    const clock_t start=clock();size_t off_bytes=0u,on_bytes=0u;uint64_t off_j=0u,on_j=0u;
    int accepted=0;error=ccso_auto_packet(c,w,0,&off_bytes);if (error!=0) goto done;
    const uint64_t off_hash=ccso_auto_hash(c->packet,off_bytes);
    if (off_bytes>UINT64_MAX/32768u || ccso_auto_cost(c,w->raw,(uint64_t)off_bytes*32768u,&off_j)!=0) {error=SVT_AV2_ERROR_INVALID_STATE;goto done;}
    *bytes=off_bytes;
    if (w->mask) {
        error=ccso_auto_packet(c,w,1,&on_bytes);if (error!=0) goto done;
        if (on_bytes>UINT64_MAX/32768u || ccso_auto_cost(c,w->post,(uint64_t)on_bytes*32768u,&on_j)!=0) {error=SVT_AV2_ERROR_INVALID_STATE;goto done;}
        accepted=on_j<off_j;
        if (accepted) *bytes=on_bytes;
        else {
            ++w->rewrites;error=ccso_auto_packet(c,w,0,bytes);if (error!=0) goto done;
            if (*bytes!=off_bytes || ccso_auto_hash(c->packet,*bytes)!=off_hash) {error=SVT_AV2_ERROR_INVALID_STATE;goto done;}
        }
    }
    w->pack_ticks=clock()-start;
    if (accepted) {
        const clock_t apply=clock();svt_av2_ccso64_apply(&w->plan,&c->recon);
        uint64_t actual[3]={w->raw[0],0u,0u};const uint32_t bd=(uint32_t)c->config.bit_depth,step=bd==10u?2u:1u;
        for (uint32_t p=1u;p<3u;++p) for (uint32_t y=0u;y<c->frame_height/2u;++y) for (uint32_t x=0u;x<c->frame_width/2u;++x) {
            const int d=(int)svt_av2_sample_load(c->frame_source.planes[p]+(size_t)y*c->frame_source.strides[p]+x*step,bd)-
                (int)svt_av2_sample_load(c->recon.planes[p]+(size_t)y*c->recon.strides[p]+x*step,bd);actual[p]+=(uint64_t)(d*d);
        }
        if (memcmp(actual,w->post,sizeof(actual))!=0) {error=SVT_AV2_ERROR_INVALID_STATE;goto done;}
        w->apply_ticks=clock()-apply;
    }
    fprintf(stderr,"CCSO_AUTO,mask=%u,accepted=%d,units=%u,lutU=%u,lutV=%u,writers=%u,rewrite=%u,no_space=%u,work_bytes=%zu,table_bytes=%zu,off_bytes=%zu,on_bytes=%zu,Joff=%llu,Jon=%llu,estimate=%llu,lambda=%llu\n",
        w->mask,accepted,w->units,w->lut[0],w->lut[1],w->writers,w->rewrites,w->no_space,sizeof(*w),sizeof(w->table),off_bytes,on_bytes,
        (unsigned long long)off_j,(unsigned long long)on_j,(unsigned long long)w->estimate,(unsigned long long)c->rd_stats.lambda_q8);
    fprintf(stderr,"CCSO_AUTO_SSE,pre=%llu/%llu/%llu,proposal=%llu/%llu/%llu,final=%llu/%llu/%llu,flagR=%llu/%llu,seq=%zu,frame_bits=%zu,frame_bytes=%zu,ticks=%lld/%lld/%lld/%lld,hz=%ld\n",
        (unsigned long long)w->raw[0],(unsigned long long)w->raw[1],(unsigned long long)w->raw[2],
        (unsigned long long)w->post[0],(unsigned long long)w->post[1],(unsigned long long)w->post[2],
        (unsigned long long)(accepted?w->post[0]:w->raw[0]),(unsigned long long)(accepted?w->post[1]:w->raw[1]),(unsigned long long)(accepted?w->post[2]:w->raw[2]),
        (unsigned long long)w->flag_rate[0],(unsigned long long)w->flag_rate[1],w->headers[w->mask].sequence_bytes,
        w->headers[w->mask].frame_bits,w->headers[w->mask].frame_bytes,(long long)w->scan_ticks,(long long)w->select_ticks,
        (long long)w->pack_ticks,(long long)w->apply_ticks,(long)CLOCKS_PER_SEC);
    fprintf(stderr,"CCSO_AUTO_MAP,");for (uint32_t u=0u;u<w->units;++u) fputc('0'+w->map[u][0]+2*w->map[u][1],stderr);fputc('\n',stderr);
#if M6_PC_WIENER
    error=pc_wiener_try(c,w,accepted,bytes);
#endif
done:
    free(w);return error;
}

#if M6_DEBLOCK
static void db_recon_copy(SvtAv2EncoderContext* c,uint8_t* raw,int restore) {
    size_t offset=0u;
    const uint32_t step=c->config.bit_depth>8u?2u:1u;
    for (uint32_t p=0u;p<3u;++p) {
        const uint32_t w=c->frame_width>>(p!=0u),h=c->frame_height>>(p!=0u);
        for (uint32_t y=0u;y<h;++y) {
            uint8_t* row=c->recon.planes[p]+(size_t)y*c->recon.strides[p];
            if (restore) memcpy(row,raw+offset,(size_t)w*step);
            else memcpy(raw+offset,row,(size_t)w*step);
            offset+=(size_t)w*step;
        }
    }
}
static SvtAv2Error db_current_cost(SvtAv2EncoderContext* c,size_t bytes,uint64_t* cost) {
    uint64_t sse[3]={0u,0u,0u};
    const uint32_t bd=(uint32_t)c->config.bit_depth,step=bd>8u?2u:1u;
    for (uint32_t p=0u;p<3u;++p) {
        const uint32_t w=c->frame_width>>(p!=0u),h=c->frame_height>>(p!=0u);
        for (uint32_t y=0u;y<h;++y) for (uint32_t x=0u;x<w;++x) {
            const int d=(int)svt_av2_sample_load(c->frame_source.planes[p]+(size_t)y*c->frame_source.strides[p]+x*step,bd)-
                (int)svt_av2_sample_load(c->recon.planes[p]+(size_t)y*c->recon.strides[p]+x*step,bd);
            sse[p]+=(uint64_t)(d*d);
        }
    }
    if (bytes>UINT64_MAX/32768u) return SVT_AV2_ERROR_INVALID_STATE;
    return ccso_auto_cost(c,sse,(uint64_t)bytes*32768u,cost);
}
#endif
static SvtAv2Error ccso_auto_encode(SvtAv2EncoderContext* c,size_t* bytes) {
#if M6_DEBLOCK
    const size_t raw_size=(size_t)c->frame_width*c->frame_height*3u/2u*(c->config.bit_depth>8u?2u:1u);
    uint8_t* raw=(uint8_t*)malloc(raw_size);
    uint8_t* best_recon=(uint8_t*)malloc(raw_size);
    uint8_t* best_packet=NULL;
    if (!raw || !best_recon) {free(raw);free(best_recon);return SVT_AV2_ERROR_OUT_OF_MEMORY;}
    db_recon_copy(c,raw,0);
    SvtAv2Error error=SVT_AV2_ERROR_NONE;
    uint64_t best_cost=UINT64_MAX,off_cost=0u,best_hash=0u;
    size_t best_bytes=0u;uint32_t winner=0u;
    int forced=-1;
#if M6_DEBLOCK_TEST
    const char* value=getenv("SVT_AV2_TEST_DEBLOCK_INDEX");
    if (value) {char* end=NULL;const long parsed=strtol(value,&end,10);
        if (!end || *end || parsed<0 || parsed>24) {error=SVT_AV2_ERROR_INVALID_PARAMETER;goto done;}
        forced=(int)parsed;
    }
#endif
    static const int deltas[4]={0,-1,1,-2};
    for (uint32_t index=0u;index<25u;++index) {
        if (forced>=0 && index && index!=(uint32_t)forced) continue;
        const uint32_t mode=!index?0u:index<=4u?1u:index<=8u?2u:3u;
        const int delta=mode==1u?deltas[index-1u]:mode==3u?deltas[(index-9u)/4u]:0;
        const int delta_h=mode==2u?deltas[index-5u]:mode==3u?deltas[(index-9u)%4u]:0;
        db_recon_copy(c,raw,1);
        uint64_t edges=0u,changed=0u;
        if (mode) error=svt_av2_deblock_luma_apply(&c->recon,c->coded_blocks,c->coded_block_count,
            mode,delta,delta_h,(uint32_t)c->config.qp+(c->config.bit_depth==10u?48u:0u),&edges,&changed);
        if (error!=SVT_AV2_ERROR_NONE) goto done;
        size_t trial_bytes=0u;
        error=ccso_auto_encode_base(c,&trial_bytes,mode,delta,delta_h);
        if (error!=SVT_AV2_ERROR_NONE) goto done;
        uint64_t cost=0u;error=db_current_cost(c,trial_bytes,&cost);
        if (error!=SVT_AV2_ERROR_NONE) goto done;
        if (!index) off_cost=cost;
        fprintf(stderr,"DEBLOCK_TRIAL index=%u mode=%u delta=%d delta_h=%d edges=%llu changed=%llu bytes=%zu J=%llu\n",
            index,mode,delta,delta_h,(unsigned long long)edges,(unsigned long long)changed,trial_bytes,(unsigned long long)cost);
        if (cost<best_cost || (forced>=0 && index==(uint32_t)forced)) {
            /* In forced tests the selected arm wins irrespective of RD. */
            if (forced>=0 && index && index!=(uint32_t)forced) continue;
            uint8_t* grown=(uint8_t*)realloc(best_packet,trial_bytes);
            if (!grown) {error=SVT_AV2_ERROR_OUT_OF_MEMORY;goto done;}
            best_packet=grown;memcpy(best_packet,c->packet,trial_bytes);
            best_bytes=trial_bytes;best_cost=cost;winner=index;
            best_hash=ccso_auto_hash(best_packet,best_bytes);db_recon_copy(c,best_recon,0);
        }
    }
    if (!best_packet || c->packet_capacity<best_bytes) {error=SVT_AV2_ERROR_INVALID_STATE;goto done;}
    memcpy(c->packet,best_packet,best_bytes);db_recon_copy(c,best_recon,1);*bytes=best_bytes;
    uint64_t final_cost=0u;error=db_current_cost(c,best_bytes,&final_cost);
    if (error==SVT_AV2_ERROR_NONE && (final_cost!=best_cost || ccso_auto_hash(c->packet,best_bytes)!=best_hash))
        error=SVT_AV2_ERROR_INVALID_STATE;
    fprintf(stderr,"DEBLOCK_FINAL winner=%u forced=%d bytes=%zu Joff=%llu Jfinal=%llu\n",
        winner,forced,best_bytes,(unsigned long long)off_cost,(unsigned long long)best_cost);
done:
    free(raw);free(best_recon);free(best_packet);return error;
#else
    return ccso_auto_encode_base(c,bytes,0u,0,0);
#endif
}

#include "satd_packet.inc"

static SvtAv2Error encode_pending_frame(SvtAv2EncoderContext* context) {
    if (context->packet_pending || !context->frame_pending) {
        return SVT_AV2_ERROR_NONE;
    }

    if (context->fatal_error) return SVT_AV2_ERROR_INTERNAL;
    const uint32_t width = context->frame_width, height = context->frame_height;
    const SvtAv2Error recon_err = encode_reconstruction(context, width, height);
    if (recon_err != SVT_AV2_ERROR_NONE) {
        context->fatal_error = 1;
        return recon_err;
    }
    size_t stream_size = 0;
    if (ccso_auto_domain(context)) {
        const SvtAv2Error error=ccso_auto_encode(context,&stream_size);
        if (error!=SVT_AV2_ERROR_NONE) {context->fatal_error=1;return error;}
#if M6_SATD_ANGLES
        const SvtAv2Error satd_error=satd_packet(context,&stream_size);
        if (satd_error!=SVT_AV2_ERROR_NONE) {context->fatal_error=1;return satd_error;}
#endif
    } else {
    size_t capacity = svt_av2_tile_writer_max_payload_bytes(width, height) + 256u;
    if (capacity < 4096u) capacity = 4096u;
    int status;
    for (;;) {
        if (context->packet_capacity < capacity) {
            uint8_t* grown = (uint8_t*)realloc(context->packet, capacity);
            if (grown == NULL) { context->fatal_error = 1; return SVT_AV2_ERROR_OUT_OF_MEMORY; }
            context->packet = grown;
            context->packet_capacity = capacity;
        }
        if (context->config.still_picture) {
            status = svt_av2_encode_still_picture_mhccp(width, height,
                (uint32_t)context->config.bit_depth, (uint32_t)context->config.chroma_format,
                (uint32_t)context->config.qp + (context->config.bit_depth == SVT_AV2_BIT_DEPTH_10 ? 48u : 0u),
                context->coded_blocks, context->coded_block_count, context->config.intra_mode_selection,
                context->partition_decision_count ? context->partition_decisions : NULL,
                context->partition_decision_count, context->config.entropy_adaptation,
                context->config.transform_selection, context->config.mrl_search,
                context->config.mhccp_search, context->packet, context->packet_capacity, &stream_size);
        } else if (context->config.entropy_adaptation) {
            const uint32_t low_delay = context->config.prediction_structure == SVT_AV2_PRED_LOW_DELAY_P;
            status = svt_av2_encode_packet_tools_entropy(width, height,
                (uint32_t)context->config.bit_depth, (uint32_t)context->config.chroma_format,
                (uint32_t)context->config.qp + (context->config.bit_depth == SVT_AV2_BIT_DEPTH_10 ? 48u : 0u),
                context->pictures_encoded, context->coded_blocks, context->coded_block_count,
                low_delay ? context->coded_modes : NULL, low_delay ? context->coded_block_count / 3u : 0u,
                low_delay, context->config.intra_mode_selection,
                context->config.partition_selection ? context->partition_decisions : NULL,
                context->config.partition_selection ? context->partition_decision_count : 0u,
                context->config.entropy_adaptation, context->packet, context->packet_capacity, &stream_size);
        } else if (context->config.partition_selection) {
            const uint32_t low_delay = context->config.prediction_structure == SVT_AV2_PRED_LOW_DELAY_P;
            status = svt_av2_encode_packet_tools(width, height,
                (uint32_t)context->config.bit_depth, (uint32_t)context->config.chroma_format,
                (uint32_t)context->config.qp + (context->config.bit_depth == SVT_AV2_BIT_DEPTH_10 ? 48u : 0u),
                context->pictures_encoded, context->coded_blocks, context->coded_block_count,
                low_delay ? context->coded_modes : NULL, low_delay ? context->coded_block_count / 3u : 0u,
                low_delay, context->config.intra_mode_selection, context->partition_decisions,
                context->partition_decision_count, context->packet, context->packet_capacity, &stream_size);
        } else if (context->config.prediction_structure == SVT_AV2_PRED_LOW_DELAY_P) {
            status = svt_av2_encode_low_delay_packet_modes(width, height,
                (uint32_t)context->config.bit_depth, (uint32_t)context->config.chroma_format,
                (uint32_t)context->config.qp + (context->config.bit_depth == SVT_AV2_BIT_DEPTH_10 ? 48u : 0u),
                context->pictures_encoded, context->coded_blocks, context->coded_block_count,
                context->coded_modes, context->coded_block_count / 3u,
                context->config.intra_mode_selection,
                context->packet, context->packet_capacity, &stream_size);
        } else if (context->config.intra_mode_selection) {
            status = svt_av2_encode_intra_packet_modes(width, height,
                (uint32_t)context->config.bit_depth, (uint32_t)context->config.chroma_format,
                (uint32_t)context->config.qp + (context->config.bit_depth == SVT_AV2_BIT_DEPTH_10 ? 48u : 0u),
                context->pictures_encoded, context->coded_blocks, context->coded_block_count,
                context->packet, context->packet_capacity, &stream_size);
        } else status = svt_av2_encode_intra_packet_ex(width, height,
            (uint32_t)context->config.bit_depth, (uint32_t)context->config.chroma_format,
            (uint32_t)context->config.qp + (context->config.bit_depth == SVT_AV2_BIT_DEPTH_10 ? 48u : 0u),
            context->pictures_encoded, context->coded_blocks, context->coded_block_count,
            context->packet, context->packet_capacity, &stream_size);
        if (status != SVT_AV2_BW_ERROR_NO_SPACE) break;
        if (capacity > SIZE_MAX / 2u) { context->fatal_error = 1; return SVT_AV2_ERROR_OUT_OF_MEMORY; }
        capacity *= 2u;
    }
    if (status != 0 || stream_size == 0) {
        context->fatal_error = 1;
        return SVT_AV2_ERROR_INTERNAL;
    }

    }
    if (context->config.prediction_structure == SVT_AV2_PRED_LOW_DELAY_P) {
        SvtAv2Error ref_status = svt_av2_reference_prepare(&context->reference, &context->recon,
            context->pictures_encoded);
        if (ref_status == SVT_AV2_ERROR_NONE) ref_status = svt_av2_reference_commit(&context->reference);
        if (ref_status != SVT_AV2_ERROR_NONE) { context->fatal_error = 1; return ref_status; }
    }
    context->recon_frame_index = context->pictures_encoded;

    context->packet_size = stream_size;
    context->packet_pending = 1;
    context->frame_pending = 0;
    context->pictures_encoded++;
    return SVT_AV2_ERROR_NONE;
}

static int collect_coefficients(void* user, uint32_t plane,
    const SvtAv2BlockView* block, const SvtAv2ReconScratch* scratch, int all_zero) {
    SvtAv2EncoderContext* context = (SvtAv2EncoderContext*)user;
    (void)all_zero;
    size_t count = (size_t)SVT_AV2_MIN(block->w, 32u) * SVT_AV2_MIN(block->h, 32u);
    if (context->coded_block_count >= context->coded_block_capacity ||
        count > context->coded_coeff_capacity - context->coded_coeff_count) return -1;
    SvtAv2TileCoeffBlock* entry = &context->coded_blocks[context->coded_block_count++];
    entry->plane = plane; entry->x = block->x; entry->y = block->y;
    entry->width = block->w; entry->height = block->h;
    entry->qcoeff = context->coded_coeffs + context->coded_coeff_count;
    entry->tx_type = 0u;
    entry->mrl = 0u;
    entry->intra_mode = plane == 0u ? (uint8_t)context->m5_last_intra_mode : 0u;
    memcpy(context->coded_coeffs + context->coded_coeff_count, scratch->qcoeff, count * sizeof(int32_t));
    context->coded_coeff_count += count;
    return 0;
}

static int collect_inter_coefficients(void* user, const SvtAv2InterBlockResult* result,
    const SvtAv2ReconScratch* scratch) {
    SvtAv2EncoderContext* c = (SvtAv2EncoderContext*)user;
    if (!c->reference.valid || result->reference_frame_index != c->reference.frame_index ||
        result->reference_frame_index + 1u != c->pictures_encoded) return -1;
    if (result->plane == 0u) {
        c->m5_last_intra_mode = result->intra_mode;
        SvtAv2TileBlockMode* m = c->coded_modes + c->coded_block_count / 3u;
        m->is_inter = result->mode == SVT_AV2_BLOCK_SINGLE_INTER;
        m->mv_x = result->mv.mv_x; m->mv_y = result->mv.mv_y;
    }
    return collect_coefficients(user, result->plane, &result->block, scratch, result->all_zero);
}

static int m5_collect_intra_mode(void* user, uint32_t mode) {
    ((SvtAv2EncoderContext*)user)->m5_last_intra_mode = mode;
    return 0;
}

/* The hook retains exactly the qcoeff used by this reconstruction. */
static SvtAv2Error encode_reconstruction(SvtAv2EncoderContext* context,
                                         uint32_t width,
                                         uint32_t height) {
    if (!context->recon_ready) {
        return SVT_AV2_ERROR_INVALID_STATE;
    }

    size_t samples = (size_t)width * height * 3u /
        (context->config.chroma_format == SVT_AV2_CHROMA_444 ? 1u : 2u);
    size_t blocks = (size_t)width * height / 64u * 3u;
    if (context->coded_block_capacity < blocks) {
        SvtAv2TileCoeffBlock* grown = (SvtAv2TileCoeffBlock*)realloc(context->coded_blocks,
            blocks * sizeof(*grown));
        if (grown == NULL) return SVT_AV2_ERROR_OUT_OF_MEMORY;
        context->coded_blocks = grown;
        SvtAv2TileBlockMode* modes = (SvtAv2TileBlockMode*)realloc(context->coded_modes,
            (blocks / 3u) * sizeof(*modes));
        if (modes == NULL) return SVT_AV2_ERROR_OUT_OF_MEMORY;
        context->coded_modes = modes; context->coded_block_capacity = blocks;
    }
    if (context->coded_coeff_capacity < samples) {
        int32_t* grown = (int32_t*)realloc(context->coded_coeffs, samples * sizeof(*grown));
        if (grown == NULL) return SVT_AV2_ERROR_OUT_OF_MEMORY;
        context->coded_coeffs = grown; context->coded_coeff_capacity = samples;
    }
    context->coded_block_count = context->coded_coeff_count = 0;
    context->partition_decision_count = 0;
    memset(&context->rd_stats, 0, sizeof(context->rd_stats));
    if (context->config.partition_selection || context->config.rd_mode) {
        SvtAv2FrameGeometry geometry;
        SvtAv2Error error = svt_av2_frame_geometry_init_ex(&geometry, width, height, 64u,
            context->config.chroma_format);
        if (error != SVT_AV2_ERROR_NONE) return error;
        const size_t count = (size_t)geometry.sb_cols * geometry.sb_rows;
        if (context->partition_decision_capacity < count) {
            uint16_t* grown = (uint16_t*)realloc(context->partition_decisions, count * sizeof(*grown));
            if (grown == NULL) return SVT_AV2_ERROR_OUT_OF_MEMORY;
            context->partition_decisions = grown;
            context->partition_decision_capacity = count;
        }
        if (!context->config.rd_mode) {
            error = svt_av2_partition_select_quads(&context->frame_source, &geometry,
                context->partition_decisions, context->partition_decision_capacity);
            if (error != SVT_AV2_ERROR_NONE) return error;
        } else memset(context->partition_decisions, 0, count * sizeof(*context->partition_decisions));
        context->partition_decision_count = count;
    }
    SvtAv2IntraFrameConfig config;
    memset(&config, 0, sizeof(config));
    config.base_q_idx = (uint32_t)context->config.qp +
        (context->config.bit_depth == SVT_AV2_BIT_DEPTH_10 ? 48u : 0u);
    config.sb_size = 64u;
    config.frame_reset_value = (uint16_t)(1u << ((uint32_t)context->config.bit_depth - 1u));
    config.block_hook = collect_coefficients;
    config.m5_select_intra = context->config.intra_mode_selection;
    config.m5_mode_hook = config.m5_select_intra ? m5_collect_intra_mode : NULL;
    config.block_hook_user = context;
    config.partition_decisions = context->partition_decision_count ? context->partition_decisions : NULL;
    config.partition_decision_count = context->partition_decision_count;

    const SvtAv2Error path_err =
        svt_av2_transform_path_init_bd(&context->path_state, &config.transform, config.base_q_idx,
            (uint32_t)context->config.bit_depth);
    if (path_err != SVT_AV2_ERROR_NONE) {
        return path_err;
    }

    /* All-Intra has no inter reference; the intra neighbour state is per frame.
     * The core overwrites every visited sample, but resetting makes the initial
     * state explicit rather than depending on the previous frame. */
    SvtAv2Error status = svt_av2_recon_reset_frame(&context->recon, config.frame_reset_value);
    if (status != SVT_AV2_ERROR_NONE) {
        return status;
    }

    if (context->config.rd_mode) {
        SvtAv2RdFrameConfig rd;
        memset(&rd, 0, sizeof(rd));
        rd.transform = config.transform;
        rd.rd_mode = context->config.rd_mode;
        rd.coeff_optimization = context->config.coeff_optimization;
        rd.motion_optimization = context->config.motion_optimization;
        rd.transform_selection = context->config.transform_selection;
        rd.tree_finalists = context->config.tree_refinement == 2u ?
            (context->config.partition_selection >= 10u ? 10u :
             context->config.partition_selection >= 8u ? 8u : 4u) :
            context->config.tree_refinement ? 2u : 0u;
        rd.rd_lambda_model = context->config.rd_lambda_model;
        rd.cfl_search = context->config.cfl_search;
        rd.mhccp_search = context->config.mhccp_search;
        rd.intra_joint = context->config.intra_joint;
        rd.angle_search = context->config.angle_search;
        rd.satd_angles = context->satd_angles;
        rd.partition_selection = context->config.partition_selection;
        svt_av2_frame_header_config_default(&rd.frame_header, width, height);
        rd.frame_header.base_q_idx = config.base_q_idx;
        rd.frame_header.multi_frame = !context->config.still_picture;
        rd.frame_header.inter_frame = context->config.prediction_structure == SVT_AV2_PRED_LOW_DELAY_P &&
            context->pictures_encoded != 0u;
        rd.frame_header.order_hint = context->pictures_encoded & 255u;
        rd.frame_header.reduced_tx_set = context->config.transform_selection == 2u ? 0u : context->config.transform_selection ? 1u : 2u;
        rd.frame_header.enable_mrls = context->config.mrl_search;
        rd.frame_header.enable_mhccp = context->config.mhccp_search;
        rd.frame_header.bit_depth = (uint32_t)context->config.bit_depth;
        rd.frame_header.chroma_format = (uint32_t)context->config.chroma_format;
        rd.frame_header.m5_intra_modes = context->config.intra_mode_selection;
        rd.frame_header.entropy_adaptation = context->config.entropy_adaptation;
        SvtAv2RdFrameOutput output;
        memset(&output, 0, sizeof(output));
        output.blocks = context->coded_blocks; output.block_capacity = context->coded_block_capacity;
        output.modes = context->coded_modes; output.mode_capacity = context->coded_block_capacity / 3u;
        output.qcoeff = context->coded_coeffs; output.coeff_capacity = context->coded_coeff_capacity;
        output.decisions = context->partition_decisions; output.decision_capacity = context->partition_decision_capacity;
        SvtAv2RdFrameStats stats;
        memset(&stats, 0, sizeof(stats));
        status = svt_av2_rd_frame_encode(&context->frame_source, &context->recon,
            rd.frame_header.inter_frame ? &context->reference : NULL, &rd, &context->scratch, &output, &stats);
        if (status != SVT_AV2_ERROR_NONE) return status;
        context->coded_block_count = output.block_count;
        context->coded_coeff_count = output.coeff_count;
        context->partition_decision_count = output.decision_count;
        context->rd_stats.candidate_count = stats.candidate_count;
        context->rd_stats.forward_calls = stats.forward_calls;
        context->rd_stats.replay_checks = stats.replay_checks;
        context->rd_stats.distortion = stats.distortion;
        context->rd_stats.rate_q12 = stats.rate_q12;
        context->rd_stats.cost = stats.cost;
        context->rd_stats.lambda_q8 = stats.lambda_q8;
        context->rd_stats.full_superblocks = stats.full_superblocks;
        context->rd_stats.partial_superblocks = stats.partial_superblocks;
        context->rd_stats.motion_candidates = stats.motion_candidates;
        context->rd_stats.winner_hash = stats.winner_hash;
        context->rd_stats.coeff_trials = stats.coeff_trials;
        context->rd_stats.coeff_accepts = stats.coeff_accepts;
        context->rd_stats.motion_trials = stats.motion_trials;
        context->rd_stats.motion_accepts = stats.motion_accepts;
        context->rd_stats.wide_trials = stats.wide_trials;
        context->rd_stats.wide_accepts = stats.wide_accepts;
        return SVT_AV2_ERROR_NONE;
    }
    if (context->config.prediction_structure == SVT_AV2_PRED_LOW_DELAY_P && context->pictures_encoded != 0u) {
        SvtAv2InterFrameConfig inter;
        memset(&inter, 0, sizeof(inter));
        inter.transform = config.transform;
        inter.block_hook = collect_inter_coefficients;
        inter.block_hook_user = context;
        inter.m5_select_intra = context->config.intra_mode_selection;
        inter.partition_decisions = config.partition_decisions;
        inter.partition_decision_count = config.partition_decision_count;
        SvtAv2InterFrameStats stats;
        memset(&stats, 0, sizeof(stats));
        return svt_av2_inter_frame_encode(&context->frame_source, &context->recon,
            &context->reference, &inter, &context->scratch, &stats);
    }
    SvtAv2IntraFrameStats stats;
    memset(&stats, 0, sizeof(stats));
    (void)width;
    (void)height;
    return svt_av2_intra_frame_encode(&context->frame_source,
                                      &context->recon,
                                      &config,
                                      &context->scratch,
                                      &stats);
}

/* Exports the visible samples of the frame most recently taken by
 * svt_av2_enc_get_packet_ex(). Mirrors SvtAv2Packet: caller-owned buffer,
 * capacity as input, and a too-small buffer reports the required size without
 * writing. The bytes come from T012's single packing primitive. */
SvtAv2Error svt_av2_enc_get_rd_stats(SvtAv2Handle handle, SvtAv2RdStats* stats) {
    SvtAv2EncoderContext* context = context_from_handle(handle);
    if (context == NULL) return SVT_AV2_ERROR_INVALID_HANDLE;
    if (stats == NULL) return SVT_AV2_ERROR_INVALID_PARAMETER;
    if (context->state != SVT_AV2_STATE_INITIALIZED || context->pictures_encoded == 0u)
        return SVT_AV2_ERROR_INVALID_STATE;
    if (context->fatal_error) return SVT_AV2_ERROR_INTERNAL;
    *stats = context->rd_stats;
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_enc_get_recon(SvtAv2Handle handle, SvtAv2ReconFrame* recon) {
    SvtAv2EncoderContext* context = context_from_handle(handle);
    if (context == NULL) {
        return SVT_AV2_ERROR_INVALID_HANDLE;
    }
    if (context->state != SVT_AV2_STATE_INITIALIZED) {
        return SVT_AV2_ERROR_INVALID_STATE;
    }
    if (recon == NULL || recon->data == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (context->fatal_error) return SVT_AV2_ERROR_INTERNAL;
    if (!context->recon_ready || context->pictures_encoded == 0 || context->recon.planes[0] == NULL) {
        return SVT_AV2_ERROR_INVALID_STATE;
    }

    SvtAv2ReconExport export_meta;
    memset(&export_meta, 0, sizeof(export_meta));
    SvtAv2Error status = svt_av2_recon_export_frame(&context->recon,
                                                    context->recon_frame_index,
                                                    (uint8_t*)recon->data,
                                                    recon->capacity,
                                                    &export_meta);
    if (status == SVT_AV2_ERROR_INVALID_PARAMETER) {
        /* Distinguish "insufficient capacity" from "bad inputs" so the caller
         * can grow and retry, exactly as svt_av2_enc_get_packet_ex() does. The
         * required size is the raw size of the recon picture. */
        size_t required = 0;
        if (svt_av2_picture_raw_size(&context->recon, &required) == SVT_AV2_ERROR_NONE &&
            recon->capacity < required) {
            recon->byte_count = required;
            return SVT_AV2_ERROR_OUT_OF_MEMORY;
        }
    }
    if (status != SVT_AV2_ERROR_NONE) {
        return status;
    }

    recon->frame_index = export_meta.frame_index;
    recon->visible_width = export_meta.visible_width;
    recon->visible_height = export_meta.visible_height;
    recon->chroma_width = export_meta.chroma_width;
    recon->chroma_height = export_meta.chroma_height;
    recon->byte_count = export_meta.byte_count;
    recon->bit_depth = context->config.bit_depth;
    recon->chroma_format = context->config.chroma_format;
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_enc_get_packet(SvtAv2Handle handle, void* packet, int32_t* has_packet) {
    SvtAv2EncoderContext* context = context_from_handle(handle);
    if (context == NULL) {
        return SVT_AV2_ERROR_INVALID_HANDLE;
    }
    if (context->state != SVT_AV2_STATE_INITIALIZED) {
        return SVT_AV2_ERROR_INVALID_STATE;
    }
    if (packet == NULL || has_packet == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    /* This legacy entry point has no capacity parameter, so it cannot deliver
     * bytes: it reports availability only. Callers that want the stream use
     * svt_av2_enc_get_packet_ex() below. */
    *has_packet = 0;
    const SvtAv2Error err = encode_pending_frame(context);
    if (err != SVT_AV2_ERROR_NONE) {
        return err;
    }
    if (context->packet_pending) {
        *has_packet = (context->packet_size > 0) ? 1 : 0;
    }
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_enc_get_packet_ex(SvtAv2Handle handle, SvtAv2Packet* packet) {
    SvtAv2EncoderContext* context = context_from_handle(handle);
    if (context == NULL) {
        return SVT_AV2_ERROR_INVALID_HANDLE;
    }
    if (context->state != SVT_AV2_STATE_INITIALIZED) {
        return SVT_AV2_ERROR_INVALID_STATE;
    }
    if (packet == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    packet->size = 0;
    packet->pts = 0;

    /* A previously produced packet that the caller did not take is handed out
     * now rather than being silently replaced. */
    const SvtAv2Error err = encode_pending_frame(context);
    if (err != SVT_AV2_ERROR_NONE) {
        return err;
    }
    if (!context->packet_pending) {
        /* Nothing encoded yet, or the stream has ended. Both are "no packet",
         * distinguishable from failure by the NONE return code. */
        return SVT_AV2_ERROR_NONE;
    }
    if (packet->data == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    /* The caller sizes the buffer; a stream that does not fit is reported (with
     * the required size) instead of being truncated. */
    if (packet->capacity < context->packet_size) {
        packet->size = context->packet_size;
        return SVT_AV2_ERROR_OUT_OF_MEMORY;
    }

    memcpy(packet->data, context->packet, context->packet_size);
    packet->size = context->packet_size;
    packet->pts = context->frame_pts;
    context->packet_pending = 0;
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_enc_deinit(SvtAv2Handle* handle) {
    if (handle == NULL || *handle == NULL) {
        return SVT_AV2_ERROR_INVALID_HANDLE;
    }
    SvtAv2EncoderContext* context = *handle;
    if (context->magic != SVT_AV2_CONTEXT_MAGIC) {
        return SVT_AV2_ERROR_INVALID_HANDLE;
    }

    context->state = SVT_AV2_STATE_CLOSED;
    context->magic = 0;
    svt_av2_context_release_pipeline(context);
    if (context->config.output_path != NULL) {
        free((void*)context->config.output_path);
        context->config.output_path = NULL;
    }
    free(context);
    *handle = NULL;
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_enc_get_config(SvtAv2Handle handle, SvtAv2EncoderConfig* config) {
    SvtAv2EncoderContext* context = context_from_handle(handle);
    if (context == NULL) {
        return SVT_AV2_ERROR_INVALID_HANDLE;
    }
    if (config == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    *config = context->config;
    return SVT_AV2_ERROR_NONE;
}

const SvtAv2EncoderInfo* svt_av2_enc_get_build_info(void) {
    static SvtAv2EncoderInfo info;
    static int              initialized = 0;
    if (!initialized) {
        const SvtAv2BuildInfo* build = svt_av2_get_build_info();
        memset(&info, 0, sizeof(info));
        info.version_major = SVT_AV2_VERSION_MAJOR;
        info.version_minor = SVT_AV2_VERSION_MINOR;
        info.version_patch = SVT_AV2_VERSION_PATCH;
        copy_string(info.version_string, sizeof(info.version_string), build->version_string);
        copy_string(info.build_type, sizeof(info.build_type), build->build_type);
        copy_string(info.compiler, sizeof(info.compiler), build->compiler);
        copy_string(info.milestone, sizeof(info.milestone), build->milestone);
        initialized = 1;
    }
    return &info;
}

/* --- M0 placeholder payload ---------------------------------------------- */

static void store_u16_le(uint8_t* buffer, uint16_t value) {
    buffer[0] = (uint8_t)(value & 0xFFu);
    buffer[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void store_u32_le(uint8_t* buffer, uint32_t value) {
    buffer[0] = (uint8_t)(value & 0xFFu);
    buffer[1] = (uint8_t)((value >> 8) & 0xFFu);
    buffer[2] = (uint8_t)((value >> 16) & 0xFFu);
    buffer[3] = (uint8_t)((value >> 24) & 0xFFu);
}

SvtAv2Error svt_av2_m0_build_placeholder_payload(const SvtAv2EncoderConfig* config,
                                                 uint32_t frame_index,
                                                 uint8_t* out_buffer,
                                                 size_t out_capacity,
                                                 size_t* out_size) {
    if (config == NULL || out_buffer == NULL || out_size == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    /* Deterministic payload length derived from the frame index only, so that
     * two runs with the same input always produce identical bytes. */
    const uint16_t payload_length = (uint16_t)(64u + (frame_index % 64u));
    const size_t   total = SVT_AV2_M0_PLACEHOLDER_HEADER_SIZE + (size_t)payload_length;
    if (out_capacity < total) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    memcpy(out_buffer, SVT_AV2_M0_PLACEHOLDER_MAGIC, SVT_AV2_M0_PLACEHOLDER_MAGIC_SIZE);
    out_buffer[6] = (uint8_t)SVT_AV2_VERSION_MAJOR;
    out_buffer[7] = (uint8_t)SVT_AV2_VERSION_MINOR;
    store_u32_le(out_buffer + 8, frame_index);
    store_u32_le(out_buffer + 12, config->width);
    store_u32_le(out_buffer + 16, config->height);
    out_buffer[20] = (uint8_t)config->chroma_format;
    out_buffer[21] = (uint8_t)config->bit_depth;
    store_u16_le(out_buffer + 22, payload_length);

    for (uint16_t i = 0; i < payload_length; ++i) {
        out_buffer[SVT_AV2_M0_PLACEHOLDER_HEADER_SIZE + i] = (uint8_t)((frame_index + i) & 0xFFu);
    }

    *out_size = total;
    return SVT_AV2_ERROR_NONE;
}
