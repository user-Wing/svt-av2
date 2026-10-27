/* Observable public API invariants across reference commits and hint wrap. */
#include "svt_av2.h"
#include "test_util.h"
#include <stdlib.h>

static void configure(SvtAv2Handle handle, uint32_t bd, SvtAv2ChromaFormat chroma) {
    const uint32_t size = 64;
    const SvtAv2BitDepth depth = (SvtAv2BitDepth)bd;
    const SvtAv2PredictionStructure mode = SVT_AV2_PRED_LOW_DELAY_P;
    const char* path = "unused-m4-contracts.av2";
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_WIDTH, &size), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_HEIGHT, &size), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_BIT_DEPTH, &depth), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_CHROMA_FORMAT, &chroma), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_OUTPUT_PATH, &path), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_PREDICTION_STRUCTURE, &mode), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), 0);
}

static void check_sequence(uint32_t bd, SvtAv2ChromaFormat chroma) {
    const uint32_t step = bd == 8 ? 1u : 2u;
    const uint32_t cw = chroma == SVT_AV2_CHROMA_444 ? 64u : 32u;
    const size_t bytes = (4096u + 2u*cw*cw)*step;
    uint8_t *input = (uint8_t*)malloc(bytes), *saved = (uint8_t*)malloc(bytes);
    uint8_t *ra = (uint8_t*)malloc(bytes), *rb = (uint8_t*)malloc(bytes);
    uint8_t packets[2][65536];
    SvtAv2Handle a = NULL, b = NULL;
    SVT_TEST_CHECK(input != NULL && saved != NULL && ra != NULL && rb != NULL);
    if (!input || !saved || !ra || !rb) { free(input); free(saved); free(ra); free(rb); return; }
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&a, NULL), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&b, NULL), 0);
    configure(a, bd, chroma); configure(b, bd, chroma);
    SvtAv2Picture picture;
    memset(&picture, 0, sizeof(picture));
    picture.width = picture.height = 64; picture.bit_depth = (SvtAv2BitDepth)bd;
    picture.chroma_format = chroma;
    picture.planes[0] = input; picture.planes[1] = input + 4096u*step;
    picture.planes[2] = input + (4096u+cw*cw)*step;
    picture.strides[0] = 64u*step; picture.strides[1] = picture.strides[2] = cw*step;
    for (uint32_t frame = 0; frame < 258; ++frame) {
        /* Deterministic varying input; no source pointer may survive send(). */
        for (size_t i = 0; i < bytes/step; ++i) {
            const uint32_t v = (uint32_t)((i*13u + frame*7u + (i/64u)*3u) & ((1u<<bd)-1u));
            input[i*step] = (uint8_t)v;
            if (step == 2) input[i*step+1] = (uint8_t)(v>>8);
        }
        memcpy(saved, input, bytes); picture.pts = (int32_t)frame*17 - 100;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(a, &picture), 0);
        memset(input, 0xa5, bytes);
        packets[0][0] = 0xcc;
        SvtAv2Packet pa = {packets[0], 1, 0, 0};
        const SvtAv2Error small = svt_av2_enc_get_packet_ex(a, &pa);
        SVT_TEST_CHECK_EQ_INT(small, SVT_AV2_ERROR_OUT_OF_MEMORY);
        SVT_TEST_CHECK_EQ_INT(packets[0][0], 0xcc);
        const size_t required = pa.size;
        pa.size = 0;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(a, &pa), SVT_AV2_ERROR_OUT_OF_MEMORY);
        SVT_TEST_CHECK_EQ_INT(pa.size, required);
        SVT_TEST_CHECK_EQ_INT(packets[0][0], 0xcc);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(a, &picture), SVT_AV2_ERROR_INVALID_STATE);
        memcpy(input, saved, bytes);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(b, &picture), 0);
        pa.capacity = sizeof(packets[0]);
        SvtAv2Packet pb = {packets[1], sizeof(packets[1]), 0, 0};
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(a, &pa), 0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(b, &pb), 0);
        SVT_TEST_CHECK_EQ_INT(pa.size, pb.size);
        SVT_TEST_CHECK_EQ_INT(pa.pts, picture.pts);
        SVT_TEST_CHECK_EQ_INT(pb.pts, picture.pts);
        SVT_TEST_CHECK(pa.size <= sizeof(packets[0]));
        if (pa.size == pb.size && pa.size <= sizeof(packets[0]))
            SVT_TEST_CHECK(memcmp(pa.data, pb.data, pa.size) == 0);
        SvtAv2ReconFrame recon_a, recon_b;
        memset(&recon_a, 0, sizeof(recon_a)); memset(&recon_b, 0, sizeof(recon_b));
        recon_a.data = ra; recon_a.capacity = 1; ra[0] = 0xcd;
        recon_a.frame_index = 999;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_recon(a, &recon_a), SVT_AV2_ERROR_OUT_OF_MEMORY);
        SVT_TEST_CHECK_EQ_INT(recon_a.byte_count, bytes);
        SVT_TEST_CHECK_EQ_INT(recon_a.frame_index, 999);
        SVT_TEST_CHECK_EQ_INT(ra[0], 0xcd);
        recon_a.capacity = bytes;
        recon_b.data = rb; recon_b.capacity = bytes;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_recon(a, &recon_a), 0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_recon(b, &recon_b), 0);
        SVT_TEST_CHECK_EQ_INT(recon_a.frame_index, frame);
        SVT_TEST_CHECK_EQ_INT(recon_a.byte_count, bytes);
        SVT_TEST_CHECK_EQ_INT(recon_a.visible_width, 64);
        SVT_TEST_CHECK_EQ_INT(recon_a.visible_height, 64);
        SVT_TEST_CHECK_EQ_INT(recon_a.chroma_width, cw);
        SVT_TEST_CHECK_EQ_INT(recon_a.chroma_height, cw);
        SVT_TEST_CHECK_EQ_INT(recon_a.bit_depth, bd);
        SVT_TEST_CHECK_EQ_INT(recon_a.chroma_format, chroma);
        SVT_TEST_CHECK(memcmp(ra, rb, bytes) == 0);
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(a, NULL), 0);
    SvtAv2Packet eos = {packets[0], sizeof(packets[0]), 999, 0};
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(a, &eos), 0);
    SVT_TEST_CHECK_EQ_INT(eos.size, 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(a, &picture), SVT_AV2_ERROR_INVALID_STATE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&a), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&b), 0);
    free(input); free(saved); free(ra); free(rb);
}

int main(void) {
    check_sequence(8, SVT_AV2_CHROMA_420);
    check_sequence(10, SVT_AV2_CHROMA_420);
    check_sequence(10, SVT_AV2_CHROMA_444);
    SVT_TEST_REPORT("test_m4_contracts");
}
