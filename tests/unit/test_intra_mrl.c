/* Forced normative modes, independent of the production candidate selector. */
#include "intra_prediction.h"
#include "transform_path.h"
#include "keyframe.h"
#include "test_util.h"
#include <stdlib.h>

static const uint32_t modes[12] = {1u,2u,8u,9u,10u,11u,12u,13u,14u,31u,44u,61u};
static const uint32_t selections[7] = {0u,1u,2u,3u,5u,6u,7u};

typedef struct Forced {
    SvtAv2FrameBuffer source, recon;
    SvtAv2FrameGeometry geometry;
    SvtAv2ReconScratch scratch;
    SvtAv2IntraTransformPath path;
    SvtAv2TileCoeffBlock blocks[1536];
    int32_t coeffs[65536];
    uint32_t y_mode, selection, mixed;
    size_t count, used;
} Forced;

static int decoded(const Forced* f, int32_t x, int32_t y) {
    for (size_t i = 0u; i < f->count; i += 3u) {
        const SvtAv2TileCoeffBlock* b = f->blocks + i;
        if (x >= (int32_t)b->x && x < (int32_t)(b->x + b->width) &&
            y >= (int32_t)b->y && y < (int32_t)(b->y + b->height)) return 1;
    }
    return 0;
}

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
        const uint32_t leaf = (uint32_t)(f->count/3u);
        const uint32_t mode = p ? 0u : f->mixed && leaf%3u==0u ? 0u : modes[(f->y_mode+(f->mixed?leaf:0u))%12u];
        const uint32_t selection = p || mode==0u ? 0u : f->mixed && leaf%3u==1u ? 0u : f->selection;
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
        if (p == 0u && (mode >= 8u || selection)) {
            uint32_t tr_count = 0u, bl_count = 0u;
            while (tr_count < b->h && decoded(f, (int32_t)(b->x + b->w + tr_count), (int32_t)b->y - 1)) ++tr_count;
            while (bl_count < b->w && decoded(f, (int32_t)b->x - 1, (int32_t)(b->y + b->h + bl_count))) ++bl_count;
            prediction = selection ? svt_av2_intra_mrls_predict(&f->recon,&intra,mode,selection,tr_count,bl_count,dst,f->recon.strides[p]) :
                svt_av2_intra_directional_predict(&f->recon,&intra,mode,tr_count,bl_count,dst,f->recon.strides[p]);
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
        out->mrl=(uint8_t)selection; out->intra_mode=(uint8_t)mode; out->qcoeff=f->coeffs+f->used;
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

int main(int argc, char** argv) {
    for (uint32_t format=0u; format<3u; ++format) {
        for (uint32_t shape=0u; shape<4u; ++shape) {
            Forced* f = (Forced*)calloc(1u,sizeof(*f));
            SVT_TEST_CHECK(f != NULL); if (!f) return 1;
            const uint32_t sides[4] = {64u,128u,72u,24u};
            const uint32_t side = sides[shape], height = shape == 2u ? 88u : side;
            const uint32_t bd = format ? 10u : 8u;
            const SvtAv2ChromaFormat chroma = format == 2u ? SVT_AV2_CHROMA_444 : SVT_AV2_CHROMA_420;
            SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&f->source,side,height,bd,chroma),0);
            SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&f->recon,side,height,bd,chroma),0);
            SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init_ex(&f->geometry,side,height,64u,chroma),0);
            SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(&f->scratch,64u,64u),0);
            SvtAv2TransformPathState state;
            SVT_TEST_CHECK_EQ_INT(svt_av2_transform_path_init_bd(&state,&f->path,bd==10u?148u:100u,bd),0);
            for (uint32_t pattern=0u; pattern<1u; ++pattern) {
            for (uint32_t p=0u; p<3u; ++p)
                for (uint32_t y=0u; y<svt_av2_picture_plane_height(&f->source,p); ++y)
                    for (uint32_t x=0u; x<svt_av2_picture_plane_width(&f->source,p); ++x)
                        svt_av2_sample_store(f->source.planes[p]+(size_t)y*f->source.strides[p]+x*(bd>8u?2u:1u),bd,
                            (uint16_t)((pattern==3u ? 127u : pattern==2u ? (x*y*17u+x*43u+y*29u+p*53u)&255u :
                                (p==2u && pattern==1u ? 255u-((x*11u+y*7u+x*y%31u)&255u) :
                                 (x*11u+y*7u+x*y%31u+p*53u)&255u))<<(bd-8u)));
            for (uint32_t adaptive=0u; adaptive<2u; ++adaptive)
            for (uint32_t mixed=0u; mixed<2u; ++mixed)
            for (uint32_t yi=0u; yi<12u; ++yi) for (uint32_t ui=0u; ui<7u; ++ui)
            for (uint32_t orientation=0u; orientation<4u; ++orientation) {
                if (argc>2 && (format!=0u || shape!=1u || yi>7u || orientation!=3u || adaptive!=1u)) continue;
                f->y_mode=yi; f->selection=selections[ui]; f->mixed=mixed; f->count=f->used=0u;
                uint16_t decisions[16] = {0};
                const size_t sb_count = (size_t)f->geometry.sb_rows*f->geometry.sb_cols;
                for (size_t sb=0u; sb<sb_count; ++sb) {
                    const uint32_t sx=(uint32_t)(sb%f->geometry.sb_cols)*64u;
                    const uint32_t sy=(uint32_t)(sb/f->geometry.sb_cols)*64u;
                    decisions[sb]=sx+64u<=side && sy+64u<=height ? (uint8_t)orientation : 0u;
                }
                SVT_TEST_CHECK_EQ_INT(svt_av2_recon_reset_frame(&f->recon,(uint16_t)(1u<<(bd-1u))),0);
                for (size_t sb=0u; sb<sb_count; ++sb)
                    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_sb_plan(&f->geometry,sb,decisions,sb_count,encode_leaf,f),0);
                uint8_t stream[262144]; size_t size=0u;
                SVT_TEST_CHECK_EQ_INT(svt_av2_encode_still_picture_mrls(side,height,bd,(uint32_t)chroma,
                    bd==10u?148u:100u,f->blocks,f->count,4u,decisions,sb_count,adaptive,0u,1u,stream,sizeof(stream),&size),0);
                if (argc>1 && size) {
                    char label[80]; snprintf(label,sizeof(label),"f%u-s%u-a%u-m%u-y%u-r%u-d%u",format,shape,adaptive,mixed,modes[yi],selections[ui],orientation);
                    write_file(argv[1],label,"av2",stream,size);
                    uint8_t raw[98304]; size_t bytes=0u;
                    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_export_raw(&f->recon,raw,sizeof(raw),&bytes),0);
                    write_file(argv[1],label,"yuv",raw,bytes);
                }
            }
            }
            svt_av2_recon_pool_free(&f->scratch);
            svt_av2_picture_free(&f->source); svt_av2_picture_free(&f->recon); free(f);
        }
    }
    SVT_TEST_REPORT("test_intra_mrl");
}
