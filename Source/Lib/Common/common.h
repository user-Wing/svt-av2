/*
 * SVT-AV2 internal common definitions.
 *
 * Keep this header free of codec logic: it only holds types and helpers shared
 * by every module (build information, sample types, small utilities).
 */
#ifndef SVT_AV2_COMMON_H
#define SVT_AV2_COMMON_H

#include <stddef.h>
#include <stdint.h>

#include "svt_av2.h"

#define SVT_AV2_UNUSED(x) ((void)(x))

#define SVT_AV2_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define SVT_AV2_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define SVT_AV2_MAX(a, b) (((a) > (b)) ? (a) : (b))

#define SVT_AV2_ALIGN_UP(value, align) (((value) + ((align) - 1)) & ~((size_t)(align) - 1))

#define SVT_AV2_DIV_UP(a, b) (((a) + (b) - 1) / (b))

/* Largest sample value for a bit depth, as a signed 32-bit integer. */
#define SVT_AV2_MAX_SAMPLE(bit_depth) ((int32_t)((1 << (bit_depth)) - 1))

/* --- build information ---------------------------------------------------- */

typedef struct SvtAv2BuildInfo {
    const char* version_string;
    const char* build_type;
    const char* compiler;
    const char* milestone;
    const char* oracle_dir; /* empty string when no AVM oracle was configured */
} SvtAv2BuildInfo;

const SvtAv2BuildInfo* svt_av2_get_build_info(void);

/* --- chroma / geometry helpers -------------------------------------------- */

/* Horizontal and vertical chroma subsampling shift for a chroma format. */
uint32_t svt_av2_chroma_shift_x(SvtAv2ChromaFormat format);
uint32_t svt_av2_chroma_shift_y(SvtAv2ChromaFormat format);

/* Number of planes for a chroma format (1 for 4:0:0, 3 otherwise). */
uint32_t svt_av2_plane_count(SvtAv2ChromaFormat format);

/* Bytes per sample for a bit depth, following the 9-bit boundary. */
uint32_t svt_av2_bytes_per_sample(SvtAv2BitDepth bit_depth);

/* True when the bit depth uses more than 8 bits per sample. */
int svt_av2_is_high_bit_depth(SvtAv2BitDepth bit_depth);

const char* svt_av2_chroma_format_name(SvtAv2ChromaFormat format);

/* --- frame size helpers --------------------------------------------------- */

/* Computes the coded plane sizes for a frame. Any output pointer may be NULL.
 * Returns SVT_AV2_ERROR_INVALID_PARAMETER for unusable geometry. */
SvtAv2Error svt_av2_compute_plane_dimensions(uint32_t width,
                                             uint32_t height,
                                             SvtAv2ChromaFormat format,
                                             uint32_t* luma_width,
                                             uint32_t* luma_height,
                                             uint32_t* chroma_width,
                                             uint32_t* chroma_height);

/* Total number of samples (all planes) for a frame. */
SvtAv2Error svt_av2_compute_frame_sample_count(uint32_t width,
                                               uint32_t height,
                                               SvtAv2ChromaFormat format,
                                               size_t* sample_count);

#endif /* SVT_AV2_COMMON_H */
