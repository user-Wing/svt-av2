#include "intra_prediction.h"
#include "test_util.h"

int main(void) {
    for(uint32_t format=0u;format<3u;++format) {
        const uint32_t bd=format?10u:8u;
        const SvtAv2ChromaFormat chroma=format==2u?SVT_AV2_CHROMA_444:SVT_AV2_CHROMA_420;
        SvtAv2FrameBuffer recon={0};
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&recon,128u,128u,bd,chroma),0);
        const SvtAv2IntraBlock invalid[]={{1u,4u,8u,8u,1u},{4u,1u,8u,8u,1u},
            {UINT32_MAX-3u,4u,8u,8u,1u},{4u,UINT32_MAX-3u,8u,8u,1u},
            {4u,4u,UINT32_C(0x80000004),8u,1u},{4u,4u,8u,UINT32_C(0x80000004),1u},
            {4u,4u,5u,8u,1u},{4u,4u,8u,6u,1u},{4u,4u,64u,8u,1u},{4u,4u,4u,4u,1u},{4u,4u,0u,8u,1u}};
        uint8_t pixels[2048],saved[2048];int32_t params[3];
        for(size_t i=0u;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
            memset(pixels,0x5a,sizeof(pixels));memcpy(saved,pixels,sizeof(saved));
            params[0]=params[1]=params[2]=12345;
            SVT_TEST_CHECK_EQ_INT(svt_av2_intra_mhccp_predict(&recon,invalid+i,0u,0u,0u,pixels,64u,params),SVT_AV2_ERROR_INVALID_PARAMETER);
            SVT_TEST_CHECK(!memcmp(pixels,saved,sizeof(pixels)));
            SVT_TEST_CHECK(params[0]==12345 && params[1]==12345 && params[2]==12345);
        }
        for(uint32_t orientation=0u;orientation<2u;++orientation)for(uint32_t dir=0u;dir<3u;++dir) {
            const SvtAv2IntraBlock block={4u,4u,orientation?32u:4u,orientation?4u:32u,1u};
            SVT_TEST_CHECK_EQ_INT(svt_av2_intra_mhccp_predict(&recon,&block,dir,0u,0u,pixels,64u,params),0);
        }
        svt_av2_picture_free(&recon);
    }
    const uint32_t invalid_dims[]={0u,1u,3u,5u,6u,12u,UINT32_C(0x80000004),UINT32_MAX};
    for(size_t i=0u;i<sizeof(invalid_dims)/sizeof(invalid_dims[0]);++i) {
        SVT_TEST_CHECK(!svt_av2_intra_mhccp_allowed(invalid_dims[i],16u,1u));
        SVT_TEST_CHECK(!svt_av2_intra_mhccp_allowed(16u,invalid_dims[i],3u));
    }
    SVT_TEST_CHECK(!svt_av2_intra_mhccp_allowed(16u,16u,0u));
    SVT_TEST_CHECK(!svt_av2_intra_mhccp_allowed(16u,16u,2u));
    SVT_TEST_CHECK(!svt_av2_intra_mhccp_allowed(16u,16u,4u));
    SVT_TEST_REPORT("test_mhccp_boundaries");
}
