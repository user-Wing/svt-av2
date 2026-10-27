/* SVT-AV2 internal reconstruction (M2, T012).
 *
 * See reconstruction.h for the frozen contract. Everything here is 8-bit and
 * independent of T009 (transform) and T010 (quantisation): this module consumes
 * an inverse residual it did not compute.
 */

#include "reconstruction.h"

#include <stdlib.h>
#include <string.h>

/* --- scratch pool ---------------------------------------------------------- */

static uint32_t align_up_u32(uint32_t value, uint32_t align) {
    return (value + (align - 1u)) & ~(align - 1u);
}

SvtAv2Error svt_av2_recon_pool_init(SvtAv2ReconScratch* scratch,
                                    uint32_t max_block_width,
                                    uint32_t max_block_height) {
    if (scratch == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    memset(scratch, 0, sizeof(*scratch));

    if (max_block_width == 0 || max_block_height == 0) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    const uint32_t stride = align_up_u32(max_block_width, SVT_AV2_RECON_SCRATCH_STRIDE_ALIGN);
    /* Check before multiplying: a wrapped size would become an undersized
     * allocation (T007 contract 1.4). */
    if (stride != 0 && max_block_height > (SIZE_MAX / stride)) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    const size_t capacity = (size_t)stride * (size_t)max_block_height;

    /* The coefficient buffers are NOT sample grids: contract section 3 fixes
     * their row stride at min(w,32) and their length at min(w,32)*min(h,32)
     * (TX_64X64 -> 1024, not 4096). Sizing them like the sample buffers would
     * both contradict the contract and invite a future writer to treat them as
     * a full (block_w x block_h) raster again, which is a silent mis-index for
     * T011's scan. */
    const uint32_t coeff_w = (max_block_width > 32u) ? 32u : max_block_width;
    const uint32_t coeff_h = (max_block_height > 32u) ? 32u : max_block_height;
    if (coeff_h != 0 && coeff_w > (SIZE_MAX / coeff_h)) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    const size_t coeff_capacity = (size_t)coeff_w * (size_t)coeff_h;

    /* The prediction plane is bytes; the other buffers are 2 or 4 bytes per
     * sample. Check the widest element size for overflow before allocating. */
    if (capacity > SIZE_MAX / sizeof(int32_t)) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    uint8_t* prediction = (uint8_t*)calloc(capacity, 1);
    int16_t* residual = (int16_t*)calloc(capacity, sizeof(int16_t));
    int16_t* inverse_residual = (int16_t*)calloc(capacity, sizeof(int16_t));
    int32_t* coeff = (int32_t*)calloc(coeff_capacity, sizeof(int32_t));
    int32_t* qcoeff = (int32_t*)calloc(coeff_capacity, sizeof(int32_t));
    int32_t* dqcoeff = (int32_t*)calloc(coeff_capacity, sizeof(int32_t));

    if (prediction == NULL || residual == NULL || inverse_residual == NULL || coeff == NULL ||
        qcoeff == NULL || dqcoeff == NULL) {
        free(prediction);
        free(residual);
        free(inverse_residual);
        free(coeff);
        free(qcoeff);
        free(dqcoeff);
        memset(scratch, 0, sizeof(*scratch));
        return SVT_AV2_ERROR_OUT_OF_MEMORY;
    }

    scratch->prediction = prediction;
    scratch->residual = residual;
    scratch->inverse_residual = inverse_residual;
    scratch->coeff = coeff;
    scratch->qcoeff = qcoeff;
    scratch->dqcoeff = dqcoeff;
    scratch->max_block_width = max_block_width;
    scratch->max_block_height = max_block_height;
    scratch->stride = stride;
    scratch->capacity = capacity;
    scratch->coeff_capacity = coeff_capacity;
    scratch->allocated = 1;
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_recon_pool_free(SvtAv2ReconScratch* scratch) {
    if (scratch == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    free(scratch->prediction);
    free(scratch->residual);
    free(scratch->inverse_residual);
    free(scratch->coeff);
    free(scratch->qcoeff);
    free(scratch->dqcoeff);
    memset(scratch, 0, sizeof(*scratch));
    return SVT_AV2_ERROR_NONE;
}

int svt_av2_recon_pool_fits(const SvtAv2ReconScratch* scratch, uint32_t block_w, uint32_t block_h) {
    if (scratch == NULL || !scratch->allocated) {
        return 0;
    }
    return (block_w != 0 && block_h != 0 && block_w <= scratch->max_block_width &&
            block_h <= scratch->max_block_height)
               ? 1
               : 0;
}

/* --- clipping -------------------------------------------------------------- */

uint8_t svt_av2_recon_clip_sample(int32_t value) {
    if (value < SVT_AV2_RECON_MIN_SAMPLE) {
        return (uint8_t)SVT_AV2_RECON_MIN_SAMPLE;
    }
    if (value > SVT_AV2_RECON_MAX_SAMPLE) {
        return (uint8_t)SVT_AV2_RECON_MAX_SAMPLE;
    }
    return (uint8_t)value;
}

/* --- the final chain step -------------------------------------------------- */

SvtAv2Error svt_av2_recon_write_block(SvtAv2FrameBuffer* recon,
                                      uint32_t plane,
                                      const SvtAv2BlockView* block,
                                      const uint8_t* prediction,
                                      uint32_t prediction_stride,
                                      const int16_t* inverse_residual,
                                      uint32_t inverse_residual_stride) {
    if (recon == NULL || block == NULL || prediction == NULL || inverse_residual == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (plane >= SVT_AV2_PICTURE_MAX_PLANES) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (block->w == 0 || block->h == 0) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    const uint32_t step = recon->bit_depth > 8 ? 2u : 1u;
    if (prediction_stride < block->w * step || inverse_residual_stride < block->w) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    uint8_t* const plane_base = svt_av2_picture_plane(recon, plane);
    if (plane_base == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    const uint32_t plane_width = svt_av2_picture_plane_width(recon, plane);
    const uint32_t plane_height = svt_av2_picture_plane_height(recon, plane);
    /* Validate the whole region before writing a single sample so a failure
     * cannot leave a partially reconstructed block behind. */
    if (block->x >= plane_width || block->y >= plane_height || block->w > plane_width - block->x ||
        block->h > plane_height - block->y) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    const uint32_t dst_stride = recon->strides[plane];
    for (uint32_t i = 0; i < block->h; ++i) {
        const uint8_t* const pred_row = prediction + (size_t)i * prediction_stride;
        const int16_t* const res_row = inverse_residual + (size_t)i * inverse_residual_stride;
        uint8_t* const dst_row = plane_base + (size_t)(block->y + i) * dst_stride + block->x * step;
        for (uint32_t j = 0; j < block->w; ++j) {
            int32_t value = (int32_t)svt_av2_sample_load(pred_row + j * step, recon->bit_depth) + res_row[j];
            const int32_t maximum = (int32_t)((1u << recon->bit_depth) - 1u);
            if (value < 0) value = 0;
            if (value > maximum) value = maximum;
            svt_av2_sample_store(dst_row + j * step, recon->bit_depth, (uint16_t)value);
        }
    }
    return SVT_AV2_ERROR_NONE;
}

/* --- per-frame lifecycle --------------------------------------------------- */

SvtAv2Error svt_av2_recon_reset_frame(SvtAv2FrameBuffer* recon, uint16_t fill_value) {
    if (recon == NULL || recon->buffer == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    for (uint32_t plane = 0; plane < SVT_AV2_PICTURE_MAX_PLANES; ++plane) {
        const uint32_t stride = recon->strides[plane];
        const uint32_t aligned_height = recon->aligned_height[plane];
        uint8_t* const base = recon->planes[plane];
        if (base == NULL) {
            return SVT_AV2_ERROR_INVALID_PARAMETER;
        }
        /* Fill visible area and padding alike: the point is that no sample of
         * this frame's allocation carries anything from the previous frame. */
        for (uint32_t row = 0; row < aligned_height; ++row) {
            const uint32_t step = recon->bit_depth > 8 ? 2u : 1u;
            for (uint32_t x = 0; x < stride; x += step)
                svt_av2_sample_store(base + (size_t)row * stride + x, recon->bit_depth, fill_value);
        }
    }
    return SVT_AV2_ERROR_NONE;
}

/* --- export --------------------------------------------------------------- */

SvtAv2Error svt_av2_recon_export_frame(const SvtAv2FrameBuffer* recon,
                                       uint32_t frame_index,
                                       uint8_t* out,
                                       size_t out_capacity,
                                       SvtAv2ReconExport* meta) {
    if (recon == NULL || out == NULL || meta == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (recon->visible_width == 0 || recon->visible_height == 0) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    size_t required = 0;
    const SvtAv2Error size_error = svt_av2_picture_raw_size(recon, &required);
    if (size_error != SVT_AV2_ERROR_NONE) {
        return size_error;
    }
    if (out_capacity < required) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    /* T007's primitive is the only raw packer in the project: this call removes
     * the memory padding and writes Y, U, V tightly packed. */
    size_t written = 0;
    const SvtAv2Error export_error = svt_av2_picture_export_raw(recon, out, out_capacity, &written);
    if (export_error != SVT_AV2_ERROR_NONE) {
        return export_error;
    }

    meta->bit_depth = recon->bit_depth;
    meta->chroma_format = recon->chroma_format;
    meta->frame_index = frame_index;
    meta->visible_width = recon->visible_width;
    meta->visible_height = recon->visible_height;
    meta->chroma_width = svt_av2_picture_plane_width(recon, 1);
    meta->chroma_height = svt_av2_picture_plane_height(recon, 1);
    meta->byte_count = written;
    return SVT_AV2_ERROR_NONE;
}
