/* Forced normative modes, independent of the production candidate selector. */
#include "intra_prediction.h"
#include "transform_path.h"
#include "keyframe.h"
#include "test_util.h"
#include <stdlib.h>

static const uint32_t modes[3] = {0u,1u,2u};

typedef struct Forced {
    SvtAv2FrameBuffer source, recon;
    SvtAv2FrameGeometry geometry;
    SvtAv2ReconScratch scratch;
    SvtAv2IntraTransformPath path;
    SvtAv2TileCoeffBlock blocks[1536];
    int32_t coeffs[65536];
    uint32_t y_mode, uv_mode, mixed;
    int32_t alpha_u, alpha_v;
    size_t count, used;
} Forced;

static int encode_leaf(void* user, const SvtAv2BlockView* y_block) {
    Forced* f = (Forced*)user;
    SvtAv2BlockView blocks[3];
    blocks[0] = *y_block;
    if (svt_av2_chroma_view(&f->geometry, y_block, &blocks[1]) != 0) return -1;
    blocks[2] = blocks[1];
    const uint32_t step = f->source.bit_depth > 8u ? 2u : 1u;
    for (uint32_t p = 0u; p < 3u; ++p) {
        const SvtAv2BlockView* b = blocks + p;
        const SvtAv2IntraBlock intra = {b->x,b->y,b->w,b->h,p};
        uint8_t* dst = f->recon.planes[p] + (size_t)b->y * f->recon.strides[p] + b->x * step;
        const uint32_t leaf=(uint32_t)(f->count/3u);
        const uint32_t classic=!f->mixed || leaf%4u<2u;
        const uint32_t mode=p?(f->mixed && leaf%4u==3u?0u:14u):modes[(f->y_mode+leaf)%3u];
        int tr = 0, bl = 0;
        const int32_t scale = p && f->source.chroma_format == SVT_AV2_CHROMA_420 ? 2 : 1;
        const int32_t tx = (int32_t)(b->x+b->w)*scale, ty = ((int32_t)b->y-1)*scale;
        const int32_t lx = ((int32_t)b->x-1)*scale, ly = (int32_t)(b->y+b->h)*scale;
        for (size_t i=0u; i+3u<=f->count; i+=3u) {
            const SvtAv2TileCoeffBlock* prev = f->blocks+i;
            tr |= tx>=(int32_t)prev->x && tx<(int32_t)(prev->x+prev->width) &&
                ty>=(int32_t)prev->y && ty<(int32_t)(prev->y+prev->height);
            bl |= lx>=(int32_t)prev->x && lx<(int32_t)(prev->x+prev->width) &&
                ly>=(int32_t)prev->y && ly<(int32_t)(prev->y+prev->height);
        }
        SvtAv2Error prediction;
        if (p && mode == 14u) {
            prediction=classic?svt_av2_intra_cfl_explicit_predict(&f->recon,&intra,64u,dst,
                f->recon.strides[p],p==1u?f->alpha_u:f->alpha_v):
                svt_av2_intra_cfl_implicit_predict(&f->recon,&intra,64u,dst,f->recon.strides[p],NULL);
        } else prediction = svt_av2_intra_basic_predict(&f->recon, &intra, mode, tr, bl, dst, f->recon.strides[p]);
        if (prediction != 0 ||
            svt_av2_residual_compute(&f->source, &f->recon, &intra, f->scratch.residual, f->scratch.stride) != 0)
            return -1;
        int zero;
        if (f->path.forward(f->path.user,p,b->w,b->h,f->scratch.residual,f->scratch.stride,
            f->scratch.coeff,f->scratch.qcoeff,f->scratch.dqcoeff,&zero) != 0 ||
            f->path.inverse(f->path.user,p,b->w,b->h,f->scratch.dqcoeff,f->scratch.stride,
                f->scratch.inverse_residual) != 0 ||
            svt_av2_recon_write_block(&f->recon,p,b,dst,f->recon.strides[p],
                f->scratch.inverse_residual,f->scratch.stride) != 0) return -1;
        const size_t count = (size_t)SVT_AV2_MIN(b->w,32u) * SVT_AV2_MIN(b->h,32u);
        if (f->count >= 1536u || count > 65536u - f->used) return -1;
        SvtAv2TileCoeffBlock* out = f->blocks + f->count++;
        out->plane=p; out->x=b->x; out->y=b->y; out->width=b->w; out->height=b->h;
        out->intra_mode=(uint8_t)mode; out->qcoeff=f->coeffs+f->used;
        out->cfl_explicit=(uint8_t)(p==0u && classic);
        const uint32_t usign=f->alpha_u<0?1u:f->alpha_u>0?2u:0u;
        const uint32_t vsign=f->alpha_v<0?1u:f->alpha_v>0?2u:0u;
        const uint32_t ui=f->alpha_u?(uint32_t)(f->alpha_u<0?-f->alpha_u:f->alpha_u)-1u:0u;
        const uint32_t vi=f->alpha_v?(uint32_t)(f->alpha_v<0?-f->alpha_v:f->alpha_v)-1u:0u;
        out->cfl_alpha_signs=(uint8_t)(p==0u && classic?usign*3u+vsign-1u:0u);
        out->cfl_alpha_idx=(uint8_t)(p==0u && classic?(ui<<3u)|vi:0u);
        memcpy(f->coeffs+f->used,f->scratch.qcoeff,count*sizeof(int32_t)); f->used+=count;
    }
    return 0;
}

static void write_file(const char* folder, const char* label, const char* suffix,
    const void* bytes, size_t size) {
    char name[1024];
    snprintf(name,sizeof(name),"%s/%s.%s",folder,label,suffix);
    FILE* file = fopen(name,"wb");
    SVT_TEST_CHECK(file != NULL);
    if (file) { SVT_TEST_CHECK_EQ_INT(fwrite(bytes,1u,size,file),size); fclose(file); }
}


static void transaction_test(Forced* f, const uint16_t* decisions, size_t count, uint32_t adaptive) {
    SvtAv2FrameHeaderConfig cfg;
    svt_av2_frame_header_config_default(&cfg,f->source.visible_width,f->source.visible_height);
    cfg.bit_depth=f->source.bit_depth;cfg.chroma_format=(uint32_t)f->source.chroma_format;
    cfg.base_q_idx=100u+(cfg.bit_depth==10u?48u:0u);cfg.m5_intra_modes=4u;
    cfg.reduced_tx_set=0u;cfg.entropy_adaptation=adaptive;
    SvtAv2TileRateSession* session=NULL;
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_create(&cfg,&session),0);
    if (!session) return;
    size_t blocks=0u;
    for (;blocks<f->count;blocks+=3u) if (f->blocks[blocks].x>=64u || f->blocks[blocks].y>=64u) break;
    uint64_t original=0u,repeat=0u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_trial(session,0u,decisions[0],f->blocks,blocks,NULL,0u,0,&original),0);
    const SvtAv2TileCoeffBlock saved=f->blocks[0];
    f->blocks[0].cfl_alpha_signs=8u;
    SVT_TEST_CHECK(svt_av2_tile_rate_trial(session,0u,decisions[0],f->blocks,blocks,NULL,0u,1,&repeat)!=0);
    f->blocks[0]=saved;f->blocks[0].cfl_alpha_idx=64u;
    SVT_TEST_CHECK(svt_av2_tile_rate_trial(session,0u,decisions[0],f->blocks,blocks,NULL,0u,1,&repeat)!=0);
    f->blocks[0]=saved;f->blocks[0].cfl_explicit=2u;
    SVT_TEST_CHECK(svt_av2_tile_rate_trial(session,0u,decisions[0],f->blocks,blocks,NULL,0u,1,&repeat)!=0);
    f->blocks[0]=saved;++f->blocks[1].x;
    SVT_TEST_CHECK(svt_av2_tile_rate_trial(session,0u,decisions[0],f->blocks,blocks,NULL,0u,1,&repeat)!=0);
    --f->blocks[1].x;
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_trial(session,0u,decisions[0],f->blocks,blocks,NULL,0u,0,&repeat),0);
    SVT_TEST_CHECK_EQ_INT(repeat,original);
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_trial(session,0u,decisions[0],f->blocks,blocks,NULL,0u,1,&repeat),0);
    SVT_TEST_CHECK_EQ_INT(repeat,original);
    SVT_TEST_CHECK(count>0u);
    svt_av2_tile_rate_free(session);
}

int main(int argc, char** argv) {
    const int pilot=getenv("SVT_AV2_CLASSIC_CFL_PILOT")!=NULL;
    const uint16_t plans[12]={0u,1u,2u,3u,85u,86u,87u,88u,89u,90u,91u,92u};
    const int8_t representative[8][2]={{0,-8},{0,8},{-8,0},{-8,-8},{-8,8},{8,0},{8,-8},{8,8}};
    size_t emitted=0u;
    for (uint32_t format=0u;format<3u;++format) for (uint32_t geometry=0u;geometry<3u;++geometry) {
        Forced* f=(Forced*)calloc(1u,sizeof(*f));SVT_TEST_CHECK(f!=NULL);if(!f)return 1;
        const uint32_t w=geometry?128u:64u,h=geometry==2u?72u:geometry?128u:64u;
        const uint32_t bd=format?10u:8u;
        const SvtAv2ChromaFormat chroma=format==2u?SVT_AV2_CHROMA_444:SVT_AV2_CHROMA_420;
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&f->source,w,h,bd,chroma),0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&f->recon,w,h,bd,chroma),0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init_ex(&f->geometry,w,h,64u,chroma),0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(&f->scratch,64u,64u),0);
        for(uint32_t p=0u;p<3u;++p)for(uint32_t y=0u;y<svt_av2_picture_plane_height(&f->source,p);++y)
            for(uint32_t x=0u;x<svt_av2_picture_plane_width(&f->source,p);++x)
                svt_av2_sample_store(f->source.planes[p]+(size_t)y*f->source.strides[p]+x*(bd>8u?2u:1u),bd,
                    (uint16_t)(((x*y*17u+x*43u+y*29u+p*53u)&255u)<<(bd-8u)));
        SvtAv2TransformPathState state;
        const uint32_t q=100u+(bd==10u?48u:0u);
        SVT_TEST_CHECK_EQ_INT(svt_av2_transform_path_init_bd(&state,&f->path,q,bd),0);
        for (uint32_t shape=0u;shape<12u;++shape) for (uint32_t pair=0u;pair<289u;++pair) {
            if (pilot && (geometry || shape!=2u || pair!=9u)) continue;
            int32_t u=(int32_t)(pair/17u)-8,v=(int32_t)(pair%17u)-8;
            if (!u && !v) continue;
            int selected=0;
            for (uint32_t i=0u;i<8u;++i) if (u==representative[i][0] && v==representative[i][1]) selected=1;
            if ((geometry || shape!=2u) && !selected) continue;
            if (geometry && shape!=2u && shape!=8u && shape!=9u) continue;
            f->alpha_u=u;f->alpha_v=v;f->mixed=geometry!=0u;f->y_mode=shape%3u;
            for(uint32_t adaptive=0u;adaptive<2u;++adaptive)for(uint32_t tx=0u;tx<3u;++tx) {
                if (!geometry && tx!=2u) continue;
                f->count=f->used=0u;
                uint16_t decisions[16]={0};
                const size_t sb_count=(size_t)f->geometry.sb_rows*f->geometry.sb_cols;
                for(size_t sb=0u;sb<sb_count;++sb) {
                    const uint32_t x=(uint32_t)(sb%f->geometry.sb_cols)*64u,y=(uint32_t)(sb/f->geometry.sb_cols)*64u;
                    if (x+64u<=w && y+64u<=h) decisions[sb]=geometry && sb?(uint8_t)(sb%2u?100u:179u):plans[shape];
                }
                SVT_TEST_CHECK_EQ_INT(svt_av2_recon_reset_frame(&f->recon,(uint16_t)(1u<<(bd-1u))),0);
                for(size_t sb=0u;sb<sb_count;++sb)
                    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_sb_plan(&f->geometry,sb,decisions,sb_count,encode_leaf,f),0);
                uint8_t stream[262144];size_t bytes=0u;
                SVT_TEST_CHECK_EQ_INT(svt_av2_encode_still_picture_tx(w,h,bd,(uint32_t)chroma,q,f->blocks,f->count,4u,
                    decisions,sb_count,adaptive,tx,stream,sizeof(stream),&bytes),0);
                if(!geometry && shape==2u && u==-8 && v==-8) transaction_test(f,decisions,sb_count,adaptive);
                if(argc>1 && bytes) {
                    char label[96];snprintf(label,sizeof(label),"f%u-g%u-s%u-u%d-v%d-a%u-t%u",format,geometry,shape,u,v,adaptive,tx);
                    write_file(argv[1],label,"av2",stream,bytes);
                    uint8_t raw[98304];size_t size=0u;
                    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_export_raw(&f->recon,raw,sizeof(raw),&size),0);
                    write_file(argv[1],label,"yuv",raw,size);++emitted;
                }
            }
        }
        svt_av2_recon_pool_free(&f->scratch);svt_av2_picture_free(&f->source);svt_av2_picture_free(&f->recon);free(f);
    }
    if(argc>1)printf("EMITTED,%zu\n",emitted);
    SVT_TEST_REPORT("test_intra_classic_cfl");
}
