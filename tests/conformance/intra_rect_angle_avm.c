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
            for (uint32_t shape = 0u; shape < 4u; ++shape)
            for (uint32_t edges = 0u; edges < 4u; ++edges)
            for (uint32_t tr = 0u; tr <= 16u; tr += 2u)
            for (uint32_t bl = 0u; bl <= 16u; bl += 2u)
            for (uint32_t mode = 8u; mode <= 61u; ++mode) {
                const uint32_t widths[4] = {4u,8u,8u,16u}, heights[4] = {8u,4u,16u,8u};
                const uint32_t w = widths[shape], h = heights[shape];
                if (tr > h || bl > w) continue;
                const SvtAv2IntraBlock b = {edges&1u ? 16u : 0u,edges&2u ? 16u : 0u,w,h,plane};
                uint16_t a_buf[132], l_buf[132], expected[4096];
                uint16_t *a = a_buf+2, *l = l_buf+2;
                const uint16_t base = (uint16_t)(1u << (bd-1u));
                const uint16_t first_top = b.y ? svt_av2_sample_load(frame.planes[plane]+(size_t)(b.y-1u)*frame.strides[plane]+b.x*step,bd) : base-1u;
                const uint16_t first_left = b.x ? svt_av2_sample_load(frame.planes[plane]+(size_t)b.y*frame.strides[plane]+(b.x-1u)*step,bd) : base+1u;
                a[-1] = b.x && b.y ? svt_av2_sample_load(frame.planes[plane]+(size_t)(b.y-1u)*frame.strides[plane]+(b.x-1u)*step,bd) : b.y ? first_top : b.x ? first_left : base;
                l[-2] = l[-1] = a[-2] = a[-1];
                const uint32_t extra_tr = plane && w > 32u ? 0u : SVT_AV2_MIN(tr,w);
                const uint32_t extra_bl = plane && h > 32u ? 0u : SVT_AV2_MIN(bl,h);
                for (uint32_t i = 0u; i < w+h+2u; ++i) {
                    const uint32_t ax = i < w+extra_tr ? i : w+extra_tr-1u;
                    const uint32_t ly = i < h+extra_bl ? i : h+extra_bl-1u;
                    a[i] = b.y ? svt_av2_sample_load(frame.planes[plane]+(size_t)(b.y-1u)*frame.strides[plane]+(b.x+ax)*step,bd) : b.x ? first_left : base-1u;
                    l[i] = b.x ? svt_av2_sample_load(frame.planes[plane]+(size_t)(b.y+ly)*frame.strides[plane]+(b.x-1u)*step,bd) : b.y ? first_top : base+1u;
                }
                MB_MODE_INFO mbmi = {0};
                const uint32_t base_mode = svt_av2_intra_mode_base(mode);
                static const PREDICTION_MODE prediction_modes[14] = {DC_PRED,V_PRED,H_PRED,DC_PRED,
                    SMOOTH_PRED,SMOOTH_V_PRED,SMOOTH_H_PRED,PAETH_PRED,D45_PRED,D67_PRED,
                    D113_PRED,D135_PRED,D157_PRED,D203_PRED};
                static const TX_SIZE tx_sizes[4] = {TX_4X8,TX_8X4,TX_8X16,TX_16X8};
                const int delta = svt_av2_intra_mode_angle(mode) - svt_av2_intra_mode_angle(base_mode);
                const int angle = wide_angle_mapping(&mbmi,delta,tx_sizes[shape],prediction_modes[base_mode],(int)plane);
                if (angle > 90 && angle < 180) { a[w] = a[w-1u]; l[h] = l[h-1u]; }
                else if (angle < 90) a[w+h] = a[w+h+1u] = a[w+h-1u];
                else l[w+h] = l[w+h+1u] = l[w+h-1u];
                const Oracle oracle = plane ? (angle < 90 ? av2_highbd_dr_prediction_z1_c :
                    angle > 180 ? av2_highbd_dr_prediction_z3_c : av2_highbd_dr_prediction_z2_c) :
                    (angle < 90 ? av2_highbd_dr_prediction_z1_idif_c :
                    angle > 180 ? av2_highbd_dr_prediction_z3_idif_c : av2_highbd_dr_prediction_z2_idif_c);
                oracle(expected,w,(int)w,(int)h,a,l,av2_get_dx(angle),av2_get_dy(angle),(int)bd,0);
                uint8_t actual[8192];
                if (svt_av2_intra_directional_predict(&frame,&b,mode,tr,bl,actual,w*step) != 0) return 2;
                for (uint32_t i = 0u; i < w*h; ++i) {
                    if (svt_av2_sample_load(actual+i*step,bd) != expected[i]) {
                        fprintf(stderr,"mismatch bd=%u size=%ux%u edges=%u tr=%u bl=%u mode=%u pixel=%u\n",bd,w,h,edges,tr,bl,mode,i);
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
