#ifndef SVT_AV2_ENTROPY_MODELS_H
#define SVT_AV2_ENTROPY_MODELS_H
#include "entropy_encoder.h"
#include "entropy_model_data.h"

typedef struct SvtAv2EntropyModels {
    /* Canonical inverse boundaries, terminal, count, three training offsets. */
    uint16_t cdf[SVT_AV2_MODEL_COUNT][12];
} SvtAv2EntropyModels;

int svt_av2_entropy_models_init(SvtAv2EntropyModels* models, uint32_t qctx);
int svt_av2_entropy_model_update(uint16_t* cdf, uint32_t nsyms, uint32_t symbol);
/* Explicit semantic ID, with the frozen forward row retained for off mode. */
int svt_av2_entropy_encode_model(SvtAv2EntropyEncoder* enc, uint32_t model,
    const uint16_t* frozen_cdf, uint32_t nsyms, uint32_t symbol);
#endif
