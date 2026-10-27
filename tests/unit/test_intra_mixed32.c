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

static void geometry_tests(void) {
    SvtAv2FrameGeometry g;
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&g,64u,64u,64u),0);
    for (uint32_t plan=93u; plan<255u; ++plan) {
        uint16_t decision=(uint8_t)plan;
        Walk walk={0};
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_sb_plan(&g,0u,&decision,1u,collect,&walk),0);
        size_t leaf=0u;
        uint32_t code=plan-(plan<174u?93u:174u);
        for (uint32_t q=0u; q<4u; ++q) {
            const uint32_t shape=code%3u; code/=3u;
            const uint32_t w=shape==1u || (!shape && plan>=174u)?16u:32u;
            const uint32_t h=shape==2u || (!shape && plan>=174u)?16u:32u;
            for (uint32_t y=0u; y<32u; y+=h) for (uint32_t x=0u; x<32u; x+=w) {
                SVT_TEST_CHECK(leaf<walk.count);
                if (leaf<walk.count) {
                    const SvtAv2BlockView* b=walk.blocks+leaf;
                    SVT_TEST_CHECK_EQ_INT(b->x,q%2u*32u+x);
                    SVT_TEST_CHECK_EQ_INT(b->y,q/2u*32u+y);
                    SVT_TEST_CHECK_EQ_INT(b->w,w); SVT_TEST_CHECK_EQ_INT(b->h,h);
                }
                ++leaf;
            }
        }
        SVT_TEST_CHECK_EQ_INT(walk.count,leaf);
        SVT_TEST_CHECK(walk.count<=16u);
    }
    for (uint32_t alias=0u; alias<2u; ++alias) {
        uint16_t old=(uint8_t)(1u+alias), added=(uint8_t)(93u+81u*alias);
        Walk a={0}, b={0};
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_sb_plan(&g,0u,&old,1u,collect,&a),0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_sb_plan(&g,0u,&added,1u,collect,&b),0);
        SVT_TEST_CHECK_EQ_INT(a.count,b.count);
        SVT_TEST_CHECK(memcmp(a.blocks,b.blocks,a.count*sizeof(a.blocks[0]))==0);
    }
    uint16_t invalid=882u;
    Walk untouched={0};
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_sb_plan(&g,0u,&invalid,1u,collect,&untouched),SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(untouched.count,0u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&g,72u,72u,64u),0);
    uint16_t partial[4]={93u,0u,0u,174u};
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_coded_plan(&g,partial,4u,collect,&untouched),SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(untouched.count,0u);
}

int main(int argc, char** argv) {
    geometry_tests();
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
        for (uint32_t plan=93u; plan<255u; ++plan) {
            /* Every code is forced on the full single SB. Neighbour/boundary
             * cases retain both families, aliases and mixed V/H/quad states. */
            if (geometry && plan!=93u && plan!=94u && plan!=98u && plan!=133u && plan!=173u &&
                plan!=174u && plan!=175u && plan!=179u && plan!=214u && plan!=254u) continue;
            for (uint32_t adaptive=0u; adaptive<2u; ++adaptive) for (uint32_t tx=0u; tx<3u; ++tx) {
                if (!geometry && tx!=2u) continue;
                uint16_t decisions[16]={0};
                const size_t sb_count=(size_t)f->geometry.sb_rows*f->geometry.sb_cols;
                for (size_t sb=0u; sb<sb_count; ++sb) {
                    const uint32_t x=(uint32_t)(sb%f->geometry.sb_cols)*64u, y=(uint32_t)(sb/f->geometry.sb_cols)*64u;
                    if (x+64u<=w && y+64u<=h)
                        decisions[sb]=geometry && sb ? (uint8_t)(1u+(uint32_t)sb%3u) : (uint8_t)plan;
                }
                f->count=f->used=0u;
                SVT_TEST_CHECK_EQ_INT(svt_av2_recon_reset_frame(&f->recon,(uint16_t)(1u<<(bd-1u))),0);
                for (size_t sb=0u; sb<sb_count; ++sb)
                    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_sb_plan(&f->geometry,sb,decisions,sb_count,encode_leaf,f),0);
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
    if (argc>1) printf("EMITTED,%zu\n",emitted);
    SVT_TEST_REPORT("test_intra_mixed32");
}
