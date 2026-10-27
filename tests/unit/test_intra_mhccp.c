/* Forced normative modes, independent of the production candidate selector. */
#include "intra_prediction.h"
#include "transform_path.h"
#include "keyframe.h"
#include "coeff_syntax.h"
#include "test_util.h"
#include <stdlib.h>

static const uint32_t modes[61] = {0u,1u,2u,4u,5u,6u,7u,8u,9u,10u,11u,12u,13u,14u,15u,16u,17u,18u,19u,20u,21u,22u,23u,24u,25u,26u,27u,28u,29u,30u,31u,32u,33u,34u,35u,36u,37u,38u,39u,40u,41u,42u,43u,44u,45u,46u,47u,48u,49u,50u,51u,52u,53u,54u,55u,56u,57u,58u,59u,60u,61u};

typedef struct Forced {
    SvtAv2FrameBuffer source, recon;
    SvtAv2FrameGeometry geometry;
    SvtAv2ReconScratch scratch;
    SvtAv2IntraTransformPath path;
    SvtAv2TileCoeffBlock blocks[1536];
    int32_t coeffs[65536];
    uint32_t y_mode, uv_mode, mixed, tx_type, direction;
    uint64_t mhccp_count, poison_count;
    uint64_t adst_count, mrl_count;
    size_t count, used;
} Forced;

static int decoded(const Forced* f, int32_t x, int32_t y) {
    for (size_t i = 0u; i + 3u <= f->count; i += 3u) {
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
        const uint32_t y_mode = modes[(f->y_mode + (f->mixed ? (uint32_t)(f->count / 3u) : 0u)) % 61u];
        const uint32_t base = svt_av2_intra_mode_base(y_mode);
        const uint32_t leaf = (uint32_t)(f->count / 3u);
        const int allowed = svt_av2_intra_mhccp_allowed(y_block->w,y_block->h,(uint32_t)f->recon.chroma_format);
        const uint32_t mhccp = allowed && (!f->mixed || leaf % 4u < 2u) ? f->direction + 1u : 0u;
        const uint32_t mode = p == 0u ? y_mode : f->uv_mode ?
            (f->mixed && leaf % 4u == 3u ? 0u : 14u) : base <= 2u ? base : 7u;
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
        static const uint32_t selections[7] = {0u,1u,2u,3u,5u,6u,7u};
        const uint32_t mrl = p == 0u && f->mixed && svt_av2_intra_mode_joint(y_mode) >= 5u &&
            b->w >= 8u && b->h >= 8u ? selections[leaf % 7u] : 0u;
        SvtAv2Error prediction;
        const int inherit = p != 0u && y_mode >= 14u && svt_av2_intra_mode_base(y_mode) == mode;
        if (p && mode == 14u && mhccp) {
            uint32_t tr_count=0u,bl_count=0u;
            while (tr_count<b->w && decoded(f,(int32_t)(b->x+b->w+tr_count)*scale,((int32_t)b->y-1)*scale)) ++tr_count;
            while (bl_count<b->h && decoded(f,((int32_t)b->x-1)*scale,(int32_t)(b->y+b->h+bl_count)*scale)) ++bl_count;
            uint8_t pixels[2048],poisoned[2048];int32_t params[3],poison_params[3];
            prediction=svt_av2_intra_mhccp_predict(&f->recon,&intra,f->direction,tr_count,bl_count,pixels,b->w*step,params);
            if (prediction!=0) return -1;
            /* Poison only unavailable UV, keeping all Y and decoded UV untouched. */
            const size_t plane_bytes=(size_t)f->recon.strides[p]*f->recon.aligned_height[p];
            uint8_t* saved=(uint8_t*)malloc(plane_bytes);if(!saved)return -1;
            memcpy(saved,f->recon.planes[p],plane_bytes);
            for(uint32_t py=0u;py<svt_av2_picture_plane_height(&f->recon,p);++py)
                for(uint32_t px=0u;px<svt_av2_picture_plane_width(&f->recon,p);++px)
                    if(!decoded(f,(int32_t)px*scale,(int32_t)py*scale))
                        svt_av2_sample_store(f->recon.planes[p]+(size_t)py*f->recon.strides[p]+px*step,
                            f->recon.bit_depth,(uint16_t)((px*179u+py*73u+leaf*53u)&((1u<<f->recon.bit_depth)-1u)));
            prediction=svt_av2_intra_mhccp_predict(&f->recon,&intra,f->direction,tr_count,bl_count,
                poisoned,b->w*step,poison_params);
            memcpy(f->recon.planes[p],saved,plane_bytes);free(saved);
            if(prediction!=0 || memcmp(params,poison_params,sizeof(params)) || memcmp(pixels,poisoned,(size_t)b->w*b->h*step)) return -1;
            ++f->poison_count;++f->mhccp_count;
            for(uint32_t y=0u;y<b->h;++y)memcpy(dst+(size_t)y*f->recon.strides[p],pixels+(size_t)y*b->w*step,b->w*step);
        } else if (p && mode == 14u) {
            prediction = f->mixed && leaf%4u==2u ?
                svt_av2_intra_cfl_explicit_predict(&f->recon,&intra,64u,dst,f->recon.strides[p],p==1u?-8:8) :
                svt_av2_intra_cfl_implicit_predict(&f->recon,&intra,64u,dst,f->recon.strides[p],NULL);
        } else if ((p == 0u && (mode >= 8u || mrl)) || inherit) {
            uint32_t tr_count = 0u, bl_count = 0u;
            while (tr_count < (mrl ? b->w : b->h) && decoded(f, (int32_t)(b->x + b->w + tr_count)*scale, ((int32_t)b->y - 1)*scale)) ++tr_count;
            while (bl_count < (mrl ? b->h : b->w) && decoded(f, ((int32_t)b->x - 1)*scale, (int32_t)(b->y + b->h + bl_count)*scale)) ++bl_count;
            prediction = mrl ? svt_av2_intra_mrls_predict(&f->recon,&intra,y_mode,mrl,tr_count,bl_count,dst,f->recon.strides[p]) :
                svt_av2_intra_directional_predict(&f->recon, &intra, inherit ? y_mode : mode, tr_count, bl_count, dst, f->recon.strides[p]);
        } else prediction = svt_av2_intra_basic_predict(&f->recon, &intra, mode, tr, bl, dst, f->recon.strides[p]);
        if (prediction != 0 ||
            svt_av2_residual_compute(&f->source, &f->recon, &intra, f->scratch.residual, f->scratch.stride) != 0)
            return -1;
        uint32_t tx_type = p == 0u && ((b->w == 8u && b->h == 16u) || (b->w == 16u && b->h == 8u)) &&
            (!f->mixed || leaf % 3u != 0u) ? f->tx_type : 0u;
        int zero;
        if (f->path.forward_type(f->path.user,p,b->w,b->h,f->scratch.residual,f->scratch.stride,
            f->scratch.coeff,f->scratch.qcoeff,f->scratch.dqcoeff,&zero,tx_type) != 0) return -1;
        if (tx_type) {
            SvtAv2TxSize size; uint16_t scan[256];
            if (svt_av2_coeff_tx_size_from_dims(b->w,b->h,&size) != 0) return -1;
            const size_t count = svt_av2_coeff_scan_dct_dct(size,scan);
            if (svt_av2_coeff_find_eob(f->scratch.qcoeff,scan,count) <= 1u) {
                tx_type = 0u;
                if (f->path.forward(f->path.user,p,b->w,b->h,f->scratch.residual,f->scratch.stride,
                    f->scratch.coeff,f->scratch.qcoeff,f->scratch.dqcoeff,&zero) != 0) return -1;
            }
        }
        f->adst_count += tx_type != 0u; f->mrl_count += mrl != 0u;
        if (f->path.inverse_type(f->path.user,p,b->w,b->h,f->scratch.dqcoeff,f->scratch.stride,
                f->scratch.inverse_residual,tx_type) != 0 ||
            svt_av2_recon_write_block(&f->recon,p,b,dst,f->recon.strides[p],
                f->scratch.inverse_residual,f->scratch.stride) != 0) return -1;
        const size_t count = (size_t)SVT_AV2_MIN(b->w,32u) * SVT_AV2_MIN(b->h,32u);
        if (f->count >= 1536u || count > 65536u - f->used) return -1;
        SvtAv2TileCoeffBlock* out = f->blocks + f->count++;
        out->plane=p; out->x=b->x; out->y=b->y; out->width=b->w; out->height=b->h;
        out->mhccp=(uint8_t)(p==0u?mhccp:0u);
        out->cfl_explicit=(uint8_t)(p==0u && f->mixed && leaf%4u==2u);
        out->cfl_alpha_signs=4u;out->cfl_alpha_idx=63u;
        out->intra_mode=(uint8_t)mode; out->mrl=(uint8_t)mrl; out->tx_type=(uint8_t)tx_type; out->qcoeff=f->coeffs+f->used;
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
    const int pilot=getenv("SVT_AV2_MHCCP_PILOT")!=NULL;
    const uint16_t plans[14]={0u,1u,2u,3u,85u,86u,87u,88u,89u,90u,91u,92u,100u,179u};
    size_t emitted=0u;uint64_t mhccp_total=0u,poison_total=0u;
    for(uint32_t format=0u;format<3u;++format)for(uint32_t geometry=0u;geometry<3u;++geometry) {
        Forced* f=(Forced*)calloc(1u,sizeof(*f));SVT_TEST_CHECK(f!=NULL);if(!f)return 1;
        const uint32_t width=geometry?128u:64u,height=geometry==2u?72u:width,bd=format?10u:8u;
        const SvtAv2ChromaFormat chroma=format==2u?SVT_AV2_CHROMA_444:SVT_AV2_CHROMA_420;
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&f->source,width,height,bd,chroma),0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&f->recon,width,height,bd,chroma),0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init_ex(&f->geometry,width,height,64u,chroma),0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(&f->scratch,64u,64u),0);
        SvtAv2TransformPathState state;
        SVT_TEST_CHECK_EQ_INT(svt_av2_transform_path_init_bd(&state,&f->path,100u+(bd==10u?48u:0u),bd),0);
        for(uint32_t pattern=0u;pattern<2u;++pattern) {
            for(uint32_t p=0u;p<3u;++p)for(uint32_t y=0u;y<svt_av2_picture_plane_height(&f->source,p);++y)
                for(uint32_t x=0u;x<svt_av2_picture_plane_width(&f->source,p);++x)
                    svt_av2_sample_store(f->source.planes[p]+(size_t)y*f->source.strides[p]+x*(bd>8u?2u:1u),bd,
                        (uint16_t)((pattern?((x+y+p)&1u?255u:0u):(x*y*17u+x*43u+y*29u+p*53u)&255u)<<(bd-8u)));
            for(uint32_t shape=0u;shape<14u;++shape)for(uint32_t dir=0u;dir<3u;++dir)
            for(uint32_t adaptive=0u;adaptive<2u;++adaptive)for(uint32_t mixed=0u;mixed<2u;++mixed) {
                if(pilot && (geometry!=1u || pattern || (shape!=2u && shape!=6u) || !mixed))continue;
                f->count=f->used=0u;f->direction=dir;f->mixed=mixed;f->uv_mode=1u;f->tx_type=1u+dir;
                f->y_mode=mixed?22u:dir;f->mhccp_count=f->poison_count=0u;
                uint16_t decisions[16]={0};const size_t sb_count=(size_t)f->geometry.sb_rows*f->geometry.sb_cols;
                for(size_t sb=0u;sb<sb_count;++sb) {
                    const uint32_t x=(uint32_t)(sb%f->geometry.sb_cols)*64u,y=(uint32_t)(sb/f->geometry.sb_cols)*64u;
                    if(x+64u<=width && y+64u<=height)decisions[sb]=plans[mixed && sb?(shape+(uint32_t)sb)%14u:shape];
                }
                SVT_TEST_CHECK_EQ_INT(svt_av2_recon_reset_frame(&f->recon,(uint16_t)(1u<<(bd-1u))),0);
                for(size_t sb=0u;sb<sb_count;++sb)
                    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_sb_plan(&f->geometry,sb,decisions,sb_count,encode_leaf,f),0);
                uint8_t stream[262144];size_t bytes=0u;
                SVT_TEST_CHECK_EQ_INT(svt_av2_encode_still_picture_mhccp(width,height,bd,(uint32_t)chroma,
                    100u+(bd==10u?48u:0u),f->blocks,f->count,4u,decisions,sb_count,adaptive,2u,1u,1u,
                    stream,sizeof(stream),&bytes),0);
                mhccp_total+=f->mhccp_count;poison_total+=f->poison_count;
                if(argc>1 && bytes) {
                    char label[96];snprintf(label,sizeof(label),"f%u-s%u-p%u-b%u-d%u-a%u-m%u",format,geometry,pattern,shape,dir,adaptive,mixed);
                    write_file(argv[1],label,"av2",stream,bytes);
                    uint8_t raw[98304];size_t rawbytes=0u;
                    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_export_raw(&f->recon,raw,sizeof(raw),&rawbytes),0);
                    write_file(argv[1],label,"yuv",raw,rawbytes);++emitted;
                }
            }
        }
        svt_av2_recon_pool_free(&f->scratch);svt_av2_picture_free(&f->source);svt_av2_picture_free(&f->recon);free(f);
    }
    printf("EMITTED,%zu MHCCP,%llu POISON,%llu\n",emitted,(unsigned long long)mhccp_total,(unsigned long long)poison_total);
    SVT_TEST_REPORT("test_intra_mhccp");
}
