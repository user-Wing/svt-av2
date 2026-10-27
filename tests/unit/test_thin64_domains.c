#include "block_geometry.h"
#include "transform.h"
#include "coeff_syntax.h"
#include "intra_prediction.h"
#include "test_util.h"
#include <stdlib.h>

typedef struct Leaves { uint32_t count, plan; } Leaves;
static int leaf(void* user, const SvtAv2BlockView* b) {
    Leaves* l=(Leaves*)user;
    SVT_TEST_CHECK_EQ_INT(b->w,l->plan==3283u?8u:64u);
    SVT_TEST_CHECK_EQ_INT(b->h,l->plan==3283u?64u:8u);
    ++l->count;return 0;
}
int main(void) {
    SvtAv2FrameGeometry g;
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&g,64u,64u,64u),0);
    if(getenv("SVT_AV2_THIN64_OLD_NEGATIVE")) {
        const uint16_t old_invalid=3283u;
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_plan_validate(&g,&old_invalid,1u),SVT_AV2_ERROR_INVALID_PARAMETER);
    }
    for(uint32_t plan=3283u;plan<=3284u;++plan) {
        const uint16_t p=(uint16_t)plan;Leaves l={0u,plan};
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_plan_validate(&g,&p,1u),0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_sb_plan(&g,0u,&p,1u,leaf,&l),0);
        SVT_TEST_CHECK_EQ_INT(l.count,8u);
        const SvtAv2BlockView b={0u,0u,plan==3283u?8u:64u,plan==3283u?64u:8u,NULL};
        SvtAv2BlockView uv;SvtAv2PartitionType part;int implied=0;
        SVT_TEST_CHECK_EQ_INT(svt_av2_chroma_view(&g,&b,&uv),0);
        SVT_TEST_CHECK_EQ_INT(uv.w,plan==3283u?4u:32u);
        SVT_TEST_CHECK_EQ_INT(uv.h,plan==3283u?32u:4u);
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_coded_type_plan(&g,&b,&p,1u,&part,&implied),0);
        SVT_TEST_CHECK_EQ_INT(part,SVT_AV2_PARTITION_NONE);
        SVT_TEST_CHECK_EQ_INT(implied,1);
    }
    const uint16_t invalid[4]={882u,3282u,3285u,UINT16_MAX};
    for(uint32_t i=0u;i<4u;++i)
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_plan_validate(&g,invalid+i,1u),SVT_AV2_ERROR_INVALID_PARAMETER);
    const uint32_t w[4]={8u,64u,4u,32u},h[4]={64u,8u,32u,4u};
    int16_t residual[512]={0},inverse[512];int32_t coeff[512];
    for(uint32_t i=0u;i<4u;++i) {
        SvtAv2TxSize tx;uint16_t scan[512];
        SVT_TEST_CHECK_EQ_INT(svt_av2_tx_supported(w[i],h[i]),1);
        SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_tx_size_from_dims(w[i],h[i],&tx),0);
        SVT_TEST_CHECK_EQ_INT((uint32_t)tx,19u+i);
        SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_scan_dct_dct(tx,scan),i<2u?256u:128u);
        SVT_TEST_CHECK_EQ_INT(svt_av2_fwd_transform_type_bd(residual,coeff,w[i],h[i],8u,0u),0);
        for(uint32_t type=1u;type<=3u;++type) {
            SVT_TEST_CHECK_EQ_INT(svt_av2_fwd_transform_type_bd(residual,coeff,w[i],h[i],8u,type),SVT_AV2_TX_ERR_UNSUPPORTED);
            SVT_TEST_CHECK_EQ_INT(svt_av2_inv_transform_type_bd(coeff,inverse,w[i],h[i],8u,type),SVT_AV2_TX_ERR_UNSUPPORTED);
        }
    }
    SvtAv2FrameBuffer recon={0};uint8_t prediction[512];
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&recon,128u,128u,8u,SVT_AV2_CHROMA_420),0);
    for(uint32_t i=0u;i<2u;++i) {
        const SvtAv2IntraBlock b={16u,16u,w[i],h[i],0u};
        SVT_TEST_CHECK_EQ_INT(svt_av2_intra_mrls_predict(&recon,&b,8u,1u,0u,0u,prediction,w[i]),SVT_AV2_ERROR_INVALID_PARAMETER);
    }
    svt_av2_picture_free(&recon);
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&g,72u,88u,64u),0);
    const uint16_t partial[4]={3283u,0u,0u,0u},bad[4]={3283u,3284u,0u,0u};
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_plan_validate(&g,partial,4u),0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_plan_validate(&g,bad,4u),SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_REPORT("test_thin64_domains");
}
