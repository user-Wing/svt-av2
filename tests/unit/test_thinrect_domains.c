#include "block_geometry.h"
#include "coeff_syntax.h"
#include "intra_prediction.h"
#include "transform.h"
#include "test_util.h"

typedef struct Domain { uint32_t w,h,count; SvtAv2FrameGeometry* geometry; } Domain;
static int visit(void* user,const SvtAv2BlockView* block) {
    Domain* d=(Domain*)user;SvtAv2BlockView uv;
    SVT_TEST_CHECK_EQ_INT(block->w,d->w);SVT_TEST_CHECK_EQ_INT(block->h,d->h);
    SVT_TEST_CHECK_EQ_INT(svt_av2_chroma_view(d->geometry,block,&uv),0);
    const uint32_t shift=d->geometry->chroma_format==SVT_AV2_CHROMA_420;
    SVT_TEST_CHECK_EQ_INT(uv.w,d->w>>shift);SVT_TEST_CHECK_EQ_INT(uv.h,d->h>>shift);
    ++d->count;return 0;
}
int main(void) {
    for(uint32_t format=0u;format<3u;++format) {
        const uint32_t bd=format?10u:8u;
        const SvtAv2ChromaFormat chroma=format==2u?SVT_AV2_CHROMA_444:SVT_AV2_CHROMA_420;
        SvtAv2FrameGeometry g;SvtAv2FrameBuffer frame={0};
        SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init_ex(&g,128u,128u,64u,chroma),0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&frame,128u,128u,bd,chroma),0);
        for(uint16_t plan=880u;plan<=881u;++plan) {
            const uint16_t plans[4]={plan,plan,plan,plan};
            Domain d={plan==880u?8u:32u,plan==880u?32u:8u,0u,&g};
            SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_coded_plan(&g,plans,4u,visit,&d),0);
            SVT_TEST_CHECK_EQ_INT(d.count,64u);
            const SvtAv2IntraBlock y={16u,16u,d.w,d.h,0u};
            uint8_t pixels[1024]={0};int16_t residual[256]={0};int32_t coeff[256]={0};
            SVT_TEST_CHECK(svt_av2_intra_mrls_predict(&frame,&y,8u,1u,0u,0u,pixels,d.w*2u)!=0);
            for(uint32_t type=1u;type<=3u;++type)
                SVT_TEST_CHECK(svt_av2_fwd_transform_type_bd(residual,coeff,d.w,d.h,bd,type)!=0);
            const uint32_t shift=chroma==SVT_AV2_CHROMA_420;
            const SvtAv2IntraBlock uv={8u,8u,d.w>>shift,d.h>>shift,1u};
            SVT_TEST_CHECK_EQ_INT(svt_av2_intra_cfl_implicit_predict(&frame,&uv,64u,pixels,uv.w*2u,NULL),0);
            SVT_TEST_CHECK_EQ_INT(svt_av2_intra_cfl_explicit_predict(&frame,&uv,64u,pixels,uv.w*2u,-8),0);
            SVT_TEST_CHECK_EQ_INT(svt_av2_intra_cfl_explicit_predict(&frame,&uv,64u,pixels,uv.w*2u,8),0);
            SVT_TEST_CHECK(svt_av2_intra_mhccp_allowed(d.w,d.h,(uint32_t)chroma));
        }
        const uint16_t invalid[4]={882u,0u,0u,0u};
        SVT_TEST_CHECK(svt_av2_partition_plan_validate(&g,invalid,4u)!=0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init_ex(&g,72u,88u,64u,chroma),0);
        const uint16_t partial[4]={880u,881u,0u,0u};
        SVT_TEST_CHECK(svt_av2_partition_plan_validate(&g,partial,4u)!=0);
        svt_av2_picture_free(&frame);
    }
    SVT_TEST_REPORT("test_thinrect_domains");
}
