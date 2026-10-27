#include "entropy_models.h"
#include <string.h>

int svt_av2_entropy_models_init(SvtAv2EntropyModels* models, uint32_t qctx) {
    if (models == NULL) return SVT_AV2_ENTROPY_ERROR_NULL;
    if (qctx > 3u) return SVT_AV2_ENTROPY_ERROR_RANGE;
    memcpy(models->cdf, svt_av2_model_defaults[qctx], sizeof(models->cdf));
    return 0;
}

int svt_av2_entropy_model_update(uint16_t* cdf, uint32_t nsyms, uint32_t symbol) {
    if (cdf == NULL) return SVT_AV2_ENTROPY_ERROR_NULL;
    if (nsyms < 2u || nsyms > 8u || symbol >= nsyms || cdf[nsyms] > 32u)
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    const uint32_t interval = cdf[nsyms] > 31u ? 2u : cdf[nsyms] > 15u ? 1u : 0u;
    const uint32_t rate = 2u + cdf[nsyms + 1u + interval];
    if (rate == 0u || rate > 15u) return SVT_AV2_ENTROPY_ERROR_RANGE;
    for (uint32_t i = 0u; i + 1u < nsyms; ++i) {
        const uint32_t target = i < symbol ? 32768u : 0u;
        const uint32_t value = cdf[i];
        cdf[i] = (uint16_t)(target < value ? value - ((value - target) >> rate) :
                                             value + ((target - value) >> rate));
    }
    cdf[nsyms] += cdf[nsyms] < 32u;
    return 0;
}

int svt_av2_entropy_encode_model(SvtAv2EntropyEncoder* enc, uint32_t model,
    const uint16_t* frozen_cdf, uint32_t nsyms, uint32_t symbol) {
    if (enc == NULL) return SVT_AV2_ENTROPY_ERROR_NULL;
    if (enc->models == NULL)
        return svt_av2_entropy_encode_symbol(enc, frozen_cdf, nsyms, symbol);
    if (model >= SVT_AV2_MODEL_COUNT ||
        nsyms != svt_av2_model_nsyms[model] || symbol >= nsyms)
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    uint16_t* state = enc->models->cdf[model];
    uint16_t forward[7];
    for (uint32_t i = 0u; i + 1u < nsyms; ++i)
        forward[i] = (uint16_t)(32768u - state[i]);
    if (enc->rate_only) {
        uint32_t probability, cost;
        if (enc->error) return SVT_AV2_ENTROPY_ERROR_NO_SPACE;
        if (state[nsyms - 1u] != 0u || state[nsyms] > 32u ||
            state[nsyms + 1u] > 13u || state[nsyms + 2u] > 13u ||
            state[nsyms + 3u] > 13u)
            return SVT_AV2_ENTROPY_ERROR_RANGE;
        for (uint32_t i = 0u; i + 1u < nsyms; ++i)
            if (state[i] > 32768u) return SVT_AV2_ENTROPY_ERROR_RANGE;
        int st = svt_av2_entropy_adjusted_probability(forward, nsyms, symbol, &probability);
        if (st != 0) return st;
        st = svt_av2_entropy_probability_cost_q12(probability, &cost);
        if (st != 0) return st;
        st = svt_av2_entropy_model_update(state, nsyms, symbol);
        if (st != 0) return st;
        enc->rate_q12 += cost;
        ++enc->rate_symbols;
        return 0;
    }
    const int st = svt_av2_entropy_encode_symbol(enc, forward, nsyms, symbol);
    return st != 0 ? st : svt_av2_entropy_model_update(state, nsyms, symbol);
}
