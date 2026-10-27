/* SVT-AV2 picture and plane storage (T007).
 *
 * See picture.h for the frozen contract and for the caller-responsibility note
 * about strides. Everything here is 8-bit 4:2:0: sample type uint8_t, stride in
 * BYTES (equal to the sample count only for 8-bit; kept as a distinct concept
 * so the 10-bit work in M3 does not have to change callers).
 */

#include "picture.h"

#include <stdlib.h>
#include <string.h>


static int format_supported(uint32_t bd, SvtAv2ChromaFormat format) {
    return (bd == 8 && format == SVT_AV2_CHROMA_420) ||
           (bd == 10 && (format == SVT_AV2_CHROMA_420 || format == SVT_AV2_CHROMA_444));
}
static void fill_samples(uint8_t* dst, uint32_t bytes, uint32_t bd, uint16_t value) {
    const uint32_t step = bd > 8 ? 2u : 1u;
    for (uint32_t x = 0; x < bytes; x += step) svt_av2_sample_store(dst + x, bd, value);
}
static int samples_valid(const SvtAv2FrameBuffer* shape, const uint8_t* const planes[3],
                         const uint32_t strides[3]) {
    if (shape->bit_depth == 8) return 1;
    for (uint32_t p = 0; p < 3; ++p)
        for (uint32_t y = 0; y < svt_av2_picture_plane_height(shape, p); ++y)
            for (uint32_t x = 0; x < svt_av2_picture_plane_width(shape, p); ++x)
                if (svt_av2_sample_load(planes[p] + (size_t)y * strides[p] + 2u * x, 10) > 1023) return 0;
    return 1;
}

/* Stride and vertical alignment, in bytes/rows. Not a specification
 * requirement: AV2 only requires the samples to be addressable. A power of two
 * keeps the address arithmetic cheap and makes the overflow check exact. */
#define SVT_AV2_PICTURE_ALIGN 32u

static uint32_t align_up(uint32_t value, uint32_t align) { return (value + (align - 1u)) & ~(align - 1u); }

/* True when the descriptor looks like a successful svt_av2_picture_alloc(). */
static int picture_is_allocated(const SvtAv2FrameBuffer* picture) {
    return picture != NULL && picture->buffer != NULL && picture->visible_width != 0 &&
           picture->visible_height != 0;
}

static int plane_is_valid(uint32_t plane) { return plane < SVT_AV2_PICTURE_MAX_PLANES; }

/* Visible width/height of one plane given only the luma geometry. Kept separate
 * from svt_av2_picture_plane_width/height (which take a descriptor) so the
 * shared copy loop can serve both SvtAv2FrameBuffer and SvtAv2BorrowedView. */
static uint32_t plane_width_from_luma(uint32_t luma_width, uint32_t plane, SvtAv2ChromaFormat format) {
    return (plane == SVT_AV2_PLANE_Y || format == SVT_AV2_CHROMA_444) ? luma_width : (luma_width >> 1);
}

static uint32_t plane_height_from_luma(uint32_t luma_height, uint32_t plane, SvtAv2ChromaFormat format) {
    return (plane == SVT_AV2_PLANE_Y || format == SVT_AV2_CHROMA_444) ? luma_height : (luma_height >> 1);
}

/* THE single visible-samples copy loop, shared by svt_av2_picture_copy_in and
 * svt_av2_picture_copy_from_view (picture.h: the module must not contain a
 * second copy of this). Both callers have already validated their planes, so
 * this only performs the row-by-row copy of the visible width.
 *
 * The destination geometry is taken from the destination descriptor (it owns
 * its strides); the source planes/strides are passed directly so the same loop
 * works for an owned source and for a borrowed view. */
static void copy_visible_planes(uint8_t* const dst_planes[SVT_AV2_PICTURE_MAX_PLANES],
                                const uint32_t dst_strides[SVT_AV2_PICTURE_MAX_PLANES],
                                const SvtAv2FrameBuffer* dst_geometry,
                                const uint8_t* const src_planes[SVT_AV2_PICTURE_MAX_PLANES],
                                const uint32_t src_strides[SVT_AV2_PICTURE_MAX_PLANES]) {
    for (uint32_t plane = 0; plane < SVT_AV2_PICTURE_MAX_PLANES; ++plane) {
        const uint32_t visible_width = plane_width_from_luma(dst_geometry->visible_width, plane, dst_geometry->chroma_format) * (dst_geometry->bit_depth > 8 ? 2u : 1u);
        const uint32_t visible_height = plane_height_from_luma(dst_geometry->visible_height, plane, dst_geometry->chroma_format);
        const uint8_t* src = src_planes[plane];
        uint8_t* dst = dst_planes[plane];
        const uint32_t src_stride = src_strides[plane];
        const uint32_t dst_stride = dst_strides[plane];

        for (uint32_t row = 0; row < visible_height; ++row) {
            memcpy(dst + (size_t)row * dst_stride, src + (size_t)row * src_stride, visible_width);
        }
    }
}

uint32_t svt_av2_picture_plane_width(const SvtAv2FrameBuffer* picture, uint32_t plane) {
    if (picture == NULL || !plane_is_valid(plane)) {
        return 0;
    }
    if (plane == SVT_AV2_PLANE_Y || picture->chroma_format == SVT_AV2_CHROMA_444) {
        return picture->visible_width;
    }
    /* 4:2:0 only. visible_width is a multiple of 8, so this is exact. */
    return picture->visible_width >> 1;
}

uint32_t svt_av2_picture_plane_height(const SvtAv2FrameBuffer* picture, uint32_t plane) {
    if (picture == NULL || !plane_is_valid(plane)) {
        return 0;
    }
    if (plane == SVT_AV2_PLANE_Y || picture->chroma_format == SVT_AV2_CHROMA_444) {
        return picture->visible_height;
    }
    return picture->visible_height >> 1;
}

const uint8_t* svt_av2_picture_plane_const(const SvtAv2FrameBuffer* picture, uint32_t plane) {
    if (picture == NULL || !plane_is_valid(plane)) {
        return NULL;
    }
    return picture->planes[plane];
}

uint8_t* svt_av2_picture_plane(SvtAv2FrameBuffer* picture, uint32_t plane) {
    if (picture == NULL || !plane_is_valid(plane)) {
        return NULL;
    }
    return picture->planes[plane];
}

SvtAv2Error svt_av2_picture_alloc(SvtAv2FrameBuffer* picture, uint32_t width, uint32_t height) {
    return svt_av2_picture_alloc_ex(picture, width, height, 8, SVT_AV2_CHROMA_420);
}

SvtAv2Error svt_av2_picture_alloc_ex(SvtAv2FrameBuffer* picture, uint32_t width, uint32_t height,
                                   uint32_t bit_depth, SvtAv2ChromaFormat chroma_format) {
    if (picture == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    memset(picture, 0, sizeof(*picture));

    if (width == 0 || height == 0) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    /* The AV2 mi grid and the chroma reference blocks are both 8x8 granular, so
     * a size that is not a multiple of 8 would leave chroma columns/rows that
     * cannot be legally coded. Reject instead of silently cropping. */
    if ((width & 7u) != 0 || (height & 7u) != 0) {
        return SVT_AV2_ERROR_UNSUPPORTED;
    }

    if (!format_supported(bit_depth, chroma_format)) return SVT_AV2_ERROR_UNSUPPORTED;
    picture->bit_depth = bit_depth;
    picture->chroma_format = chroma_format;
    picture->visible_width = width;
    picture->visible_height = height;
    picture->plane_count = SVT_AV2_PICTURE_MAX_PLANES;

    /* The allocation overflow check must happen BEFORE every multiplication and
     * before align_up: a wrapped size would otherwise turn into an undersized
     * allocation.
     *
     * `plane_width`/`plane_height` are uint32_t and align_up adds ALIGN-1 before
     * masking, so the round-up can wrap exactly when the value exceeds
     * UINT32_MAX - (ALIGN - 1). That check is made in the uint32_t domain on
     * purpose: it is the widest comparison that is genuinely reachable, and it
     * stays correct whether or not size_t is wider than uint32_t. Comparing the
     * same operands against a SIZE_MAX-derived bound instead would be provably
     * always-false wherever size_t > uint32_t, which -Wtype-limits rejects.
     *
     * "Fits in size_t" is NOT a useful acceptance test on its own. 0x80000000 x
     * 0x80000000 with 4:2:0 padding totals 0x6000000000000000 (6.9 EiB): it does
     * not wrap, so a pure overflow check correctly sees nothing wrong, yet no
     * allocator can ever satisfy it. Plain libc calloc would return NULL, but
     * AddressSanitizer *aborts* the process on an oversized request rather than
     * reporting failure, so an impossible request becomes a crash instead of the
     * observable error the caller must handle.
     *
     * The ceiling below is therefore an explicit "largest frame we will even
     * try" bound rather than SIZE_MAX. 1 << 34 (16 GiB) is far above any size
     * this encoder is scoped for (1920x1080 4:2:0 is ~3 MiB; even 16K would be
     * well under this) while staying small enough that the request is always a
     * real, failable allocation instead of an absurd one. Anything larger is
     * reported as OUT_OF_MEMORY without calling the allocator. */
    const size_t max_total = (size_t)1 << 34;
    size_t total = 0;
    for (uint32_t plane = 0; plane < SVT_AV2_PICTURE_MAX_PLANES; ++plane) {
        const uint32_t samples = svt_av2_picture_plane_width(picture, plane);
        if (samples > UINT32_MAX / (bit_depth > 8 ? 2u : 1u)) return SVT_AV2_ERROR_INVALID_PARAMETER;
        const uint32_t plane_width = samples * (bit_depth > 8 ? 2u : 1u);
        const uint32_t plane_height = svt_av2_picture_plane_height(picture, plane);
        if (plane_width == 0 || plane_height == 0) {
            return SVT_AV2_ERROR_INVALID_PARAMETER;
        }
        if (plane_width > UINT32_MAX - (SVT_AV2_PICTURE_ALIGN - 1u) ||
            plane_height > UINT32_MAX - (SVT_AV2_PICTURE_ALIGN - 1u)) {
            return SVT_AV2_ERROR_INVALID_PARAMETER;
        }
        const uint32_t aligned_height = align_up(plane_height, SVT_AV2_PICTURE_ALIGN);
        const uint32_t stride = align_up(plane_width, SVT_AV2_PICTURE_ALIGN);

        picture->aligned_height[plane] = aligned_height;
        picture->strides[plane] = stride;

        if (aligned_height != 0 && stride > (max_total - total) / aligned_height) {
            memset(picture, 0, sizeof(*picture));
            return SVT_AV2_ERROR_OUT_OF_MEMORY;
        }
        total += (size_t)stride * (size_t)aligned_height;
    }

    uint8_t* buffer = (uint8_t*)calloc(total, 1);
    if (buffer == NULL) {
        /* Leave the descriptor zeroed so free() stays safe. */
        memset(picture, 0, sizeof(*picture));
        return SVT_AV2_ERROR_OUT_OF_MEMORY;
    }

    size_t offset = 0;
    for (uint32_t plane = 0; plane < SVT_AV2_PICTURE_MAX_PLANES; ++plane) {
        picture->planes[plane] = buffer + offset;
        offset += (size_t)picture->strides[plane] * (size_t)picture->aligned_height[plane];
    }
    picture->buffer = buffer;
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_picture_free(SvtAv2FrameBuffer* picture) {
    if (picture == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    free(picture->buffer);
    memset(picture, 0, sizeof(*picture));
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_picture_pad(SvtAv2FrameBuffer* picture, uint16_t pad_value) {
    if (!picture_is_allocated(picture)) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    for (uint32_t plane = 0; plane < SVT_AV2_PICTURE_MAX_PLANES; ++plane) {
        uint8_t* const base = picture->planes[plane];
        const uint32_t stride = picture->strides[plane];
        const uint32_t visible_width = svt_av2_picture_plane_width(picture, plane) * (picture->bit_depth > 8 ? 2u : 1u);
        const uint32_t visible_height = svt_av2_picture_plane_height(picture, plane);
        const uint32_t aligned_height = picture->aligned_height[plane];

        for (uint32_t row = 0; row < aligned_height; ++row) {
            uint8_t* const row_ptr = base + (size_t)row * stride;
            if (row >= visible_height) {
                fill_samples(row_ptr, stride, picture->bit_depth, pad_value);
            } else {
                fill_samples(row_ptr + visible_width, stride - visible_width, picture->bit_depth, pad_value);
            }
        }
    }
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_picture_copy_in(SvtAv2FrameBuffer* destination, const SvtAv2FrameBuffer* source) {
    if (!picture_is_allocated(destination) || !picture_is_allocated(source)) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (destination == source) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (destination->visible_width != source->visible_width ||
        destination->visible_height != source->visible_height ||
        destination->bit_depth != source->bit_depth || destination->chroma_format != source->chroma_format) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    /* Validate every plane before writing anything: a failure must not leave a
     * half-copied frame behind (contract 1.4). */
    for (uint32_t plane = 0; plane < SVT_AV2_PICTURE_MAX_PLANES; ++plane) {
        const uint32_t visible_width = svt_av2_picture_plane_width(source, plane) * (source->bit_depth > 8 ? 2u : 1u);
        if (source->planes[plane] == NULL) {
            return SVT_AV2_ERROR_INVALID_PARAMETER;
        }
        if (source->strides[plane] < visible_width) {
            return SVT_AV2_ERROR_INVALID_PARAMETER;
        }
    }

    if (!samples_valid(destination, (const uint8_t* const*)source->planes, source->strides))
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    copy_visible_planes(destination->planes,
                        destination->strides,
                        destination,
                        (const uint8_t* const*)source->planes,
                        source->strides);
    /* No source pointer is retained: the caller owns `source` and may free it
     * as soon as this call returns. */
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_picture_view(SvtAv2BorrowedView* out,
                                 const uint8_t* const planes[SVT_AV2_PICTURE_MAX_PLANES],
                                 const uint32_t strides[SVT_AV2_PICTURE_MAX_PLANES],
                                 uint32_t width,
                                 uint32_t height) {
    return svt_av2_picture_view_ex(out, planes, strides, width, height, 8, SVT_AV2_CHROMA_420);
}

SvtAv2Error svt_av2_picture_view_ex(SvtAv2BorrowedView* out,
                                  const uint8_t* const planes[3], const uint32_t strides[3],
                                  uint32_t width, uint32_t height, uint32_t bit_depth,
                                  SvtAv2ChromaFormat chroma_format) {
    if (out == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    memset(out, 0, sizeof(*out));

    if (planes == NULL || strides == NULL || width == 0 || height == 0) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    /* Same geometry rule as svt_av2_picture_alloc: non-zero multiples of 8. */
    if ((width & 7u) != 0 || (height & 7u) != 0) {
        return SVT_AV2_ERROR_UNSUPPORTED;
    }

    if (!format_supported(bit_depth, chroma_format)) return SVT_AV2_ERROR_UNSUPPORTED;
    out->bit_depth = bit_depth;
    out->chroma_format = chroma_format;
    out->visible_width = width;
    out->visible_height = height;

    for (uint32_t plane = 0; plane < SVT_AV2_PICTURE_MAX_PLANES; ++plane) {
        /* Reuse the plane geometry rule without needing an allocated buffer:
         * plane_width only reads the luma width. */
        const uint32_t plane_width = plane_width_from_luma(width, plane, chroma_format) * (bit_depth > 8 ? 2u : 1u);
        if (planes[plane] == NULL) {
            memset(out, 0, sizeof(*out));
            return SVT_AV2_ERROR_INVALID_PARAMETER;
        }
        if (strides[plane] < plane_width) {
            memset(out, 0, sizeof(*out));
            return SVT_AV2_ERROR_INVALID_PARAMETER;
        }
        out->planes[plane] = planes[plane];
        out->strides[plane] = strides[plane];
    }
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_picture_copy_from_view(SvtAv2FrameBuffer* destination,
                                           const SvtAv2BorrowedView* source) {
    if (!picture_is_allocated(destination) || source == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (source->visible_width != destination->visible_width ||
        source->visible_height != destination->visible_height ||
        source->bit_depth != destination->bit_depth || source->chroma_format != destination->chroma_format) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    /* Same pre-write validation as svt_av2_picture_copy_in (contract 1.4). */
    for (uint32_t plane = 0; plane < SVT_AV2_PICTURE_MAX_PLANES; ++plane) {
        const uint32_t visible_width = plane_width_from_luma(source->visible_width, plane, source->chroma_format) * (source->bit_depth > 8 ? 2u : 1u);
        if (source->planes[plane] == NULL) {
            return SVT_AV2_ERROR_INVALID_PARAMETER;
        }
        if (source->strides[plane] < visible_width) {
            return SVT_AV2_ERROR_INVALID_PARAMETER;
        }
    }

    if (!samples_valid(destination, (const uint8_t* const*)source->planes, source->strides))
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    copy_visible_planes(destination->planes,
                        destination->strides,
                        destination,
                        (const uint8_t* const*)source->planes,
                        source->strides);
    /* No view pointer is retained: the caller owns the borrowed planes and may
     * release them as soon as this call returns. */
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_picture_raw_size(const SvtAv2FrameBuffer* picture, size_t* out_size) {
    if (picture == NULL || out_size == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (picture->visible_width == 0 || picture->visible_height == 0) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    const size_t luma = (size_t)picture->visible_width * (size_t)picture->visible_height;
    const size_t chroma = (size_t)svt_av2_picture_plane_width(picture, 1) * svt_av2_picture_plane_height(picture, 1);
    /* Both factors are bounded by the validated visible size, so luma and
     * chroma cannot overflow for any picture this module can allocate. */
    *out_size = (luma + 2u * chroma) * (picture->bit_depth > 8 ? 2u : 1u);
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_picture_export_raw(const SvtAv2FrameBuffer* picture,
                                       uint8_t* out,
                                       size_t out_capacity,
                                       size_t* out_size) {
    if (!picture_is_allocated(picture) || out == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    size_t required = 0;
    const SvtAv2Error size_error = svt_av2_picture_raw_size(picture, &required);
    if (size_error != SVT_AV2_ERROR_NONE) {
        return size_error;
    }
    if (out_capacity < required) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    size_t offset = 0;
    for (uint32_t plane = 0; plane < SVT_AV2_PICTURE_MAX_PLANES; ++plane) {
        const uint32_t visible_width = svt_av2_picture_plane_width(picture, plane) * (picture->bit_depth > 8 ? 2u : 1u);
        const uint32_t visible_height = svt_av2_picture_plane_height(picture, plane);
        const uint8_t* src = picture->planes[plane];
        const uint32_t stride = picture->strides[plane];

        for (uint32_t row = 0; row < visible_height; ++row) {
            memcpy(out + offset, src + (size_t)row * stride, visible_width);
            offset += visible_width;
        }
    }

    if (out_size != NULL) {
        *out_size = offset;
    }
    return SVT_AV2_ERROR_NONE;
}
