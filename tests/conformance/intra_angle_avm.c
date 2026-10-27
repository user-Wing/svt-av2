/* External fixed-AVM scalar differential, never linked into the encoder. */
#include "intra_prediction.h"
#include "av2/common/reconintra.h"
#include <stdio.h>

typedef void (*Oracle)(uint16_t*, ptrdiff_t, int, int, const uint16_t*, const uint16_t*, int, int, int, int);
extern void av2_highbd_dr_prediction_z1_c(uint16_t*, ptrdiff_t, int, int, const uint16_t*, const uint16_t*, int, int, int, int);
extern void av2_highbd_dr_prediction_z2_c(uint16_t*, ptrdiff_t, int, int, const uint16_t*, const uint16_t*, int, int, int, int);
extern void av2_highbd_dr_prediction_z3_c(uint16_t*, ptrdiff_t, int, int, const uint16_t*, const uint16_t*, int, int, int, int);
extern void av2_highbd_dr_prediction_z1_idif_c(uint16_t*, ptrdiff_t, int, int, const uint16_t*, const uint16_t*, int, int, int, int);
extern void av2_highbd_dr_prediction_z2_idif_c(uint16_t*, ptrdiff_t, int, int, const uint16_t*, const uint16_t*, int, int, int, int);
extern void av2_highbd_dr_prediction_z3_idif_c(uint16_t*, ptrdiff_t, int, int, const uint16_t*, const uint16_t*, int, int, int, int);

int main(void) {
    uint64_t cases = 0u, samples = 0u;
    uint32_t random = 18392017u;
    for (uint32_t bd = 8u; bd <= 10u; bd += 2u) {
        SvtAv2FrameBuffer frame = {0};
        if (svt_av2_picture_alloc_ex(&frame,384u,384u,bd,
            bd == 8u ? SVT_AV2_CHROMA_420 : SVT_AV2_CHROMA_444) != 0) return 1;
        const uint32_t step = bd == 10u ? 2u : 1u, limit = (1u << bd) - 1u;
        for (uint32_t plane = 0u; plane < 3u; ++plane)
        for (uint32_t trial = 0u; trial < 20u; ++trial) {
            for (uint32_t y = 0u; y < 192u; ++y) for (uint32_t x = 0u; x < 192u; ++x) {
                random = random * 1664525u + 1013904223u;
                const uint32_t v = trial == 0u ? ((x+y)&1u ? limit : 0u) : (random >> 10u) & limit;
                svt_av2_sample_store(frame.planes[plane]+(size_t)y*frame.strides[plane]+x*step,bd,(uint16_t)v);
            }
            for (uint32_t side = 4u; side <= 64u; side *= 2u)
            for (uint32_t edges = 0u; edges < 4u; ++edges)
            for (uint32_t tr = 0u; tr <= side; tr += side/2u)
            for (uint32_t bl = 0u; bl <= side; bl += side/2u)
            for (uint32_t mode = 8u; mode <= 61u; ++mode) {
                const SvtAv2IntraBlock b = {edges&1u ? 16u : 0u,edges&2u ? 16u : 0u,side,side,plane};
                uint16_t a_buf[132], l_buf[132], expected[4096];
                uint16_t *a = a_buf+2, *l = l_buf+2;
                const uint16_t base = (uint16_t)(1u << (bd-1u));
                const uint16_t first_top = b.y ? svt_av2_sample_load(frame.planes[plane]+(size_t)(b.y-1u)*frame.strides[plane]+b.x*step,bd) : base-1u;
                const uint16_t first_left = b.x ? svt_av2_sample_load(frame.planes[plane]+(size_t)b.y*frame.strides[plane]+(b.x-1u)*step,bd) : base+1u;
                a[-1] = b.x && b.y ? svt_av2_sample_load(frame.planes[plane]+(size_t)(b.y-1u)*frame.strides[plane]+(b.x-1u)*step,bd) : b.y ? first_top : b.x ? first_left : base;
                l[-2] = l[-1] = a[-2] = a[-1];
                const uint32_t extra_tr = plane && side > 32u ? 0u : tr;
                const uint32_t extra_bl = plane && side > 32u ? 0u : bl;
                for (uint32_t i = 0u; i < side*2u+2u; ++i) {
                    const uint32_t ax = i < side+extra_tr ? i : side+extra_tr-1u;
                    const uint32_t ly = i < side+extra_bl ? i : side+extra_bl-1u;
                    a[i] = b.y ? svt_av2_sample_load(frame.planes[plane]+(size_t)(b.y-1u)*frame.strides[plane]+(b.x+ax)*step,bd) : b.x ? first_left : base-1u;
                    l[i] = b.x ? svt_av2_sample_load(frame.planes[plane]+(size_t)(b.y+ly)*frame.strides[plane]+(b.x-1u)*step,bd) : b.y ? first_top : base+1u;
                }
                const int angle = svt_av2_intra_mode_angle(mode);
                if (angle > 90 && angle < 180) { a[side] = a[side-1u]; l[side] = l[side-1u]; }
                else if (angle < 90) a[side*2u] = a[side*2u+1u] = a[side*2u-1u];
                else l[side*2u] = l[side*2u+1u] = l[side*2u-1u];
                const Oracle oracle = plane ? (angle < 90 ? av2_highbd_dr_prediction_z1_c :
                    angle > 180 ? av2_highbd_dr_prediction_z3_c : av2_highbd_dr_prediction_z2_c) :
                    (angle < 90 ? av2_highbd_dr_prediction_z1_idif_c :
                    angle > 180 ? av2_highbd_dr_prediction_z3_idif_c : av2_highbd_dr_prediction_z2_idif_c);
                oracle(expected,side,(int)side,(int)side,a,l,av2_get_dx(angle),av2_get_dy(angle),(int)bd,0);
                uint8_t actual[8192];
                if (svt_av2_intra_directional_predict(&frame,&b,mode,tr,bl,actual,side*step) != 0) return 2;
                for (uint32_t i = 0u; i < side*side; ++i) {
                    if (svt_av2_sample_load(actual+i*step,bd) != expected[i]) {
                        fprintf(stderr,"mismatch bd=%u size=%u edges=%u tr=%u bl=%u mode=%u pixel=%u\n",bd,side,edges,tr,bl,mode,i);
                        return 3;
                    }
                    ++samples;
                }
                ++cases;
            }
        }
        svt_av2_picture_free(&frame);
    }
    printf("PASS %llu cases, %llu samples\n",(unsigned long long)cases,(unsigned long long)samples);
    return 0;
}
