#include "svt_av2.h"
#include "test_util.h"
#include <stdlib.h>

static SvtAv2Handle configure(uint32_t still, uint32_t basic, uint32_t rd) {
    SvtAv2Handle h = NULL;
    const uint32_t side = 64u;
    const char* output = "still-test";
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&h, NULL), SVT_AV2_ERROR_NONE);
    SvtAv2EncoderConfig defaults;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(h, &defaults), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(defaults.still_picture, 0u);
    SVT_TEST_CHECK_EQ_INT(defaults.transform_selection, 0u);
    SVT_TEST_CHECK_EQ_INT(defaults.mrl_search, 0u);
    SVT_TEST_CHECK_EQ_INT(defaults.tree_refinement, 0u);
    SVT_TEST_CHECK_EQ_INT(defaults.cfl_search, 0u);
    SVT_TEST_CHECK_EQ_INT(defaults.mhccp_search, 0u);
    SVT_TEST_CHECK_EQ_INT(defaults.intra_joint, 0u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_WIDTH, &side), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_HEIGHT, &side), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_OUTPUT_PATH, &output), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_STILL_PICTURE, &still), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_INTRA_MODE_SELECTION, &basic), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_RD_MODE, &rd), SVT_AV2_ERROR_NONE);
    return h;
}

int main(void) {
    SvtAv2Handle h = configure(1u, 3u, 2u);
    const uint32_t adaptive = 1u, fine = 2u, mixed = 3u, primary = 1u, full = 2u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_PARTITION_SELECTION, &mixed), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_ENTROPY_ADAPTATION, &adaptive), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_TRANSFORM_SELECTION, &full), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_ANGLE_SEARCH, &primary), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_MRL_SEARCH, &primary), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_TREE_REFINEMENT, &primary), 0);
    uint32_t invalid = 2u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_STILL_PICTURE, &invalid), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_ANGLE_SEARCH, &invalid), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_MRL_SEARCH, &invalid), SVT_AV2_ERROR_INVALID_PARAMETER);
    const uint32_t invalid_tree = 3u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_TREE_REFINEMENT, &invalid_tree), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(h), SVT_AV2_ERROR_NONE);
    uint8_t source[6144], stream[65536];
    for (size_t i = 0u; i < sizeof(source); ++i) source[i] = (uint8_t)(i * 7u + i / 64u);
    const SvtAv2Picture picture = {{source, source + 4096, source + 5120},
        {64u,32u,32u},64u,64u,SVT_AV2_CHROMA_420,SVT_AV2_BIT_DEPTH_8,17};
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(h, &picture), SVT_AV2_ERROR_NONE);
    size_t required = 0u;
    for (unsigned retry = 0u; retry < 17u; ++retry) {
        uint8_t sentinel = 0xA5u;
        SvtAv2Packet tiny = {&sentinel, 1u, 0u, 0};
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(h, &tiny), SVT_AV2_ERROR_OUT_OF_MEMORY);
        SVT_TEST_CHECK_EQ_INT(sentinel, 0xA5u);
        if (retry) SVT_TEST_CHECK_EQ_INT(tiny.size, required);
        required = tiny.size;
    }
    SvtAv2Packet packet = {stream,sizeof(stream),0u,0};
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(h, &packet), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(packet.size, required);
    /* TD, then length-prefixed sequence header. Its first uvlc(0) and five
     * profile bits precede single_picture_header_flag in bit 1. */
    SVT_TEST_CHECK(packet.size > 4u && stream[0] == 1u && stream[1] == 8u &&
        stream[3] == 4u && (stream[4] & 2u) != 0u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(h, &picture), SVT_AV2_ERROR_INVALID_STATE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(h, NULL), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&h), SVT_AV2_ERROR_NONE);

    h = configure(1u, 4u, 2u);
    const uint32_t mixed8 = 7u, thinrect = 8u, mixedthin = 9u, thin64 = 10u, invalid_partition = 11u;
    SvtAv2EncoderConfig actual;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_PARTITION_SELECTION, &mixed8), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_PARTITION_SELECTION, &invalid_partition), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(h, &actual), 0);
    SVT_TEST_CHECK_EQ_INT(actual.partition_selection, mixed8);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_PARTITION_SELECTION, &thinrect), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(h, &actual), 0);
    SVT_TEST_CHECK_EQ_INT(actual.partition_selection, thinrect);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_PARTITION_SELECTION, &mixedthin), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(h, &actual), 0);
    SVT_TEST_CHECK_EQ_INT(actual.partition_selection, mixedthin);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_PARTITION_SELECTION, &thin64), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(h, &actual), 0);
    SVT_TEST_CHECK_EQ_INT(actual.partition_selection, thin64);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(h), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&h), SVT_AV2_ERROR_NONE);

    for (uint32_t still=0u; still<=1u; ++still)
    for (uint32_t rd=0u; rd<=2u; ++rd)
    for (uint32_t ai=1u; ai<=2u; ++ai) {
        h = configure(still, 0u, rd);
        const uint32_t bad=3u;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(h, &actual), 0);
        SVT_TEST_CHECK_EQ_INT(actual.rd_lambda_model, 0u);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_RD_LAMBDA_MODEL, &ai), 0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_RD_LAMBDA_MODEL, &bad), SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_RD_LAMBDA_MODEL, NULL), SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(h, &actual), 0);
        SVT_TEST_CHECK_EQ_INT(actual.rd_lambda_model, ai);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(h), still && rd ? SVT_AV2_ERROR_NONE : SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&h), SVT_AV2_ERROR_NONE);
    }

    h = configure(1u, 0u, 0u);
    const SvtAv2PredictionStructure inter = SVT_AV2_PRED_LOW_DELAY_P;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_PREDICTION_STRUCTURE, &inter), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(h), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&h), SVT_AV2_ERROR_NONE);
    for (uint32_t basic = 2u; basic <= 3u; ++basic)
    for (uint32_t still = 0u; still <= 1u; ++still) {
        h = configure(still, basic, 0u);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(h), SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&h), SVT_AV2_ERROR_NONE);
    }
    for (uint32_t basic = 0u; basic < 2u; ++basic) {
        h = configure(1u, basic, 2u);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_PARTITION_SELECTION, &fine), 0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(h), SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&h), 0);
    }

    for (uint32_t basic=0u; basic<=4u; ++basic)
    for (uint32_t still=0u; still<=1u; ++still)
    for (uint32_t rd=0u; rd<=2u; ++rd) {
        h = configure(still, basic, rd);
        const uint32_t search=1u, bad=2u;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_CFL_SEARCH, &search), 0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_CFL_SEARCH, &bad), SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_CFL_SEARCH, NULL), SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(h, &actual), 0);
        SVT_TEST_CHECK_EQ_INT(actual.cfl_search, search);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(h), still && rd && basic==4u ? SVT_AV2_ERROR_NONE : SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&h), 0);
        h = configure(still, basic, rd);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_MHCCP_SEARCH, &search), 0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_MHCCP_SEARCH, &bad), SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_MHCCP_SEARCH, NULL), SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(h, &actual), 0);
        SVT_TEST_CHECK_EQ_INT(actual.mhccp_search, search);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(h), still && rd && basic==4u ? SVT_AV2_ERROR_NONE : SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&h), 0);
    }
    for (uint32_t basic=0u; basic<=4u; ++basic)
    for (uint32_t still=0u; still<=1u; ++still)
    for (uint32_t rd=0u; rd<=2u; ++rd)
    for (uint32_t tx=0u; tx<=2u; ++tx) {
        h=configure(still,basic,rd);
        const uint32_t on=1u,bad=2u;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h,SVT_AV2_PARAM_TRANSFORM_SELECTION,&tx),0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h,SVT_AV2_PARAM_INTRA_JOINT,&on),0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h,SVT_AV2_PARAM_INTRA_JOINT,&bad),SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h,SVT_AV2_PARAM_INTRA_JOINT,NULL),SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(h,&actual),0);
        SVT_TEST_CHECK_EQ_INT(actual.intra_joint,on);
        const uint32_t off=0u;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h,SVT_AV2_PARAM_INTRA_JOINT,&off),0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(h,&actual),0);
        SVT_TEST_CHECK_EQ_INT(actual.intra_joint,off);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h,SVT_AV2_PARAM_INTRA_JOINT,&on),0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(h),still && rd && basic>=3u && tx==2u ?
            SVT_AV2_ERROR_NONE:SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&h),0);
    }
    const uint32_t invalid_tx = 3u;
    for (uint32_t basic = 0u; basic < 3u; ++basic) {
        h = configure(basic == 2u ? 0u : 1u, basic, 2u);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_TRANSFORM_SELECTION, &primary), 0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_TRANSFORM_SELECTION, &invalid_tx), SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(h), SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&h), 0);
    }
    /* Fresh single-picture handles, including the 1/2/3/17/1000 checkpoints.
     * A tiny partial SB exercises the same basic/CDF transactions economically. */
    uint8_t first_stream[4096]; size_t first_size = 0u;
    for (uint32_t attempt = 0u; attempt < 1000u; ++attempt) {
        h = configure(1u, 2u, 2u);
        const uint32_t side = 8u;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_WIDTH, &side), 0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_HEIGHT, &side), 0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(h, SVT_AV2_PARAM_ENTROPY_ADAPTATION, &adaptive), 0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(h), 0);
        const SvtAv2Picture small = {{source,source+64,source+80},{8u,4u,4u},
            8u,8u,SVT_AV2_CHROMA_420,SVT_AV2_BIT_DEPTH_8,0};
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(h, &small), 0);
        SvtAv2Packet result = {stream,sizeof(stream),0u,0};
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(h, &result), 0);
        if (!attempt) {
            first_size = result.size;
            SVT_TEST_CHECK(first_size <= sizeof(first_stream));
            if (first_size <= sizeof(first_stream)) memcpy(first_stream,stream,first_size);
        } else {
            SVT_TEST_CHECK_EQ_INT(result.size, first_size);
            SVT_TEST_CHECK(first_size <= sizeof(first_stream) && memcmp(first_stream,stream,first_size) == 0);
        }
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&h), 0);
    }
    SVT_TEST_REPORT("test_still_picture");
}
