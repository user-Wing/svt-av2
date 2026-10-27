/*
 * T008 unit tests: minimal scalar intra prediction (DC_PRED).
 *
 * The expected values are produced by an INDEPENDENT model written directly from
 * AV2 v1.0.0 §7.13.2.10 and §4.8 in this file (see model_* below). It shares no
 * code with Source/Lib/Prediction: the div_lut-free power-of-two shortcut it uses
 * is justified separately in svt_av2_dc_model_divide() and was verified against
 * the resolve_divisor/approx_divide chain outside this test.
 *
 * Coverage required by the task book:
 *   - all-zero and all-255 reconstructions
 *   - constant, gradient and checkerboard content
 *   - random neighbours, >= 100 fixed-seed cases per implemented size
 *   - above-only, left-only and neither
 *   - chroma sizes
 *   - non-tight strides
 *   - partial right/bottom edge blocks
 *   - first block must not read uninitialised memory
 */

#include "block_geometry.h"
#include "intra_prediction.h"
#include "picture.h"
#include "test_util.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Independent model                                                          */
/* ------------------------------------------------------------------------- */

/* Round2(x, n) = floor((x + 2^(n-1)) / 2^n), AV2 §4.8 (pseudo-code form). */
static uint32_t model_round2(uint32_t x, uint32_t n) {
    if (n == 0) {
        return x;
    }
    return (x + (1u << (n - 1u))) >> n;
}

/* FloorLog2(x), AV2 §4.8. */
static uint32_t model_log2(uint32_t x) {
    uint32_t s = 0;
    while (x != 0) {
        x >>= 1;
        ++s;
    }
    return s - 1u;
}

/* The spec's div_lut and its constants (DIV_LUT_BITS = 7, DIV_LUT_PREC_BITS = 9),
 * taken from the normative definition of resolve_divisor and the decoder's table
 * (avm\av2\common\warped_motion.h:50-60 / enums.h:1157-1158).
 *
 * This table is a constant OF THE SPEC's approx_divide, not an implementation
 * detail, so the model must use the same values; not using it would make the model
 * assert a different function. It is written out here rather than shared with the
 * implementation so that a typo in either copy is caught by the comparison. */
#define MODEL_DIV_LUT_BITS 7
#define MODEL_DIV_LUT_PREC_BITS 9

static const int16_t model_div_lut[129] = {
    512, 508, 504, 500, 496, 493, 489, 485, 482, 478, 475, 471, 468, 465, 462,
    458, 455, 452, 449, 446, 443, 440, 437, 434, 431, 428, 426, 423, 420, 417,
    415, 412, 410, 407, 405, 402, 400, 397, 395, 392, 390, 388, 386, 383, 381,
    379, 377, 374, 372, 370, 368, 366, 364, 362, 360, 358, 356, 354, 352, 350,
    349, 347, 345, 343, 341, 340, 338, 336, 334, 333, 331, 329, 328, 326, 324,
    323, 321, 320, 318, 317, 315, 314, 312, 311, 309, 308, 306, 305, 303, 302,
    301, 299, 298, 297, 295, 294, 293, 291, 290, 289, 287, 286, 285, 284, 282,
    281, 280, 279, 278, 277, 275, 274, 273, 272, 271, 270, 269, 267, 266, 265,
    264, 263, 262, 261, 260, 259, 258, 257, 256
};

/* resolve_divisor(D) -> (scale, shift), the spec's own helper. */
static void model_resolve_divisor(uint32_t d, uint32_t* scale, uint32_t* shift) {
    *shift = model_log2(d);
    const uint32_t e = d - (1u << *shift);
    uint32_t f;
    if (*shift > MODEL_DIV_LUT_BITS) {
        f = model_round2(e, *shift - MODEL_DIV_LUT_BITS);
    } else {
        f = e << (MODEL_DIV_LUT_BITS - *shift);
    }
    *scale = (uint32_t)model_div_lut[f];
    *shift += MODEL_DIV_LUT_PREC_BITS;
}

/* approx_divide(sum, den) = Round2(sum * scale, shift) — the real thing.
 *
 * This is NOT an exact division. It is exact only when den is a power of two,
 * which happens to hold for every square block (den = 2w) but NOT for rectangular
 * ones (8x16 gives den = 24, 4x16 gives den = 20). Using a plain rounded division
 * here would make the model disagree with the spec on rectangular blocks; that is
 * exactly the bug this implementation caught while the tests were being written. */
static uint32_t model_approx_divide(uint32_t sum, uint32_t den) {
    uint32_t scale = 0;
    uint32_t shift = 0;
    model_resolve_divisor(den, &scale, &shift);
    return model_round2(sum * scale, shift);
}

/* The substituted AboveRow/LeftCol of one region, exactly as §7.13.2.1 builds
 * them. Reads only the visible area of `plane`. */
typedef struct ModelRef {
    uint32_t above[SVT_AV2_MAX_REF_SAMPLES];
    uint32_t left[SVT_AV2_MAX_REF_SAMPLES];
} ModelRef;

static void model_build_ref(const uint8_t* plane,
                            uint32_t stride,
                            uint32_t x,
                            uint32_t y,
                            uint32_t w,
                            uint32_t h,
                            int have_left,
                            int have_above,
                            ModelRef* out) {
    memset(out, 0, sizeof(*out));
    if (have_left && have_above) {
        for (uint32_t j = 0; j < w; ++j) {
            out->above[j] = plane[(size_t)(y - 1u) * stride + x + j];
        }
        for (uint32_t i = 0; i < h; ++i) {
            out->left[i] = plane[(size_t)(y + i) * stride + (x - 1u)];
        }
    } else if (have_left) {
        const uint32_t first = plane[(size_t)y * stride + (x - 1u)];
        for (uint32_t j = 0; j < w; ++j) {
            out->above[j] = first;
        }
        for (uint32_t i = 0; i < h; ++i) {
            out->left[i] = plane[(size_t)(y + i) * stride + (x - 1u)];
        }
    } else if (have_above) {
        const uint32_t first = plane[(size_t)(y - 1u) * stride + x];
        for (uint32_t j = 0; j < w; ++j) {
            out->above[j] = plane[(size_t)(y - 1u) * stride + x + j];
        }
        for (uint32_t i = 0; i < h; ++i) {
            out->left[i] = first;
        }
    } else {
        for (uint32_t j = 0; j < w; ++j) {
            out->above[j] = 128;
        }
        for (uint32_t i = 0; i < h; ++i) {
            out->left[i] = 128;
        }
    }
}

/* The DC value of §7.13.2.10, all four branches. */
static uint32_t model_dc(const ModelRef* ref,
                         uint32_t w,
                         uint32_t h,
                         int have_left,
                         int have_above) {
    if (have_left && have_above) {
        uint32_t sum = 0;
        for (uint32_t i = 0; i < h; ++i) {
            sum += ref->left[i];
        }
        for (uint32_t j = 0; j < w; ++j) {
            sum += ref->above[j];
        }
        const uint32_t avg = model_approx_divide(sum, w + h);
        return avg > 255u ? 255u : avg; /* Clip1 */
    }
    if (have_left) {
        uint32_t sum = 0;
        for (uint32_t i = 0; i < h; ++i) {
            sum += ref->left[i];
        }
        return model_round2(sum, model_log2(h));
    }
    if (have_above) {
        uint32_t sum = 0;
        for (uint32_t j = 0; j < w; ++j) {
            sum += ref->above[j];
        }
        return model_round2(sum, model_log2(w));
    }
    return 128u;
}

/* ------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* ------------------------------------------------------------------------- */

/* Deterministic pseudo-random generator so every case is reproducible: a fixed
 * seed always yields the same bytes, and the model expectation is derived from
 * the same bytes rather than from the implementation. */
static uint32_t g_rng_state = 0;

static void rng_seed(uint32_t seed) {
    g_rng_state = seed ? seed : 1u;
}

static uint32_t rng_next(void) {
    /* xorshift32 */
    uint32_t x = g_rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_rng_state = x;
    return x;
}

/* Fills the visible area of every plane with a caller-supplied function of
 * (plane, row, col), leaving padding untouched. */
typedef uint8_t (*PlaneFn)(uint32_t plane, uint32_t row, uint32_t col, void* user);

static void fill_plane(SvtAv2FrameBuffer* pic,
                       uint32_t plane,
                       PlaneFn fn,
                       void* user) {
    uint8_t* base = svt_av2_picture_plane(pic, plane);
    const uint32_t w = svt_av2_picture_plane_width(pic, plane);
    const uint32_t h = svt_av2_picture_plane_height(pic, plane);
    const uint32_t stride = pic->strides[plane];
    for (uint32_t row = 0; row < h; ++row) {
        for (uint32_t col = 0; col < w; ++col) {
            base[(size_t)row * stride + col] = fn(plane, row, col, user);
        }
    }
}

static uint8_t fn_const(uint32_t plane, uint32_t row, uint32_t col, void* user) {
    (void)plane;
    (void)row;
    (void)col;
    return *(const uint8_t*)user;
}

static uint8_t fn_gradient(uint32_t plane, uint32_t row, uint32_t col, void* user) {
    (void)user;
    return (uint8_t)((plane * 40u + row * 3u + col * 5u) & 0xFFu);
}

static uint8_t fn_checker(uint32_t plane, uint32_t row, uint32_t col, void* user) {
    (void)user;
    return (uint8_t)((((plane + row / 2u + col / 2u) & 1u) != 0u) ? 255u : 0u);
}

static uint32_t g_random_base = 0;

static uint8_t fn_random(uint32_t plane, uint32_t row, uint32_t col, void* user) {
    (void)user;
    /* Deterministic hash of the coordinate, independent of call order. */
    uint32_t h = (plane + 1u) * 0x9E3779B9u;
    h ^= (row + 1u) * 0x85EBCA6Bu;
    h ^= (col + 1u) * 0xC2B2AE35u;
    h ^= g_random_base;
    h ^= h >> 15;
    h *= 0x2545F491u;
    h ^= h >> 13;
    return (uint8_t)(h & 0xFFu);
}

/* Runs one predictor case: builds the reference both ways (implementation and
 * model) and compares the whole predicted block, plus every sample against the
 * model's DC value. */
static void check_predict_case(const SvtAv2FrameBuffer* pic,
                               uint32_t plane,
                               uint32_t x,
                               uint32_t y,
                               uint32_t w,
                               uint32_t h,
                               int have_left,
                               int have_above) {
    SvtAv2IntraBlock blk;
    memset(&blk, 0, sizeof(blk));
    blk.x = x;
    blk.y = y;
    blk.w = w;
    blk.h = h;
    blk.plane = plane;

    SvtAv2IntraReference ref;
    memset(&ref, 0, sizeof(ref));
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_reference_build_flags(pic, &blk, have_left, have_above, &ref),
                          SVT_AV2_ERROR_NONE);

    /* The reference array itself must match the model. */
    const uint8_t* const base = svt_av2_picture_plane_const(pic, plane);
    const uint32_t stride = pic->strides[plane];
    ModelRef mref;
    model_build_ref(base, stride, x, y, w, h, have_left, have_above, &mref);
    for (uint32_t j = 0; j < w; ++j) {
        SVT_TEST_CHECK_EQ_INT(ref.above[j], mref.above[j]);
    }
    for (uint32_t i = 0; i < h; ++i) {
        SVT_TEST_CHECK_EQ_INT(ref.left[i], mref.left[i]);
    }

    /* Predict into a sentinel-filled buffer with a deliberately non-tight stride
     * so an out-of-range write is visible. One extra guard row is allocated so
     * the check at index h * DST_STRIDE is inside the array. */
    enum { DST_STRIDE = SVT_AV2_MAX_TX_SIZE + 7 };
    uint8_t dst[(SVT_AV2_MAX_TX_SIZE + 1) * DST_STRIDE];
    memset(dst, 0xA5, sizeof(dst));

    SVT_TEST_CHECK_EQ_INT(
        svt_av2_intra_dc_predict_flags(&ref, &blk, have_left, have_above, dst, DST_STRIDE),
        SVT_AV2_ERROR_NONE);

    const uint32_t expected_dc = model_dc(&mref, w, h, have_left, have_above);
    for (uint32_t i = 0; i < h; ++i) {
        for (uint32_t j = 0; j < w; ++j) {
            SVT_TEST_CHECK_EQ_INT(dst[(size_t)i * DST_STRIDE + j], expected_dc);
        }
        /* Nothing beyond the row's w samples may be written. */
        SVT_TEST_CHECK_EQ_INT(dst[(size_t)i * DST_STRIDE + w], 0xA5);
    }
    SVT_TEST_CHECK_EQ_INT(dst[(size_t)h * DST_STRIDE], 0xA5);
}

/* ------------------------------------------------------------------------- */
/* Tests                                                                      */
/* ------------------------------------------------------------------------- */

/* A 64x64 frame: luma 64x64, chroma 32x32. */
static SvtAv2Error make_frame(SvtAv2FrameBuffer* pic) {
    return svt_av2_picture_alloc(pic, 64, 64);
}

static void test_first_block_no_uninitialised_read(void) {
    /* The very first block of the frame has neither neighbour. Its prediction must
     * be 128 and must not depend on any byte of the reconstruction, including the
     * padding. Fill the whole allocation with a poison pattern first: if the
     * predictor reads a neighbour it will show up here. */
    SvtAv2FrameBuffer pic;
    SVT_TEST_CHECK_EQ_INT(make_frame(&pic), SVT_AV2_ERROR_NONE);

    for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
        uint8_t* const base = svt_av2_picture_plane(&pic, p);
        /* Poison the entire allocation, visible area included, via the padded
         * region: pad() writes only padding, so poison manually. */
        const uint32_t ah = pic.aligned_height[p];
        const uint32_t st = pic.strides[p];
        memset(base, 0x5A, (size_t)ah * st);
    }

    for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
        check_predict_case(&pic, p, 0, 0, 8, 8, 0, 0);
        check_predict_case(&pic, p, 0, 0, 4, 4, 0, 0);
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&pic), SVT_AV2_ERROR_NONE);
}

static void test_uniform_content(void) {
    /* all-zero, all-255 and a mid constant, with all four availability cases. */
    static const uint8_t values[] = {0u, 128u, 255u};
    for (size_t v = 0; v < SVT_AV2_ARRAY_SIZE(values); ++v) {
        SvtAv2FrameBuffer pic;
        SVT_TEST_CHECK_EQ_INT(make_frame(&pic), SVT_AV2_ERROR_NONE);
        uint8_t value = values[v];
        for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
            fill_plane(&pic, p, fn_const, &value);
        }

        for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
            const uint32_t pw = svt_av2_picture_plane_width(&pic, p);
            const uint32_t ph = svt_av2_picture_plane_height(&pic, p);
            /* First block: neither neighbour -> 128 regardless of content. */
            check_predict_case(&pic, p, 0, 0, 8, 8, 0, 0);
            /* Interior: both. */
            check_predict_case(&pic, p, 8, 8, 8, 8, 1, 1);
            /* Above only (first column). */
            check_predict_case(&pic, p, 0, 8, 8, 8, 0, 1);
            /* Left only (first row). */
            check_predict_case(&pic, p, 8, 0, 8, 8, 1, 0);
            /* Bottom-right edge block. */
            check_predict_case(&pic, p, pw - 8u, ph - 8u, 8, 8, 1, 1);
        }
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&pic), SVT_AV2_ERROR_NONE);
    }
}

static void test_gradient_and_checker(void) {
    SvtAv2FrameBuffer pic;
    SVT_TEST_CHECK_EQ_INT(make_frame(&pic), SVT_AV2_ERROR_NONE);

    for (int which = 0; which < 2; ++which) {
        PlaneFn fn = (which == 0) ? fn_gradient : fn_checker;
        for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
            fill_plane(&pic, p, fn, NULL);
        }
        for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
            const uint32_t pw = svt_av2_picture_plane_width(&pic, p);
            const uint32_t ph = svt_av2_picture_plane_height(&pic, p);
            check_predict_case(&pic, p, 0, 0, 8, 8, 0, 0);
            check_predict_case(&pic, p, 8, 8, 16, 16, 1, 1);
            check_predict_case(&pic, p, 0, 16, 16, 8, 0, 1);
            check_predict_case(&pic, p, 16, 0, 8, 16, 1, 0);
            check_predict_case(&pic, p, pw - 8u, ph - 8u, 8, 8, 1, 1);
            /* 64x64 region on luma is the largest frozen transform. */
            if (p == SVT_AV2_PLANE_Y) {
                check_predict_case(&pic, p, 0, 0, 64, 64, 0, 0);
            }
        }
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&pic), SVT_AV2_ERROR_NONE);
}

static void test_random_neighbours_100_per_size(void) {
    /* >= 100 fixed-seed random cases per implemented size, with a different
     * reconstruction pattern per case. Sizes cover every square transform the
     * frozen configuration can use plus rectangular luma shapes. */
    static const struct {
        uint32_t w;
        uint32_t h;
    } sizes[] = {
        {4, 4}, {8, 8}, {16, 16}, {32, 32}, {64, 64},
        {8, 16}, {16, 8}, {4, 16}, {16, 4}, {32, 8}, {8, 32},
    };

    SvtAv2FrameBuffer pic;
    SVT_TEST_CHECK_EQ_INT(make_frame(&pic), SVT_AV2_ERROR_NONE);

    uint32_t seed = 0x12345678u;
    for (size_t s = 0; s < SVT_AV2_ARRAY_SIZE(sizes); ++s) {
        const uint32_t w = sizes[s].w;
        const uint32_t h = sizes[s].h;
        for (uint32_t iter = 0; iter < 100; ++iter) {
            rng_seed(seed + iter);
            g_random_base = rng_next();

            for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
                fill_plane(&pic, p, fn_random, NULL);
                const uint32_t pw = svt_av2_picture_plane_width(&pic, p);
                const uint32_t ph = svt_av2_picture_plane_height(&pic, p);
                if (w > pw || h > ph) {
                    continue; /* size does not fit this plane */
                }

                /* Pick a position, and derive the availability from it so the
                 * four branches are all exercised across the iterations. The
                 * position is random, so edge blocks occur too. */
                const uint32_t x = (pw > w) ? (rng_next() % (pw - w + 1u)) : 0u;
                const uint32_t y = (ph > h) ? (rng_next() % (ph - h + 1u)) : 0u;
                /* Derive the branch from the coordinate so the model and the
                 * implementation agree on the truth, and additionally test a
                 * deliberately unavailable neighbour in the interior. */
                int hl = (x > 0) ? 1 : 0;
                int ha = (y > 0) ? 1 : 0;
                check_predict_case(&pic, p, x, y, w, h, hl, ha);

                /* Interior position with both real neighbours, to guarantee the
                 * both-available branch is hit often. */
                if (pw > w && ph > h) {
                    const uint32_t ix = 1u + (rng_next() % (pw - w));
                    const uint32_t iy = 1u + (rng_next() % (ph - h));
                    check_predict_case(&pic, p, ix, iy, w, h, 1, 1);
                }
            }
        }
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&pic), SVT_AV2_ERROR_NONE);
}

static void test_non_tight_stride(void) {
    /* The predictor must honour the picture's own stride. Alloc builds a padded
     * allocation, so its stride is already >= the visible width; a larger stride
     * is exercised by T007's picture tests, and here we assert the property that
     * matters: every visible row is addressed through strides[plane], by filling
     * the padding with a poison value that must not appear in any prediction. */
    SvtAv2FrameBuffer pic;
    SVT_TEST_CHECK_EQ_INT(make_frame(&pic), SVT_AV2_ERROR_NONE);

    for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
        /* Poison the padding with a value that cannot be a valid prediction here:
         * content is a constant 0, so any 0xAB in the output means padding leaked
         * into the reference. */
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_pad(&pic, 0xABu), SVT_AV2_ERROR_NONE);
        uint8_t zero = 0;
        fill_plane(&pic, p, fn_const, &zero);
    }

    for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
        const uint32_t pw = svt_av2_picture_plane_width(&pic, p);
        const uint32_t ph = svt_av2_picture_plane_height(&pic, p);
        /* Every block of the plane; content is 0 so any non-zero prediction is a
         * leak. The first block predicts 128, which is legitimate. */
        for (uint32_t y = 0; y + 8u <= ph; y += 8u) {
            for (uint32_t x = 0; x + 8u <= pw; x += 8u) {
                check_predict_case(&pic, p, x, y, 8, 8, x > 0, y > 0);
            }
        }
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&pic), SVT_AV2_ERROR_NONE);
}

static void test_chroma_sizes(void) {
    /* Chroma plane of a 64x64 frame is 32x32. Everything above already loops over
     * all three planes; this case pins the chroma geometry explicitly and covers
     * the smallest chroma block (4x4) plus the full chroma plane. */
    SvtAv2FrameBuffer pic;
    SVT_TEST_CHECK_EQ_INT(make_frame(&pic), SVT_AV2_ERROR_NONE);
    for (uint32_t p = SVT_AV2_PLANE_U; p <= SVT_AV2_PLANE_V; ++p) {
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_plane_width(&pic, p), 32u);
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_plane_height(&pic, p), 32u);
        rng_seed(0xC0FFEEu + p);
        g_random_base = 0xABCDEF01u;
        fill_plane(&pic, p, fn_random, NULL);

        check_predict_case(&pic, p, 0, 0, 32, 32, 0, 0);
        check_predict_case(&pic, p, 0, 0, 4, 4, 0, 0);
        check_predict_case(&pic, p, 28, 28, 4, 4, 1, 1);
        check_predict_case(&pic, p, 0, 28, 4, 4, 0, 1);
        check_predict_case(&pic, p, 28, 0, 4, 4, 1, 0);
        check_predict_case(&pic, p, 16, 16, 16, 16, 1, 1);
        check_predict_case(&pic, p, 16, 0, 16, 16, 1, 0);
        check_predict_case(&pic, p, 0, 16, 16, 16, 0, 1);
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&pic), SVT_AV2_ERROR_NONE);
}

static void test_edge_blocks_partial(void) {
    /* A 176x144 frame has a right column and bottom row of partial superblocks;
     * T007's traversal splits them. Build a 64x64 frame (which has no partial
     * superblock) and additionally a 24x24 frame whose single superblock is
     * partial on both edges, then predict every block the traversal produces. */
    SvtAv2FrameBuffer pic;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&pic, 24, 24), SVT_AV2_ERROR_NONE);
    for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
        rng_seed(0xED6E0000u + p);
        g_random_base = 0x13572468u;
        fill_plane(&pic, p, fn_random, NULL);
    }

    SvtAv2FrameGeometry geo;
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&geo, 24, 24, SVT_AV2_SB_SIZE_64),
                          SVT_AV2_ERROR_NONE);

    /* Walk every syntax block the geometry produces and predict it. This is the
     * only supported way to enumerate coding blocks, and it is exactly what T012
     * will do, so predicting each of them also checks that no visited block falls
     * outside the visible area (validate_block would reject it). */
    struct WalkState {
        const SvtAv2FrameBuffer* pic;
        uint32_t count;
    } state;
    state.pic = &pic;
    state.count = 0;

    /* The visitor needs the plane loop, so it is written as a helper closure
     * pattern: one traversal, three planes inside. */
    for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
        for (uint32_t sb_row = 0; sb_row < geo.sb_rows; ++sb_row) {
            for (uint32_t sb_col = 0; sb_col < geo.sb_cols; ++sb_col) {
                SvtAv2BlockView luma;
                SVT_TEST_CHECK_EQ_INT(svt_av2_block_from_sb(&geo, sb_col, sb_row, &luma),
                                      SVT_AV2_ERROR_NONE);
                SvtAv2BlockView view = luma;
                if (p != SVT_AV2_PLANE_Y) {
                    SVT_TEST_CHECK_EQ_INT(svt_av2_chroma_view(&geo, &luma, &view),
                                          SVT_AV2_ERROR_NONE);
                }
                state.count++;
                check_predict_case(&pic, p, view.x, view.y, view.w, view.h,
                                   view.x > 0 ? 1 : 0, view.y > 0 ? 1 : 0);
            }
        }
    }
    SVT_TEST_CHECK(state.count > 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&pic), SVT_AV2_ERROR_NONE);
}

static void test_neighbors_from_t007_agree(void) {
    /* The availability flags the predictor uses to derive its substitution must
     * agree with T007's neighbour query, otherwise the two modules disagree about
     * where a frame boundary is. Compare them over every block of a frame whose
     * left/top edges are inside the frame. */
    SvtAv2FrameBuffer pic;
    SVT_TEST_CHECK_EQ_INT(make_frame(&pic), SVT_AV2_ERROR_NONE);
    for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
        rng_seed(0xBEEF0000u + p);
        g_random_base = 0x0F0F0F0Fu;
        fill_plane(&pic, p, fn_random, NULL);
    }

    SvtAv2FrameGeometry geo;
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&geo, 64, 64, SVT_AV2_SB_SIZE_64),
                          SVT_AV2_ERROR_NONE);

    for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
        for (uint32_t sb_row = 0; sb_row < geo.sb_rows; ++sb_row) {
            for (uint32_t sb_col = 0; sb_col < geo.sb_cols; ++sb_col) {
                SvtAv2BlockView luma;
                SVT_TEST_CHECK_EQ_INT(svt_av2_block_from_sb(&geo, sb_col, sb_row, &luma),
                                      SVT_AV2_ERROR_NONE);
                /* For a chroma plane the block geometry must come from the chroma
                 * view, not from the luma block: the shapes differ for
                 * non-square luma blocks. */
                SvtAv2BlockView blk = luma;
                if (p != SVT_AV2_PLANE_Y) {
                    SVT_TEST_CHECK_EQ_INT(svt_av2_chroma_view(&geo, &luma, &blk),
                                          SVT_AV2_ERROR_NONE);
                }

                SvtAv2BlockNeighbors nb;
                memset(&nb, 0, sizeof(nb));
                SVT_TEST_CHECK_EQ_INT(svt_av2_block_neighbors(&pic, p, &blk, &nb),
                                      SVT_AV2_ERROR_NONE);
                SVT_TEST_CHECK_EQ_INT(nb.above_valid, (blk.y > 0) ? 1 : 0);
                SVT_TEST_CHECK_EQ_INT(nb.left_valid, (blk.x > 0) ? 1 : 0);

                /* When both are valid, the samples T007 reports must equal the
                 * ones the predictor's reference builder reads. */
                if (nb.above_valid && nb.left_valid) {
                    SvtAv2IntraBlock ib;
                    memset(&ib, 0, sizeof(ib));
                    ib.x = blk.x;
                    ib.y = blk.y;
                    ib.w = blk.w;
                    ib.h = blk.h;
                    ib.plane = p;

                    SvtAv2IntraReference ref;
                    memset(&ref, 0, sizeof(ref));
                    SVT_TEST_CHECK_EQ_INT(
                        svt_av2_intra_reference_build_flags(&pic, &ib, 1, 1, &ref),
                        SVT_AV2_ERROR_NONE);
                    SVT_TEST_CHECK_EQ_INT(ref.above[0], nb.above[1]);
                    SVT_TEST_CHECK_EQ_INT(ref.left[0], nb.left[1]);
                }
            }
        }
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&pic), SVT_AV2_ERROR_NONE);
}

static void test_invalid_arguments(void) {
    SvtAv2FrameBuffer pic;
    SVT_TEST_CHECK_EQ_INT(make_frame(&pic), SVT_AV2_ERROR_NONE);

    SvtAv2IntraBlock blk;
    memset(&blk, 0, sizeof(blk));
    blk.x = 0;
    blk.y = 0;
    blk.w = 8;
    blk.h = 8;
    blk.plane = SVT_AV2_PLANE_Y;

    SvtAv2IntraReference ref;
    uint8_t dst[SVT_AV2_MAX_TX_SIZE * SVT_AV2_MAX_TX_SIZE];

    /* NULL arguments. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_reference_build_flags(NULL, &blk, 1, 1, &ref),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_reference_build_flags(&pic, NULL, 1, 1, &ref),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_reference_build_flags(&pic, &blk, 1, 1, NULL),
                          SVT_AV2_ERROR_INVALID_PARAMETER);

    /* Zero size and oversize. */
    blk.w = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_reference_build_flags(&pic, &blk, 1, 1, &ref),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    blk.w = 8;
    blk.h = 128;
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_reference_build_flags(&pic, &blk, 1, 1, &ref),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    blk.h = 8;

    /* Region extending past the visible area must be rejected, not clipped. */
    blk.x = 60;
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_reference_build_flags(&pic, &blk, 1, 1, &ref),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    blk.x = 0;
    blk.y = 60;
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_reference_build_flags(&pic, &blk, 1, 1, &ref),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    blk.y = 0;

    /* A chroma region is validated against the chroma plane, not the luma one. */
    blk.plane = SVT_AV2_PLANE_U;
    blk.x = 28;
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_reference_build_flags(&pic, &blk, 1, 1, &ref),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    blk.x = 0;

    /* Out-of-range plane. */
    blk.plane = SVT_AV2_PICTURE_MAX_PLANES;
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_reference_build_flags(&pic, &blk, 1, 1, &ref),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    blk.plane = SVT_AV2_PLANE_Y;

    /* Predictor argument checks. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_reference_build_flags(&pic, &blk, 0, 0, &ref),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_dc_predict_flags(NULL, &blk, 0, 0, dst, 64),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_dc_predict_flags(&ref, &blk, 0, 0, NULL, 64),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    /* Stride smaller than the block width would make rows overlap. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_dc_predict_flags(&ref, &blk, 0, 0, dst, 4),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    /* A reference that describes another region must be rejected. */
    SvtAv2IntraReference wrong = ref;
    wrong.width = 4;
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_dc_predict_flags(&wrong, &blk, 0, 0, dst, 64),
                          SVT_AV2_ERROR_INVALID_PARAMETER);

    /* Mode name helper. */
    SVT_TEST_CHECK_EQ_STR(svt_av2_intra_mode_name(SVT_AV2_INTRA_DC_PRED), "DC_PRED");
    SVT_TEST_CHECK_EQ_STR(svt_av2_intra_mode_name((SvtAv2IntraMode)99), "INVALID");

    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&pic), SVT_AV2_ERROR_NONE);
}

static void test_flags_override_boundary(void) {
    /* A tile or superblock boundary inside the frame makes a neighbour
     * unavailable even though x > 0 / y > 0. The substituted reference and the
     * prediction must then follow the unavailable branch, and the picture samples
     * that would have been read must be ignored. */
    SvtAv2FrameBuffer pic;
    SVT_TEST_CHECK_EQ_INT(make_frame(&pic), SVT_AV2_ERROR_NONE);
    for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
        uint8_t value = 200u;
        fill_plane(&pic, p, fn_const, &value);
    }

    SvtAv2IntraBlock blk;
    memset(&blk, 0, sizeof(blk));
    blk.x = 16; /* interior: the picture says both neighbours exist */
    blk.y = 16;
    blk.w = 8;
    blk.h = 8;
    blk.plane = SVT_AV2_PLANE_Y;

    /* Both real: constant 200 content -> DC 200. */
    SvtAv2IntraReference real_ref;
    uint8_t real_dst[SVT_AV2_MAX_TX_SIZE * SVT_AV2_MAX_TX_SIZE];
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_reference_build_flags(&pic, &blk, 1, 1, &real_ref),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_dc_predict_flags(&real_ref, &blk, 1, 1, real_dst, 64),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(real_dst[0], 200);

    /* Force both unavailable: the reference becomes all-128 and so does the
     * prediction, even though real 200 samples sit right next to the block. */
    SvtAv2IntraReference none_ref;
    uint8_t none_dst[SVT_AV2_MAX_TX_SIZE * SVT_AV2_MAX_TX_SIZE];
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_reference_build_flags(&pic, &blk, 0, 0, &none_ref),
                          SVT_AV2_ERROR_NONE);
    for (uint32_t j = 0; j < blk.w; ++j) {
        SVT_TEST_CHECK_EQ_INT(none_ref.above[j], 128);
    }
    for (uint32_t i = 0; i < blk.h; ++i) {
        SVT_TEST_CHECK_EQ_INT(none_ref.left[i], 128);
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_dc_predict_flags(&none_ref, &blk, 0, 0, none_dst, 64),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(none_dst[0], 128);

    /* Above only: every LeftCol entry becomes the single above sample. */
    SvtAv2IntraReference above_ref;
    uint8_t above_dst[SVT_AV2_MAX_TX_SIZE * SVT_AV2_MAX_TX_SIZE];
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_reference_build_flags(&pic, &blk, 0, 1, &above_ref),
                          SVT_AV2_ERROR_NONE);
    for (uint32_t i = 0; i < blk.h; ++i) {
        SVT_TEST_CHECK_EQ_INT(above_ref.left[i], 200);
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_dc_predict_flags(&above_ref, &blk, 0, 1, above_dst, 64),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(above_dst[0], 200);

    /* Left only, symmetric. */
    SvtAv2IntraReference left_ref;
    uint8_t left_dst[SVT_AV2_MAX_TX_SIZE * SVT_AV2_MAX_TX_SIZE];
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_reference_build_flags(&pic, &blk, 1, 0, &left_ref),
                          SVT_AV2_ERROR_NONE);
    for (uint32_t j = 0; j < blk.w; ++j) {
        SVT_TEST_CHECK_EQ_INT(left_ref.above[j], 200);
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_dc_predict_flags(&left_ref, &blk, 1, 0, left_dst, 64),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(left_dst[0], 200);

    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&pic), SVT_AV2_ERROR_NONE);
}

/* DC values of the exact spec examples, hand-derived, to pin the rounding. */
static void test_dc_rounding_examples(void) {
    SvtAv2FrameBuffer pic;
    SVT_TEST_CHECK_EQ_INT(make_frame(&pic), SVT_AV2_ERROR_NONE);

    /* Content 255 everywhere, block at (8,8) 8x8: both neighbours are 255, so
     * sum = 128, den = 16, Round2(128,4) = 8. Wait: sum = 8*255 + 8*255 = 4080,
     * den = 16, 4080/16 = 255 exactly. */
    uint8_t v255 = 255u;
    for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
        fill_plane(&pic, p, fn_const, &v255);
    }
    SvtAv2IntraBlock blk;
    memset(&blk, 0, sizeof(blk));
    blk.x = 8;
    blk.y = 8;
    blk.w = 8;
    blk.h = 8;
    blk.plane = SVT_AV2_PLANE_Y;

    SvtAv2IntraReference ref;
    uint8_t dst[SVT_AV2_MAX_TX_SIZE * SVT_AV2_MAX_TX_SIZE];
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_reference_build_flags(&pic, &blk, 1, 1, &ref),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_dc_predict_flags(&ref, &blk, 1, 1, dst, 64),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(dst[0], 255);

    /* Above-only rounding: t samples of value v, expected Round2(t*v, log2(h)).
     * Build a 4x4 block with above = {1,2,3,4} and check the exact midpoint case
     * sum = 10, h = 4 -> Round2(10,2) = (10+2)>>2 = 3. */
    uint8_t* const base = svt_av2_picture_plane(&pic, SVT_AV2_PLANE_Y);
    const uint32_t stride = pic.strides[SVT_AV2_PLANE_Y];
    memset(base, 0, (size_t)stride * pic.aligned_height[SVT_AV2_PLANE_Y]);
    /* Row 3 is the "above" row for a block at y = 4, x = 0. */
    base[(size_t)3 * stride + 0] = 1;
    base[(size_t)3 * stride + 1] = 2;
    base[(size_t)3 * stride + 2] = 3;
    base[(size_t)3 * stride + 3] = 4;

    memset(&blk, 0, sizeof(blk));
    blk.x = 0;
    blk.y = 4;
    blk.w = 4;
    blk.h = 4;
    blk.plane = SVT_AV2_PLANE_Y;
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_reference_build_flags(&pic, &blk, 0, 1, &ref),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(ref.above[0], 1);
    SVT_TEST_CHECK_EQ_INT(ref.above[1], 2);
    SVT_TEST_CHECK_EQ_INT(ref.above[2], 3);
    SVT_TEST_CHECK_EQ_INT(ref.above[3], 4);
    /* Every LeftCol entry is the first above sample, per the substitution. */
    for (uint32_t i = 0; i < 4; ++i) {
        SVT_TEST_CHECK_EQ_INT(ref.left[i], 1);
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_dc_predict_flags(&ref, &blk, 0, 1, dst, 64),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(dst[0], 3);

    /* Left-only, same numbers: left = {1,2,3,4}, w = 4 -> Round2(10,2) = 3. */
    memset(base, 0, (size_t)stride * pic.aligned_height[SVT_AV2_PLANE_Y]);
    base[(size_t)4 * stride + 3] = 1;
    base[(size_t)5 * stride + 3] = 2;
    base[(size_t)6 * stride + 3] = 3;
    base[(size_t)7 * stride + 3] = 4;
    memset(&blk, 0, sizeof(blk));
    blk.x = 4;
    blk.y = 4;
    blk.w = 4;
    blk.h = 4;
    blk.plane = SVT_AV2_PLANE_Y;
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_reference_build_flags(&pic, &blk, 1, 0, &ref),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(ref.left[0], 1);
    SVT_TEST_CHECK_EQ_INT(ref.left[1], 2);
    SVT_TEST_CHECK_EQ_INT(ref.left[2], 3);
    SVT_TEST_CHECK_EQ_INT(ref.left[3], 4);
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_dc_predict_flags(&ref, &blk, 1, 0, dst, 64),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(dst[0], 3);

    /* Both neighbours, hand-computed: above = {1,2,3,4}, left = {5,6,7,8},
     * w = h = 4 -> sum = 10 + 26 = 36, den = 8, Round2(36,3) = (36+4)>>3 = 5. */
    memset(base, 0, (size_t)stride * pic.aligned_height[SVT_AV2_PLANE_Y]);
    base[(size_t)3 * stride + 4] = 1;
    base[(size_t)3 * stride + 5] = 2;
    base[(size_t)3 * stride + 6] = 3;
    base[(size_t)3 * stride + 7] = 4;
    base[(size_t)4 * stride + 3] = 5;
    base[(size_t)5 * stride + 3] = 6;
    base[(size_t)6 * stride + 3] = 7;
    base[(size_t)7 * stride + 3] = 8;
    memset(&blk, 0, sizeof(blk));
    blk.x = 4;
    blk.y = 4;
    blk.w = 4;
    blk.h = 4;
    blk.plane = SVT_AV2_PLANE_Y;
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_reference_build_flags(&pic, &blk, 1, 1, &ref),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_dc_predict_flags(&ref, &blk, 1, 1, dst, 64),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(dst[0], 5);

    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&pic), SVT_AV2_ERROR_NONE);
}

/* The both-available DC branch divides by w + h, and approx_divide is NOT an
 * exact division. For square blocks w + h is a power of two and the two agree, but
 * for a rectangular block such as 8x16 the divisor is 24 and the approximate
 * reciprocal gives a different (lower) result than a rounded division would.
 *
 * This case pins that difference against hand-computed spec values. It exists
 * because the first version of the test model used a plain rounded division and
 * silently disagreed with the spec on rectangular blocks; a regression here would
 * mean someone replaced approx_divide with an exact division. */
static void test_rectangular_divisor_is_approximate(void) {
    /* 8x16 block with both neighbours. Divisors that are not powers of two:
     *   8x16 / 16x8 -> 24,  4x16 / 16x4 -> 20,  32x8 / 8x32 -> 40.
     * For each, w + h must be non-power-of-two (the premise of this test). */
    static const struct {
        uint32_t w;
        uint32_t h;
        uint32_t den;
    } cases[] = {
        {8, 16, 24}, {16, 8, 24}, {4, 16, 20}, {16, 4, 20}, {32, 8, 40}, {8, 32, 40},
    };

    for (size_t c = 0; c < SVT_AV2_ARRAY_SIZE(cases); ++c) {
        const uint32_t w = cases[c].w;
        const uint32_t h = cases[c].h;
        const uint32_t den = cases[c].den;
        SVT_TEST_CHECK_EQ_INT(den, w + h);
        /* Premise: this divisor really is not a power of two, so the approximate
         * and exact forms can differ. */
        SVT_TEST_CHECK((den & (den - 1u)) != 0u);

        /* Build a synthetic reference with a controlled sum so the expectation is
         * computable by hand: every sample 255 gives sum = 255 * (w + h) and the
         * result must be clipped to 255. Then use value 1 everywhere, which gives
         * sum = w + h and a DC of exactly 1. These two are exact for any divide,
         * so they pin clipping and the trivial case rather than the rounding. */
        SvtAv2IntraReference ref;
        memset(&ref, 0, sizeof(ref));
        ref.width = w;
        ref.height = h;
        for (uint32_t j = 0; j < w; ++j) {
            ref.above[j] = 255;
        }
        for (uint32_t i = 0; i < h; ++i) {
            ref.left[i] = 255;
        }

        SvtAv2IntraBlock blk;
        memset(&blk, 0, sizeof(blk));
        blk.x = 0;
        blk.y = 0;
        blk.w = w;
        blk.h = h;
        blk.plane = SVT_AV2_PLANE_Y;

        enum { DST_STRIDE = SVT_AV2_MAX_TX_SIZE + 7 };
        uint8_t dst[(SVT_AV2_MAX_TX_SIZE + 1) * DST_STRIDE];
        memset(dst, 0xA5, sizeof(dst));

        SVT_TEST_CHECK_EQ_INT(svt_av2_intra_dc_predict_flags(&ref, &blk, 1, 1, dst, DST_STRIDE),
                              SVT_AV2_ERROR_NONE);
        /* Model and implementation must agree, and the model uses the real
         * resolve_divisor chain. */
        const uint32_t model = model_approx_divide(255u * den, den);
        SVT_TEST_CHECK_EQ_INT(dst[0], model > 255u ? 255u : model);
        SVT_TEST_CHECK_EQ_INT(dst[0], 255);

        /* All-ones: sum = den, so approx_divide(den, den) must be 1. */
        for (uint32_t j = 0; j < w; ++j) {
            ref.above[j] = 1;
        }
        for (uint32_t i = 0; i < h; ++i) {
            ref.left[i] = 1;
        }
        SVT_TEST_CHECK_EQ_INT(svt_av2_intra_dc_predict_flags(&ref, &blk, 1, 1, dst, DST_STRIDE),
                              SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(dst[0], 1);

        /* The decisive case: a sum where the approximate reciprocal and an exact
         * rounded division disagree. approx_divide(den * 255, den) happens to be
         * exact, so search a mid-range sum and assert the implementation follows
         * the model (resolve_divisor) rather than the rounded division. */
        uint32_t disagreements = 0;
        for (uint32_t sum = 0; sum <= 255u * den; ++sum) {
            const uint32_t approximate = model_approx_divide(sum, den);
            const uint32_t exact = (sum + (den >> 1)) / den;
            if (approximate != exact) {
                if (disagreements < 3) {
                    for (uint32_t j = 0; j < w; ++j) {
                        ref.above[j] = 0;
                    }
                    for (uint32_t i = 0; i < h; ++i) {
                        ref.left[i] = 0;
                    }
                    /* Distribute `sum` over the reference samples: put it all in
                     * one sample when it fits, otherwise spread it in equal parts
                     * across the w + h entries (the DC branch only reads the sum,
                     * so any distribution with the right total is equivalent). */
                    uint32_t remaining = sum;
                    uint32_t idx = 0;
                    while (remaining > 0 && idx < den) {
                        const uint32_t take = (remaining > 255u) ? 255u : remaining;
                        if (idx < w) {
                            ref.above[idx] = (uint8_t)take;
                        } else {
                            ref.left[idx - w] = (uint8_t)take;
                        }
                        remaining -= take;
                        ++idx;
                    }
                    if (remaining == 0) {
                        SVT_TEST_CHECK_EQ_INT(
                            svt_av2_intra_dc_predict_flags(&ref, &blk, 1, 1, dst, DST_STRIDE),
                            SVT_AV2_ERROR_NONE);
                        SVT_TEST_CHECK_EQ_INT(dst[0], approximate);
                    }
                }
                ++disagreements;
            }
        }
        /* The premise must actually hold for this divisor, otherwise the loop
         * above proved nothing. */
        SVT_TEST_CHECK(disagreements > 0);
    }
}
static void test_default_builder_matches_flags(void) {
    SvtAv2FrameBuffer pic;
    SVT_TEST_CHECK_EQ_INT(make_frame(&pic), SVT_AV2_ERROR_NONE);
    for (uint32_t p = 0; p < SVT_AV2_PICTURE_MAX_PLANES; ++p) {
        rng_seed(0x5EED0000u + p);
        g_random_base = 0x2468ACE0u;
        fill_plane(&pic, p, fn_random, NULL);
    }

    SvtAv2IntraBlock blk;
    memset(&blk, 0, sizeof(blk));
    blk.plane = SVT_AV2_PLANE_Y;
    blk.w = 8;
    blk.h = 8;

    for (uint32_t y = 0; y < 64; y += 8) {
        for (uint32_t x = 0; x < 64; x += 8) {
            blk.x = x;
            blk.y = y;
            SvtAv2IntraReference a;
            SvtAv2IntraReference b;
            memset(&a, 0, sizeof(a));
            memset(&b, 0, sizeof(b));
            SVT_TEST_CHECK_EQ_INT(svt_av2_intra_reference_build(&pic, &blk, &a),
                                  SVT_AV2_ERROR_NONE);
            SVT_TEST_CHECK_EQ_INT(
                svt_av2_intra_reference_build_flags(&pic, &blk, x > 0, y > 0, &b),
                SVT_AV2_ERROR_NONE);
            /* Only the first w / h entries are meaningful; the rest of the array
             * is not part of the contract. */
            SVT_TEST_CHECK_EQ_INT(memcmp(a.above, b.above, blk.w * sizeof(a.above[0])), 0);
            SVT_TEST_CHECK_EQ_INT(memcmp(a.left, b.left, blk.h * sizeof(a.left[0])), 0);

            /* Compare only the w x h region the predictor writes, row by row, so
             * both the values and the row pitch are checked. */
            uint8_t da[SVT_AV2_MAX_TX_SIZE * SVT_AV2_MAX_TX_SIZE];
            uint8_t db[SVT_AV2_MAX_TX_SIZE * SVT_AV2_MAX_TX_SIZE];
            memset(da, 0x11, sizeof(da));
            memset(db, 0x22, sizeof(db));
            SVT_TEST_CHECK_EQ_INT(svt_av2_intra_dc_predict(&a, &blk, da, 64),
                                  SVT_AV2_ERROR_NONE);
            SVT_TEST_CHECK_EQ_INT(
                svt_av2_intra_dc_predict_flags(&b, &blk, x > 0, y > 0, db, 64),
                SVT_AV2_ERROR_NONE);
            for (uint32_t i = 0; i < blk.h; ++i) {
                SVT_TEST_CHECK_EQ_INT(memcmp(da + (size_t)i * 64, db + (size_t)i * 64, blk.w), 0);
            }
        }
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&pic), SVT_AV2_ERROR_NONE);
}

static void test_highbd_random_dc_and_residual(void) {
    uint32_t seed = 0x27182818u;
    SvtAv2FrameBuffer source, pred;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&source, 128, 128, 10, SVT_AV2_CHROMA_444), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&pred, 128, 128, 10, SVT_AV2_CHROMA_444), SVT_AV2_ERROR_NONE);
    const uint32_t sizes[] = { 4, 8, 16, 32, 64 };
    int16_t residual[64 * 64];
    for (uint32_t bd = 8; bd <= 10; bd += 2)
        for (uint32_t g = 0; g < 5; ++g)
            for (uint32_t trial = 0; trial < 100; ++trial) {
                SvtAv2IntraReference ref;
                memset(&ref, 0, sizeof(ref));
                const uint32_t n = sizes[g], maximum = (1u << bd) - 1u;
                ref.bit_depth = bd; ref.width = n; ref.height = n;
                uint32_t sum_above = 0, sum_left = 0;
                for (uint32_t i = 0; i < n; ++i) {
                    seed = seed * 1664525u + 1013904223u;
                    ref.above[i] = (uint16_t)(seed & maximum);
                    seed = seed * 1664525u + 1013904223u;
                    ref.left[i] = (uint16_t)(seed & maximum);
                    sum_above += ref.above[i]; sum_left += ref.left[i];
                }
                SvtAv2IntraBlock block = { 1, 1, n, n, 0 };
                for (uint32_t flags = 0; flags < 4; ++flags) {
                    uint8_t dst[64 * 131];
                    memset(dst, 0xA5, sizeof(dst));
                    const uint32_t step = bd > 8 ? 2u : 1u;
                    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_dc_predict_flags(&ref, &block, flags & 1, flags & 2, dst + 1, 131), SVT_AV2_ERROR_NONE);
                    const uint32_t expected = flags == 0 ? 1u << (bd - 1) :
                        flags == 3 ? (sum_above + sum_left + n) / (2 * n) :
                        ((flags & 1 ? sum_left : sum_above) + n / 2) / n;
                    for (uint32_t y = 0; y < n; ++y) {
                        for (uint32_t x = 0; x < n; ++x)
                            SVT_TEST_CHECK_EQ_INT(svt_av2_sample_load(dst + 1 + y * 131 + x * step, bd), expected);
                        SVT_TEST_CHECK_EQ_INT(dst[1 + y * 131 + n * step], 0xA5);
                    }
                }
                if (bd == 10) {
                    block.x = 0; block.y = 0;
                    for (uint32_t y = 0; y < n; ++y)
                        for (uint32_t x = 0; x < n; ++x) {
                            const uint16_t a = (uint16_t)((x * 255 + y * 511 + trial) & 1023);
                            const uint16_t b = (uint16_t)((x + y + trial * 13) & 1023);
                            svt_av2_sample_store(source.planes[0] + y * source.strides[0] + 2 * x, 10, a);
                            svt_av2_sample_store(pred.planes[0] + y * pred.strides[0] + 2 * x, 10, b);
                        }
                    SVT_TEST_CHECK_EQ_INT(svt_av2_residual_compute(&source, &pred, &block, residual, 64), SVT_AV2_ERROR_NONE);
                    for (uint32_t y = 0; y < n; ++y)
                        for (uint32_t x = 0; x < n; ++x)
                            SVT_TEST_CHECK_EQ_INT(residual[y * 64 + x], (int)((x * 255 + y * 511 + trial) & 1023) - (int)((x + y + trial * 13) & 1023));
                }
            }
    svt_av2_picture_free(&source); svt_av2_picture_free(&pred);
}

/* M5 G1: independent axis-projection expectations. No AV2 mode is signaled. */
static void test_m5_axis_prediction(void) {
    const uint32_t sizes[] = {4u, 8u, 16u, 32u, 64u};
    uint32_t state = 0x4d350817u;
    for (uint32_t bd = 8u; bd <= 10u; bd += 2u)
        for (uint32_t size_index = 0; size_index < 5u; ++size_index)
            for (uint32_t trial = 0; trial < 100u; ++trial) {
                const uint32_t w = sizes[size_index];
                const uint32_t h = (trial & 1u) ? w : sizes[(size_index + 1u) % 5u];
                const uint32_t max_value = (1u << bd) - 1u;
                const uint32_t step = bd > 8u ? 2u : 1u;
                const uint32_t stride = 131u; /* odd byte stride */
                SvtAv2IntraReference ref;
                memset(&ref, 0, sizeof(ref));
                ref.bit_depth = bd;
                ref.width = w;
                ref.height = h;
                for (uint32_t i = 0; i < w; ++i) {
                    state = 1664525u * state + 1013904223u;
                    ref.above[i] = (uint16_t)(trial == 0u ? 0u :
                        trial == 1u ? max_value : state & max_value);
                }
                for (uint32_t i = 0; i < h; ++i) {
                    state = 1664525u * state + 1013904223u;
                    ref.left[i] = (uint16_t)(trial == 0u ? max_value :
                        trial == 1u ? 0u : state & max_value);
                }
                const SvtAv2IntraBlock block = {0u, 0u, w, h, 0u};
                for (uint32_t mode = 0; mode < 2u; ++mode) {
                    uint8_t output[64u * 131u + 2u];
                    memset(output, 0xA5, sizeof(output));
                    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_axis_predict(&ref, &block,
                        (SvtAv2IntraAxis)mode, output + 1u, stride), SVT_AV2_ERROR_NONE);
                    SVT_TEST_CHECK_EQ_INT(output[0], 0xA5);
                    for (uint32_t y = 0; y < h; ++y) {
                        for (uint32_t x = 0; x < w; ++x) {
                            const uint16_t expected = mode == 0u ? ref.above[x] : ref.left[y];
                            SVT_TEST_CHECK_EQ_INT(svt_av2_sample_load(output + 1u +
                                (size_t)y * stride + x * step, bd), expected);
                        }
                        SVT_TEST_CHECK_EQ_INT(output[1u + (size_t)y * stride + w * step], 0xA5);
                    }
                    SVT_TEST_CHECK_EQ_INT(output[1u + (size_t)h * stride], 0xA5);
                }
            }
    SvtAv2IntraReference ref = {0};
    SvtAv2IntraBlock block = {0u, 0u, 8u, 8u, 0u};
    uint8_t dst[128];
    ref.bit_depth = 10u; ref.width = 8u; ref.height = 8u;
    memset(dst, 0x7Bu, sizeof(dst));
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_axis_predict(&ref, &block,
        (SvtAv2IntraAxis)2, dst, 17u), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_axis_predict(&ref, &block,
        SVT_AV2_INTRA_AXIS_VERTICAL, dst, 15u), SVT_AV2_ERROR_INVALID_PARAMETER);
    block.w = 0u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_axis_predict(&ref, &block,
        SVT_AV2_INTRA_AXIS_HORIZONTAL, dst, 17u), SVT_AV2_ERROR_INVALID_PARAMETER);
    block.w = 8u; ref.width = 4u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_axis_predict(&ref, &block,
        SVT_AV2_INTRA_AXIS_VERTICAL, dst, 17u), SVT_AV2_ERROR_INVALID_PARAMETER);
    ref.width = 8u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_axis_predict(NULL, &block,
        SVT_AV2_INTRA_AXIS_VERTICAL, dst, 17u), SVT_AV2_ERROR_INVALID_PARAMETER);
    for (size_t i = 0; i < sizeof(dst); ++i)
        SVT_TEST_CHECK_EQ_INT(dst[i], 0x7B);
}

int main(void) {
    test_m5_axis_prediction();
    test_highbd_random_dc_and_residual();
    test_first_block_no_uninitialised_read();
    test_uniform_content();
    test_gradient_and_checker();
    test_dc_rounding_examples();
    test_rectangular_divisor_is_approximate();
    test_random_neighbours_100_per_size();
    test_non_tight_stride();
    test_chroma_sizes();
    test_edge_blocks_partial();
    test_neighbors_from_t007_agree();
    test_flags_override_boundary();
    test_default_builder_matches_flags();
    test_invalid_arguments();

    SVT_TEST_REPORT("test_intra_prediction");
}
