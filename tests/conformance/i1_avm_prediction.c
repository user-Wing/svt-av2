/* External differential harness. Link only this test against fixed AVM v1.0.0;
 * the production library has no AVM dependency. */
#include "intra_prediction.h"
#include <stdio.h>

typedef void (*Oracle)(uint16_t*, ptrdiff_t, const uint16_t*, const uint16_t*, int);
#define DECLARE(W,H) \
    extern void avm_highbd_smooth_predictor_##W##x##H##_c(uint16_t*,ptrdiff_t,const uint16_t*,const uint16_t*,int); \
    extern void avm_highbd_smooth_v_predictor_##W##x##H##_c(uint16_t*,ptrdiff_t,const uint16_t*,const uint16_t*,int); \
    extern void avm_highbd_smooth_h_predictor_##W##x##H##_c(uint16_t*,ptrdiff_t,const uint16_t*,const uint16_t*,int); \
    extern void avm_highbd_paeth_predictor_##W##x##H##_c(uint16_t*,ptrdiff_t,const uint16_t*,const uint16_t*,int);
#define SHAPE(W,H) {W,H,{avm_highbd_smooth_predictor_##W##x##H##_c, \
    avm_highbd_smooth_v_predictor_##W##x##H##_c,avm_highbd_smooth_h_predictor_##W##x##H##_c, \
    avm_highbd_paeth_predictor_##W##x##H##_c}}
DECLARE(4,4) DECLARE(8,8) DECLARE(16,16) DECLARE(32,32) DECLARE(64,64)
DECLARE(4,8) DECLARE(8,4) DECLARE(8,16) DECLARE(16,8)
DECLARE(16,32) DECLARE(32,16) DECLARE(32,64) DECLARE(64,32)
DECLARE(4,16) DECLARE(16,4) DECLARE(8,32) DECLARE(32,8) DECLARE(16,64) DECLARE(64,16)

int main(void) {
    static const struct {uint32_t w,h; Oracle functions[4];} shapes[] = {
        SHAPE(4,4),SHAPE(8,8),SHAPE(16,16),SHAPE(32,32),SHAPE(64,64),
        SHAPE(4,8),SHAPE(8,4),SHAPE(8,16),SHAPE(16,8),SHAPE(16,32),SHAPE(32,16),
        SHAPE(32,64),SHAPE(64,32),SHAPE(4,16),SHAPE(16,4),SHAPE(8,32),SHAPE(32,8),SHAPE(16,64),SHAPE(64,16)
    };
    uint64_t cases=0u, samples=0u;
    uint32_t random=912367u;
    for (uint32_t bd=8u; bd<=10u; bd+=2u) {
        SvtAv2FrameBuffer frame={0};
        if (svt_av2_picture_alloc_ex(&frame,192u,192u,bd,SVT_AV2_CHROMA_420)!=0) return 1;
        for (uint32_t trial=0u; trial<20u; ++trial) {
            for (uint32_t y=0u; y<192u; ++y) for (uint32_t x=0u; x<192u; ++x) {
                random=random*1664525u+1013904223u;
                svt_av2_sample_store(frame.planes[0]+(size_t)y*frame.strides[0]+x*(bd>8u?2u:1u),
                    bd,(uint16_t)((random>>12u)&((1u<<bd)-1u)));
            }
            for (size_t s=0u; s<sizeof(shapes)/sizeof(shapes[0]); ++s)
                for (uint32_t edges=0u; edges<16u; ++edges) {
                    const SvtAv2IntraBlock b={edges&1u?8u:0u,edges&2u?8u:0u,shapes[s].w,shapes[s].h,0u};
                    const uint32_t step=bd>8u?2u:1u;
                    const uint16_t base=(uint16_t)(1u<<(bd-1u));
                    uint16_t above_storage[66],left[65],expected[4096];
                    uint16_t* above=above_storage+1;
                    for (uint32_t x=0u; x<=b.w; ++x) {
                        if (b.y) above[x]=svt_av2_sample_load(frame.planes[0]+(size_t)(b.y-1u)*frame.strides[0]+
                            (b.x+(x==b.w && !(edges&4u)?x-1u:x))*step,bd);
                        else if (b.x) above[x]=svt_av2_sample_load(frame.planes[0]+(size_t)b.y*frame.strides[0]+(b.x-1u)*step,bd);
                        else above[x]=base-1u;
                    }
                    for (uint32_t y=0u; y<=b.h; ++y) {
                        if (b.x) left[y]=svt_av2_sample_load(frame.planes[0]+
                            (size_t)(b.y+(y==b.h && !(edges&8u)?y-1u:y))*frame.strides[0]+(b.x-1u)*step,bd);
                        else if (b.y) left[y]=above[0];
                        else left[y]=base+1u;
                    }
                    above[-1]=b.x && b.y ? svt_av2_sample_load(frame.planes[0]+(size_t)(b.y-1u)*frame.strides[0]+(b.x-1u)*step,bd) :
                        b.y?above[0]:b.x?left[0]:base;
                    for (uint32_t mode=4u; mode<=7u; ++mode) {
                        uint8_t actual[8192];
                        if (svt_av2_intra_basic_predict(&frame,&b,mode,(edges&4u)!=0,(edges&8u)!=0,actual,b.w*step)!=0) return 2;
                        shapes[s].functions[mode-4u](expected,b.w,above,left,(int)bd);
                        for (uint32_t i=0u; i<b.w*b.h; ++i) {
                            if (svt_av2_sample_load(actual+i*step,bd)!=expected[i]) {
                                fprintf(stderr,"mismatch bd=%u w=%u h=%u edges=%u mode=%u pixel=%u\n",bd,b.w,b.h,edges,mode,i);
                                return 3;
                            }
                            ++samples;
                        }
                        ++cases;
                    }
                }
        }
        svt_av2_picture_free(&frame);
    }
    printf("PASS %llu cases, %llu samples\n",(unsigned long long)cases,(unsigned long long)samples);
    return 0;
}
