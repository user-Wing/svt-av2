#include "quantization_internal.h"
#include <immintrin.h>
void svt_av2_quantize_avx2(const SvtAv2QuantCoeffs* coeffs, int32_t clip_min, int32_t clip_max,
    const int32_t* coeff, int32_t* qcoeff, int32_t* dqcoeff, size_t count) {
    const __m256i round = _mm256_set1_epi32(svt_av2_round_power_of_two(coeffs->round_QTX[0], coeffs->log_scale));
    const __m256i quant = _mm256_set1_epi32(coeffs->quant_fp_QTX[0]);
    const __m256i dequant = _mm256_set1_epi32(coeffs->dequant_QTX[0]);
    const __m256i signbit = _mm256_set1_epi32(INT32_MIN);
    const __m256i threshold = _mm256_xor_si256(_mm256_set1_epi32(coeffs->deadzone_QTX[0] - 1), signbit);
    const __m256i mask24 = _mm256_set1_epi32(0xffffff), four = _mm256_set1_epi32(4);
    const __m256i low = _mm256_set1_epi32(clip_min);
    const __m256i high = _mm256_set1_epi32(clip_max);
    const __m128i shift = _mm_cvtsi32_si128(coeffs->shift), txshift = _mm_cvtsi32_si128(coeffs->log_scale);
    for (size_t i = 0u; i < count; i += 8u) {
        const __m256i input = _mm256_loadu_si256((const __m256i*)(const void*)(coeff + i));
        const __m256i magnitude = _mm256_abs_epi32(input);
        const __m256i tmp = _mm256_add_epi32(magnitude, round);
        const __m256i even = _mm256_srl_epi64(_mm256_mul_epu32(tmp, quant), shift);
        const __m256i odd = _mm256_srl_epi64(_mm256_mul_epu32(_mm256_srli_epi64(tmp, 32), quant), shift);
        __m256i level = _mm256_or_si256(even, _mm256_slli_epi64(odd, 32));
        level = _mm256_and_si256(level, _mm256_cmpgt_epi32(_mm256_xor_si256(magnitude, signbit), threshold));
        const __m256i signed_q = _mm256_sign_epi32(level, input);
        __m256i dq = _mm256_and_si256(_mm256_mullo_epi32(level, dequant), mask24);
        dq = _mm256_srl_epi32(_mm256_srli_epi32(_mm256_add_epi32(dq, four), 3), txshift);
        dq = _mm256_min_epi32(high, _mm256_max_epi32(low, _mm256_sign_epi32(dq, input)));
        _mm256_storeu_si256((__m256i*)(void*)(qcoeff + i), signed_q);
        _mm256_storeu_si256((__m256i*)(void*)(dqcoeff + i), dq);
    }
}
