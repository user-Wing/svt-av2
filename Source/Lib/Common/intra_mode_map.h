#ifndef SVT_AV2_INTRA_MODE_MAP_H
#define SVT_AV2_INTRA_MODE_MAP_H
#include <stdint.h>

/* Stable zero-angle IDs 0..13; 14..61 are six nonzero deltas per
 * directional base in joint order (D45,D67,V,D113,D135,D157,H,D203). */
static inline uint32_t svt_av2_intra_mode_joint(uint32_t mode) {
    static const uint8_t joint[14] = {0,22,50,0,1,2,3,4,8,15,29,36,43,57};
    if (mode < 14u) return joint[mode];
    if (mode > 61u) return UINT32_MAX;
    const uint32_t extra = mode - 14u, slot = extra % 6u;
    return 5u + 7u * (extra / 6u) + (slot < 3u ? slot : slot + 1u);
}

static inline uint32_t svt_av2_intra_mode_base(uint32_t mode) {
    static const uint8_t base[8] = {8,9,1,10,11,12,2,13};
    if (mode < 14u) return mode;
    return mode <= 61u ? base[(mode - 14u) / 6u] : UINT32_MAX;
}

static inline int svt_av2_intra_mode_angle(uint32_t mode) {
    static const int base_angle[8] = {45,67,90,113,135,157,180,203};
    const uint32_t joint = svt_av2_intra_mode_joint(mode);
    if (joint < 5u || joint > 60u) return 0;
    return base_angle[(joint - 5u) / 7u] + ((int)((joint - 5u) % 7u) - 3) * 3;
}

#ifndef M6_FLIP_TX
#define M6_FLIP_TX 0
#endif
#if M6_FLIP_TX
/* Fixed AVM entropymode.h av2_md_idx2type; current own Y geometry domain.
 * Only 2-D types 0..8 are used here; 1-D/IDTX need separate coefficient paths. */
/* AVM reconintra.h wide_angle_mapping: TX symbol class follows the
 * mapped prediction mode, including angle delta and the MRL angle offset. */
static inline uint32_t svt_av2_intra_tx_mode(uint32_t mode,uint32_t w,uint32_t h,uint32_t mrl) {
    static const int offsets[4]={0,1,-1,0};
    const uint32_t base=svt_av2_intra_mode_base(mode);
    const uint32_t joint=svt_av2_intra_mode_joint(mode);
    if (joint<5u || joint>60u) return base;
    const int angle=svt_av2_intra_mode_angle(mode)+offsets[mrl&3u];
    if (h==2u*w && angle<61) return 13u; /* D203 */
    if (w==2u*h && angle>209) return 8u; /* D45 */
    return base;
}

static inline uint32_t svt_av2_intra_tx_symbol(uint32_t mode,uint32_t w,uint32_t h,uint32_t type) {
    static const uint8_t avm_mode[14]={0,1,2,0,9,10,11,12,3,8,5,4,6,7};
    const uint32_t base=svt_av2_intra_mode_base(mode);
    if (base>=14u || base==3u || type>8u ||
        !((w==h && w<=16u) || (w==8u && h==16u) || (w==16u && h==8u))) return UINT32_MAX;
    const uint32_t side=w<h?w:h;
    if (side!=4u && side!=8u && side!=16u) return UINT32_MAX;
    static const uint8_t types[3][13][7]={
        {
            {0,3,1,2,7,8,13},
            {0,3,1,2,7,10,12},
            {0,3,1,2,8,11,13},
            {0,3,1,2,6,7,8},
            {0,3,1,2,7,8,13},
            {0,3,1,2,7,12,14},
            {0,3,1,2,7,8,13},
            {0,3,1,2,8,11,13},
            {0,3,1,2,7,10,12},
            {0,3,1,2,6,7,8},
            {0,3,1,2,7,8,12},
            {0,3,1,2,7,8,13},
            {0,3,2,10,11,12,13},
        },
        {
            {0,3,1,2,4,7,8},
            {0,3,1,2,7,6,8},
            {0,3,1,2,8,4,7},
            {0,3,1,2,6,7,8},
            {0,3,1,2,5,7,8},
            {0,3,1,2,5,7,8},
            {0,3,1,2,4,7,8},
            {0,3,1,2,4,8,7},
            {0,3,1,2,5,6,7},
            {0,3,1,2,6,7,8},
            {0,3,1,2,4,7,8},
            {0,3,1,2,5,7,8},
            {0,3,1,2,10,11,13},
        },
        {
            {0,3,1,2,4,7,8},
            {0,3,1,2,5,7,8},
            {0,3,1,2,4,8,7},
            {0,3,1,2,4,7,8},
            {0,3,1,2,4,7,8},
            {0,3,1,2,5,7,8},
            {0,3,1,2,4,7,8},
            {0,3,1,2,4,6,8},
            {0,3,1,2,5,7,8},
            {0,3,1,2,4,7,8},
            {0,3,1,2,4,7,8},
            {0,3,1,2,5,7,8},
            {0,3,1,2,10,11,12},
        },
    };
    const uint32_t size=side==4u?0u:side==8u?1u:2u;
    for (uint32_t symbol=0u;symbol<7u;++symbol)
        if (types[size][avm_mode[base]][symbol]==type) return symbol;
    return UINT32_MAX;
}
#endif

#endif
