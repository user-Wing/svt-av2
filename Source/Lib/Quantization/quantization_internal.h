#ifndef SVT_AV2_QUANTIZATION_INTERNAL_H
#define SVT_AV2_QUANTIZATION_INTERNAL_H
#include "quantization.h"
typedef SvtAv2QuantStatus (*SvtAv2QuantizeBlockFn)(const SvtAv2QuantParams*, const int32_t*,
    int32_t*, int32_t*, size_t, int, int);
typedef void (*SvtAv2QuantKernel)(const SvtAv2QuantCoeffs*, int32_t, int32_t,
    const int32_t*, int32_t*, int32_t*, size_t);
SvtAv2QuantizeBlockFn svt_av2_quantize_block_select(void);
#if defined(SVT_AV2_HAVE_QUANT_AVX2)
void svt_av2_quantize_avx2(const SvtAv2QuantCoeffs*, int32_t, int32_t,
    const int32_t*, int32_t*, int32_t*, size_t);
#endif
#endif
