/* M5 adaptive-mode public API, retry determinism and owned input tests. */
#include "svt_av2.h"
#include "test_util.h"
#include <limits.h>
#include <stdlib.h>

static void configure(SvtAv2Handle* handle, uint32_t format, uint32_t prediction, uint32_t partition, int32_t qp) {
    const uint32_t side = 128u, enabled = 1u;
    const SvtAv2BitDepth depth = format ? SVT_AV2_BIT_DEPTH_10 : SVT_AV2_BIT_DEPTH_8;
    const SvtAv2ChromaFormat chroma = format == 2u ? SVT_AV2_CHROMA_444 : SVT_AV2_CHROMA_420;
    const SvtAv2PredictionStructure structure = (SvtAv2PredictionStructure)prediction;
    const char* label = "test-m5-api";
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(handle, NULL), SVT_AV2_ERROR_NONE);
    SvtAv2EncoderConfig defaults;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(*handle, &defaults), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(defaults.partition_selection, 0u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*handle, SVT_AV2_PARAM_WIDTH, &side), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*handle, SVT_AV2_PARAM_HEIGHT, &side), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*handle, SVT_AV2_PARAM_BIT_DEPTH, &depth), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*handle, SVT_AV2_PARAM_CHROMA_FORMAT, &chroma), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*handle, SVT_AV2_PARAM_OUTPUT_PATH, &label), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*handle, SVT_AV2_PARAM_PREDICTION_STRUCTURE, &structure), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*handle, SVT_AV2_PARAM_INTRA_MODE_SELECTION, &enabled), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*handle, SVT_AV2_PARAM_PARTITION_SELECTION, &partition), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*handle, SVT_AV2_PARAM_QP, &qp), SVT_AV2_ERROR_NONE);
    const uint32_t invalid[] = {2u, UINT32_MAX};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        const uint32_t invalid_intra = invalid[i] == 2u ? 5u : invalid[i];
        const uint32_t invalid_partition = invalid[i] == 2u ? 11u : invalid[i];
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*handle, SVT_AV2_PARAM_INTRA_MODE_SELECTION, &invalid_intra), SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*handle, SVT_AV2_PARAM_PARTITION_SELECTION, &invalid_partition), SVT_AV2_ERROR_INVALID_PARAMETER);
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*handle, SVT_AV2_PARAM_INTRA_MODE_SELECTION, NULL), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*handle, SVT_AV2_PARAM_PARTITION_SELECTION, NULL), SVT_AV2_ERROR_INVALID_PARAMETER);
    SvtAv2EncoderConfig actual;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(*handle, &actual), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(actual.intra_mode_selection, enabled);
    SVT_TEST_CHECK_EQ_INT(actual.partition_selection, partition);
    SVT_TEST_CHECK_EQ_INT(actual.prediction_structure, structure);
    SVT_TEST_CHECK_EQ_INT(actual.qp, qp);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(*handle), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*handle, SVT_AV2_PARAM_INTRA_MODE_SELECTION, &enabled), SVT_AV2_ERROR_INVALID_STATE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(*handle, SVT_AV2_PARAM_PARTITION_SELECTION, &partition), SVT_AV2_ERROR_INVALID_STATE);
}

static void run_pair(uint32_t format, uint32_t structure, uint32_t partition, int32_t qp) {
    SvtAv2Handle handles[2] = {NULL, NULL};
    uint8_t* packets[2] = {malloc(1024u * 1024u), malloc(1024u * 1024u)};
    uint8_t* recons[2] = {malloc(128u * 128u * 6u), malloc(128u * 128u * 6u)};
    uint8_t storage[3][128u * 259u + 1u];
    const uint32_t step = format ? 2u : 1u;
    const uint32_t mask = format ? 1023u : 255u;
    const int32_t timestamps[] = {-17, 100, INT32_MAX};
    SVT_TEST_CHECK(packets[0] && packets[1] && recons[0] && recons[1]);
    if (!packets[0] || !packets[1] || !recons[0] || !recons[1]) goto done;
    configure(handles, format, structure, partition, qp);
    configure(handles + 1, format, structure, partition, qp);
    SvtAv2Picture pic;
    memset(&pic, 0, sizeof(pic));
    pic.width = pic.height = 128u;
    pic.bit_depth = format ? SVT_AV2_BIT_DEPTH_10 : SVT_AV2_BIT_DEPTH_8;
    pic.chroma_format = format == 2u ? SVT_AV2_CHROMA_444 : SVT_AV2_CHROMA_420;
    for (uint32_t p = 0; p < 3u; ++p) {
        pic.planes[p] = storage[p] + 1u;
        pic.strides[p] = 259u; /* deliberately odd byte stride and unaligned u16le */
    }
    for (uint32_t frame = 0; frame < 3u; ++frame) {
        memset(storage, 0xA5, sizeof(storage));
        pic.pts = timestamps[frame];
        for (uint32_t p = 0; p < 3u; ++p) {
            const uint32_t dim = p == 0u || format == 2u ? 128u : 64u;
            for (uint32_t y = 0; y < dim; ++y)
                for (uint32_t x = 0; x < dim; ++x) {
                    const uint32_t value = partition && frame == 1u && p == 0u ?
                        (format ? 512u : 128u) :
                        ((y < 64u ? y : x) * 4u + frame * 37u + p * 29u) & mask;
                    uint8_t* dst = storage[p] + 1u + y * 259u + x * step;
                    dst[0] = (uint8_t)value;
                    if (step == 2u) dst[1] = (uint8_t)(value >> 8u);
                }
        }
        for (uint32_t h = 0; h < 2u; ++h) {
            SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handles[h], &pic), SVT_AV2_ERROR_NONE);
            SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handles[h], &pic), SVT_AV2_ERROR_INVALID_STATE);
            SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handles[h], NULL), SVT_AV2_ERROR_INVALID_STATE);
        }
        size_t sizes[2] = {0, 0}, recon_sizes[2] = {0, 0};
        for (uint32_t h = 0; h < 2u; ++h) {
            uint8_t sentinel = 0xA5u;
            SvtAv2Packet pkt = {packets[h], 1024u * 1024u, 0, 0};
            size_t required = 0;
            if (h == 1u) {
                /* Handle 0 has already encoded the original input. A borrowed
                 * input pointer in handle 1 must therefore fail comparison. */
                memset(storage, 0, sizeof(storage));
                for (uint32_t retry = 0; retry < 3u; ++retry) {
                    SvtAv2Packet tiny = {&sentinel, 1u, 0, 0};
                    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(handles[h], &tiny), SVT_AV2_ERROR_OUT_OF_MEMORY);
                    SVT_TEST_CHECK_EQ_INT(sentinel, 0xA5u);
                    SVT_TEST_CHECK(tiny.size > 1u);
                    if (retry) SVT_TEST_CHECK_EQ_INT(tiny.size, required);
                    required = tiny.size;
                    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handles[h], &pic), SVT_AV2_ERROR_INVALID_STATE);
                    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handles[h], NULL), SVT_AV2_ERROR_INVALID_STATE);
                }
            }
            SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(handles[h], &pkt), SVT_AV2_ERROR_NONE);
            SVT_TEST_CHECK(pkt.size > 0u);
            if (h == 1u) SVT_TEST_CHECK_EQ_INT(pkt.size, required);
            SVT_TEST_CHECK_EQ_INT(pkt.pts, timestamps[frame]);
            sizes[h] = pkt.size;
            SvtAv2ReconFrame recon;
            memset(&recon, 0, sizeof(recon));
            recon.data = &sentinel; recon.capacity = 1u;
            SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_recon(handles[h], &recon), SVT_AV2_ERROR_OUT_OF_MEMORY);
            SVT_TEST_CHECK_EQ_INT(sentinel, 0xA5u);
            const size_t recon_required = recon.byte_count;
            recon.data = recons[h]; recon.capacity = 128u * 128u * 6u;
            SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_recon(handles[h], &recon), SVT_AV2_ERROR_NONE);
            SVT_TEST_CHECK_EQ_INT(recon.frame_index, frame);
            SVT_TEST_CHECK_EQ_INT(recon.bit_depth, pic.bit_depth);
            SVT_TEST_CHECK_EQ_INT(recon.chroma_format, pic.chroma_format);
            recon_sizes[h] = recon.byte_count;
            SVT_TEST_CHECK_EQ_INT(recon.byte_count, recon_required);
            SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(handles[h], &pkt), SVT_AV2_ERROR_NONE);
            SVT_TEST_CHECK_EQ_INT(pkt.size, 0u);
        }
        SVT_TEST_CHECK_EQ_INT(sizes[0], sizes[1]);
        SVT_TEST_CHECK_EQ_INT(memcmp(packets[0], packets[1], sizes[0]), 0);
        SVT_TEST_CHECK_EQ_INT(recon_sizes[0], recon_sizes[1]);
        SVT_TEST_CHECK_EQ_INT(memcmp(recons[0], recons[1], recon_sizes[0]), 0);
    }
    for (uint32_t h = 0; h < 2u; ++h) {
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handles[h], NULL), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handles[h], &pic), SVT_AV2_ERROR_INVALID_STATE);
        SvtAv2Packet end = {packets[h], 1024u * 1024u, 99u, 99};
        for (uint32_t retry = 0; retry < 2u; ++retry) {
            SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(handles[h], &end), SVT_AV2_ERROR_NONE);
            SVT_TEST_CHECK_EQ_INT(end.size, 0u);
        }
    }
done:
    for (uint32_t h = 0; h < 2u; ++h) {
        if (handles[h]) SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(handles + h), SVT_AV2_ERROR_NONE);
        free(packets[h]); free(recons[h]);
    }
}

int main(void) {
    const int32_t qps[] = {1, 100, 255};
    for (uint32_t format = 0; format < 3u; ++format)
        for (uint32_t structure = 0; structure < 2u; ++structure)
            for (uint32_t partition = 0; partition < 2u; ++partition)
                for (size_t q = 0; q < sizeof(qps) / sizeof(qps[0]); ++q)
                    run_pair(format, structure, partition, qps[q]);
    SVT_TEST_REPORT("test_m5_api");
}
