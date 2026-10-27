/* SVT-AV2 common helpers: build information and geometry arithmetic. */

#include "common.h"

#include <string.h>

/* --- build information ---------------------------------------------------- */

/* Injected by the build system; empty strings when built outside CMake. */
#ifndef SVT_AV2_BUILD_TYPE_STRING
#define SVT_AV2_BUILD_TYPE_STRING "unknown"
#endif
#ifndef SVT_AV2_COMPILER_STRING
#define SVT_AV2_COMPILER_STRING "unknown"
#endif
#ifndef SVT_AV2_ORACLE_DIR_STRING
#define SVT_AV2_ORACLE_DIR_STRING ""
#endif

static const SvtAv2BuildInfo g_svt_av2_build_info = {
    SVT_AV2_VERSION_STRING,
    SVT_AV2_BUILD_TYPE_STRING,
    SVT_AV2_COMPILER_STRING,
    "M5",
    SVT_AV2_ORACLE_DIR_STRING,
};

const SvtAv2BuildInfo* svt_av2_get_build_info(void) { return &g_svt_av2_build_info; }

/* --- chroma helpers ------------------------------------------------------- */

uint32_t svt_av2_chroma_shift_x(SvtAv2ChromaFormat format) {
    switch (format) {
    case SVT_AV2_CHROMA_400:
    case SVT_AV2_CHROMA_444: return 0;
    case SVT_AV2_CHROMA_422:
    case SVT_AV2_CHROMA_420: return 1;
    default: return 0;
    }
}

uint32_t svt_av2_chroma_shift_y(SvtAv2ChromaFormat format) {
    switch (format) {
    case SVT_AV2_CHROMA_400:
    case SVT_AV2_CHROMA_444:
    case SVT_AV2_CHROMA_422: return 0;
    case SVT_AV2_CHROMA_420: return 1;
    default: return 0;
    }
}

uint32_t svt_av2_plane_count(SvtAv2ChromaFormat format) {
    return (format == SVT_AV2_CHROMA_400) ? 1u : 3u;
}

uint32_t svt_av2_bytes_per_sample(SvtAv2BitDepth bit_depth) {
    return (bit_depth > SVT_AV2_BIT_DEPTH_8) ? 2u : 1u;
}

int svt_av2_is_high_bit_depth(SvtAv2BitDepth bit_depth) { return bit_depth > SVT_AV2_BIT_DEPTH_8; }

const char* svt_av2_chroma_format_name(SvtAv2ChromaFormat format) {
    switch (format) {
    case SVT_AV2_CHROMA_400: return "4:0:0";
    case SVT_AV2_CHROMA_420: return "4:2:0";
    case SVT_AV2_CHROMA_422: return "4:2:2";
    case SVT_AV2_CHROMA_444: return "4:4:4";
    default: return "unknown";
    }
}

/* --- geometry ------------------------------------------------------------- */

SvtAv2Error svt_av2_compute_plane_dimensions(uint32_t width,
                                             uint32_t height,
                                             SvtAv2ChromaFormat format,
                                             uint32_t* luma_width,
                                             uint32_t* luma_height,
                                             uint32_t* chroma_width,
                                             uint32_t* chroma_height) {
    if (width == 0 || height == 0) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (format != SVT_AV2_CHROMA_400 && format != SVT_AV2_CHROMA_420 && format != SVT_AV2_CHROMA_422 &&
        format != SVT_AV2_CHROMA_444) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    const uint32_t shift_x = svt_av2_chroma_shift_x(format);
    const uint32_t shift_y = svt_av2_chroma_shift_y(format);

    /* Chroma planes are rounded up: an odd luma size still needs a full chroma
     * sample at the right/bottom edge. Compute the ceil-division without
     * SVT_AV2_DIV_UP: for width == UINT32_MAX the macro's "+1" wraps to zero
     * and would silently report a zero-sized chroma plane. */
    const uint32_t cw = (shift_x == 0) ? width : (width >> shift_x) + ((width & ((1u << shift_x) - 1u)) ? 1u : 0u);
    const uint32_t ch = (shift_y == 0) ? height : (height >> shift_y) + ((height & ((1u << shift_y) - 1u)) ? 1u : 0u);

    if (luma_width != NULL) {
        *luma_width = width;
    }
    if (luma_height != NULL) {
        *luma_height = height;
    }
    if (chroma_width != NULL) {
        *chroma_width = (format == SVT_AV2_CHROMA_400) ? 0u : cw;
    }
    if (chroma_height != NULL) {
        *chroma_height = (format == SVT_AV2_CHROMA_400) ? 0u : ch;
    }
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_compute_frame_sample_count(uint32_t width,
                                               uint32_t height,
                                               SvtAv2ChromaFormat format,
                                               size_t* sample_count) {
    if (sample_count == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    uint32_t lw = 0;
    uint32_t lh = 0;
    uint32_t cw = 0;
    uint32_t ch = 0;
    const SvtAv2Error err = svt_av2_compute_plane_dimensions(width, height, format, &lw, &lh, &cw, &ch);
    if (err != SVT_AV2_ERROR_NONE) {
        return err;
    }

    /* The luma plane is always present. Guard every multiplication and the
     * accumulation against size_t overflow: a silently truncated sample count
     * would later turn into an undersized allocation. */
    const size_t plane_count = svt_av2_plane_count(format);
    size_t       total = 0;

    if (lw != 0 && lh > (SIZE_MAX / (size_t)lw)) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    total = (size_t)lw * (size_t)lh;

    if (plane_count > 1) {
        if (cw != 0 && ch > (SIZE_MAX / (size_t)cw)) {
            return SVT_AV2_ERROR_INVALID_PARAMETER;
        }
        const size_t chroma_samples = (size_t)cw * (size_t)ch;
        if (chroma_samples > (SIZE_MAX - total) / 2u) {
            return SVT_AV2_ERROR_INVALID_PARAMETER;
        }
        total += 2u * chroma_samples;
    }

    *sample_count = total;
    return SVT_AV2_ERROR_NONE;
}
