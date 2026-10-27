#include "rd_frame.h"
static SvtAv2Error deep_encode(const SvtAv2FrameBuffer* source,SvtAv2FrameBuffer* recon,
    const SvtAv2ReferenceFrame* reference,const SvtAv2RdFrameConfig* config,
    SvtAv2ReconScratch* scratch,SvtAv2RdFrameOutput* output,SvtAv2RdFrameStats* stats);
int test_mhccp_original_main(int argc,char** argv);
#define main test_mhccp_original_main
#define svt_av2_rd_frame_encode deep_encode
#include "test_mhccp_rd.c"
#undef svt_av2_rd_frame_encode
#undef main
typedef struct DeepFault {SvtAv2IntraTransformPath real;uint64_t calls,fail_at;int armed;} DeepFault;
static int injected;
static SvtAv2Error deep_forward(void* user,uint32_t p,uint32_t w,uint32_t h,const int16_t* r,
    uint32_t stride,int32_t* c,int32_t* q,int32_t* dq,int* zero) {
    DeepFault* f=(DeepFault*)user;
    const SvtAv2Error error=f->real.forward(f->real.user,p,w,h,r,stride,c,q,dq,zero);
    return ++f->calls==f->fail_at?SVT_AV2_ERROR_INVALID_STATE:error;
}
static SvtAv2Error deep_forward_type(void* user,uint32_t p,uint32_t w,uint32_t h,const int16_t* r,
    uint32_t stride,int32_t* c,int32_t* q,int32_t* dq,int* zero,uint32_t type) {
    DeepFault* f=(DeepFault*)user;
    const SvtAv2Error error=f->real.forward_type(f->real.user,p,w,h,r,stride,c,q,dq,zero,type);
    return ++f->calls==f->fail_at?SVT_AV2_ERROR_INVALID_STATE:error;
}
static SvtAv2Error deep_inverse(void* user,uint32_t p,uint32_t w,uint32_t h,const int32_t* c,uint32_t stride,int16_t* r) {
    DeepFault* f=(DeepFault*)user;return f->real.inverse(f->real.user,p,w,h,c,stride,r);
}
static SvtAv2Error deep_inverse_type(void* user,uint32_t p,uint32_t w,uint32_t h,const int32_t* c,uint32_t stride,int16_t* r,uint32_t type) {
    DeepFault* f=(DeepFault*)user;return f->real.inverse_type(f->real.user,p,w,h,c,stride,r,type);
}
static void arm(void* user,size_t sb,uint64_t id,uint64_t d,uint64_t r,uint64_t cost,int committed) {
    DeepFault* f=(DeepFault*)user;(void)sb;(void)d;(void)r;(void)cost;
    if(!committed && (id&(UINT32_C(1)<<30u)) && !f->armed) {f->armed=1;f->fail_at=f->calls+2u;}
}
static SvtAv2Error deep_encode(const SvtAv2FrameBuffer* source,SvtAv2FrameBuffer* recon,
    const SvtAv2ReferenceFrame* reference,const SvtAv2RdFrameConfig* config,
    SvtAv2ReconScratch* scratch,SvtAv2RdFrameOutput* output,SvtAv2RdFrameStats* stats) {
    if(!injected) {
        DeepFault fault={config->transform,0u,UINT64_MAX,0};SvtAv2RdFrameConfig broken=*config;
        broken.transform.user=&fault;broken.transform.forward=deep_forward;broken.transform.inverse=deep_inverse;
        broken.transform.forward_type=deep_forward_type;broken.transform.inverse_type=deep_inverse_type;
        broken.candidate_hook=arm;broken.candidate_hook_user=&fault;
        SVT_TEST_CHECK_EQ_INT(svt_av2_rd_frame_encode(source,recon,reference,&broken,scratch,output,stats),SVT_AV2_ERROR_INVALID_STATE);
        SVT_TEST_CHECK(fault.armed);SVT_TEST_CHECK_EQ_INT(fault.calls,fault.fail_at);
        SVT_TEST_CHECK_EQ_INT(output->block_count,0u);SVT_TEST_CHECK_EQ_INT(output->coeff_count,0u);
        printf("DEEP_MHCCP_FAULT,calls=%llu,armed=%d\n",(unsigned long long)fault.calls,fault.armed);injected=1;
    }
    return svt_av2_rd_frame_encode(source,recon,reference,config,scratch,output,stats);
}
static void transaction(uint32_t bd,SvtAv2ChromaFormat chroma,uint32_t adaptive) {
    int32_t zero[1024]={0};SvtAv2TileCoeffBlock blocks[12]={0};const uint32_t sub=chroma==SVT_AV2_CHROMA_420?1u:0u;
    for(uint32_t leaf=0u;leaf<4u;++leaf)for(uint32_t p=0u;p<3u;++p) {
        SvtAv2TileCoeffBlock* b=blocks+leaf*3u+p;b->plane=p;
        b->x=(leaf%2u*32u)>>(p?sub:0u);b->y=(leaf/2u*32u)>>(p?sub:0u);
        b->width=b->height=32u>>(p?sub:0u);b->qcoeff=zero;b->intra_mode=(uint8_t)(p?14u:0u);b->mhccp=(uint8_t)(p?0u:1u);
    }
    SvtAv2FrameHeaderConfig cfg;svt_av2_frame_header_config_default(&cfg,64u,64u);
    cfg.bit_depth=bd;cfg.chroma_format=(uint32_t)chroma;cfg.base_q_idx=100u+(bd==10u?48u:0u);
    cfg.m5_intra_modes=4u;cfg.enable_mhccp=1u;cfg.reduced_tx_set=0u;cfg.entropy_adaptation=adaptive;
    SvtAv2TileRateSession* rate=NULL;uint64_t original=0u,repeat=0u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_create(&cfg,&rate),0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_trial(rate,0u,1u,blocks,12u,NULL,0u,0,&original),0);
    blocks[0].mhccp=4u;SVT_TEST_CHECK(svt_av2_tile_rate_trial(rate,0u,1u,blocks,12u,NULL,0u,1,&repeat)!=0);
    blocks[0].mhccp=1u;blocks[0].cfl_explicit=1u;SVT_TEST_CHECK(svt_av2_tile_rate_trial(rate,0u,1u,blocks,12u,NULL,0u,1,&repeat)!=0);
    blocks[0].cfl_explicit=0u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_trial(rate,0u,1u,blocks,12u,NULL,0u,0,&repeat),0);SVT_TEST_CHECK_EQ_INT(repeat,original);
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_trial(rate,0u,1u,blocks,12u,NULL,0u,1,&repeat),0);SVT_TEST_CHECK_EQ_INT(repeat,original);
    svt_av2_tile_rate_free(rate);cfg.enable_mhccp=0u;rate=NULL;
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_create(&cfg,&rate),0);
    SVT_TEST_CHECK(svt_av2_tile_rate_trial(rate,0u,1u,blocks,12u,NULL,0u,1,&repeat)!=0);svt_av2_tile_rate_free(rate);
}
int main(int argc,char** argv) {
    const uint32_t format=argc>1?(uint32_t)strtoul(argv[1],NULL,10):0u,bd=format?10u:8u;
    const SvtAv2ChromaFormat chroma=format==2u?SVT_AV2_CHROMA_444:SVT_AV2_CHROMA_420;
    transaction(bd,chroma,0u);transaction(bd,chroma,1u);numeric_tests();source_pattern=1u;
    frame_test(64u,64u,bd,chroma,0,4u,1u,1u,2u,1u,1u);SVT_TEST_CHECK(injected);
    SVT_TEST_REPORT("test_mhccp_fault");
}
