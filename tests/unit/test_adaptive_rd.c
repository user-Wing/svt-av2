#include "entropy_models.h"
#include "test_util.h"
#include <string.h>

/* Independent AVM get_adjusted_prob expression, limited to reachable N=2..8. */
static const uint16_t padding[7][8] = {
    {8, 0}, {10, 5, 0}, {12, 8, 4, 0}, {12, 9, 6, 3, 0},
    {13, 10, 8, 5, 2, 0}, {13, 11, 9, 6, 4, 2, 0},
    {14, 12, 10, 8, 6, 4, 2, 0}
};

static uint32_t reference_boundary(uint16_t inverse, uint32_t i, uint32_t n) {
    return (uint32_t)(inverse / 128u) * 128u + 8u * padding[n - 2u][i];
}

static void probabilities(void) {
    uint32_t equal = 0u;
    for (uint32_t q = 0u; q < 4u; ++q) {
        for (uint32_t id = 0u; id < SVT_AV2_MODEL_COUNT; ++id) {
            const uint32_t n = svt_av2_model_nsyms[id];
            uint16_t row[12], forward[7];
            memcpy(row, svt_av2_model_defaults[q][id], sizeof(row));
            for (uint32_t step = 0u; step <= 512u; ++step) {
                if (step == 0u || step == 15u || step == 16u ||
                    step == 31u || step == 32u || step == 512u) {
                    uint32_t sum = 0u;
                    for (uint32_t i = 0u; i + 1u < n; ++i)
                        forward[i] = (uint16_t)(32768u - row[i]);
                    for (uint32_t s = 0u; s < n; ++s) {
                        const uint32_t upper = s == 0u ? 32768u :
                            reference_boundary(row[s - 1u], s - 1u, n);
                        const uint32_t lower = reference_boundary(row[s], s, n);
                        uint32_t actual = 0u, cost = 0u;
                        SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_adjusted_probability(
                            forward, n, s, &actual), 0);
                        SVT_TEST_CHECK_EQ_INT(actual, upper - lower);
                        SVT_TEST_CHECK(actual > 0u);
                        SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_probability_cost_q12(actual, &cost), 0);
                        sum += actual;
                    }
                    SVT_TEST_CHECK_EQ_INT(sum, 32768u);
                    for (uint32_t i = 1u; i + 1u < n; ++i)
                        equal += row[i - 1u] == row[i];
                }
                SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_model_update(row, n, n / 2u), 0);
            }
        }
    }
    SVT_TEST_CHECK(equal > 0u);
}

static void model_trajectories(void) {
    static SvtAv2EntropyModels formal, rate, before;
    uint8_t output[65536];
    uint16_t carry[65538];
    for (uint32_t q = 0u; q < 4u; ++q) {
        SvtAv2EntropyEncoder writer, estimator;
        SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_models_init(&formal, q), 0);
        rate = formal;
        svt_av2_entropy_encoder_init(&writer, output, sizeof(output), carry, 65538u);
        svt_av2_entropy_rate_init(&estimator);
        writer.models = &formal;
        estimator.models = &rate;
        for (uint32_t id = 0u; id < SVT_AV2_MODEL_COUNT; ++id) {
            const uint32_t n = svt_av2_model_nsyms[id];
            for (uint32_t step = 0u; step < 64u; ++step) {
                const uint32_t s = step < 40u ? n / 2u : (id + step) % n;
                const uint32_t upper = s == 0u ? 32768u :
                    reference_boundary(rate.cdf[id][s - 1u], s - 1u, n);
                const uint32_t lower = reference_boundary(rate.cdf[id][s], s, n);
                uint32_t cost = 0u;
                SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_probability_cost_q12(upper - lower, &cost), 0);
                const uint64_t entry = estimator.rate_q12;
                SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_encode_model(&writer, id, NULL, n, s), 0);
                SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_encode_model(&estimator, id, NULL, n, s), 0);
                SVT_TEST_CHECK_EQ_INT(estimator.rate_q12 - entry, cost);
                SVT_TEST_CHECK(memcmp(&formal, &rate, sizeof(rate)) == 0);
            }
        }
        before = rate;
        const uint64_t entry = estimator.rate_q12;
        SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_encode_literal(&estimator, 5u, 3), 0);
        SVT_TEST_CHECK_EQ_INT(estimator.rate_q12 - entry, 3u * 4096u);
        SVT_TEST_CHECK(memcmp(&before, &rate, sizeof(rate)) == 0);
        uint8_t* bytes = NULL;
        size_t size = 0u;
        SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_encoder_done(&writer, &bytes, &size), 0);
        SVT_TEST_CHECK(size > 0u);
        printf("qctx=%u estimated_q12=%llu arithmetic_bytes=%zu\n", q,
            (unsigned long long)(estimator.rate_q12 - 3u * 4096u), size);
    }
}

static void invalid_and_frozen(void) {
    static SvtAv2EntropyModels bank, before;
    SvtAv2EntropyEncoder enc;
    uint16_t row[7] = {16384u};
    uint32_t p = 77u;
    SVT_TEST_CHECK(svt_av2_entropy_adjusted_probability(NULL, 2u, 0u, &p) != 0);
    SVT_TEST_CHECK(svt_av2_entropy_adjusted_probability(row, 2u, 0u, NULL) != 0);
    SVT_TEST_CHECK(svt_av2_entropy_adjusted_probability(row, 9u, 0u, &p) != 0);
    SVT_TEST_CHECK(svt_av2_entropy_adjusted_probability(row, 2u, 2u, &p) != 0);
    row[0] = 32769u;
    SVT_TEST_CHECK(svt_av2_entropy_adjusted_probability(row, 2u, 0u, &p) != 0);
    row[0] = 0u;
    SVT_TEST_CHECK(svt_av2_entropy_adjusted_probability(row, 2u, 0u, &p) != 0);
    row[0] = 20000u; row[1] = 19000u;
    SVT_TEST_CHECK(svt_av2_entropy_adjusted_probability(row, 3u, 0u, &p) != 0);
    SVT_TEST_CHECK_EQ_INT(p, 77u);
    const uint32_t id = SVT_AV2_MODEL_Y_IDX;
    const uint32_t n = svt_av2_model_nsyms[id];
    for (uint32_t failure = 0u; failure < 7u; ++failure) {
        SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_models_init(&bank, 0u), 0);
        svt_av2_entropy_rate_init(&enc); enc.models = &bank;
        if (failure == 0u) bank.cdf[id][n - 1u] = 1u;
        if (failure == 1u) bank.cdf[id][n] = 33u;
        if (failure == 2u) bank.cdf[id][n + 1u] = 14u;
        if (failure == 3u) bank.cdf[id][0] = 32769u;
        if (failure == 4u) bank.cdf[id][1] = 32768u;
        if (failure == 5u) enc.error = 1;
        before = bank;
        SVT_TEST_CHECK(svt_av2_entropy_encode_model(&enc, id, NULL, n,
            failure == 6u ? n : 0u) != 0);
        SVT_TEST_CHECK_EQ_INT(enc.rate_q12, 0u);
        SVT_TEST_CHECK_EQ_INT(enc.rate_symbols, 0u);
        SVT_TEST_CHECK(memcmp(&bank, &before, sizeof(bank)) == 0);
    }
    /* The frozen estimator retains raw adjacent differences for every p15. */
    for (uint32_t probability = 1u; probability <= 32768u; ++probability) {
        uint32_t expected = 0u;
        row[0] = (uint16_t)probability;
        svt_av2_entropy_rate_init(&enc);
        SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_probability_cost_q12(probability, &expected), 0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_encode_symbol(&enc, row, 2u, 0u), 0);
        SVT_TEST_CHECK_EQ_INT(enc.rate_q12, expected);
    }
}

int main(void) {
    probabilities();
    model_trajectories();
    invalid_and_frozen();
    SVT_TEST_REPORT("adaptive_rd_kernel");
}
