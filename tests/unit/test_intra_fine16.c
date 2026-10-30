#ifndef M6_FINE16
#define M6_FINE16 0
#endif
#ifndef M6_FINE16_PAIR
#define M6_FINE16_PAIR 0
#endif
#if M6_FINE16
/* Forced normative modes, independent of the production candidate selector. */
#include "intra_prediction.h"
#include "transform_path.h"
#include "keyframe.h"
#include "test_util.h"
#include <stdlib.h>

static const uint32_t modes[7] = {0u,1u,2u,4u,5u,6u,7u};

typedef struct Forced {
    SvtAv2FrameBuffer source, recon;
    SvtAv2FrameGeometry geometry;
    SvtAv2ReconScratch scratch;
    SvtAv2IntraTransformPath path;
    SvtAv2TileCoeffBlock blocks[1536];
    int32_t coeffs[65536];
    uint32_t y_mode, uv_mode, mixed;
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
        const uint32_t leaf = (uint32_t)(f->count / 3u);
        const uint32_t mode = p ? (leaf % 3u ? 14u : modes[(leaf + 3u) % 7u]) : modes[leaf % 7u];
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
            prediction = svt_av2_intra_cfl_implicit_predict(&f->recon,&intra,64u,dst,f->recon.strides[p],NULL);
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


typedef struct Walk {
    SvtAv2BlockView blocks[64];
    size_t count;
} Walk;

static int collect(void* user, const SvtAv2BlockView* block) {
    Walk* walk = (Walk*)user;
    if (walk->count == 64u) return -1;
    walk->blocks[walk->count++] = *block;
    return 0;
}

static uint16_t fine16_test_plan(uint32_t q,uint32_t mask) {
    uint32_t others=(mask*37u+q*19u)%125u;
    if (q==0u && mask==1u) others=0u;
    if (q==3u && mask==14u) others=124u;
    if (q==2u && mask==7u) others=31u;
    return (uint16_t)(4096u+(q*14u+mask-1u)*125u+others);
}
static int fine16_events(const SvtAv2FrameGeometry* g,const SvtAv2BlockView* b,
    const uint16_t* plan,Walk* walk,uint32_t* events) {
    if (++*events>127u || b->w<8u || b->h<8u) return -1;
    SvtAv2PartitionType part;int implied=0;
    if (svt_av2_partition_coded_type_plan(g,b,plan,1u,&part,&implied)!=0) return -1;
    if (part==SVT_AV2_PARTITION_NONE) return collect(walk,b);
    if (part!=SVT_AV2_PARTITION_HORZ && part!=SVT_AV2_PARTITION_VERT) return -1;
    for (uint32_t i=0u;i<2u;++i) {
        SvtAv2BlockView child=*b;
        if (part==SVT_AV2_PARTITION_HORZ) {child.h/=2u;child.y+=i*child.h;}
        else {child.w/=2u;child.x+=i*child.w;}
        if (fine16_events(g,&child,plan,walk,events)!=0) return -1;
    }
    return 0;
}
static void geometry_tests(void) {
    static const uint32_t widths[5]={16u,8u,16u,8u,32u};
    static const uint32_t heights[5]={16u,8u,8u,16u,32u};
    SvtAv2FrameGeometry g;
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&g,64u,64u,64u),0);
    uint32_t max_leaves=0u,max_events=0u,checked=0u;
    for (uint32_t id=4096u;id<=11095u;++id) {
        const uint16_t decision=(uint16_t)id;
        const uint32_t code=id-4096u,special=code/125u/14u,mask=code/125u%14u+1u;
        uint32_t digits=code%125u,shapes[4]={0u},expected=4u;
        for (uint32_t k=0u;k<4u;++k) expected+=3u*((mask>>k)&1u);
        for (uint32_t q=0u;q<4u;++q) if (q!=special) {
            shapes[q]=digits%5u;digits/=5u;
            expected+=1024u/(widths[shapes[q]]*heights[shapes[q]]);
        }
        Walk walk={0},tree={0};uint32_t events=0u;
        const SvtAv2BlockView root={0u,0u,64u,64u,NULL};
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_sb_plan(&g,0u,&decision,1u,collect,&walk),0);
        SVT_TEST_CHECK_EQ_INT(fine16_events(&g,&root,&decision,&tree,&events),0);
        SVT_TEST_CHECK_EQ_INT(walk.count,expected);
        SVT_TEST_CHECK(walk.count>0u && walk.count<=64u && events<=127u);
        SVT_TEST_CHECK_EQ_INT(events,2u*walk.count-1u);
        SVT_TEST_CHECK_EQ_INT(tree.count,walk.count);
        uint8_t covered[64]={0};
        for (size_t i=0u;i<walk.count;++i) {
            const SvtAv2BlockView* b=walk.blocks+i;
            const int valid=b->w>=8u && b->h>=8u && b->x+b->w<=64u && b->y+b->h<=64u &&
                !(b->x%8u) && !(b->y%8u) && !(b->w%8u) && !(b->h%8u);
            SVT_TEST_CHECK(valid);if (!valid) continue;
            if (i<tree.count) {
                SVT_TEST_CHECK_EQ_INT(b->x,tree.blocks[i].x);SVT_TEST_CHECK_EQ_INT(b->y,tree.blocks[i].y);
                SVT_TEST_CHECK_EQ_INT(b->w,tree.blocks[i].w);SVT_TEST_CHECK_EQ_INT(b->h,tree.blocks[i].h);
            }
            const uint32_t q=b->y/32u*2u+b->x/32u;
            uint32_t w=widths[shapes[q]],h=heights[shapes[q]];
            if (q==special) {
                const uint32_t cell=(b->y%32u/16u)*2u+b->x%32u/16u;
                w=h=(mask&(1u<<cell))?8u:16u;
            }
            SVT_TEST_CHECK_EQ_INT(b->w,w);SVT_TEST_CHECK_EQ_INT(b->h,h);
            SVT_TEST_CHECK_EQ_INT(b->x%w,0u);SVT_TEST_CHECK_EQ_INT(b->y%h,0u);
            if (b->y) for (uint32_t x=b->x;x<b->x+b->w;x+=8u)
                SVT_TEST_CHECK_EQ_INT(covered[(b->y/8u-1u)*8u+x/8u],1u);
            if (b->x) for (uint32_t y=b->y;y<b->y+b->h;y+=8u)
                SVT_TEST_CHECK_EQ_INT(covered[y/8u*8u+b->x/8u-1u],1u);
            if (b->x && b->y) SVT_TEST_CHECK_EQ_INT(covered[(b->y/8u-1u)*8u+b->x/8u-1u],1u);
            for (uint32_t y=b->y;y<b->y+b->h;y+=8u) for (uint32_t x=b->x;x<b->x+b->w;x+=8u) {
                const uint32_t at=y/8u*8u+x/8u;
                SVT_TEST_CHECK_EQ_INT(covered[at],0u);covered[at]=1u;
            }
        }
        for (uint32_t i=0u;i<64u;++i) SVT_TEST_CHECK_EQ_INT(covered[i],1u);
        if (walk.count>max_leaves) max_leaves=(uint32_t)walk.count;
        if (events>max_events) max_events=events;
        ++checked;
    }
    SVT_TEST_CHECK_EQ_INT(checked,7000u);
    SVT_TEST_CHECK_EQ_INT(max_leaves,61u);SVT_TEST_CHECK_EQ_INT(max_events,121u);
    const uint16_t invalid[3]={4095u,M6_FINE16_PAIR?40496u:11096u,65535u};Walk untouched={0};
    for (uint32_t i=0u;i<3u;++i)
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_sb_plan(&g,0u,invalid+i,1u,collect,&untouched),SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(untouched.count,0u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&g,72u,72u,64u),0);
    const uint16_t partial[4]={4096u,0u,0u,4096u};
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_coded_plan(&g,partial,4u,collect,&untouched),SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(untouched.count,0u);
    printf("FINE16_GEOMETRY plans=%u max_leaves=%u max_events=%u\n",checked,max_leaves,max_events);
}

static void transaction_test(Forced* f,uint16_t decision,uint32_t adaptive) {
    SvtAv2FrameHeaderConfig cfg;
    svt_av2_frame_header_config_default(&cfg,64u,64u);
    cfg.bit_depth=f->source.bit_depth;cfg.chroma_format=(uint32_t)f->source.chroma_format;
    cfg.base_q_idx=100u+(cfg.bit_depth==10u?48u:0u);cfg.m5_intra_modes=4u;
    cfg.reduced_tx_set=0u;cfg.entropy_adaptation=adaptive;
    SvtAv2TileRateSession* session=NULL;
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_create(&cfg,&session),0);
    if (!session) return;
    uint64_t original=0u,repeat=0u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_trial(session,0u,decision,f->blocks,f->count,NULL,0u,0,&original),0);
    ++f->blocks[1].x;
    SVT_TEST_CHECK(svt_av2_tile_rate_trial(session,0u,decision,f->blocks,f->count,NULL,0u,1,&repeat)!=0);
    --f->blocks[1].x;
    SVT_TEST_CHECK(svt_av2_tile_rate_trial(session,0u,4095u,f->blocks,f->count,NULL,0u,1,&repeat)!=0);
    SVT_TEST_CHECK(svt_av2_tile_rate_trial(session,0u,M6_FINE16_PAIR?40496u:11096u,f->blocks,f->count,NULL,0u,1,&repeat)!=0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_trial(session,0u,decision,f->blocks,f->count,NULL,0u,0,&repeat),0);
    SVT_TEST_CHECK_EQ_INT(repeat,original);
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_trial(session,0u,decision,f->blocks,f->count,NULL,0u,1,&repeat),0);
    SVT_TEST_CHECK_EQ_INT(repeat,original);
    svt_av2_tile_rate_free(session);
}

int main(int argc, char** argv) {
    geometry_tests();
    if (g_svt_av2_test_failures) SVT_TEST_REPORT("test_intra_fine16");
    size_t emitted=0u;
    for (uint32_t format=0u; format<3u; ++format) for (uint32_t geometry=0u; geometry<3u; ++geometry) {
        Forced* f=(Forced*)calloc(1u,sizeof(*f));
        SVT_TEST_CHECK(f!=NULL); if (!f) return 1;
        const uint32_t w=geometry?128u:64u, h=geometry==2u?72u:geometry?128u:64u;
        const uint32_t bd=format?10u:8u;
        const SvtAv2ChromaFormat chroma=format==2u?SVT_AV2_CHROMA_444:SVT_AV2_CHROMA_420;
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&f->source,w,h,bd,chroma),0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&f->recon,w,h,bd,chroma),0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init_ex(&f->geometry,w,h,64u,chroma),0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(&f->scratch,64u,64u),0);
        for (uint32_t p=0u; p<3u; ++p)
            for (uint32_t y=0u; y<svt_av2_picture_plane_height(&f->source,p); ++y)
                for (uint32_t x=0u; x<svt_av2_picture_plane_width(&f->source,p); ++x)
                    svt_av2_sample_store(f->source.planes[p]+(size_t)y*f->source.strides[p]+x*(bd>8u?2u:1u),bd,
                        (uint16_t)(((x*y*17u+x*43u+y*29u+p*53u)&255u)<<(bd-8u)));
        SvtAv2TransformPathState state;
        const uint32_t q=100u+(bd==10u?48u:0u);
        SVT_TEST_CHECK_EQ_INT(svt_av2_transform_path_init_bd(&state,&f->path,q,bd),0);
        for (uint32_t qid=0u;qid<4u;++qid) for (uint32_t mask=1u;mask<15u;++mask) {
            const uint32_t plan=fine16_test_plan(qid,mask);
            if (geometry && mask!=(qid==2u?7u:qid%2u?14u:1u)) continue;
            for (uint32_t adaptive=0u; adaptive<2u; ++adaptive) for (uint32_t tx=0u; tx<3u; ++tx) {
                if (!geometry && tx!=2u) continue;
                uint16_t decisions[16]={0};
                const size_t sb_count=(size_t)f->geometry.sb_rows*f->geometry.sb_cols;
                for (size_t sb=0u; sb<sb_count; ++sb) {
                    const uint32_t x=(uint32_t)(sb%f->geometry.sb_cols)*64u, y=(uint32_t)(sb/f->geometry.sb_cols)*64u;
                    if (x+64u<=w && y+64u<=h)
                        decisions[sb]=(uint16_t)plan;
                }
                f->count=f->used=0u;
                SVT_TEST_CHECK_EQ_INT(svt_av2_recon_reset_frame(&f->recon,(uint16_t)(1u<<(bd-1u))),0);
                for (size_t sb=0u; sb<sb_count; ++sb)
                    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_sb_plan(&f->geometry,sb,decisions,sb_count,encode_leaf,f),0);
                if (!geometry) transaction_test(f,(uint16_t)plan,adaptive);
                uint8_t stream[262144]; size_t bytes=0u;
                SVT_TEST_CHECK_EQ_INT(svt_av2_encode_still_picture_tx(w,h,bd,(uint32_t)chroma,q,f->blocks,f->count,4u,
                    decisions,sb_count,adaptive,tx,stream,sizeof(stream),&bytes),0);
                if (argc>1 && bytes) {
                    char label[80]; snprintf(label,sizeof(label),"f%u-s%u-plan%u-a%u-t%u",format,geometry,plan,adaptive,tx);
                    write_file(argv[1],label,"av2",stream,bytes);
                    uint8_t raw[98304]; size_t raw_bytes=0u;
                    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_export_raw(&f->recon,raw,sizeof(raw),&raw_bytes),0);
                    write_file(argv[1],label,"yuv",raw,raw_bytes); ++emitted;
                }
            }
        }
        svt_av2_recon_pool_free(&f->scratch); svt_av2_picture_free(&f->source); svt_av2_picture_free(&f->recon); free(f);
    }
    if (argc>1) {SVT_TEST_CHECK_EQ_INT(emitted,480u);printf("EMITTED,%zu\n",emitted);}
    SVT_TEST_REPORT("test_intra_fine16");
}

#else
#include <stdio.h>
#include "test_util.h"
int main(void) { puts("SKIPPED fine16 disabled"); SVT_TEST_REPORT("test_intra_fine16"); }
#endif
