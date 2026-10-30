/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at aomedia.org/license/software-license/bsd-3-c-c/.  If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * aomedia.org/license/patent-license/.
 */

#include "deblock_luma.h"
#include "quantization.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define AVMMIN(a,b) ((a)<(b)?(a):(b))
#define AVMMAX(a,b) ((a)>(b)?(a):(b))
#define ROUND_POWER_OF_TWO(v,n) (((v)+((1<<(n))>>1))>>(n))
#define MAX_SIDE_TABLE 296
static int clamp(int v,int lo,int hi) {return v<lo?lo:v>hi?hi:v;}
static uint16_t clip_pixel_highbd(int v,int bd) {
    return (uint16_t)clamp(v,0,(1<<bd)-1);
}
#define DF_8_THRESH 3
#define DF_6_THRESH 4
#define FILT_8_THRESH_SHIFT 3

#define MAX_DBL_FLT_LEN 8

#define DF_SHIFT 8
static const int w_mult[MAX_DBL_FLT_LEN] = { 85, 51, 37, 28, 23, 20, 17, 15 };
static const int q_thresh_mults[MAX_DBL_FLT_LEN] = { 32, 25, 19, 19,
                                                     18, 18, 17, 17 };

#define DBL_CUSTOM_DECIS 3
#define DBL_REG_DECIS_LEN MAX_DBL_FLT_LEN - DBL_CUSTOM_DECIS

#define DF_Q_THRESH_SHIFT 4
static const int q_first[DBL_REG_DECIS_LEN] = { 45, 43, 40, 35, 32 };

#define SEC_DERIV_ARRAY_LEN (MAX_DBL_FLT_LEN + 1) * 2

static const int16_t side_thresholds[MAX_SIDE_TABLE] = {
  -16,  -16,  -16,  -16,  -16,  -16,  -16,  -16,  -16,  -16,  -16,  -16,  -16,
  -16,  -16,  -16,  -16,  -16,  -16,  -16,  -16,  -16,  -16,  -16,  -16,  -16,
  -16,  -16,  -16,  -16,  -16,  -16,  -16,  -16,  -16,  -16,  -16,  -16,  -16,
  -16,  -16,  -16,  -16,  -16,  -16,  -16,  -16,  -16,  -16,  -16,  -16,  -16,
  -16,  -16,  -16,  -16,  -16,  -14,  -13,  -11,  -10,  -9,   -7,   -6,   -4,
  -3,   -2,   0,    0,    2,    3,    5,    6,    7,    9,    10,   12,   13,
  15,   16,   17,   19,   20,   22,   23,   24,   26,   27,   29,   30,   32,
  33,   34,   36,   37,   39,   40,   42,   43,   44,   46,   47,   49,   50,
  51,   53,   54,   56,   57,   59,   60,   61,   63,   64,   66,   67,   69,
  70,   71,   73,   74,   76,   77,   78,   80,   81,   83,   84,   86,   87,
  88,   90,   91,   93,   94,   96,   101,  111,  120,  130,  140,  150,  160,
  170,  180,  190,  200,  210,  220,  230,  240,  249,  259,  269,  279,  289,
  299,  309,  319,  329,  339,  349,  359,  368,  378,  388,  398,  408,  418,
  428,  438,  448,  458,  468,  478,  488,  497,  507,  517,  527,  537,  547,
  557,  567,  577,  587,  597,  607,  616,  626,  636,  646,  656,  666,  676,
  686,  696,  706,  716,  726,  736,  745,  755,  765,  775,  785,  795,  805,
  815,  825,  835,  845,  855,  864,  874,  884,  894,  904,  914,  924,  934,
  944,  954,  964,  974,  984,  993,  1003, 1013, 1023, 1033, 1043, 1053, 1063,
  1073, 1083, 1093, 1103, 1112, 1122, 1132, 1142, 1152, 1162, 1172, 1182, 1192,
  1202, 1212, 1222, 1232, 1241, 1251, 1261, 1271, 1281, 1291, 1301, 1311, 1321,
  1331, 1341, 1351, 1360, 1370, 1380, 1390, 1400, 1410, 1420, 1430, 1440, 1450,
  1460, 1470, 1480, 1489, 1499, 1509, 1519, 1529, 1539, 1549, 1559, 1569, 1579,
  1589, 1599, 1608, 1618, 1628, 1638, 1648, 1658, 1668, 1678
};

static inline void filt_generic_asym_highbd(int q_threshold, int width_neg,
                                            int width_pos, uint16_t *s,
                                            const int pitch, int bd,
                                            int is_lossless_neg,
                                            int is_lossless_pos

) {
  if (width_neg < 1) return;
  if (width_pos < 1) return;

  int width = AVMMAX(width_neg, width_pos);
  int delta_m2 = (3 * (s[0] - s[-1 * pitch]) - (s[pitch] - s[-2 * pitch])) * 4;

  int q_thresh_clamp = q_threshold * q_thresh_mults[width - 1];
  delta_m2 = clamp(delta_m2, -q_thresh_clamp, q_thresh_clamp);

  if (!is_lossless_neg) {
    int delta_m2_neg = delta_m2 * w_mult[width_neg - 1];
    for (int i = 0; i < width_neg; i++) {
      s[(-i - 1) * pitch] = clip_pixel_highbd(
          s[(-i - 1) * pitch] +
              ROUND_POWER_OF_TWO(delta_m2_neg * (width_neg - i), 3 + DF_SHIFT),
          bd);
    }
  }
  if (!is_lossless_pos) {
    int delta_m2_pos = delta_m2 * w_mult[width_pos - 1];
    for (int i = 0; i < width_pos; i++) {
      s[i * pitch] = clip_pixel_highbd(
          s[i * pitch] -
              ROUND_POWER_OF_TWO(delta_m2_pos * (width_pos - i), 3 + DF_SHIFT),
          bd);
    }
  }
}

// Determining number of samples to be modified for the current row/column
static int filt_choice_highbd(uint16_t *s, int pitch, int max_filt_neg,
                       int max_filt_pos, uint16_t q_thresh,
                       uint16_t side_thresh, uint16_t *t) {
  if (!q_thresh || !side_thresh) return 0;

  int max_samples_neg = max_filt_neg == 0 ? 0 : max_filt_neg / 2 - 1;
  int max_samples_pos = max_filt_pos / 2 - 1;

  assert(max_samples_pos == 1 || max_samples_pos == 3 || max_samples_pos == 4 ||
         max_samples_pos == 6 || max_samples_pos == 8);

  if (max_samples_pos < 1 || max_samples_pos < max_samples_neg) return 0;

  int32_t second_derivs_buf[SEC_DERIV_ARRAY_LEN];
  int32_t *second_deriv = &second_derivs_buf[(SEC_DERIV_ARRAY_LEN >> 1)];

  int mask = 0;

  // Testing for 1 sample modification
  //-----------------------------------------------
  second_deriv[-2] = abs(s[-3 * pitch] - (s[-2 * pitch] << 1) + s[-pitch]);
  second_deriv[1] = abs(s[0] - (s[pitch] << 1) + s[2 * pitch]);

  second_deriv[-2] += abs(t[-3 * pitch] - (t[-2 * pitch] << 1) + t[-pitch]);
  second_deriv[-2] = (second_deriv[-2] + 1) >> 1;

  second_deriv[1] += abs(t[0] - (t[pitch] << 1) + t[2 * pitch]);
  second_deriv[1] = (second_deriv[1] + 1) >> 1;

  mask |= (second_deriv[-2] > side_thresh) * -1;
  mask |= (second_deriv[1] > side_thresh) * -1;

  if (mask) return 0;

  if (max_samples_pos == 1) return 1;

  // Testing for 2 sample modification
  //-----------------------------------------------
  const int side_thresh2 = side_thresh >> 2;

  mask |= (second_deriv[-2] > side_thresh2) * -1;
  mask |= (second_deriv[1] > side_thresh2) * -1;

  second_deriv[-1] = abs(s[-2 * pitch] - (s[-pitch] << 1) + s[0]);

  second_deriv[-1] += abs(t[-2 * pitch] - (t[-pitch] << 1) + t[0]);
  second_deriv[-1] = (second_deriv[-1] + 1) >> 1;

  second_deriv[0] = abs(s[-1 * pitch] - (s[0] << 1) + s[pitch]);

  second_deriv[0] += abs(t[-1 * pitch] - (t[0] << 1) + t[pitch]);
  second_deriv[0] = (second_deriv[0] + 1) >> 1;

  mask |= ((second_deriv[-1] + second_deriv[0]) > q_thresh * DF_6_THRESH) * -1;

  if (mask) return 1;

  // Testing 3 sample modification
  //-----------------------------------------------
  const int side_thresh3 = side_thresh >> FILT_8_THRESH_SHIFT;

  mask |= (second_deriv[-2] > side_thresh3) * -1;
  mask |= (second_deriv[1] > side_thresh3) * -1;

  mask |= ((second_deriv[-1] + second_deriv[0]) > q_thresh * DF_8_THRESH) * -1;

  int end_dir_thresh = (side_thresh * 3) >> 4;

  if (max_samples_neg > 2)
    mask |= (((abs((s[-1 * pitch] - s[(-3 - 1) * pitch]) -
                   3 * (s[-1 * pitch] - s[-2 * pitch])) +
               abs((t[-1 * pitch] - t[(-3 - 1) * pitch]) -
                   3 * (t[-1 * pitch] - t[-2 * pitch])) +
               1) >>
              1) > end_dir_thresh) *
            -1;
  mask |= (((abs((s[0] - s[3 * pitch]) - 3 * (s[0] - s[1 * pitch])) +
             abs((t[0] - t[3 * pitch]) - 3 * (t[0] - t[1 * pitch])) + 1) >>
            1) > end_dir_thresh) *
          -1;

  if (mask) return 2;

  if (max_samples_pos == 3) return 3;

  // Testing  4 sample modification and above
  //-----------------------------------------------

  int transition = (second_deriv[-1] + second_deriv[0]) << DF_Q_THRESH_SHIFT;

  for (int dist = 4; dist < MAX_DBL_FLT_LEN + 1; dist += 2) {
    const int q_thresh4 = q_thresh * q_first[dist - 4];

    mask |= (transition > q_thresh4) * -1;

    end_dir_thresh = (side_thresh * dist) >> 4;

    if (dist == 8) dist = 7;

    if (max_samples_neg >= dist)

      mask |= (((abs((s[-1 * pitch] - s[(-dist - 1) * pitch]) -
                     dist * (s[-1 * pitch] - s[-2 * pitch])) +
                 abs((t[-1 * pitch] - t[(-dist - 1) * pitch]) -
                     dist * (t[-1 * pitch] - t[-2 * pitch])) +
                 1) >>
                1) > end_dir_thresh) *
              -1;
    mask |=
        (((abs((s[0] - s[dist * pitch]) - dist * (s[0] - s[1 * pitch])) +
           abs((t[0] - t[dist * pitch]) - dist * (t[0] - t[1 * pitch])) + 1) >>
          1) > end_dir_thresh) *
        -1;

    if (dist == 7) dist = 8;

    if (mask) return dist == 4 ? dist - 1 : dist - 2;
    if (max_samples_pos <= dist) return ((dist >> 1) << 1);
  }
  return MAX_DBL_FLT_LEN;
}

static void avm_highbd_lpf_horizontal_generic_c(uint16_t *s, int pitch,
                                         int filt_width_neg, int filt_width_pos,
                                         const uint16_t *q_thresh,
                                         const uint16_t *side_thresh, int bd,
                                         int is_lossless_neg,
                                         int is_lossless_pos) {
  int i;

  int count = 4;

  int filt_neg = (filt_width_neg >> 1) - 1;
  int filter = filt_choice_highbd(s, pitch, filt_width_neg, filt_width_pos,
                                  *q_thresh, *side_thresh, s + count - 1);

  for (i = 0; i < count; ++i) {
    filt_generic_asym_highbd(*q_thresh, AVMMIN(filter, filt_neg), filter, s,
                             pitch, bd, is_lossless_neg, is_lossless_pos);
    ++s;
  }
}

static void avm_highbd_lpf_vertical_generic_c(uint16_t *s, int pitch,
                                       int filt_width_neg, int filt_width_pos,
                                       const uint16_t *q_thresh,
                                       const uint16_t *side_thresh, int bd,
                                       int is_lossless_neg,
                                       int is_lossless_pos) {
  int i;
  int count = 4;

  int filt_neg = (filt_width_neg >> 1) - 1;
  int filter =
      filt_choice_highbd(s, 1, filt_width_neg, filt_width_pos, *q_thresh,
                         *side_thresh, s + (count - 1) * pitch);

  // loop filter designed to work using chars so that we can make maximum use
  // of 8 bit simd instructions.
  for (i = 0; i < count; ++i) {
    filt_generic_asym_highbd(*q_thresh, AVMMIN(filter, filt_neg), filter, s, 1,
                             bd, is_lossless_neg, is_lossless_pos);
    s += pitch;
  }
}

static void db_filter_sb(uint16_t* pixels,uint32_t w,uint32_t h,
    const uint32_t* map,const SvtAv2TileCoeffBlock* blocks,
    uint32_t bx,uint32_t by,uint32_t mode,uint16_t q_threshold,
    uint16_t side_threshold,uint32_t bd,uint64_t* edge_calls) {
    const uint32_t cols=w/4u;

        const uint32_t outer_end=mode==1u?AVMMIN(by+64u,h):AVMMIN(bx+64u,w);
        const uint32_t inner_end=mode==1u?AVMMIN(bx+64u,w):AVMMIN(by+64u,h);
        for (uint32_t outer=mode==1u?by:bx;outer<outer_end;outer+=4u) {
            for (uint32_t inner=mode==1u?bx:by;inner<inner_end;inner+=4u) {
                const uint32_t x=mode==1u?inner:outer,y=mode==1u?outer:inner;
                if (!inner) continue;
                const uint32_t cur=map[(size_t)(y/4u)*cols+x/4u];
                const uint32_t prev=map[(size_t)((y-(mode==2u?4u:0u))/4u)*cols+
                    (x-(mode==1u?4u:0u))/4u];
                const SvtAv2TileCoeffBlock* b=blocks+cur;const SvtAv2TileCoeffBlock* a=blocks+prev;
                if (cur==prev || (mode==1u?b->x!=x:b->y!=y)) continue;
                uint32_t current=mode==1u?b->width:b->height;
                const uint32_t previous=mode==1u?a->width:a->height;
                const uint32_t available=mode==1u?w-x:h-y;
                if (available<16u) current=AVMMIN(current,16u);
                const uint32_t minimum=AVMMIN(current,previous);
                int neg,pos;
                if (minimum<=4u) neg=pos=4;
                else if (minimum==8u) neg=pos=8;
                else if (minimum==16u) neg=pos=14;
                else {pos=18;neg=(mode==2u && !(y&63u))?14:18;}
                if (!q_threshold || !side_threshold) continue;
                uint16_t* at=pixels+(size_t)y*w+x;
                if (mode==1u) avm_highbd_lpf_vertical_generic_c(at,(int)w,neg,pos,&q_threshold,&side_threshold,(int)bd,0,0);
                else avm_highbd_lpf_horizontal_generic_c(at,(int)w,neg,pos,&q_threshold,&side_threshold,(int)bd,0,0);
                ++*edge_calls;
            }
        }
    
}

SvtAv2Error svt_av2_deblock_luma_apply(SvtAv2FrameBuffer* recon,
    const SvtAv2TileCoeffBlock* blocks,size_t count,uint32_t mode,int delta,int delta_h,
    uint32_t actual_qindex,uint64_t* edge_calls,uint64_t* changed_pixels) {
    if (!recon || !blocks || !edge_calls || !changed_pixels || mode<1u || mode>3u ||
        delta< -2 || delta>1 || delta_h< -2 || delta_h>1 || (recon->bit_depth!=8u && recon->bit_depth!=10u) ||
        recon->chroma_format!=SVT_AV2_CHROMA_420 || (recon->visible_width&7u) ||
        (recon->visible_height&7u) || recon->visible_width>4096u || recon->visible_height>2160u)
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    *edge_calls=0u;*changed_pixels=0u;
    const uint32_t w=recon->visible_width,h=recon->visible_height,cols=w/4u,rows=h/4u;
    if (!w || !h || count>UINT32_MAX) return SVT_AV2_ERROR_INVALID_PARAMETER;
    uint16_t* pixels=(uint16_t*)malloc((size_t)w*h*sizeof(*pixels));
    uint32_t* map=(uint32_t*)malloc((size_t)cols*rows*sizeof(*map));
    if (!pixels || !map) {free(pixels);free(map);return SVT_AV2_ERROR_OUT_OF_MEMORY;}
    memset(map,255,(size_t)cols*rows*sizeof(*map));
    SvtAv2Error error=SVT_AV2_ERROR_INVALID_STATE;
    for (size_t i=0u;i<count;++i) {
        const SvtAv2TileCoeffBlock* b=blocks+i;if (b->plane) continue;
        if (!b->width || !b->height || ((b->x|b->y|b->width|b->height)&3u) ||
            b->x>w || b->y>h || b->width>w-b->x || b->height>h-b->y) goto done;
        for (uint32_t y=b->y/4u;y<(b->y+b->height)/4u;++y)
            for (uint32_t x=b->x/4u;x<(b->x+b->width)/4u;++x) {
                if (map[(size_t)y*cols+x]!=UINT32_MAX) goto done;
                map[(size_t)y*cols+x]=(uint32_t)i;
            }
    }
    for (size_t i=0u;i<(size_t)cols*rows;++i) if (map[i]==UINT32_MAX) goto done;
    const uint32_t bd=recon->bit_depth,step=bd>8u?2u:1u;
    for (uint32_t y=0u;y<h;++y) for (uint32_t x=0u;x<w;++x)
        pixels[(size_t)y*w+x]=svt_av2_sample_load(recon->planes[0]+(size_t)y*recon->strides[0]+x*step,bd);
    uint16_t q_threshold[2],side_threshold[2];
    const int deltas[2]={delta,delta_h};
    for (uint32_t direction=0u;direction<2u;++direction) {
        const int rawq=(int)actual_qindex+deltas[direction]*8;
        const int q=rawq==0?0:clamp(rawq,1,bd==10u?303:255);
        q_threshold[direction]=(uint16_t)(((svt_av2_quant_qlookup(q)+4)>>3)>>6);
        const int side_index=clamp(rawq-24*((int)bd-8),0,MAX_SIDE_TABLE-1);
        side_threshold[direction]=(uint16_t)(AVMMAX(side_thresholds[side_index]+(1<<(12-bd)),0)>>(13-bd));
    }
    /* AVM loop_filter_rows combined schedule: V(current), H(previous),
     * then flush H(last) at every SB row end. */
    for (uint32_t by=0u;by<h;by+=64u) {
        for (uint32_t bx=0u;bx<w;bx+=64u) {
            if (mode&1u) db_filter_sb(pixels,w,h,map,blocks,bx,by,1u,
                q_threshold[0],side_threshold[0],bd,edge_calls);
            if (mode==2u) db_filter_sb(pixels,w,h,map,blocks,bx,by,2u,
                q_threshold[1],side_threshold[1],bd,edge_calls);
            else if (mode==3u && bx>=64u) db_filter_sb(pixels,w,h,map,blocks,bx-64u,by,2u,
                q_threshold[1],side_threshold[1],bd,edge_calls);
        }
        if (mode==3u) db_filter_sb(pixels,w,h,map,blocks,((w-1u)/64u)*64u,by,2u,
            q_threshold[1],side_threshold[1],bd,edge_calls);
    }
    for (uint32_t y=0u;y<h;++y) for (uint32_t x=0u;x<w;++x) {
        uint8_t* target=recon->planes[0]+(size_t)y*recon->strides[0]+x*step;
        if (svt_av2_sample_load(target,bd)!=pixels[(size_t)y*w+x]) ++*changed_pixels;
        svt_av2_sample_store(target,bd,pixels[(size_t)y*w+x]);
    }
    error=SVT_AV2_ERROR_NONE;
done:
    free(pixels);free(map);return error;
}
