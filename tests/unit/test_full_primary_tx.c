/* Forced normative modes, independent of the production candidate selector. */
#include "intra_prediction.h"
#include "transform_path.h"
#include "keyframe.h"
#include "coeff_syntax.h"
#include "entropy_models.h"
#include "test_util.h"
#include <stdlib.h>

static const uint32_t modes[13] = {0u,1u,2u,4u,5u,6u,7u,8u,9u,10u,11u,12u,13u};

typedef struct Forced {
    SvtAv2FrameBuffer source, recon;
    SvtAv2FrameGeometry geometry;
    SvtAv2ReconScratch scratch;
    SvtAv2IntraTransformPath path;
    SvtAv2TileCoeffBlock blocks[192];
    int32_t coeffs[12288];
    uint32_t y_mode, uv_mode, mixed, tx_type;
    uint64_t adst_count;
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
        uint32_t mode = modes[((p ? f->uv_mode : f->y_mode) +
            (f->mixed ? (uint32_t)(f->count / 3u) : 0u)) % (p ? 7u : 13u)];
        if (p == 0u && b->w != b->h && mode >= 8u) mode = 0u;
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
        if (p == 0u && mode >= 8u) {
            uint32_t tr_count = 0u, bl_count = 0u;
            while (tr_count < b->h && decoded(f, (int32_t)(b->x + b->w + tr_count), (int32_t)b->y - 1)) ++tr_count;
            while (bl_count < b->w && decoded(f, (int32_t)b->x - 1, (int32_t)(b->y + b->h + bl_count))) ++bl_count;
            prediction = svt_av2_intra_directional_predict(&f->recon, &intra, mode, tr_count, bl_count, dst, f->recon.strides[p]);
        } else prediction = svt_av2_intra_basic_predict(&f->recon, &intra, mode, tr, bl, dst, f->recon.strides[p]);
        if (prediction != 0 ||
            svt_av2_residual_compute(&f->source, &f->recon, &intra, f->scratch.residual, f->scratch.stride) != 0)
            return -1;
        uint32_t tx_type = p == 0u && b->w == b->h && b->w <= 16u &&
            (!f->mixed || (f->count / 3u) % 2u == 0u) ? f->tx_type : 0u;
        if (tx_type == 1u && b->w == 4u && mode == 7u) tx_type = 0u;
        int zero;
        if (f->path.forward_type(f->path.user,p,b->w,b->h,f->scratch.residual,f->scratch.stride,
            f->scratch.coeff,f->scratch.qcoeff,f->scratch.dqcoeff,&zero,tx_type) != 0) return -1;
        if (tx_type) {
            uint16_t scan[256]; uint32_t size=0u;
            for (uint32_t side=4u; side<b->w; side*=2u) ++size;
            const size_t n=svt_av2_coeff_scan_dct_dct((SvtAv2TxSize)size,scan);
            if (svt_av2_coeff_find_eob(f->scratch.qcoeff,scan,n)<=1u) {
                tx_type=0u;
                if (f->path.forward(f->path.user,p,b->w,b->h,f->scratch.residual,f->scratch.stride,
                    f->scratch.coeff,f->scratch.qcoeff,f->scratch.dqcoeff,&zero)!=0) return -1;
            }
        }
        f->adst_count+=tx_type!=0u;
        if (f->path.inverse_type(f->path.user,p,b->w,b->h,f->scratch.dqcoeff,f->scratch.stride,
                f->scratch.inverse_residual,tx_type) != 0 ||
            svt_av2_recon_write_block(&f->recon,p,b,dst,f->recon.strides[p],
                f->scratch.inverse_residual,f->scratch.stride) != 0) return -1;
        const size_t count = (size_t)SVT_AV2_MIN(b->w,32u) * SVT_AV2_MIN(b->h,32u);
        if (f->count >= 192u || count > 12288u - f->used) return -1;
        SvtAv2TileCoeffBlock* out = f->blocks + f->count++;
        out->plane=p; out->x=b->x; out->y=b->y; out->width=b->w; out->height=b->h;
        out->intra_mode=(uint8_t)mode; out->tx_type=(uint8_t)tx_type; out->qcoeff=f->coeffs+f->used;
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

static void syntax_boundary_tests(void) {
    const SvtAv2CoeffPlaneInfo pi = {0u,SVT_AV2_TX_4X4,0u,0u,2u,2u,4u,4u};
    int32_t q[16] = {2,-1};
    for (uint32_t type=0u; type<4u; ++type) {
        SvtAv2EntropyEncoder enc;
        SvtAv2EntropyModels models;
        SvtAv2CoeffNeighbours neighbours;
        svt_av2_entropy_rate_init(&enc);
        svt_av2_coeff_neighbours_reset(&neighbours);
        SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_models_init(&models,1u),0);
        enc.models=&models;
        uint16_t expected[12];
        memcpy(expected,models.cdf[SVT_AV2_MODEL_INTRA_TX_FULL],sizeof(expected));
        /* Fixed v1 av2_md_idx2type[size_class=0][Paeth] starts 0,3,2. */
        const uint32_t symbol = type==0u ? 0u : type==3u ? 1u : 2u;
        if (type!=1u) SVT_TEST_CHECK_EQ_INT(svt_av2_entropy_model_update(expected,7u,symbol),0);
        const int status=svt_av2_coeff_write_block_transform_intra(&enc,&pi,0u,q,&neighbours,
            1u,0u,0u,type,7u);
        SVT_TEST_CHECK_EQ_INT(status,type==1u ? SVT_AV2_COEFF_ERR_UNSUPPORTED : SVT_AV2_COEFF_OK);
        SVT_TEST_CHECK(memcmp(expected,models.cdf[SVT_AV2_MODEL_INTRA_TX_FULL],sizeof(expected))==0);
    }
    SvtAv2EntropyEncoder enc;
    SvtAv2CoeffNeighbours neighbours;
    q[1]=0;
    svt_av2_entropy_rate_init(&enc);
    svt_av2_coeff_neighbours_reset(&neighbours);
    SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_write_block_transform_intra(&enc,&pi,0u,q,&neighbours,
        1u,0u,0u,2u,7u),SVT_AV2_COEFF_ERR_UNSUPPORTED);
}

int main(int argc, char** argv) {
    syntax_boundary_tests();
    for (uint32_t format=0u; format<3u; ++format) {
        for (uint32_t shape=0u; shape<6u; ++shape) {
            Forced* f = (Forced*)calloc(1u,sizeof(*f));
            SVT_TEST_CHECK(f != NULL); if (!f) return 1;
            const uint32_t side = shape == 1u ? 24u : shape == 2u ? 8u : 64u;
            const uint32_t bd = format ? 10u : 8u;
            const SvtAv2ChromaFormat chroma = format == 2u ? SVT_AV2_CHROMA_444 : SVT_AV2_CHROMA_420;
            SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&f->source,side,side,bd,chroma),0);
            SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&f->recon,side,side,bd,chroma),0);
            SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init_ex(&f->geometry,side,side,64u,chroma),0);
            SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(&f->scratch,64u,64u),0);
            SvtAv2TransformPathState state;
            SVT_TEST_CHECK_EQ_INT(svt_av2_transform_path_init_bd(&state,&f->path,bd==10u?148u:100u,bd),0);
            for (uint32_t p=0u; p<3u; ++p)
                for (uint32_t y=0u; y<svt_av2_picture_plane_height(&f->source,p); ++y)
                    for (uint32_t x=0u; x<svt_av2_picture_plane_width(&f->source,p); ++x)
                        svt_av2_sample_store(f->source.planes[p]+(size_t)y*f->source.strides[p]+x*(bd>8u?2u:1u),bd,
                            (uint16_t)(((x*11u+y*7u+x*y%31u+p*53u)&255u)<<(bd-8u)));
            for (uint32_t type=1u; type<=3u; ++type)
            for (uint32_t adaptive=0u; adaptive<2u; ++adaptive)
            for (uint32_t mixed=0u; mixed<2u; ++mixed)
            for (uint32_t yi=0u; yi<13u; ++yi) for (uint32_t ui=0u; ui<7u; ++ui) {
                f->y_mode=yi; f->uv_mode=ui; f->mixed=mixed; f->tx_type=type; f->count=f->used=0u;
                const uint16_t decision = shape == 0u ? 2u : shape == 3u ? 85u : shape == 4u ? 1u : 0u;
                SVT_TEST_CHECK_EQ_INT(svt_av2_recon_reset_frame(&f->recon,(uint16_t)(1u<<(bd-1u))),0);
                SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_sb_plan(&f->geometry,0u,&decision,1u,encode_leaf,f),0);
                uint8_t stream[65536]; size_t size=0u;
                SVT_TEST_CHECK_EQ_INT(svt_av2_encode_still_picture_tx(side,side,bd,(uint32_t)chroma,
                    bd==10u?148u:100u,f->blocks,f->count,3u,&decision,1u,adaptive,2u,stream,sizeof(stream),&size),0);
                if (argc>1 && size) {
                    char label[80]; snprintf(label,sizeof(label),"f%u-s%u-t%u-a%u-m%u-y%u-uv%u",format,shape,type,adaptive,mixed,modes[yi],modes[ui]);
                    write_file(argv[1],label,"av2",stream,size);
                    uint8_t raw[24576]; size_t bytes=0u;
                    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_export_raw(&f->recon,raw,sizeof(raw),&bytes),0);
                    write_file(argv[1],label,"yuv",raw,bytes);
                }
            }
            if (shape < 3u) SVT_TEST_CHECK(f->adst_count > 0u);
            printf("forced format=%u shape=%u adst=%llu\n",format,shape,(unsigned long long)f->adst_count);
            svt_av2_recon_pool_free(&f->scratch);
            svt_av2_picture_free(&f->source); svt_av2_picture_free(&f->recon); free(f);
        }
    }
    SVT_TEST_REPORT("test_full_primary_tx");
}
