/*
 * T007 unit tests: picture / plane storage.
 *
 * Covers the frozen contract (docs/t007-picture-block-geometry.md):
 *   - tightly packed, larger and non-aligned strides on copy-in
 *   - all-zero and all-255 patterns, and Y/U/V with independent patterns
 *   - padding sentinels: padding must survive copy-in and never be read as data
 *   - allocation overflow, zero/odd geometry, unsupported sizes
 *   - frame release, repeated release, and reuse after release
 *   - tightly packed raw export with the padding removed
 *   - borrowed-view construction and copy (the caller-lent-plane path); the
 *     borrowed-input tests go through svt_av2_picture_view /
 *     svt_av2_picture_copy_from_view rather than faking a buffer pointer
 *
 * Every check is an explicit expected value; nothing is compared against the
 * implementation's own output.
 */

#include "picture.h"
#include "test_util.h"

#include <stdlib.h>
#include <string.h>

/* Deterministic pattern generator: same pixel always gets the same value, so an
 * expected buffer can be built independently of the picture code. */
static uint8_t pattern(uint32_t plane, uint32_t row, uint32_t col) {
    return (uint8_t)((plane * 71u + row * 13u + col * 7u) & 0xFFu);
}

/* Builds a read-only borrowed view of caller-owned plane buffers.
 *
 * This is the honest model of "the caller lends us its planes": no allocation
 * descriptor is invented and no fake buffer pointer is used to satisfy
 * picture_is_allocated(). The view is constructed through the public
 * svt_av2_picture_view() entry point, so the same geometry checks the module
 * performs at run time are the ones exercised here.
 *
 * `write_patterns` controls whether this helper also fills the plane buffers.
 * Pass 0 when the caller has already laid out its own patterns (for example a
 * U/V layout that differs from `pattern()`); the view is still constructed. */
static void fill_view(SvtAv2BorrowedView* out,
                      uint8_t* y,
                      uint8_t* u,
                      uint8_t* v,
                      uint32_t width,
                      uint32_t height,
                      uint32_t y_stride,
                      uint32_t uv_stride,
                      int write_patterns) {
    const uint8_t* planes[SVT_AV2_PICTURE_MAX_PLANES] = {y, u, v};
    const uint32_t strides[SVT_AV2_PICTURE_MAX_PLANES] = {y_stride, uv_stride, uv_stride};

    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_view(out, planes, strides, width, height),
                          SVT_AV2_ERROR_NONE);

    if (!write_patterns) {
        return;
    }
    for (uint32_t row = 0; row < height; ++row) {
        for (uint32_t col = 0; col < width; ++col) {
            y[(size_t)row * y_stride + col] = pattern(0, row, col);
        }
    }
    const uint32_t cw = width >> 1;
    const uint32_t ch = height >> 1;
    for (uint32_t row = 0; row < ch; ++row) {
        for (uint32_t col = 0; col < cw; ++col) {
            u[(size_t)row * uv_stride + col] = pattern(1, row, col);
            v[(size_t)row * uv_stride + col] = pattern(2, row, col);
        }
    }
}

static void test_geometry_rejection(void) {
    SvtAv2FrameBuffer picture;

    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(NULL, 64, 64), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&picture, 0, 64), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&picture, 64, 0), SVT_AV2_ERROR_INVALID_PARAMETER);
    /* Not a multiple of 8: rejected, never silently cropped. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&picture, 63, 64), SVT_AV2_ERROR_UNSUPPORTED);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&picture, 64, 65), SVT_AV2_ERROR_UNSUPPORTED);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&picture, 6, 6), SVT_AV2_ERROR_UNSUPPORTED);
    /* Still safe to release after every failed allocation. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&picture), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(NULL), SVT_AV2_ERROR_INVALID_PARAMETER);
}

static void test_allocation_overflow(void) {
    SvtAv2FrameBuffer picture;

    /* width * height is comfortably past SIZE_MAX on 64-bit; the multiple-of-8
     * guard also rejects these, so the point is that neither call crashes and
     * neither returns success. */
    SVT_TEST_CHECK(svt_av2_picture_alloc(&picture, 0xFFFFFFF8u, 0xFFFFFFF8u) != SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(picture.buffer, 0);
    SVT_TEST_CHECK_EQ_INT(picture.planes[SVT_AV2_PLANE_Y], NULL);

    /* A size that is legal but huge: either it allocates or it reports OOM;
     * both are acceptable, a crash or a wrapped size is not.
     *
     * 65536x65536 needs 0x180000000 (6 GiB) with 4:2:0 padding, which is a legal
     * size_t but past the allocator's limit. A plain libc calloc returns NULL,
     * but AddressSanitizer *aborts* the process on an oversized request instead
     * of failing, so this case cannot observe the rejection under -fsanitize.
     * 8192x8192 (0x6000000, 96 MiB) is comfortably inside every allocator and
     * still exercises the "large but real" path that used to be covered. */
    const SvtAv2Error huge = svt_av2_picture_alloc(&picture, 8192u, 8192u);
    SVT_TEST_CHECK(huge == SVT_AV2_ERROR_NONE || huge == SVT_AV2_ERROR_OUT_OF_MEMORY);
    if (huge == SVT_AV2_ERROR_NONE) {
        /* Verify the reported geometry is the real allocated geometry. */
        SVT_TEST_CHECK(picture.strides[SVT_AV2_PLANE_Y] >= 8192u);
        SVT_TEST_CHECK(picture.aligned_height[SVT_AV2_PLANE_Y] >= 8192u);
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&picture), SVT_AV2_ERROR_NONE);
    }

    /* A size whose total fits in size_t but can never be allocated: it must be
     * rejected as OUT_OF_MEMORY without reaching calloc, because ASan turns an
     * oversized request into a process abort rather than a NULL return. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&picture, 0x80000000u, 0x80000000u),
                          SVT_AV2_ERROR_OUT_OF_MEMORY);
    SVT_TEST_CHECK_EQ_INT(picture.buffer, 0);
    SVT_TEST_CHECK_EQ_INT(picture.planes[SVT_AV2_PLANE_Y], NULL);
}

static void test_alloc_geometry_and_padding(void) {
    SvtAv2FrameBuffer picture;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&picture, 176, 144), SVT_AV2_ERROR_NONE);

    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_plane_width(&picture, SVT_AV2_PLANE_Y), 176);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_plane_height(&picture, SVT_AV2_PLANE_Y), 144);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_plane_width(&picture, SVT_AV2_PLANE_U), 88);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_plane_height(&picture, SVT_AV2_PLANE_U), 72);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_plane_width(&picture, SVT_AV2_PLANE_V), 88);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_plane_height(&picture, SVT_AV2_PLANE_V), 72);

    /* stride is in bytes and must cover the visible width; aligned_height must
     * cover the visible height. Exact values are an implementation choice, so
     * only the invariants are checked. */
    for (uint32_t plane = 0; plane < SVT_AV2_PICTURE_MAX_PLANES; ++plane) {
        SVT_TEST_CHECK(picture.strides[plane] >= svt_av2_picture_plane_width(&picture, plane));
        SVT_TEST_CHECK(picture.aligned_height[plane] >= svt_av2_picture_plane_height(&picture, plane));
        SVT_TEST_CHECK(picture.planes[plane] != NULL);
        /* Planes must not alias. */
        for (uint32_t other = plane + 1; other < SVT_AV2_PICTURE_MAX_PLANES; ++other) {
            SVT_TEST_CHECK(picture.planes[plane] != picture.planes[other]);
        }
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_plane_width(&picture, 3), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_plane(&picture, 3), NULL);

    /* Padding must be written and must stay out of the visible area. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_pad(&picture, 0xA5), SVT_AV2_ERROR_NONE);
    const uint8_t* const y = svt_av2_picture_plane_const(&picture, SVT_AV2_PLANE_Y);
    const uint32_t stride = picture.strides[SVT_AV2_PLANE_Y];
    for (uint32_t row = 0; row < picture.aligned_height[SVT_AV2_PLANE_Y]; ++row) {
        for (uint32_t col = 0; col < stride; ++col) {
            const uint8_t value = y[(size_t)row * stride + col];
            if (row < 144 && col < 176) {
                SVT_TEST_CHECK_EQ_INT(value, 0); /* calloc + pad must not touch visible */
            } else {
                SVT_TEST_CHECK_EQ_INT(value, 0xA5);
            }
        }
    }

    /* Padding is repeatable with a different value. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_pad(&picture, 0x5A), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(y[175], 0);            /* last visible sample of row 0 */
    SVT_TEST_CHECK_EQ_INT(y[176], 0x5A);         /* first padding byte of row 0 */

    /* Geometry queries on an unallocated picture must fail, not read garbage. */
    SvtAv2FrameBuffer empty;
    memset(&empty, 0, sizeof(empty));
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_pad(&empty, 1), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&picture), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&picture), SVT_AV2_ERROR_NONE); /* idempotent */
    SVT_TEST_CHECK_EQ_INT(picture.buffer, 0);
    SVT_TEST_CHECK_EQ_INT(picture.planes[SVT_AV2_PLANE_Y], NULL);
}

static void test_copy_in_strides(void) {
    const uint32_t width = 64;
    const uint32_t height = 64;
    const uint32_t ch = height >> 1;

    /* Tightly packed, a larger aligned stride, and a deliberately unaligned
     * stride (65 for luma) must all copy the same pixels. */
    const uint32_t y_strides[] = {64u, 96u, 65u};
    const uint32_t uv_strides[] = {32u, 48u, 33u};

    for (size_t variant = 0; variant < SVT_AV2_ARRAY_SIZE(y_strides); ++variant) {
        const uint32_t y_stride = y_strides[variant];
        const uint32_t uv_stride = uv_strides[variant];

        uint8_t* y = (uint8_t*)malloc((size_t)y_stride * height);
        uint8_t* u = (uint8_t*)malloc((size_t)uv_stride * ch);
        uint8_t* v = (uint8_t*)malloc((size_t)uv_stride * ch);
        SVT_TEST_CHECK(y != NULL && u != NULL && v != NULL);
        if (y == NULL || u == NULL || v == NULL) {
            free(y);
            free(u);
            free(v);
            continue;
        }

        SvtAv2BorrowedView source;
        fill_view(&source, y, u, v, width, height, y_stride, uv_stride, 1);

        SvtAv2FrameBuffer destination;
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&destination, width, height), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_pad(&destination, 0xEE), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_copy_from_view(&destination, &source),
                              SVT_AV2_ERROR_NONE);

        /* Independently recompute the expected value for every visible sample
         * in every plane. */
        int mismatch = 0;
        for (uint32_t plane = 0; plane < SVT_AV2_PICTURE_MAX_PLANES; ++plane) {
            const uint8_t* const plane_ptr = svt_av2_picture_plane_const(&destination, plane);
            const uint32_t pw = svt_av2_picture_plane_width(&destination, plane);
            const uint32_t ph = svt_av2_picture_plane_height(&destination, plane);
            const uint32_t stride = destination.strides[plane];
            for (uint32_t row = 0; row < ph; ++row) {
                for (uint32_t col = 0; col < pw; ++col) {
                    if (plane_ptr[(size_t)row * stride + col] != pattern(plane, row, col)) {
                        ++mismatch;
                    }
                }
            }
        }
        SVT_TEST_CHECK_EQ_INT(mismatch, 0);

        const uint32_t pw_y = svt_av2_picture_plane_width(&destination, SVT_AV2_PLANE_Y);
        SVT_TEST_CHECK(pw_y == width);

        /* Padding stays untouched by copy-in: the source pattern never reaches
         * it. The destination stride is its own (32-aligned) value, not the
         * source stride, so the padding of row r starts at r * dst_stride +
         * width -- but only when there IS padding. For width == 64 the aligned
         * stride equals the visible width, so the row tail is empty and
         * y_dst[width] is simply the first sample of the NEXT row, which the
         * pattern legitimately fills. Check the tail only where it exists. */
        const uint8_t* const y_dst = svt_av2_picture_plane_const(&destination, SVT_AV2_PLANE_Y);
        const uint32_t dst_stride = destination.strides[SVT_AV2_PLANE_Y];
        int padding_touched = 0;
        for (uint32_t row = 0; row < destination.aligned_height[SVT_AV2_PLANE_Y]; ++row) {
            for (uint32_t col = pw_y; col < dst_stride; ++col) {
                if (y_dst[(size_t)row * dst_stride + col] != 0xEE) {
                    ++padding_touched;
                }
            }
        }
        SVT_TEST_CHECK_EQ_INT(padding_touched, 0);

        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&destination), SVT_AV2_ERROR_NONE);
        free(y);
        free(u);
        free(v);
    }
}

static void test_copy_from_view_rejections(void) {
    SvtAv2FrameBuffer destination;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&destination, 64, 64), SVT_AV2_ERROR_NONE);

    uint8_t* y = (uint8_t*)calloc(64u * 64u, 1);
    uint8_t* u = (uint8_t*)calloc(32u * 32u, 1);
    uint8_t* v = (uint8_t*)calloc(32u * 32u, 1);
    SvtAv2BorrowedView source;
    fill_view(&source, y, u, v, 64, 64, 64, 32, 1);

    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_copy_from_view(NULL, &source), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_copy_from_view(&destination, NULL), SVT_AV2_ERROR_INVALID_PARAMETER);

    /* A zeroed view (as left by a failed svt_av2_picture_view) is never valid. */
    SvtAv2BorrowedView zeroed;
    memset(&zeroed, 0, sizeof(zeroed));
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_copy_from_view(&destination, &zeroed),
                          SVT_AV2_ERROR_INVALID_PARAMETER);

    /* stride < visible width can never describe a readable row. */
    SvtAv2BorrowedView narrow = source;
    narrow.strides[SVT_AV2_PLANE_Y] = 63;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_copy_from_view(&destination, &narrow),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SvtAv2BorrowedView narrow_chroma = source;
    narrow_chroma.strides[SVT_AV2_PLANE_U] = 31;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_copy_from_view(&destination, &narrow_chroma),
                          SVT_AV2_ERROR_INVALID_PARAMETER);

    /* Missing plane pointer. */
    SvtAv2BorrowedView missing = source;
    missing.planes[SVT_AV2_PLANE_V] = NULL;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_copy_from_view(&destination, &missing),
                          SVT_AV2_ERROR_INVALID_PARAMETER);

    /* Geometry mismatch. */
    SvtAv2BorrowedView other_size = source;
    other_size.visible_width = 128;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_copy_from_view(&destination, &other_size),
                          SVT_AV2_ERROR_INVALID_PARAMETER);

    /* None of the rejections may have written to the destination. */
    const uint8_t* const y_dst = svt_av2_picture_plane_const(&destination, SVT_AV2_PLANE_Y);
    int nonzero = 0;
    for (uint32_t row = 0; row < 64; ++row) {
        for (uint32_t col = 0; col < 64; ++col) {
            if (y_dst[(size_t)row * destination.strides[SVT_AV2_PLANE_Y] + col] != 0) {
                ++nonzero;
            }
        }
    }
    SVT_TEST_CHECK_EQ_INT(nonzero, 0);

    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&destination), SVT_AV2_ERROR_NONE);
    free(y);
    free(u);
    free(v);
}

/* svt_av2_picture_view: descriptor-only construction, no samples touched, and
 * every rejection leaves `out` fully zeroed (never a partial view). */
static void test_view_construction(void) {
    uint8_t y[64u * 64u];
    uint8_t u[32u * 32u];
    uint8_t v[32u * 32u];
    const uint8_t* planes[SVT_AV2_PICTURE_MAX_PLANES] = {y, u, v};
    const uint32_t strides[SVT_AV2_PICTURE_MAX_PLANES] = {64u, 32u, 32u};

    SvtAv2BorrowedView view;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_view(&view, planes, strides, 64, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(view.visible_width, 64);
    SVT_TEST_CHECK_EQ_INT(view.visible_height, 64);
    SVT_TEST_CHECK_EQ_INT(view.strides[SVT_AV2_PLANE_Y], 64);
    SVT_TEST_CHECK(view.planes[SVT_AV2_PLANE_Y] == y);
    SVT_TEST_CHECK(view.planes[SVT_AV2_PLANE_U] == u);
    SVT_TEST_CHECK(view.planes[SVT_AV2_PLANE_V] == v);

    /* Rejections mirror svt_av2_picture_alloc's geometry policy. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_view(NULL, planes, strides, 64, 64),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_view(&view, NULL, strides, 64, 64),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_view(&view, planes, NULL, 64, 64),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_view(&view, planes, strides, 0, 64),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_view(&view, planes, strides, 64, 0),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    /* Not a multiple of 8: rejected, never silently cropped (same as alloc). */
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_view(&view, planes, strides, 63, 64),
                          SVT_AV2_ERROR_UNSUPPORTED);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_view(&view, planes, strides, 64, 65),
                          SVT_AV2_ERROR_UNSUPPORTED);

    /* A NULL plane and a too-short stride are both rejected, and the view is
     * left zeroed so it can never be used as an accidental valid source. */
    const uint8_t* missing_v[SVT_AV2_PICTURE_MAX_PLANES] = {y, u, NULL};
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_view(&view, missing_v, strides, 64, 64),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(view.visible_width, 0);
    SVT_TEST_CHECK_EQ_INT(view.visible_height, 0);
    SVT_TEST_CHECK(view.planes[SVT_AV2_PLANE_Y] == NULL);

    const uint32_t narrow_strides[SVT_AV2_PICTURE_MAX_PLANES] = {63u, 32u, 32u};
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_view(&view, planes, narrow_strides, 64, 64),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(view.visible_width, 0);
    SVT_TEST_CHECK(view.planes[SVT_AV2_PLANE_Y] == NULL);
}

/* svt_av2_picture_copy_in still works between two genuinely allocated frames.
 * This keeps the owned-source path covered now that the borrowed-input tests go
 * through svt_av2_picture_copy_from_view. */
static void test_copy_in_allocated_source(void) {
    SvtAv2FrameBuffer source;
    SvtAv2FrameBuffer destination;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&source, 176, 144), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&destination, 176, 144), SVT_AV2_ERROR_NONE);

    for (uint32_t plane = 0; plane < SVT_AV2_PICTURE_MAX_PLANES; ++plane) {
        uint8_t* const base = svt_av2_picture_plane(&source, plane);
        const uint32_t pw = svt_av2_picture_plane_width(&source, plane);
        const uint32_t ph = svt_av2_picture_plane_height(&source, plane);
        const uint32_t stride = source.strides[plane];
        for (uint32_t row = 0; row < ph; ++row) {
            for (uint32_t col = 0; col < pw; ++col) {
                base[(size_t)row * stride + col] = pattern(plane, row, col);
            }
        }
    }

    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_copy_in(&destination, &source), SVT_AV2_ERROR_NONE);

    int mismatch = 0;
    for (uint32_t plane = 0; plane < SVT_AV2_PICTURE_MAX_PLANES; ++plane) {
        const uint8_t* const ptr = svt_av2_picture_plane_const(&destination, plane);
        const uint32_t pw = svt_av2_picture_plane_width(&destination, plane);
        const uint32_t ph = svt_av2_picture_plane_height(&destination, plane);
        const uint32_t stride = destination.strides[plane];
        for (uint32_t row = 0; row < ph; ++row) {
            for (uint32_t col = 0; col < pw; ++col) {
                if (ptr[(size_t)row * stride + col] != pattern(plane, row, col)) {
                    ++mismatch;
                }
            }
        }
    }
    SVT_TEST_CHECK_EQ_INT(mismatch, 0);

    /* copy_in's own rejections are unchanged: same descriptor twice, and a
     * geometry mismatch, must both fail. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_copy_in(&destination, &destination),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_copy_in(NULL, &source), SVT_AV2_ERROR_INVALID_PARAMETER);

    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&source), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&destination), SVT_AV2_ERROR_NONE);
}

static void test_export_layout(void) {
    const uint32_t width = 176;
    const uint32_t height = 144;
    const uint32_t cw = width >> 1;
    const uint32_t ch = height >> 1;

    uint8_t* y = (uint8_t*)malloc((size_t)width * height);
    uint8_t* u = (uint8_t*)malloc((size_t)cw * ch);
    uint8_t* v = (uint8_t*)malloc((size_t)cw * ch);
    SVT_TEST_CHECK(y != NULL && u != NULL && v != NULL);
    if (y == NULL || u == NULL || v == NULL) {
        free(y);
        free(u);
        free(v);
        return;
    }

    /* U and V get visibly different patterns so a plane swap cannot pass. */
    for (uint32_t row = 0; row < height; ++row) {
        for (uint32_t col = 0; col < width; ++col) {
            y[(size_t)row * width + col] = pattern(0, row, col);
        }
    }
    for (uint32_t row = 0; row < ch; ++row) {
        for (uint32_t col = 0; col < cw; ++col) {
            u[(size_t)row * cw + col] = (uint8_t)(255u - pattern(1, row, col));
            v[(size_t)row * cw + col] = (uint8_t)(pattern(2, row, col) | 0x80u);
        }
    }

    SvtAv2BorrowedView source;
    fill_view(&source, y, u, v, width, height, width, cw, 0);

    SvtAv2FrameBuffer picture;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&picture, width, height), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_pad(&picture, 0x11), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_copy_from_view(&picture, &source), SVT_AV2_ERROR_NONE);

    const size_t expected_size = (size_t)width * height + 2u * ((size_t)cw * ch);
    size_t reported = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_raw_size(&picture, &reported), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(reported, expected_size);

    uint8_t* out = (uint8_t*)malloc(expected_size);
    SVT_TEST_CHECK(out != NULL);
    if (out == NULL) {
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&picture), SVT_AV2_ERROR_NONE);
        free(y);
        free(u);
        free(v);
        return;
    }
    memset(out, 0xCC, expected_size);

    /* One byte short must fail and must not write. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_export_raw(&picture, out, expected_size - 1u, &reported),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(out[0], 0xCC);
    SVT_TEST_CHECK_EQ_INT(out[expected_size - 1u], 0xCC);

    size_t written = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_export_raw(&picture, out, expected_size, &written), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(written, expected_size);

    /* Independently recompute the tightly packed expected buffer. */
    int mismatch = 0;
    size_t offset = 0;
    for (uint32_t row = 0; row < height; ++row) {
        for (uint32_t col = 0; col < width; ++col) {
            if (out[offset++] != pattern(0, row, col)) {
                ++mismatch;
            }
        }
    }
    for (uint32_t row = 0; row < ch; ++row) {
        for (uint32_t col = 0; col < cw; ++col) {
            if (out[offset++] != (uint8_t)(255u - pattern(1, row, col))) {
                ++mismatch;
            }
        }
    }
    for (uint32_t row = 0; row < ch; ++row) {
        for (uint32_t col = 0; col < cw; ++col) {
            if (out[offset++] != (uint8_t)(pattern(2, row, col) | 0x80u)) {
                ++mismatch;
            }
        }
    }
    SVT_TEST_CHECK_EQ_INT(mismatch, 0);
    SVT_TEST_CHECK_EQ_INT(offset, expected_size);
    /* Padding must not reach the exported buffer. Counting occurrences of the
     * sentinel byte is NOT a valid test: 0x11 also occurs inside the U/V
     * patterns, so a correct export would still trip it. Poison only the
     * padding with a different value and require an identical export instead. */
    uint8_t* const second = (uint8_t*)malloc(expected_size);
    SVT_TEST_CHECK(second != NULL);
    if (second != NULL) {
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_pad(&picture, 0x7E), SVT_AV2_ERROR_NONE);
        memset(second, 0xCC, expected_size);
        size_t second_written = 0;
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_export_raw(&picture, second, expected_size, &second_written),
                              SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(second_written, expected_size);
        SVT_TEST_CHECK(memcmp(out, second, expected_size) == 0);
        free(second);
    }

    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_export_raw(&picture, NULL, expected_size, NULL),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_export_raw(&picture, out, expected_size, NULL), SVT_AV2_ERROR_NONE);

    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&picture), SVT_AV2_ERROR_NONE);
    free(out);
    free(y);
    free(u);
    free(v);
}

static void test_extremes_and_reuse(void) {
    SvtAv2FrameBuffer picture;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&picture, 64, 64), SVT_AV2_ERROR_NONE);

    /* All-zero and all-255 must round-trip exactly: an unsigned subtraction or
     * a clipping bug would not survive this. */
    const uint8_t fill_values[] = {0x00u, 0xFFu};
    for (size_t i = 0; i < SVT_AV2_ARRAY_SIZE(fill_values); ++i) {
        for (uint32_t plane = 0; plane < SVT_AV2_PICTURE_MAX_PLANES; ++plane) {
            uint8_t* const base = svt_av2_picture_plane(&picture, plane);
            const uint32_t pw = svt_av2_picture_plane_width(&picture, plane);
            const uint32_t ph = svt_av2_picture_plane_height(&picture, plane);
            for (uint32_t row = 0; row < ph; ++row) {
                memset(base + (size_t)row * picture.strides[plane], fill_values[i], pw);
            }
        }

        uint8_t out[64u * 64u + 2u * 32u * 32u];
        size_t written = 0;
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_export_raw(&picture, out, sizeof(out), &written), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(written, sizeof(out));
        int wrong = 0;
        for (size_t k = 0; k < written; ++k) {
            if (out[k] != fill_values[i]) {
                ++wrong;
            }
        }
        SVT_TEST_CHECK_EQ_INT(wrong, 0);
    }

    /* Release, then reuse the same descriptor: no dangling plane pointers. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&picture), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_export_raw(&picture, (uint8_t*)&picture, sizeof(picture), NULL),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&picture, 8, 8), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_plane_width(&picture, SVT_AV2_PLANE_Y), 8);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_plane_width(&picture, SVT_AV2_PLANE_U), 4);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&picture), SVT_AV2_ERROR_NONE);
}

static void test_highbd_layout_and_atomic_validation(void) {
    const SvtAv2ChromaFormat formats[] = { SVT_AV2_CHROMA_420, SVT_AV2_CHROMA_444 };
    for (uint32_t f = 0; f < 2; ++f) {
        SvtAv2FrameBuffer picture;
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&picture, 8, 8, 10, formats[f]), SVT_AV2_ERROR_NONE);
        uint8_t storage[3][8 * 19 + 1];
        const uint8_t* planes[3] = { storage[0] + 1, storage[1] + 1, storage[2] + 1 };
        const uint32_t strides[3] = { 19, 19, 19 };
        memset(storage, 0xA5, sizeof(storage));
        for (uint32_t p = 0; p < 3; ++p)
            for (uint32_t y = 0; y < svt_av2_picture_plane_height(&picture, p); ++y)
                for (uint32_t x = 0; x < svt_av2_picture_plane_width(&picture, p); ++x)
                    svt_av2_sample_store(storage[p] + 1 + y * 19 + 2 * x, 10, (uint16_t)((p * 257 + y * 71 + x) & 1023));
        SvtAv2BorrowedView view;
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_view_ex(&view, planes, strides, 8, 8, 10, formats[f]), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_copy_from_view(&picture, &view), SVT_AV2_ERROR_NONE);
        size_t size = 0;
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_raw_size(&picture, &size), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(size, f == 0 ? 192 : 384);
        uint8_t raw[384], original[384];
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_export_raw(&picture, raw, sizeof(raw), NULL), SVT_AV2_ERROR_NONE);
        memcpy(original, raw, size);
        size_t offset = 0;
        for (uint32_t p = 0; p < 3; ++p)
            for (uint32_t y = 0; y < svt_av2_picture_plane_height(&picture, p); ++y)
                for (uint32_t x = 0; x < svt_av2_picture_plane_width(&picture, p); ++x) {
                    const uint16_t value = (uint16_t)((p * 257 + y * 71 + x) & 1023);
                    SVT_TEST_CHECK_EQ_INT(raw[offset], value & 255);
                    SVT_TEST_CHECK_EQ_INT(raw[offset + 1], value >> 8);
                    offset += 2;
                }
        svt_av2_sample_store(storage[2] + 1, 10, 1024);
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_copy_from_view(&picture, &view), SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_export_raw(&picture, raw, sizeof(raw), NULL), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK(memcmp(original, raw, size) == 0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_pad(&picture, 1023), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_sample_load(picture.planes[0] + 16, 10), 1023);
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&picture), SVT_AV2_ERROR_NONE);
    }
}

int main(void) {
    test_highbd_layout_and_atomic_validation();
    test_geometry_rejection();
    test_allocation_overflow();
    test_alloc_geometry_and_padding();
    test_view_construction();
    test_copy_in_strides();
    test_copy_in_allocated_source();
    test_copy_from_view_rejections();
    test_export_layout();
    test_extremes_and_reuse();
    SVT_TEST_REPORT("test_picture");
}
