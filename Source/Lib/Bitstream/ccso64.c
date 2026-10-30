#include "ccso64.h"
#include "entropy_models.h"
#include <string.h>

int svt_av2_ccso64_validate(const SvtAv2Ccso64Plan* plan,
    uint32_t width, uint32_t height, uint32_t bit_depth, uint32_t chroma_format) {
    if (plan == NULL) return 0;
    if (width == 0u || width > 4096u || height == 0u || height > 2160u ||
        (width & 7u) || (height & 7u) || chroma_format != 1u ||
        (bit_depth != 8u && bit_depth != 10u)) return SVT_AV2_BW_ERROR_RANGE;
    const size_t units = (size_t)((width + 63u) / 64u) * ((height + 63u) / 64u);
    if (plan->unit_flags ? plan->unit_count != units :
        units != 1u || plan->unit_count != 0u) return SVT_AV2_BW_ERROR_RANGE;
    for (uint32_t p = 0u; p < 2u; ++p) {
        if (plan->enabled[p] > 1u || plan->block_flag[p] > plan->enabled[p])
            return SVT_AV2_BW_ERROR_RANGE;
        for (uint32_t b = 0u; b < 4u; ++b)
            if (plan->offset[p][b] < -1 || plan->offset[p][b] > 1)
                return SVT_AV2_BW_ERROR_RANGE;
    }
    if (plan->unit_flags) for (size_t i = 0u; i < units; ++i)
        for (uint32_t p = 0u; p < 2u; ++p)
            if (plan->unit_flags[i][p] > plan->enabled[p])
                return SVT_AV2_BW_ERROR_RANGE;
    return 0;
}

int svt_av2_ccso64_picture_validate(const SvtAv2Ccso64Plan* plan,
    const SvtAv2FrameBuffer* picture) {
    if (plan == NULL) return 0;
    if (picture == NULL || picture->buffer == NULL || picture->plane_count != 3u)
        return SVT_AV2_BW_ERROR_NULL;
    int st = svt_av2_ccso64_validate(plan, picture->visible_width,
        picture->visible_height, picture->bit_depth, (uint32_t)picture->chroma_format);
    if (st != 0) return st;
    const uint32_t bytes = picture->bit_depth == 10u ? 2u : 1u;
    for (uint32_t p = 0u; p < 3u; ++p) {
        const uint32_t width = picture->visible_width >> (p != 0u);
        const uint32_t height = picture->visible_height >> (p != 0u);
        if (picture->planes[p] == NULL || picture->strides[p] < width * bytes ||
            picture->aligned_height[p] < height) return SVT_AV2_BW_ERROR_RANGE;
    }
    return 0;
}

void svt_av2_ccso64_apply(const SvtAv2Ccso64Plan* plan, SvtAv2FrameBuffer* picture) {
    if (plan == NULL) return;
    const uint32_t bytes = picture->bit_depth == 10u ? 2u : 1u;
    const int maximum = (1 << picture->bit_depth) - 1;
    const uint32_t cols = (picture->visible_width + 63u) / 64u;
    for (uint32_t p = 0u; p < 2u; ++p) {
        if (!plan->enabled[p]) continue;
        for (uint32_t y = 0u; y < picture->visible_height / 2u; ++y)
            for (uint32_t x = 0u; x < picture->visible_width / 2u; ++x) {
            const size_t unit = (size_t)(y / 32u) * cols + x / 32u;
            const uint8_t* flags = plan->unit_flags ? plan->unit_flags[unit] : plan->block_flag;
            if (!flags[p]) continue;
            const uint8_t* luma = picture->planes[0] + (size_t)(2u*y) * picture->strides[0] + 2u*x*bytes;
            uint8_t* dst = picture->planes[p+1u] + (size_t)y * picture->strides[p+1u] + x*bytes;
            const uint32_t value = luma[0] | (bytes == 2u ? (uint32_t)luma[1] << 8u : 0u);
            const uint32_t band = value >> (picture->bit_depth - 2u);
            int filtered = (int)(dst[0] | (bytes == 2u ? (uint32_t)dst[1] << 8u : 0u)) + plan->offset[p][band];
            if (filtered < 0) filtered = 0;
            if (filtered > maximum) filtered = maximum;
            dst[0] = (uint8_t)filtered;
            if (bytes == 2u) dst[1] = (uint8_t)((uint32_t)filtered >> 8u);
        }
    }
}

void svt_av2_ccso64_cdf_init(uint16_t cdf[2][4][6]) {
    static const uint16_t initial[2][4][6] = {
        {{9298,0,0,1,1,2},{16384,0,0,2,3,4},{26102,0,0,1,1,2},{16384,0,0,2,3,4}},
        {{9854,0,0,1,1,2},{16384,0,0,2,3,4},{25775,0,0,1,1,2},{16384,0,0,2,3,4}}
    };
    memcpy(cdf,initial,sizeof(initial));
}
uint32_t svt_av2_ccso64_context(const SvtAv2Ccso64Plan* plan,
    size_t unit,uint32_t cols,uint32_t plane) {
    return unit%cols && plan->unit_flags && plan->unit_flags[unit-1u][plane]?2u:0u;
}
int svt_av2_ccso64_symbol(SvtAv2EntropyEncoder* enc,uint16_t state[6],
    uint32_t flag,int adaptive) {
    const uint16_t forward=(uint16_t)(32768u-state[0]);
    if (!adaptive) return svt_av2_entropy_encode_symbol(enc,&forward,2u,flag);
    if (enc->rate_only) {
        uint32_t probability,cost;
        if (enc->error) return SVT_AV2_ENTROPY_ERROR_NO_SPACE;
        int st=svt_av2_entropy_adjusted_probability(&forward,2u,flag,&probability);
        if (st!=0) return st;
        st=svt_av2_entropy_probability_cost_q12(probability,&cost);
        if (st!=0) return st;
        st=svt_av2_entropy_model_update(state,2u,flag);
        if (st!=0) return st;
        enc->rate_q12+=cost;++enc->rate_symbols;
        return 0;
    }
    const int st=svt_av2_entropy_encode_symbol(enc,&forward,2u,flag);
    return st!=0?st:svt_av2_entropy_model_update(state,2u,flag);
}
