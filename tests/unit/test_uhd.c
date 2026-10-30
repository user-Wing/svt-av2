#include "intra_prediction.h"
#include "transform_path.h"
#include "keyframe.h"
#include "deblock_luma.h"
#include "pc_wiener.h"
#include "test_util.h"
#include <stdlib.h>

typedef struct Fixture {
    SvtAv2FrameBuffer source,recon;
    SvtAv2FrameGeometry geometry;
    SvtAv2ReconScratch scratch;
    SvtAv2IntraTransformPath path;
    SvtAv2TileCoeffBlock* blocks;
    int32_t* coeff;
    size_t count,used,capacity;
} Fixture;

static int leaf(void* user,const SvtAv2BlockView* y) {
    Fixture* f=(Fixture*)user;
    SvtAv2BlockView views[3];views[0]=*y;
    if(svt_av2_chroma_view(&f->geometry,y,&views[1]))return -1;
    views[2]=views[1];
    for(uint32_t p=0;p<3u;++p) {
        const SvtAv2BlockView* b=views+p;
        const SvtAv2IntraBlock intra={b->x,b->y,b->w,b->h,p};
        const uint32_t step=f->source.bit_depth>8u?2u:1u;
        uint8_t* dst=f->recon.planes[p]+(size_t)b->y*f->recon.strides[p]+b->x*step;
        int zero;
        if(svt_av2_intra_basic_predict(&f->recon,&intra,0u,0,0,dst,f->recon.strides[p]) ||
           svt_av2_residual_compute(&f->source,&f->recon,&intra,f->scratch.residual,f->scratch.stride) ||
           f->path.forward(f->path.user,p,b->w,b->h,f->scratch.residual,f->scratch.stride,
               f->scratch.coeff,f->scratch.qcoeff,f->scratch.dqcoeff,&zero) ||
           f->path.inverse(f->path.user,p,b->w,b->h,f->scratch.dqcoeff,f->scratch.stride,f->scratch.inverse_residual) ||
           svt_av2_recon_write_block(&f->recon,p,b,dst,f->recon.strides[p],f->scratch.inverse_residual,f->scratch.stride))return -1;
        if(f->count>=f->capacity)return -1;
        SvtAv2TileCoeffBlock* block=f->blocks+f->count++;
        block->plane=p;block->x=b->x;block->y=b->y;block->width=b->w;block->height=b->h;
        block->qcoeff=f->coeff+f->used;
        const size_t n=(size_t)SVT_AV2_MIN(b->w,32u)*SVT_AV2_MIN(b->h,32u);
        memcpy(f->coeff+f->used,f->scratch.qcoeff,n*sizeof(*f->coeff));f->used+=n;
    }
    return 0;
}

static void save(const char* folder,const char* label,const char* suffix,const void* data,size_t size) {
    char path[1024];snprintf(path,sizeof(path),"%s/%s.%s",folder,label,suffix);
    FILE* fp=fopen(path,"wb");SVT_TEST_CHECK(fp!=NULL);
    if(fp){SVT_TEST_CHECK_EQ_INT(fwrite(data,1u,size,fp),size);fclose(fp);}
}

static void run(const char* folder,uint32_t w,uint32_t h,uint32_t bd,uint32_t adaptive,uint32_t mixed) {
    Fixture* f=(Fixture*)calloc(1u,sizeof(*f));SVT_TEST_CHECK(f!=NULL);if(!f)return;
    const uint32_t q=110u+(bd==10u?48u:0u),step=bd>8u?2u:1u;
    const size_t pixels=(size_t)w*h,sbs=(size_t)((w+63u)/64u)*((h+63u)/64u);
    f->capacity=sbs*192u;f->blocks=(SvtAv2TileCoeffBlock*)calloc(f->capacity,sizeof(*f->blocks));
    f->coeff=(int32_t*)calloc(pixels*3u/2u,sizeof(*f->coeff));
    uint16_t* plan=(uint16_t*)calloc(sbs,sizeof(*plan));
    SVT_TEST_CHECK(f->blocks && f->coeff && plan);if(!f->blocks || !f->coeff || !plan)exit(2);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&f->source,w,h,bd,SVT_AV2_CHROMA_420),0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&f->recon,w,h,bd,SVT_AV2_CHROMA_420),0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&f->geometry,w,h,64u),0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(&f->scratch,64u,64u),0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_reset_frame(&f->recon,(uint16_t)(1u<<(bd-1u))),0);
    for(uint32_t p=0;p<3u;++p)for(uint32_t y=0;y<(h>>(p!=0));++y)for(uint32_t x=0;x<(w>>(p!=0));++x) {
        const uint32_t value=(x*7u+y*11u+((x/19u+y/23u)%7u)*17u+p*31u)&255u;
        svt_av2_sample_store(f->source.planes[p]+(size_t)y*f->source.strides[p]+x*step,bd,(uint16_t)(value<<(bd-8u)));
    }
    SvtAv2TransformPathState state;SVT_TEST_CHECK_EQ_INT(svt_av2_transform_path_init_bd(&state,&f->path,q,bd),0);
    for(size_t sb=0;sb<sbs;++sb)SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_sb_plan(&f->geometry,sb,plan,sbs,leaf,f),0);
    SvtAv2FrameHeaderConfig cfg;svt_av2_frame_header_config_default(&cfg,w,h);
    cfg.bit_depth=bd;cfg.chroma_format=1u;cfg.base_q_idx=q;cfg.entropy_adaptation=adaptive;
    cfg.partition_decisions=plan;cfg.partition_decision_count=sbs;cfg.reduced_tx_set=2u;
    SvtAv2TileRateSession* rate=NULL;SvtAv2TileRatePrefix* prefix=NULL;size_t leaves=0u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_create(&cfg,&rate),0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_prefix_create(&prefix),0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_prefix_begin(prefix,rate,0u,0u,&leaves),0);
    SVT_TEST_CHECK(leaves>0u);svt_av2_tile_rate_prefix_free(prefix);svt_av2_tile_rate_free(rate);
    uint64_t edges,changed;cfg.deblock_y=3u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_deblock_luma_apply(&f->recon,f->blocks,f->count,3u,0,0,q,&edges,&changed),0);
    uint8_t (*flags)[2]=(uint8_t (*)[2])calloc(sbs,2u);SVT_TEST_CHECK(flags!=NULL);if(!flags)exit(2);
    SvtAv2Ccso64Plan ccso={0};ccso.enabled[0]=ccso.enabled[1]=1u;ccso.offset[0][0]=1;ccso.offset[1][3]=-1;
    for(size_t u=0;u<sbs;++u){flags[u][0]=(uint8_t)(u&1u);flags[u][1]=(uint8_t)((u/3u)&1u);}
    ccso.unit_flags=(const uint8_t (*)[2])flags;ccso.unit_count=sbs;cfg.ccso64=&ccso;
    SVT_TEST_CHECK_EQ_INT(svt_av2_ccso64_picture_validate(&ccso,&f->recon),0);svt_av2_ccso64_apply(&ccso,&f->recon);
    uint16_t* src=(uint16_t*)malloc(pixels*sizeof(*src));uint16_t* dst=(uint16_t*)malloc(pixels*sizeof(*dst));
    uint8_t* skip=(uint8_t*)calloc(pixels/16u,1u);SVT_TEST_CHECK(src && dst && skip);if(!src || !dst || !skip)exit(2);
    for(size_t i=0;i<f->count;++i) {
        const SvtAv2TileCoeffBlock* b=f->blocks+i;if(b->plane)continue;
        uint8_t zero=1u;const size_t n=(size_t)SVT_AV2_MIN(b->width,32u)*SVT_AV2_MIN(b->height,32u);
        for(size_t j=0;j<n;++j)if(b->qcoeff[j]){zero=0u;break;}
        for(uint32_t y=b->y/4u;y<(b->y+b->height)/4u;++y)for(uint32_t x=b->x/4u;x<(b->x+b->width)/4u;++x)skip[(size_t)y*(w/4u)+x]=zero;
    }
    for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<w;++x)src[(size_t)y*w+x]=svt_av2_sample_load(f->recon.planes[0]+(size_t)y*f->recon.strides[0]+x*step,bd);
    SVT_TEST_CHECK_EQ_INT(svt_av2_pcwiener_filter_y(src,w,dst,w,w,h,bd,q,skip,w/4u),0);
    const uint32_t cols=SVT_AV2_MAX((w+128u)/256u,1u),rows=SVT_AV2_MAX((h+128u)/256u,1u);
    cfg.enable_pc_wiener=1u;
    for(uint32_t r=0;r<rows;++r)for(uint32_t c=0;c<cols;++c) {
        const uint32_t unit=r*cols+c;
        if(mixed && ((r+c)&1u)==0u && !(mixed==2u && unit==0u)) {
            cfg.pc_wiener_none_mask[unit/64u]|=UINT64_C(1)<<(unit&63u);
            const uint32_t top=r?r*256u-8u:0u,bottom=r+1u==rows?h:(r+1u)*256u-8u;
            const uint32_t left=c*256u,right=c+1u==cols?w:left+256u;
            for(uint32_t y=top;y<bottom;++y)memcpy(dst+(size_t)y*w+left,src+(size_t)y*w+left,(right-left)*sizeof(*dst));
        }
    }
    if(mixed==2u) {
        SVT_TEST_CHECK_EQ_INT(cols*rows,128u);
        SVT_TEST_CHECK((cfg.pc_wiener_none_mask[0]&1u)==0u);
        SVT_TEST_CHECK((cfg.pc_wiener_none_mask[1]>>63u)==1u);
    }
    for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<w;++x)svt_av2_sample_store(f->recon.planes[0]+(size_t)y*f->recon.strides[0]+x*step,bd,dst[(size_t)y*w+x]);
    const size_t capacity=pixels*16u+4096u;uint8_t* packet=(uint8_t*)malloc(capacity);uint8_t* raw=(uint8_t*)malloc(pixels*3u);
    SVT_TEST_CHECK(packet && raw);if(!packet || !raw)exit(2);size_t bytes=0u,raw_size=0u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_encode_still_picture_ccso64_packet(&cfg,f->blocks,f->count,packet,capacity,&bytes),0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_export_raw(&f->recon,raw,pixels*3u,&raw_size),0);
    char label[100];snprintf(label,sizeof(label),"w%u-h%u-b%u-a%u-m%u",w,h,bd,adaptive,mixed);
    if(folder){save(folder,label,"av2",packet,bytes);save(folder,label,"yuv",raw,raw_size);}
    printf("UHD %s blocks=%zu sb=%zu ru=%u mask=%llu,%llu bytes=%zu\n",label,f->count,sbs,cols*rows,(unsigned long long)cfg.pc_wiener_none_mask[0],(unsigned long long)cfg.pc_wiener_none_mask[1],bytes);
    free(packet);free(raw);free(flags);free(src);free(dst);free(skip);free(f->blocks);free(f->coeff);free(plan);
    svt_av2_recon_pool_free(&f->scratch);svt_av2_picture_free(&f->source);svt_av2_picture_free(&f->recon);free(f);
}

int main(int argc,char** argv) {
    if(argc>2) {run(argv[1],4096u,2160u,10u,1u,2u);SVT_TEST_REPORT("test_uhd_max");}
    static const uint32_t sizes[][2]={{80u,80u},{4096u,128u},{128u,2160u},{4096u,1152u}};
    for(size_t s=0;s<SVT_AV2_ARRAY_SIZE(sizes);++s)for(uint32_t bd=8u;bd<=10u;bd+=2u)
        for(uint32_t adaptive=0u;adaptive<2u;++adaptive)run(argc>1?argv[1]:NULL,sizes[s][0],sizes[s][1],bd,adaptive,1u);
    SVT_TEST_REPORT("test_uhd");
}
