/* Compare carried coefficients and reconstruction directly with frozen AVM. */
#include "transform.h"
#include "av2/common/idct.h"
#include "av2/common/common_data.h"
#include <stdio.h>
#include <string.h>

extern void fwd_txfm_c(const int16_t*, tran_low_t*, int, TxfmParam*);

int main(void) {
    const TX_SIZE sizes[8] = {TX_16X32,TX_32X16,TX_32X64,TX_64X32,TX_8X32,TX_32X8,TX_16X64,TX_64X16};
    uint32_t random=104729u;
    uint64_t cases=0u,samples=0u;
    for (uint32_t bd=8u;bd<=10u;bd+=2u)
    for (uint32_t size=0u;size<8u;++size)
    for (uint32_t trial=0u;trial<100u;++trial) {
        const uint32_t w=(uint32_t)tx_size_wide[sizes[size]],h=(uint32_t)tx_size_high[sizes[size]];
        const uint32_t count=(w<32u?w:32u)*(h<32u?h:32u),maximum=(1u<<bd)-1u;
        int16_t input[4096],inverse[4096];
        int32_t actual[4096],expected[4096],dq[4096];
        uint16_t decoded[4096];
        for (uint32_t i=0u;i<w*h;++i) {
            random=random*1664525u+1013904223u;
            input[i]=(int16_t)((int)((random>>10u)%(2u*maximum+1u))-(int)maximum);
            if (trial<6u) input[i]=(int16_t)(trial==0u?0:trial==1u?(int)maximum:trial==2u?-(int)maximum:
                trial==3u?(i&1u?(int)maximum:-(int)maximum):trial==4u?(i==0u?(int)maximum:0):(i==w*h-1u?-(int)maximum:0));
        }
        TxfmParam param={0};param.tx_type=DCT_DCT;param.tx_size=sizes[size];param.bd=(int)bd;
        fwd_txfm_c(input,expected,(int)w,&param);
        if (svt_av2_fwd_transform_bd(input,actual,w,h,bd)!=0 || memcmp(actual,expected,count*sizeof(*actual))!=0) {
            for (uint32_t i=0u;i<count;++i) if(actual[i]!=expected[i]) {
                printf("FAIL forward bd=%u w=%u h=%u trial=%u i=%u got=%d expected=%d\n",bd,w,h,trial,i,actual[i],expected[i]);break;
            }
            return 1;
        }
        for (uint32_t i=0u;i<count;++i) {
            random=random*1664525u+1013904223u;
            dq[i]=(int32_t)((random>>10u)%524289u)-262144;
            if (trial<7u) dq[i]=trial==0u?0:trial==1u?262144:trial==2u?-262144:
                trial==3u?(i==0u?12345:0):trial==4u?(i==1u?-12345:0):
                trial==5u?(i==count-1u?4197:0):(i&1u?131071:-131072);
        }
        if (svt_av2_inv_transform_bd(dq,inverse,w,h,bd)!=0) return 1;
        for (uint32_t prediction=0u;prediction<3u;++prediction) {
            const int middle=prediction==0u?0:prediction==1u?(1<<(bd-1u)):(int)maximum;
            for (uint32_t i=0u;i<w*h;++i) decoded[i]=(uint16_t)middle;
            inv_txfm_c(dq,decoded,(int)w,&param);
            for (uint32_t i=0u;i<w*h;++i) {
                const int v=middle+inverse[i];
                const uint16_t clipped=(uint16_t)(v<0?0:v>(int)maximum?maximum:(uint32_t)v);
                if (clipped!=decoded[i]) {
                    printf("FAIL inverse bd=%u w=%u h=%u trial=%u i=%u got=%u expected=%u\n",bd,w,h,trial,i,clipped,decoded[i]);return 1;
                }
            }
        }
        cases+=2u;samples+=count+3u*w*h;
    }
    printf("PASS fixed AVM long TX: %llu cases, %llu samples\n",(unsigned long long)cases,(unsigned long long)samples);
    return 0;
}
