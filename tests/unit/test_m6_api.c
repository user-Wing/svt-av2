/* M6 public configuration, owned input and packet/recon retry determinism. */
#include "svt_av2.h"
#include "test_util.h"
#include <stdlib.h>

static void configure(SvtAv2Handle* h, uint32_t rd, uint32_t prediction, uint32_t format) {
    const uint32_t side = 64u, one = 1u;
    const SvtAv2BitDepth bd = format ? SVT_AV2_BIT_DEPTH_10 : SVT_AV2_BIT_DEPTH_8;
    const SvtAv2ChromaFormat chroma = format ? SVT_AV2_CHROMA_444 : SVT_AV2_CHROMA_420;
    const SvtAv2PredictionStructure structure = (SvtAv2PredictionStructure)prediction;
    const char* label = "m6-api";
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(h, NULL), SVT_AV2_ERROR_NONE);
    SvtAv2EncoderConfig c;
    SvtAv2RdStats stats;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(*h, &c), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(c.rd_mode, 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_rd_stats(*h, &stats), SVT_AV2_ERROR_INVALID_STATE);
    const uint32_t invalid[] = {4u, UINT32_MAX};
    for (size_t i = 0; i < 2u; ++i)
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*h, SVT_AV2_PARAM_RD_MODE, invalid + i), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*h, SVT_AV2_PARAM_RD_MODE, NULL), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*h, SVT_AV2_PARAM_WIDTH, &side), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*h, SVT_AV2_PARAM_HEIGHT, &side), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*h, SVT_AV2_PARAM_BIT_DEPTH, &bd), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*h, SVT_AV2_PARAM_CHROMA_FORMAT, &chroma), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*h, SVT_AV2_PARAM_OUTPUT_PATH, &label), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*h, SVT_AV2_PARAM_PREDICTION_STRUCTURE, &structure), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*h, SVT_AV2_PARAM_INTRA_MODE_SELECTION, &one), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*h, SVT_AV2_PARAM_PARTITION_SELECTION, &one), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*h, SVT_AV2_PARAM_RD_MODE, &rd), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(*h, &c), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(c.rd_mode, rd);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(*h), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*h, SVT_AV2_PARAM_RD_MODE, &rd), SVT_AV2_ERROR_INVALID_STATE);
}

static void pair(uint32_t rd, uint32_t prediction, uint32_t format) {
    SvtAv2Handle h[2] = {NULL, NULL};
    uint8_t source[3][8192];
    uint8_t packets[2][131072], recons[2][24576];
    configure(h, rd, prediction, format);
    configure(h + 1, rd, prediction, format);
    SvtAv2Picture pic;
    memset(&pic, 0, sizeof(pic));
    pic.width = pic.height = 64u;
    pic.bit_depth = format ? SVT_AV2_BIT_DEPTH_10 : SVT_AV2_BIT_DEPTH_8;
    pic.chroma_format = format ? SVT_AV2_CHROMA_444 : SVT_AV2_CHROMA_420;
    for (uint32_t p = 0; p < 3u; ++p) { pic.planes[p] = source[p]; pic.strides[p] = format ? 128u : 64u; }
    for (uint32_t frame = 0; frame < 2u; ++frame) {
        for (uint32_t p = 0; p < 3u; ++p)
            for (uint32_t i = 0; i < 4096u; ++i) {
                uint32_t value = ((i / 64u) * 7u + (i % 64u) * 3u + p * 29u + frame * 17u) & (format ? 1023u : 255u);
                source[p][format ? 2u*i : i] = (uint8_t)value;
                if (format) source[p][2u*i+1u] = (uint8_t)(value >> 8u);
            }
        pic.pts = frame ? INT32_MAX : -71;
        for (uint32_t k = 0; k < 2u; ++k)
            SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(h[k], &pic), SVT_AV2_ERROR_NONE);
        memset(source, 0xA5, sizeof(source)); /* send_picture must own the input. */
        size_t size[2], raw_size[2];
        SvtAv2RdStats stats[2];
        for (uint32_t k = 0; k < 2u; ++k) {
            SvtAv2Packet pkt = {packets[k], sizeof(packets[k]), 0, 0};
            if (k) {
                uint8_t sentinel = 0xA5;
                SvtAv2Packet small = {&sentinel, 1, 0, 0};
                SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(h[k], &small), SVT_AV2_ERROR_OUT_OF_MEMORY);
                SVT_TEST_CHECK_EQ_INT(sentinel, 0xA5);
                SVT_TEST_CHECK(small.size > 1);
            }
            SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(h[k], &pkt), SVT_AV2_ERROR_NONE);
            SVT_TEST_CHECK_EQ_INT(pkt.pts, pic.pts); size[k] = pkt.size;
            SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_rd_stats(h[k], stats + k), SVT_AV2_ERROR_NONE);
            SVT_TEST_CHECK(stats[k].candidate_count > 0 && stats[k].replay_checks > 0);
            SvtAv2ReconFrame raw;
            memset(&raw, 0, sizeof(raw)); raw.data = recons[k]; raw.capacity = sizeof(recons[k]);
            SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_recon(h[k], &raw), SVT_AV2_ERROR_NONE);
            raw_size[k] = raw.byte_count;
        }
        SVT_TEST_CHECK_EQ_INT(size[0], size[1]); SVT_TEST_CHECK(memcmp(packets[0], packets[1], size[0]) == 0);
        SVT_TEST_CHECK_EQ_INT(raw_size[0], raw_size[1]); SVT_TEST_CHECK(memcmp(recons[0], recons[1], raw_size[0]) == 0);
        SVT_TEST_CHECK_EQ_INT(stats[0].candidate_count, stats[1].candidate_count);
        SVT_TEST_CHECK_EQ_INT(stats[0].winner_hash, stats[1].winner_hash);
    }
    for (uint32_t k = 0; k < 2u; ++k) {
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(h[k], NULL), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(h + k), SVT_AV2_ERROR_NONE);
    }
}
int main(void) {
    SvtAv2RdStats stats;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_rd_stats(NULL, &stats), SVT_AV2_ERROR_INVALID_HANDLE);
    for (uint32_t rd = 1; rd <= 2; ++rd)
        for (uint32_t p = 0; p < 2; ++p)
            for (uint32_t f = 0; f < 2; ++f) pair(rd, p, f);
    SVT_TEST_REPORT("test_m6_api");
}
