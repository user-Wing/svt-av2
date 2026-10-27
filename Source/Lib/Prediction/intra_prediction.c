/*
 * SVT-AV2 minimal scalar intra prediction (T008). See intra_prediction.h for the
 * contract, the spec clause for every rule and the AVM cross-check locations.
 */
#include "intra_prediction.h"

#include <string.h>

/* --- spec primitive functions (AV2 §4.8) ------------------------------------ */

/* Round2(x, n) = floor((x + 2^(n-1)) / 2^n) for x >= 0, n >= 0.
 * §4.8 gives the integer-equivalent pseudo-code `(x + (1 << (n-1))) >> n`, which
 * is what this implements, with the n == 0 case returning x unchanged. */
static uint32_t round2_u32(uint32_t x, uint32_t n) {
    if (n == 0) {
        return x;
    }
    return (x + (1u << (n - 1u))) >> n;
}

/* Clip1(x) = Clip3(0, 2^BitDepth - 1, x) (§4.8, eq. 2). 8-bit only this round. */
static uint16_t clip1(int32_t x, uint32_t bd) {
    if (x < 0) {
        return 0;
    }
    if (x > (int32_t)((1u << bd) - 1u)) {
        return (uint16_t)((1u << bd) - 1u);
    }
    return (uint16_t)x;
}

/* FloorLog2(x) for x >= 1 (§4.8): index of the most significant set bit. */
static uint32_t floor_log2_u32(uint32_t x) {
    uint32_t s = 0;
    while (x != 0) {
        x >>= 1;
        ++s;
    }
    return s - 1u;
}

/* DIV_LUT_BITS / DIV_LUT_PREC_BITS and the div_lut table.
 *
 * These belong to the spec's approx_divide (§7.13.2.10's "approx_divide(num, den)
 * norange { (shift, scale) = resolve_divisor(den); return Round2(num * scale,
 * shift) }"). The DC process uses it for the w+h average, and it is NOT an exact
 * division: the result is only approximate and the spec explicitly notes it must
 * then be clipped ("The divide is only approximate so the average value computed
 * by approx_divide needs to be clipped so that the predicted value fits within
 * BitDepth bits").
 *
 * The table and the constants are defined by the spec's resolve_divisor and by
 * the decoder's implementation of it, so they are reproduced rather than
 * recomputed. The tests additionally verify the useful property that for a
 * power-of-two divisor D, resolve_divisor returns exactly (2^DIV_LUT_PREC_BITS/D)
 * and approx_divide therefore equals Round2(num, log2(D)); every divisor the DC
 * both-available branch can use (w + h) is a power of two for the square blocks
 * this encoder codes, which gives the test model an independent, division-free
 * expectation. den is never 0 here because that branch requires w > 0 and h > 0.
 */
#define SVT_AV2_DIV_LUT_BITS 7
#define SVT_AV2_DIV_LUT_PREC_BITS 9
#define SVT_AV2_DIV_LUT_NUM (1 << SVT_AV2_DIV_LUT_BITS)

/* Byte-for-byte the decoder's table (avm\av2\common\warped_motion.h:50-60, whose
 * constants live at avm\av2\common\enums.h:1157-1158). It is a constant of the
 * spec's approx_divide, not an AVM search heuristic, so it must be reproduced
 * exactly rather than recomputed. */
static const int16_t svt_av2_div_lut[SVT_AV2_DIV_LUT_NUM + 1] = {
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

/* resolve_divisor(D, &shift) -> scale, per the spec's resolve_divisor and AVM's
 * resolve_divisor_32. Decomposes 1/D as scale / 2^shift with `scale` at
 * DIV_LUT_PREC_BITS precision. D must be >= 1. */
static int16_t resolve_divisor_u32(uint32_t d, uint32_t* shift) {
    *shift = floor_log2_u32(d);
    /* e = D with its most significant 1 bit cleared. */
    const uint32_t e = d - (1u << *shift);
    uint32_t f;
    if (*shift > SVT_AV2_DIV_LUT_BITS) {
        f = round2_u32(e, *shift - SVT_AV2_DIV_LUT_BITS);
    } else {
        f = e << (SVT_AV2_DIV_LUT_BITS - *shift);
    }
    *shift += SVT_AV2_DIV_LUT_PREC_BITS;
    return svt_av2_div_lut[f];
}

/* approx_divide(num, den) = Round2(num * scale, shift) (§7.13.2.10). */
static uint32_t approx_divide_u32(uint32_t num, uint32_t den) {
    uint32_t shift = 0;
    const int16_t scale = resolve_divisor_u32(den, &shift);
    return round2_u32(num * (uint32_t)scale, shift);
}

/* --- reference construction -------------------------------------------------- */

/* Validates the geometry shared by every entry point. `plane_width`/`plane_height`
 * are the visible dimensions the block must lie inside. */
static SvtAv2Error validate_block(const SvtAv2IntraBlock* block,
                                  uint32_t plane_width,
                                  uint32_t plane_height) {
    if (block == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (block->w == 0 || block->h == 0) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (block->w > SVT_AV2_MAX_TX_SIZE || block->h > SVT_AV2_MAX_TX_SIZE) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (block->plane >= SVT_AV2_PICTURE_MAX_PLANES) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    /* Reject a region that is not fully inside the visible area rather than
     * clipping it: clipping would silently change which samples are predicted. */
    if (block->x + block->w > plane_width || block->y + block->h > plane_height) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_intra_reference_build_flags(const SvtAv2FrameBuffer* recon,
                                                const SvtAv2IntraBlock* block,
                                                int have_left,
                                                int have_above,
                                                SvtAv2IntraReference* ref) {
    if (recon == NULL || ref == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    memset(ref, 0, sizeof(*ref));

    const uint32_t plane_width = svt_av2_picture_plane_width(recon, block ? block->plane : 0);
    const uint32_t plane_height = svt_av2_picture_plane_height(recon, block ? block->plane : 0);
    const SvtAv2Error err = validate_block(block, plane_width, plane_height);
    if (err != SVT_AV2_ERROR_NONE) {
        return err;
    }

    const uint8_t* const base = svt_av2_picture_plane_const(recon, block->plane);
    if (base == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    const uint32_t stride = recon->strides[block->plane];
    const uint32_t step = recon->bit_depth > 8 ? 2u : 1u;
    ref->bit_depth = recon->bit_depth;

    ref->width = block->w;
    ref->height = block->h;

    /* Availability cannot exceed what the picture can actually supply: asking for
     * a neighbour at x == 0 would read out of bounds. This is a caller error, and
     * the flags are clamped to the truth so a mistaken caller cannot make this
     * function read outside the picture. */
    if (block->x == 0) {
        have_left = 0;
    }
    if (block->y == 0) {
        have_above = 0;
    }

    if (have_left && have_above) {
        /* Both real. §7.13.2.10 reads AboveRow[0..w-1] = CurrFrame[y-1][x..x+w-1]
         * and LeftCol[0..h-1] = CurrFrame[y..y+h-1][x-1]. */
        const uint8_t* const above = base + (size_t)(block->y - 1u) * stride + block->x * step;
        const uint8_t* const left = base + (size_t)block->y * stride + (block->x - 1u) * step;
        for (uint32_t j = 0; j < block->w; ++j) {
            ref->above[j] = svt_av2_sample_load(above + j * step, recon->bit_depth);
        }
        for (uint32_t i = 0; i < block->h; ++i) {
            ref->left[i] = svt_av2_sample_load(left + (size_t)i * stride, recon->bit_depth);
        }
    } else if (have_left) {
        /* No above row: §7.13.2.1 sets every AboveRow sample to the single
         * available edge sample, which in the decoder is above_ref[0] = the
         * sample at LeftCol[0] = CurrFrame[y][x-1]
         * (reconintra.c:1197-1200: `memset16(left_col, above_ref[0], ...)` fills
         * the mirrored direction; above_row is filled from left_ref[0] at
         * reconintra.c:1233-1235). */
        const uint8_t* const left = base + (size_t)block->y * stride + (block->x - 1u) * step;
        const uint16_t first = svt_av2_sample_load(left, recon->bit_depth);
        for (uint32_t j = 0; j < block->w; ++j) {
            ref->above[j] = first;
        }
        for (uint32_t i = 0; i < block->h; ++i) {
            ref->left[i] = svt_av2_sample_load(left + (size_t)i * stride, recon->bit_depth);
        }
    } else if (have_above) {
        /* No left column: every LeftCol sample = the sample at (x, y-1)
         * (reconintra.c:1215-1235). */
        const uint8_t* const above = base + (size_t)(block->y - 1u) * stride + block->x * step;
        const uint16_t first = svt_av2_sample_load(above, recon->bit_depth);
        for (uint32_t j = 0; j < block->w; ++j) {
            ref->above[j] = svt_av2_sample_load(above + j * step, recon->bit_depth);
        }
        for (uint32_t i = 0; i < block->h; ++i) {
            ref->left[i] = first;
        }
    } else {
        /* Neither neighbour: §7.13.2.1 and the DC process's final branch both use
         * 1 << (BitDepth - 1). No picture sample is read at all. */
        const uint16_t dc = (uint16_t)(1u << (recon->bit_depth - 1u));
        for (uint32_t j = 0; j < block->w; ++j) {
            ref->above[j] = dc;
        }
        for (uint32_t i = 0; i < block->h; ++i) {
            ref->left[i] = dc;
        }
    }

    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_intra_reference_build(const SvtAv2FrameBuffer* recon,
                                          const SvtAv2IntraBlock* block,
                                          SvtAv2IntraReference* ref) {
    if (recon == NULL || block == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    /* Availability at this level is a pure frame-boundary property; tile and
     * superblock boundaries are T012's to pass through the _flags entry point. */
    const int have_left = (block->x > 0) ? 1 : 0;
    const int have_above = (block->y > 0) ? 1 : 0;
    return svt_av2_intra_reference_build_flags(recon, block, have_left, have_above, ref);
}

/* --- DC prediction ------------------------------------------------------------ */

SvtAv2Error svt_av2_intra_dc_predict_flags(const SvtAv2IntraReference* ref,
                                           const SvtAv2IntraBlock* block,
                                           int have_left,
                                           int have_above,
                                           uint8_t* dst,
                                           uint32_t dst_stride) {
    if (ref == NULL || block == NULL || dst == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (block->w == 0 || block->h == 0) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (block->w > SVT_AV2_MAX_TX_SIZE || block->h > SVT_AV2_MAX_TX_SIZE) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    const uint32_t bd = ref->bit_depth == 0 ? 8u : ref->bit_depth;
    const uint32_t step = bd > 8 ? 2u : 1u;
    if (dst_stride < block->w * step) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    /* The reference must describe exactly this region; a mismatch would make the
     * prediction depend on samples belonging to another block. */
    if (ref->width != block->w || ref->height != block->h) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    int32_t dc = 0;
    if (have_left && have_above) {
        /* avg = Clip1( approx_divide(sum, w + h) ), w+h >= 2 so the divisor is
         * valid. The clip is required because approx_divide is inexact. */
        uint32_t sum = 0;
        for (uint32_t i = 0; i < block->h; ++i) {
            sum += ref->left[i];
        }
        for (uint32_t j = 0; j < block->w; ++j) {
            sum += ref->above[j];
        }
        dc = (int32_t)clip1((int32_t)approx_divide_u32(sum, block->w + block->h), bd);
    } else if (have_left) {
        /* leftAvg = Round2(sum_left, log2H). h is a power of two for every block
         * this encoder codes, and floor_log2 of that power is exactly log2H. */
        uint32_t sum = 0;
        for (uint32_t i = 0; i < block->h; ++i) {
            sum += ref->left[i];
        }
        dc = (int32_t)round2_u32(sum, floor_log2_u32(block->h));
    } else if (have_above) {
        uint32_t sum = 0;
        for (uint32_t j = 0; j < block->w; ++j) {
            sum += ref->above[j];
        }
        dc = (int32_t)round2_u32(sum, floor_log2_u32(block->w));
    } else {
        dc = 1 << (bd - 1); /* 1 << (BitDepth - 1) */
    }

    const uint16_t value = clip1(dc, bd);
    for (uint32_t i = 0; i < block->h; ++i) {
        for (uint32_t j = 0; j < block->w; ++j)
            svt_av2_sample_store(dst + (size_t)i * dst_stride + j * step, bd, value);
    }
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_intra_dc_predict(const SvtAv2IntraReference* ref,
                                     const SvtAv2IntraBlock* block,
                                     uint8_t* dst,
                                     uint32_t dst_stride) {
    if (block == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    /* Derive the flags the reference was built with. A substituted reference is
     * indistinguishable from a real one by content alone, so the caller that used
     * the _flags builder must use the _flags predictor; this overload exists for
     * the frame-boundary-only case used by the standalone tests. */
    const int have_left = (block->x > 0) ? 1 : 0;
    const int have_above = (block->y > 0) ? 1 : 0;
    return svt_av2_intra_dc_predict_flags(ref, block, have_left, have_above, dst, dst_stride);
}

/* M5 G1: scalar pixel projection only; no encoder mode or syntax changes. */
SvtAv2Error svt_av2_intra_axis_predict(const SvtAv2IntraReference* ref,
                                      const SvtAv2IntraBlock* block,
                                      SvtAv2IntraAxis axis,
                                      uint8_t* dst,
                                      uint32_t dst_stride) {
    if (ref == NULL || block == NULL || dst == NULL ||
        block->w == 0 || block->h == 0 ||
        block->w > SVT_AV2_MAX_TX_SIZE || block->h > SVT_AV2_MAX_TX_SIZE ||
        block->plane >= SVT_AV2_PICTURE_MAX_PLANES ||
        ref->width != block->w || ref->height != block->h ||
        (ref->bit_depth != 8u && ref->bit_depth != 10u) ||
        (axis != SVT_AV2_INTRA_AXIS_VERTICAL && axis != SVT_AV2_INTRA_AXIS_HORIZONTAL))
        return SVT_AV2_ERROR_INVALID_PARAMETER;

    const uint32_t step = ref->bit_depth > 8u ? 2u : 1u;
    if (dst_stride < block->w * step)
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    for (uint32_t y = 0; y < block->h; ++y)
        for (uint32_t x = 0; x < block->w; ++x) {
            const uint16_t prediction = axis == SVT_AV2_INTRA_AXIS_VERTICAL
                ? ref->above[x] : ref->left[y];
            svt_av2_sample_store(dst + (size_t)y * dst_stride + x * step,
                                ref->bit_depth, prediction);
        }
    return SVT_AV2_ERROR_NONE;
}

/* Normative signed Round2, including negative interpolation differences. */
static int32_t intra_round2(int32_t value, uint32_t bits) {
    const int32_t rounded = value + (int32_t)(1u << (bits - 1u));
    return rounded >= 0 ? (int32_t)((uint32_t)rounded >> bits) :
        -(int32_t)(((uint32_t)(-rounded) + (1u << bits) - 1u) >> bits);
}

SvtAv2Error svt_av2_intra_basic_predict(const SvtAv2FrameBuffer* recon,
    const SvtAv2IntraBlock* block, uint32_t mode, int top_right, int bottom_left,
    uint8_t* dst, uint32_t stride) {
    if (recon == NULL || block == NULL || dst == NULL || mode == 3u || mode > 7u ||
        (recon->bit_depth != 8u && recon->bit_depth != 10u) ||
        block->w < 4u || block->h < 4u ||
        (block->w & (block->w - 1u)) || (block->h & (block->h - 1u)))
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    const uint32_t step = recon->bit_depth > 8u ? 2u : 1u;
    if (stride < block->w * step) return SVT_AV2_ERROR_INVALID_PARAMETER;
    SvtAv2IntraReference ref;
    SvtAv2Error error = svt_av2_intra_reference_build(recon, block, &ref);
    if (error != SVT_AV2_ERROR_NONE) return error;
    if (mode == 0u) return svt_av2_intra_dc_predict(&ref, block, dst, stride);
    if (mode < 3u) {
        /* Single-edge predictors return base-1 (V) or base+1 (H) when
         * neither neighbour exists. DC's substitution is intentionally distinct. */
        if (!block->x && !block->y) {
            for (uint32_t i = 0u; i < block->w; ++i) ref.above[i]--;
            for (uint32_t i = 0u; i < block->h; ++i) ref.left[i]++;
        }
        return svt_av2_intra_axis_predict(&ref, block,
            mode == 1u ? SVT_AV2_INTRA_AXIS_VERTICAL : SVT_AV2_INTRA_AXIS_HORIZONTAL,
            dst, stride);
    }
    uint16_t corner = (uint16_t)(1u << (recon->bit_depth - 1u));
    if (block->x && block->y) {
        corner = svt_av2_sample_load(recon->planes[block->plane] +
            (size_t)(block->y - 1u) * recon->strides[block->plane] + (block->x - 1u) * step,
            recon->bit_depth);
    } else if (block->y) corner = ref.above[0];
    else if (block->x) corner = ref.left[0];
    else {
        for (uint32_t i = 0u; i < block->w; ++i) ref.above[i]--;
        for (uint32_t i = 0u; i < block->h; ++i) ref.left[i]++;
    }
    /* AV2 uses the highbd predictor for both 8- and 10-bit pictures. Unlike
     * the legacy 8-bit helper, smooth uses one extra TR/BL reference sample. */
    const uint32_t logw = floor_log2_u32(block->w), logh = floor_log2_u32(block->h);
    const uint32_t scale = (logw + logh - 4u + 2u) >> 2u;
    int32_t tr = ref.above[block->w - 1u], bl = ref.left[block->h - 1u];
    if (top_right && block->y && block->x + block->w < svt_av2_picture_plane_width(recon, block->plane) &&
        (block->plane == 0u || block->w <= 32u))
        tr = svt_av2_sample_load(recon->planes[block->plane] +
            (size_t)(block->y - 1u) * recon->strides[block->plane] + (block->x + block->w) * step, recon->bit_depth);
    if (bottom_left && block->x && block->y + block->h < svt_av2_picture_plane_height(recon, block->plane) &&
        (block->plane == 0u || block->h <= 32u))
        bl = svt_av2_sample_load(recon->planes[block->plane] +
            (size_t)(block->y + block->h) * recon->strides[block->plane] + (block->x - 1u) * step, recon->bit_depth);
    for (uint32_t y = 0u; y < block->h; ++y) {
        for (uint32_t x = 0u; x < block->w; ++x) {
            uint32_t value;
            if (mode == SVT_AV2_INTRA_PAETH) {
                const int left = ref.left[y], top = ref.above[x];
                const int base = left + top - corner;
                const int dl = base > left ? base - left : left - base;
                const int dt = base > top ? base - top : top - base;
                const int dc = base > corner ? base - corner : corner - base;
                value = (uint32_t)(dl <= dt && dl <= dc ? left : dt <= dc ? top : corner);
            } else {
                const uint32_t sy = SVT_AV2_MIN(6u, (y * 2u) >> scale);
                const uint32_t sx = SVT_AV2_MIN(6u, (x * 2u) >> scale);
                int32_t v = bl + intra_round2(((int32_t)ref.above[x] - bl) * (int32_t)(block->h - 1u - y), logh);
                int32_t h = tr + intra_round2(((int32_t)ref.left[y] - tr) * (int32_t)(block->w - 1u - x), logw);
                v += intra_round2(((int32_t)ref.above[x] - v) * (int32_t)(32u >> sy), 6u);
                h += intra_round2(((int32_t)ref.left[y] - h) * (int32_t)(32u >> sx), 6u);
                value = (uint32_t)(mode == SVT_AV2_INTRA_SMOOTH ? (v + h + 1) / 2 :
                    mode == SVT_AV2_INTRA_SMOOTH_V ? v : h);
            }
            svt_av2_sample_store(dst + (size_t)y * stride + x * step, recon->bit_depth, (uint16_t)value);
        }
    }
    return SVT_AV2_ERROR_NONE;
}

/* Fixed AVM v1 reconintra.h IDIF, 7-bit coefficients. Nonzero deltas use the
 * frozen derivative table below; edge filtering/MRLS/IBP stay disabled. */
static const int16_t intra_direction_filter[32][4] = {
  { 0, 128, 0, 0 },     { -2, 127, 4, -1 },   { -3, 125, 8, -2 },
  { -5, 123, 13, -3 },  { -6, 121, 17, -4 },  { -7, 118, 22, -5 },
  { -9, 116, 27, -6 },  { -9, 112, 32, -7 },  { -10, 109, 37, -8 },
  { -11, 106, 41, -8 }, { -11, 102, 46, -9 }, { -12, 98, 52, -10 },
  { -12, 94, 56, -10 }, { -12, 90, 61, -11 }, { -12, 85, 66, -11 },
  { -12, 81, 71, -12 }, { -12, 76, 76, -12 }, { -12, 71, 81, -12 },
  { -11, 66, 85, -12 }, { -11, 61, 90, -12 }, { -10, 56, 94, -12 },
  { -10, 52, 98, -12 }, { -9, 46, 102, -11 }, { -8, 41, 106, -11 },
  { -8, 37, 109, -10 }, { -7, 32, 112, -9 },  { -6, 27, 116, -9 },
  { -5, 22, 118, -7 },  { -4, 17, 121, -6 },  { -3, 13, 123, -5 },
  { -2, 8, 125, -3 },   { -1, 4, 127, -2 }
};

static const uint16_t intra_direction_derivative[90] = {
    0, 4096, 2048, 1365, 1024, 819, 682, 585, 512, 455,
    409, 409, 409, 372, 341, 292, 273, 256, 227, 215,
    204, 186, 178, 170, 157, 151, 146, 136, 132, 128,
    117, 110, 107, 99, 97, 97, 93, 87, 83, 81,
    77, 74, 73, 69, 66, 64, 62, 59, 56, 55,
    53, 50, 49, 47, 44, 42, 42, 41, 38, 37,
    35, 32, 31, 30, 28, 27, 26, 24, 23, 22,
    20, 19, 18, 16, 15, 14, 12, 11, 10, 10,
    10, 9, 8, 7, 6, 5, 4, 3, 2, 1,
};

SvtAv2Error svt_av2_intra_directional_predict(const SvtAv2FrameBuffer* recon,
    const SvtAv2IntraBlock* block, uint32_t mode, uint32_t top_right,
    uint32_t bottom_left, uint8_t* dst, uint32_t stride) {
    if (recon == NULL || block == NULL || dst == NULL || mode < 8u || mode > 61u ||
        block->plane > 2u ||
        (block->w != block->h && !((block->w == 4u && block->h == 8u) ||
            (block->w == 8u && block->h == 4u) || (block->w == 4u && block->h == 16u) ||
            (block->w == 16u && block->h == 4u) || (block->w == 4u && block->h == 32u) ||
            (block->w == 32u && block->h == 4u) || (block->w == 8u && block->h == 64u) ||
            (block->w == 64u && block->h == 8u) || (block->w >= 8u && block->h >= 8u &&
            block->w <= 4u * block->h && block->h <= 4u * block->w))) || block->w < 4u || block->h < 4u ||
        block->w > 64u || block->h > 64u || (block->h & (block->h - 1u)) ||
        (block->w & (block->w - 1u)) || (recon->bit_depth != 8u && recon->bit_depth != 10u))
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    SvtAv2IntraReference ref;
    SvtAv2Error error = svt_av2_intra_reference_build(recon, block, &ref);
    if (error != SVT_AV2_ERROR_NONE) return error;
    const uint32_t step = recon->bit_depth > 8u ? 2u : 1u;
    if (stride < block->w * step || top_right > block->h || bottom_left > block->w)
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    /* Fixed AVM limits each chroma edge independently to 64 reference samples. */
    if (block->plane != 0u && block->w > 32u) top_right = 0u;
    if (block->plane != 0u && block->h > 32u) bottom_left = 0u;
    /* The decoder caps TR by TX width and BL by TX height, even for rectangles. */
    top_right = SVT_AV2_MIN(top_right, block->w);
    bottom_left = SVT_AV2_MIN(bottom_left, block->h);
    uint16_t above_storage[132], left_storage[132];
    uint16_t* above = above_storage + 2;
    uint16_t* left = left_storage + 2;
    const uint32_t max = block->w + block->h - 1u;
    const uint32_t limit = (1u << recon->bit_depth) - 1u;
    const uint16_t base = (uint16_t)(1u << (recon->bit_depth - 1u));
    uint16_t corner = base;
    if (block->x && block->y)
        corner = svt_av2_sample_load(recon->planes[block->plane] + (size_t)(block->y - 1u) *
            recon->strides[block->plane] + (block->x - 1u) * step, recon->bit_depth);
    else if (block->y) corner = ref.above[0];
    else if (block->x) corner = ref.left[0];
    above[-2] = above[-1] = left[-2] = left[-1] = corner;
    for (uint32_t i = 0u; i <= max + 2u; ++i) {
        above[i] = ref.above[SVT_AV2_MIN(i, block->w - 1u)];
        left[i] = ref.left[SVT_AV2_MIN(i, block->h - 1u)];
        if (!block->x && !block->y) { above[i] = base - 1u; left[i] = base + 1u; }
    }
    if (block->y) {
        const uint32_t available = SVT_AV2_MIN(top_right,
            svt_av2_picture_plane_width(recon, block->plane) - block->x - block->w);
        for (uint32_t i = 0u; i < available; ++i)
            above[block->w + i] = svt_av2_sample_load(recon->planes[block->plane] +
                (size_t)(block->y - 1u) * recon->strides[block->plane] + (block->x + block->w + i) * step,
                recon->bit_depth);
        for (uint32_t i = block->w + available; i <= max + 2u; ++i) above[i] = above[i - 1u];
    }
    if (block->x) {
        const uint32_t available = SVT_AV2_MIN(bottom_left,
            svt_av2_picture_plane_height(recon, block->plane) - block->y - block->h);
        for (uint32_t i = 0u; i < available; ++i)
            left[block->h + i] = svt_av2_sample_load(recon->planes[block->plane] +
                (size_t)(block->y + block->h + i) * recon->strides[block->plane] + (block->x - 1u) * step,
                recon->bit_depth);
        for (uint32_t i = block->h + available; i <= max + 2u; ++i) left[i] = left[i - 1u];
    }
    /* Zone 2 extends each ordinary edge by one repeated sample, independently
     * of the additional decoded TR/BL samples used by zones 1 and 3. */
    int angle = svt_av2_intra_mode_angle(mode);
    /* Fixed v1 wide-angle mapping is implicit in rectangular TX geometry. */
    if ((block->h == 2u * block->w && angle < 61) ||
        (block->h == 4u * block->w && angle < 73) ||
        (block->h == 8u * block->w && angle < 82)) angle += 180;
    else if ((block->w == 2u * block->h && angle > 209) ||
        (block->w == 4u * block->h && angle > 197) ||
        (block->w == 8u * block->h && angle > 188)) angle -= 180;
    if (angle > 90 && angle < 180) {
        above[block->w] = above[block->w - 1u];
        left[block->h] = left[block->h - 1u];
    }
    const uint32_t dx = angle < 90 ? intra_direction_derivative[angle] :
        angle < 180 ? intra_direction_derivative[180 - angle] : 1u;
    const uint32_t dy = angle > 180 ? intra_direction_derivative[270 - angle] :
        angle > 90 ? intra_direction_derivative[angle - 90] : 1u;
    for (uint32_t y = 0u; y < block->h; ++y) {
        for (uint32_t x = 0u; x < block->w; ++x) {
            int32_t position;
            const uint16_t* edge;
            if (angle < 90) { position = (int32_t)(64u * x + (y + 1u) * dx); edge = above; }
            else if (angle > 180) { position = (int32_t)(64u * y + (x + 1u) * dy); edge = left; }
            else {
                position = (int32_t)(64u * x) - (int32_t)((y + 1u) * dx);
                edge = above;
                if (position < -64) {
                    position = (int32_t)(64u * y) - (int32_t)((x + 1u) * dy);
                    edge = left;
                }
            }
            const int32_t index = position >= 0 ? position / 64 : -((-position + 63) / 64);
            int32_t value;
            if (index > (int32_t)max) value = edge[max];
            else {
                const uint32_t phase = (uint32_t)(position - index * 64) / 2u;
                if (block->plane != 0u) {
                    /* A co-located V/H chroma mode inherits the Y angle but
                     * uses AVM's two-tap chroma interpolation, not IDIF. */
                    value = (int32_t)((32u - phase) * edge[index] + phase * edge[index + 1] + 16u) >> 5;
                } else {
                    value = 0;
                    for (int32_t tap = 0; tap < 4; ++tap)
                        value += intra_direction_filter[phase][tap] * (int32_t)edge[index + tap - 1];
                    value = intra_round2(value, 7u);
                }
                value = SVT_AV2_MAX(0, SVT_AV2_MIN(value, (int32_t)limit));
            }
            svt_av2_sample_store(dst + (size_t)y * stride + x * step, recon->bit_depth, (uint16_t)value);
        }
    }
    return SVT_AV2_ERROR_NONE;
}

/* AV2 CfL uses the neighbouring luma DC, not the current block mean. */
static int32_t cfl_derive_alpha(int32_t numerator, uint32_t denominator) {
    if (numerator == 0 || denominator == 0u) return 0;
    const uint32_t n = (uint32_t)(numerator < 0 ? -numerator : numerator);
    const uint32_t sn = floor_log2_u32(n), sd = floor_log2_u32(denominator);
    const uint32_t remainder = denominator - (1u << sd);
    const uint32_t fd = sd > SVT_AV2_DIV_LUT_BITS ?
        round2_u32(remainder, sd - SVT_AV2_DIV_LUT_BITS) : remainder << (SVT_AV2_DIV_LUT_BITS - sd);
    const uint32_t fn = sn > SVT_AV2_DIV_LUT_BITS ?
        round2_u32(n, sn - SVT_AV2_DIV_LUT_BITS) : n << (SVT_AV2_DIV_LUT_BITS - sn);
    const int32_t extra = (int32_t)sd - (int32_t)sn - 8;
    uint32_t value = 0u;
    if (extra <= 1) {
        const int32_t shift = SVT_AV2_DIV_LUT_PREC_BITS + SVT_AV2_DIV_LUT_BITS + extra;
        value = shift >= 0 ? ((uint32_t)svt_av2_div_lut[fd] * fn) >> (uint32_t)shift : 511u;
    }
    value = SVT_AV2_MIN(value, 511u);
    return numerator < 0 ? -(int32_t)value : (int32_t)value;
}

static uint32_t cfl_sample(const SvtAv2FrameBuffer* recon, uint32_t plane,
    uint32_t x, uint32_t y) {
    const uint32_t step = recon->bit_depth > 8u ? 2u : 1u;
    return svt_av2_sample_load(recon->planes[plane] + (size_t)y * recon->strides[plane] + x * step,
        recon->bit_depth);
}

static SvtAv2Error intra_cfl_predict(const SvtAv2FrameBuffer* recon,
    const SvtAv2IntraBlock* block, uint32_t sb_size, uint8_t* dst,
    uint32_t stride, int32_t* alpha_q8, int32_t explicit_alpha_q3, uint32_t explicit_alpha) {
    if (recon == NULL || block == NULL || dst == NULL || block->plane < 1u || block->plane > 2u ||
        (recon->bit_depth != 8u && recon->bit_depth != 10u) ||
        (recon->chroma_format != SVT_AV2_CHROMA_420 && recon->chroma_format != SVT_AV2_CHROMA_444) ||
        (block->w != block->h && !((block->w == 4u && block->h == 8u) ||
            (block->w == 8u && block->h == 4u) || (block->w == 4u && block->h == 16u) ||
            (block->w == 16u && block->h == 4u) || (block->w == 4u && block->h == 32u) ||
            (block->w == 32u && block->h == 4u) || (block->w == 8u && block->h == 64u) ||
            (block->w == 64u && block->h == 8u) || (block->w >= 8u && block->h >= 8u &&
            block->w <= 4u * block->h && block->h <= 4u * block->w))) ||
        block->w < 4u || block->w > 64u || block->h < 4u || block->h > 64u ||
        (block->w & (block->w - 1u)) != 0u || (block->h & (block->h - 1u)) != 0u || sb_size != 64u ||
        validate_block(block, svt_av2_picture_plane_width(recon, block->plane),
            svt_av2_picture_plane_height(recon, block->plane)) != SVT_AV2_ERROR_NONE)
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    const uint32_t step = recon->bit_depth > 8u ? 2u : 1u;
    if (stride < block->w * step) return SVT_AV2_ERROR_INVALID_PARAMETER;
    const uint32_t sub = recon->chroma_format == SVT_AV2_CHROMA_420 ? 1u : 0u;
    const uint32_t lx = block->x << sub, ly = block->y << sub;
    if ((block->w << sub) > sb_size || (block->h << sub) > sb_size)
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    uint16_t above_y[64] = {0}, left_y[64] = {0};
    uint32_t sum_l = 0u, sum_c = 0u, count = 0u;
    const uint32_t sample_step_x = block->w > 32u ? 2u : 1u;
    const uint32_t sample_step_y = block->h > 32u ? 2u : 1u;
    if (block->y) {
        const uint32_t top = ly - (sub && ly % sb_size ? 2u : 1u);
        const uint32_t bottom = ly - 1u;
        for (uint32_t x = 0u; x < block->w; ++x) {
            const uint32_t px = lx + (x << sub);
            above_y[x] = (uint16_t)(sub ?
                (cfl_sample(recon,0u,px,top) + cfl_sample(recon,0u,px+1u,top) +
                 cfl_sample(recon,0u,px,bottom) + cfl_sample(recon,0u,px+1u,bottom)) * 2u :
                cfl_sample(recon,0u,px,top) * 8u);
            if (x % sample_step_x == 0u) {
                sum_l += above_y[x]; sum_c += cfl_sample(recon,block->plane,block->x+x,block->y-1u); ++count;
            }
        }
    }
    if (block->x) {
        for (uint32_t y = 0u; y < block->h; ++y) {
            const uint32_t py = ly + (y << sub);
            left_y[y] = (uint16_t)(sub ?
                (cfl_sample(recon,0u,lx-2u,py) + cfl_sample(recon,0u,lx-1u,py) +
                 cfl_sample(recon,0u,lx-2u,py+1u) + cfl_sample(recon,0u,lx-1u,py+1u)) * 2u :
                cfl_sample(recon,0u,lx-1u,py) * 8u);
            if (y % sample_step_y == 0u) {
                sum_l += left_y[y]; sum_c += cfl_sample(recon,block->plane,block->x-1u,block->y+y); ++count;
            }
        }
    }
    const uint32_t avg_l = count ? SVT_AV2_MIN(approx_divide_u32(sum_l,count), (1u << (recon->bit_depth+3u))-1u) :
        1u << (recon->bit_depth+2u);
    const uint32_t dc = count ? SVT_AV2_MIN(approx_divide_u32(sum_c,count), (1u << recon->bit_depth)-1u) :
        1u << (recon->bit_depth-1u);
    int32_t sx = 0, sy = 0, sxx = 0, sxy = 0, samples = 0;
    const uint32_t both_edges = block->x && block->y;
    const uint32_t refs = both_edges && block->w <= 2u * block->h && block->h <= 2u * block->w ? 4u : 8u;
    for (uint32_t edge = 0u; edge < 2u; ++edge) {
        if (!(edge ? block->x : block->y)) continue;
        if (both_edges && (edge ? block->w > 2u * block->h : block->h > 2u * block->w)) continue;
        const uint32_t length = edge ? block->h : block->w;
        const uint32_t n = SVT_AV2_MIN(refs, length), spacing = length / n;
        const uint32_t start = spacing == 1u ? 0u : spacing / 2u;
        for (uint32_t i = start; i < length; i += spacing) {
            const int32_t a = (int32_t)((edge ? left_y[i] : above_y[i]) >> 3u);
            const int32_t c = (int32_t)(edge ? cfl_sample(recon,block->plane,block->x-1u,block->y+i) :
                cfl_sample(recon,block->plane,block->x+i,block->y-1u));
            sx += a; sy += c; sxx += a*a; sxy += a*c; ++samples;
        }
    }
    int32_t alpha = 0;
    if (samples && !explicit_alpha) alpha = cfl_derive_alpha(sxy-(int32_t)((int64_t)sx*sy/samples),
        (uint32_t)(sxx-(int32_t)((int64_t)sx*sx/samples)));
    if (explicit_alpha) alpha = explicit_alpha_q3 * 32;
    for (uint32_t y = 0u; y < block->h; ++y) {
        for (uint32_t x = 0u; x < block->w; ++x) {
            const uint32_t px = lx + (x << sub), py = ly + (y << sub);
            const uint32_t luma = sub ?
                (cfl_sample(recon,0u,px,py) + cfl_sample(recon,0u,px+1u,py) +
                 cfl_sample(recon,0u,px,py+1u) + cfl_sample(recon,0u,px+1u,py+1u)) * 2u :
                cfl_sample(recon,0u,px,py) * 8u;
            const int32_t scaled = alpha * ((int32_t)luma - (int32_t)avg_l);
            const int32_t delta = scaled < 0 ? -(int32_t)round2_u32((uint32_t)-scaled,11u) :
                (int32_t)round2_u32((uint32_t)scaled,11u);
            svt_av2_sample_store(dst+(size_t)y*stride+x*step,recon->bit_depth,clip1((int32_t)dc+delta,recon->bit_depth));
        }
    }
    if (alpha_q8) *alpha_q8 = alpha;
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_intra_cfl_implicit_predict(const SvtAv2FrameBuffer* recon,
    const SvtAv2IntraBlock* block, uint32_t sb_size, uint8_t* dst,
    uint32_t stride, int32_t* alpha_q8) {
    return intra_cfl_predict(recon, block, sb_size, dst, stride, alpha_q8, 0, 0u);
}

SvtAv2Error svt_av2_intra_cfl_explicit_predict(const SvtAv2FrameBuffer* recon,
    const SvtAv2IntraBlock* block, uint32_t sb_size, uint8_t* dst,
    uint32_t stride, int32_t alpha_q3) {
    if (alpha_q3 < -8 || alpha_q3 > 8) return SVT_AV2_ERROR_INVALID_PARAMETER;
    return intra_cfl_predict(recon, block, sb_size, dst, stride, NULL, alpha_q3, 1u);
}

/* --- residual ------------------------------------------------------------------ */

SvtAv2Error svt_av2_residual_compute(const SvtAv2FrameBuffer* source,
                                     const SvtAv2FrameBuffer* prediction,
                                     const SvtAv2IntraBlock* block,
                                     int16_t* residual,
                                     uint32_t residual_stride) {
    if (source == NULL || prediction == NULL || residual == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (block == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (block->plane >= SVT_AV2_PICTURE_MAX_PLANES) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    const uint32_t plane_width = svt_av2_picture_plane_width(source, block->plane);
    const uint32_t plane_height = svt_av2_picture_plane_height(source, block->plane);
    const SvtAv2Error err = validate_block(block, plane_width, plane_height);
    if (err != SVT_AV2_ERROR_NONE) {
        return err;
    }
    /* Both images must agree on geometry: a mismatch means one of them describes a
     * different frame, and the residual would be meaningless. */
    if (svt_av2_picture_plane_width(prediction, block->plane) != plane_width ||
        svt_av2_picture_plane_height(prediction, block->plane) != plane_height) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    /* The residual buffer is block-local (see the header): it is checked against
     * the block width, not the plane width, because it only ever holds this
     * block. */
    if (residual_stride < block->w) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    const uint8_t* const src = svt_av2_picture_plane_const(source, block->plane);
    const uint8_t* const pred = svt_av2_picture_plane_const(prediction, block->plane);
    if (src == NULL || pred == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (source->bit_depth != prediction->bit_depth || source->chroma_format != prediction->chroma_format) return SVT_AV2_ERROR_INVALID_PARAMETER;
    const uint32_t step = source->bit_depth > 8 ? 2u : 1u;
    const uint32_t src_stride = source->strides[block->plane];
    const uint32_t pred_stride = prediction->strides[block->plane];

    for (uint32_t i = 0; i < block->h; ++i) {
        const uint8_t* const src_row = src + (size_t)(block->y + i) * src_stride + block->x * step;
        const uint8_t* const pred_row = pred + (size_t)(block->y + i) * pred_stride + block->x * step;
        /* Row 0 of the output is the block's first row: the plane coordinate
         * block->y is deliberately not part of this address. */
        int16_t* const out_row = residual + (size_t)i * residual_stride;
        for (uint32_t j = 0; j < block->w; ++j) {
            /* Signed 32-bit arithmetic before narrowing: the result is in
             * -255..+255 so int16_t always holds it exactly (contract 2.3). */
            out_row[j] = (int16_t)((int32_t)svt_av2_sample_load(src_row + j * step, source->bit_depth) - (int32_t)svt_av2_sample_load(pred_row + j * step, source->bit_depth));
        }
    }
    return SVT_AV2_ERROR_NONE;
}

/* --- introspection -------------------------------------------------------------- */

const char* svt_av2_intra_mode_name(SvtAv2IntraMode mode) {
    switch (mode) {
    case SVT_AV2_INTRA_DC_PRED:
        return "DC_PRED";
    default:
        return "INVALID";
    }
}

/* Fixed AVM v1 Y MRLS. Ordinary line zero keeps the frozen predictor path. */
static void mrl_idif(uint16_t* out, uint32_t w, uint32_t h, const uint16_t* above,
    const uint16_t* left, int angle, uint32_t mrl, uint32_t limit) {
    const uint32_t max = w + h - 1u + 2u * mrl;
    const uint32_t dx = angle < 90 ? intra_direction_derivative[angle] :
        angle > 90 && angle < 180 ? intra_direction_derivative[180 - angle] : 1u;
    const uint32_t dy = angle > 180 ? intra_direction_derivative[270 - angle] :
        angle > 90 && angle < 180 ? intra_direction_derivative[angle - 90] : 1u;
    for (uint32_t y = 0u; y < h; ++y) for (uint32_t x = 0u; x < w; ++x) {
        if (angle == 90 || angle == 180) {
            out[y*w+x] = angle == 90 ? above[x] : left[y];
            continue;
        }
        int32_t position;
        const uint16_t* edge;
        if (angle < 90) { position = (int32_t)(64u*x+(y+1u+mrl)*dx); edge=above; }
        else if (angle > 180) { position=(int32_t)(64u*y+(x+1u+mrl)*dy); edge=left; }
        else {
            position=(int32_t)(64u*x)-(int32_t)((y+1u+mrl)*dx); edge=above;
            if (position < -(int32_t)(64u*(1u+mrl))) {
                position=(int32_t)(64u*y)-(int32_t)((x+1u+mrl)*dy); edge=left;
            }
        }
        const int32_t index=position>=0 ? position/64 : -((-position+63)/64);
        int32_t value;
        if (index>(int32_t)max) value=edge[max];
        else {
            const uint32_t phase=(uint32_t)(position-index*64)/2u;
            value=0;
            for (int32_t tap=0; tap<4; ++tap)
                value+=intra_direction_filter[phase][tap]*(int32_t)edge[index+tap-1];
            value=intra_round2(value,7u);
            value=SVT_AV2_MAX(0,SVT_AV2_MIN(value,(int32_t)limit));
        }
        out[y*w+x]=(uint16_t)value;
    }
}

SvtAv2Error svt_av2_intra_mrls_predict(const SvtAv2FrameBuffer* recon,
    const SvtAv2IntraBlock* block, uint32_t mode, uint32_t selection,
    uint32_t top_right, uint32_t bottom_left, uint8_t* dst, uint32_t stride) {
    const uint32_t mrl=selection&3u, multi=selection>>2u;
    if (recon==NULL || block==NULL || dst==NULL || mode>61u ||
        svt_av2_intra_mode_joint(mode)<5u || selection>7u || mrl==0u ||
        block->plane!=0u || (block->x&3u) || (block->y&3u) ||
        (block->w!=block->h && block->w!=2u*block->h && block->h!=2u*block->w &&
            !((block->w==16u && block->h==64u) || (block->w==64u && block->h==16u))) ||
        block->w<8u || block->w>64u ||
        block->h<8u || block->h>64u || (block->w&(block->w-1u)) ||
        (block->h&(block->h-1u)) || top_right>block->w || bottom_left>block->h ||
        (recon->bit_depth!=8u && recon->bit_depth!=10u) ||
        validate_block(block,recon->visible_width,recon->visible_height)!=SVT_AV2_ERROR_NONE)
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    const uint32_t step=recon->bit_depth>8u ? 2u : 1u;
    if (stride<block->w*step) return SVT_AV2_ERROR_INVALID_PARAMETER;
    static const int offsets[4]={0,1,-1,0};
    int angle=svt_av2_intra_mode_angle(mode)+offsets[mrl];
    /* Apply the MRL angle offset before fixed-v1 rectangular wide mapping. */
    if ((block->h==2u*block->w && angle<61) ||
        (block->h==4u*block->w && angle<73)) angle+=180;
    else if ((block->w==2u*block->h && angle>209) ||
        (block->w==4u*block->h && angle>197)) angle-=180;
    const int need_top=angle<=180 && angle!=180;
    const int need_left=angle>=90 && angle!=90;
    const int sb_top=block->y%64u==0u;
    const uint32_t top_y=block->y ? block->y-(sb_top?1u:mrl+1u) : 0u;
    const uint32_t left_x=block->x ? block->x-mrl-1u : 0u;
    const uint16_t base=(uint16_t)(1u<<(recon->bit_depth-1u));
    uint16_t edges[2][2][144], predictions[2][4096];
    for (uint32_t line=0u; line<=multi; ++line) {
        uint16_t* above=edges[line][0]+6, *left=edges[line][1]+6;
        const uint32_t ty=line ? (block->y ? block->y-1u : 0u) : top_y;
        const uint32_t lx=line ? (block->x ? block->x-1u : 0u) : left_x;
        const uint32_t tr=angle<90 ? SVT_AV2_MIN(top_right,recon->visible_width-block->x-block->w) : 0u;
        const uint32_t bl=angle>180 ? SVT_AV2_MIN(bottom_left,recon->visible_height-block->y-block->h) : 0u;
        for (uint32_t i=0u; i<144u; ++i) {
            above[(int32_t)i-6]=base-1u;
            left[(int32_t)i-6]=base+1u;
        }
        if (block->y) {
            for (uint32_t i=0u; i<block->w+tr; ++i)
                above[i]=svt_av2_sample_load(recon->planes[0]+(size_t)ty*recon->strides[0]+(block->x+i)*step,recon->bit_depth);
            for (uint32_t i=block->w+tr; i<138u; ++i) above[i]=above[i-1u];
        }
        if (block->x) {
            for (uint32_t i=0u; i<block->h+bl; ++i)
                left[i]=svt_av2_sample_load(recon->planes[0]+(size_t)(block->y+i)*recon->strides[0]+lx*step,recon->bit_depth);
            for (uint32_t i=block->h+bl; i<138u; ++i) left[i]=left[i-1u];
        }
        if (!block->y && block->x)
            for (uint32_t i=0u;i<138u;++i) above[i]=left[0];
        if (!block->x && block->y)
            for (uint32_t i=0u;i<138u;++i) left[i]=above[0];
        for (uint32_t i=1u;i<=mrl+1u;++i) {
            if (block->x && block->y) {
                above[-(int32_t)i]=svt_av2_sample_load(recon->planes[0]+(size_t)ty*recon->strides[0]+(block->x-i)*step,recon->bit_depth);
                left[-(int32_t)i]=svt_av2_sample_load(recon->planes[0]+(size_t)(block->y-(sb_top?1u:i))*recon->strides[0]+lx*step,recon->bit_depth);
            } else above[-(int32_t)i]=left[-(int32_t)i]=block->y ? above[0] : block->x ? left[0] : base;
        }
        const uint32_t line_mrl=line?0u:mrl;
        /* Rectangular availability can extend past the projection endpoint.
         * Fixed AVM repeats that endpoint for both 4-tap overhang samples. */
        const uint32_t endpoint=block->w+block->h-1u+2u*line_mrl;
        if (angle<90) above[endpoint+1u]=above[endpoint+2u]=above[endpoint];
        else if (angle>180) left[endpoint+1u]=left[endpoint+2u]=left[endpoint];
        if (angle>90 && angle<180) {
            above[-(int32_t)line_mrl-2]=above[-(int32_t)line_mrl-1];
            left[-(int32_t)line_mrl-2]=left[-(int32_t)line_mrl-1];
            if (line_mrl==0u) { above[block->w]=above[block->w-1u]; left[block->h]=left[block->h-1u]; }
        }
        mrl_idif(predictions[line],block->w,block->h,above,left,angle,line_mrl,(1u<<recon->bit_depth)-1u);
        if (!multi && ((!need_top && !block->x) || (!need_left && !block->y))) {
            const uint16_t value=need_left ? (block->y?above[0]:base+1u) : (block->x?left[0]:base-1u);
            for (uint32_t i=0u;i<block->w*block->h;++i) predictions[line][i]=value;
        }
    }
    for (uint32_t y=0u;y<block->h;++y) for (uint32_t x=0u;x<block->w;++x) {
        const uint32_t i=y*block->w+x;
        const uint16_t value=multi ? (uint16_t)((predictions[0][i]+predictions[1][i]+1u)/2u) : predictions[0][i];
        svt_av2_sample_store(dst+(size_t)y*stride+x*step,recon->bit_depth,value);
    }
    return SVT_AV2_ERROR_NONE;
}
