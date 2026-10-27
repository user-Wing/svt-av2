#include "entropy_models.h"
#include "tile_writer.h"
#include "test_util.h"
#include <string.h>

/* Independent scalar expression of the fixed inverse-CDF update. The external
 * extractor/update oracle also compiles the actual fixed AVM prob.h. */
static void reference_update(uint16_t* cdf, uint32_t n, uint32_t s) {
    const uint32_t interval = cdf[n] < 16u ? 0u : cdf[n] < 32u ? 1u : 2u;
    const uint32_t divisor = 1u << (cdf[n + 1u + interval] + 2u);
    for (uint32_t i = 0; i < n - 1u; ++i) {
        if (i >= s) cdf[i] = (uint16_t)(cdf[i] - cdf[i] / divisor);
        else cdf[i] = (uint16_t)(cdf[i] + (32768u - cdf[i]) / divisor);
    }
    if (cdf[n] != 32u) ++cdf[n];
}

static void updates(void) {
    uint32_t equal = 0u;
    for (uint32_t q = 0u; q < 4u; ++q) {
        for (uint32_t id = 0u; id < SVT_AV2_MODEL_COUNT; ++id) {
            const uint32_t n = svt_av2_model_nsyms[id];
            uint16_t actual[12], expected[12];
            memcpy(actual, svt_av2_model_defaults[q][id], sizeof(actual));
            memcpy(expected, actual, sizeof(expected));
            for (uint32_t step = 0u; step < 512u; ++step) {
                const uint32_t s = step < 300u ? n / 2u : (step * 17u + id) % n;
                reference_update(expected, n, s);
                SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_model_update(actual, n, s), 0);
                SVT_TEST_CHECK(memcmp(actual, expected, sizeof(actual)) == 0);
                for (uint32_t j = 1u; j + 1u < n; ++j) equal += actual[j - 1u] == actual[j];
            }
        }
    }
    SVT_TEST_CHECK(equal > 0u);
}

static void isolation(void) {
    SvtAv2EntropyModels a, b, before;
    SvtAv2EntropyEncoder ea, eb;
    uint8_t bytes_a[65536], bytes_b[65536];
    uint16_t carry_a[65538], carry_b[65538];
    SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_models_init(&a, 1u), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_models_init(&b, 1u), 0);
    before = b;
    svt_av2_entropy_encoder_init(&ea, bytes_a, sizeof(bytes_a), carry_a, 65538u);
    svt_av2_entropy_encoder_init(&eb, bytes_b, sizeof(bytes_b), carry_b, 65538u);
    ea.models = &a; eb.models = &b;
    const uint32_t id = SVT_AV2_MODEL_Y_IDX;
    for (uint32_t s = 0u; s < 1200u; ++s) {
        SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_encode_model(&ea, id, NULL, 8u, s < 1100u ? 4u : s % 8u), 0);
    }
    SVT_TEST_CHECK(memcmp(&b, &before, sizeof(b)) == 0);
    /* A separate equal-initialized semantic model was not modified. */
    SVT_TEST_CHECK(memcmp(a.cdf[SVT_AV2_MODEL_INTER_TX], before.cdf[SVT_AV2_MODEL_INTER_TX], 24u) == 0);
    for (uint32_t s = 0u; s < 1200u; ++s)
        SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_encode_model(&eb, id, NULL, 8u, s < 1100u ? 4u : s % 8u), 0);
    uint8_t *out_a = NULL, *out_b = NULL;
    size_t size_a = 0, size_b = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_encoder_done(&ea, &out_a, &size_a), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_encoder_done(&eb, &out_b, &size_b), 0);
    SVT_TEST_CHECK(size_a == size_b && memcmp(out_a, out_b, size_a) == 0);
    SVT_TEST_CHECK(memcmp(&a, &b, sizeof(a)) == 0);
    before = a;
    SVT_TEST_CHECK(svt_av2_entropy_encode_model(&ea, SVT_AV2_MODEL_COUNT, NULL, 2u, 0u) != 0);
    SVT_TEST_CHECK(svt_av2_entropy_encode_model(&ea, id, NULL, 7u, 0u) != 0);
    SVT_TEST_CHECK(memcmp(&a, &before, sizeof(a)) == 0);
    ea.error = 1;
    SVT_TEST_CHECK(svt_av2_entropy_encode_model(&ea, id, NULL, 8u, 0u) != 0);
    SVT_TEST_CHECK(memcmp(&a, &before, sizeof(a)) == 0);
    svt_av2_entropy_rate_init(&ea); ea.models = &a;
    SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_encode_model(&ea, id, NULL, 8u, 0u), 0);
    SVT_TEST_CHECK(memcmp(&a, &before, sizeof(a)) != 0);
    SVT_TEST_CHECK(ea.rate_q12 > 0u && ea.rate_symbols == 1u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_models_init(&a, 1u), 0);
    SVT_TEST_CHECK(memcmp(a.cdf, svt_av2_model_defaults[1], sizeof(a.cdf)) == 0);
}

static void fresh_writer(uint32_t depth, uint32_t format, uint32_t inter) {
    SvtAv2FrameHeaderConfig cfg;
    svt_av2_frame_header_config_default(&cfg, 128u, 64u);
    cfg.bit_depth = depth; cfg.chroma_format = format;
    cfg.base_q_idx = depth == 10u ? 148u : 100u;
    cfg.m5_intra_modes = 1u; cfg.inter_frame = inter; cfg.multi_frame = 1u;
    cfg.reduced_tx_set = 2u; cfg.entropy_adaptation = 1u;
    int32_t coeffs[6][1024] = {{0}};
    SvtAv2TileCoeffBlock blocks[6];
    SvtAv2TileBlockMode modes[2] = {{1u, 3, -2}, {0u, 0, 0}};
    for (uint32_t sb = 0u; sb < 2u; ++sb) {
        for (uint32_t p = 0u; p < 3u; ++p) {
            const uint32_t i = sb * 3u + p;
            const uint32_t shift = p && format == 1u ? 1u : 0u;
            blocks[i].plane = p; blocks[i].x = (sb * 64u) >> shift;
            blocks[i].y = 0u; blocks[i].width = blocks[i].height = 64u >> shift;
            blocks[i].intra_mode = (uint8_t)sb; blocks[i].qcoeff = coeffs[i];
            for (uint32_t c = 0u; c < 100u; ++c) coeffs[i][c] = (int32_t)((c + i) % 17u) - 8;
        }
    }
    int32_t saved[6][1024]; memcpy(saved, coeffs, sizeof(saved));
    uint8_t clean[32768], retry[32768], tiny[16];
    memset(tiny, 0xa7, sizeof(tiny));
    size_t first = 0u, second = 0u, need = 0u;
    SvtAv2TileRateSession* session = NULL;
    uint64_t rate = 0u;
    SVT_TEST_CHECK(svt_av2_tile_rate_create(&cfg, &session) != 0 && session == NULL);
    SVT_TEST_CHECK(svt_av2_tile_rate_frame(&cfg, blocks, 6u, inter ? modes : NULL, inter ? 2u : 0u, &rate) != 0);
    SVT_TEST_CHECK(svt_av2_tile_writer_write_closed_loop_key(&cfg, tiny, sizeof(tiny), &need) != 0);
    if (inter) {
        SVT_TEST_CHECK_EQ_INT(svt_av2_tile_writer_write_inter_coeffs(&cfg, blocks, 6u, modes, 2u, clean, sizeof(clean), &first), 0);
        SVT_TEST_CHECK(svt_av2_tile_writer_write_inter_coeffs(&cfg, blocks, 6u, modes, 2u, tiny, sizeof(tiny), &need) != 0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_tile_writer_write_inter_coeffs(&cfg, blocks, 6u, modes, 2u, retry, sizeof(retry), &second), 0);
    } else {
        SVT_TEST_CHECK_EQ_INT(svt_av2_tile_writer_write_closed_loop_key_coeffs(&cfg, blocks, 6u, clean, sizeof(clean), &first, NULL), 0);
        SVT_TEST_CHECK(svt_av2_tile_writer_write_closed_loop_key_coeffs(&cfg, blocks, 6u, tiny, sizeof(tiny), &need, NULL) != 0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_tile_writer_write_closed_loop_key_coeffs(&cfg, blocks, 6u, retry, sizeof(retry), &second, NULL), 0);
    }
    SVT_TEST_CHECK(first > sizeof(tiny) && first == second && memcmp(clean, retry, first) == 0);
    for (size_t i = 0u; i < sizeof(tiny); ++i) SVT_TEST_CHECK(tiny[i] == 0xa7);
    SVT_TEST_CHECK(memcmp(coeffs, saved, sizeof(saved)) == 0);
}

int main(void) {
    updates(); isolation();
    for (uint32_t inter = 0u; inter < 2u; ++inter) {
        fresh_writer(8u, 1u, inter); fresh_writer(10u, 1u, inter); fresh_writer(10u, 3u, inter);
    }
    SVT_TEST_REPORT("m7_entropy");
}
