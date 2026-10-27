#include <stdio.h>
#include "coeff_syntax.h"
#include "av2/common/common_data.h"
#include "av2/common/scan.h"

int main(void) {
    const TX_SIZE avm_sizes[SVT_AV2_TX_SIZE_COUNT] = {TX_4X4,TX_8X8,TX_16X16,TX_32X32,TX_64X64,
        TX_4X8,TX_8X4,TX_8X16,TX_16X8,TX_32X64,TX_64X32,TX_16X32,TX_32X16,TX_16X64,TX_64X16,TX_8X32,TX_32X8};
    size_t positions = 0u;
    for (uint32_t i = 0u; i < SVT_AV2_TX_SIZE_COUNT; ++i) {
        const SvtAv2TxSize size = (SvtAv2TxSize)i;
        const TX_SIZE expected = avm_sizes[i];
        uint16_t actual[1024];
        const size_t count = svt_av2_coeff_scan_dct_dct(size,actual);
        const uint32_t width = (uint32_t)tx_size_wide[expected];
        const uint32_t height = (uint32_t)tx_size_high[expected];
        const uint32_t context = ((uint32_t)txsize_sqr_map[expected]+(uint32_t)txsize_sqr_up_map[expected]+1u)>>1u;
        SvtAv2TxSize mapped;
        if (count != (size_t)(width<32u?width:32u)*(height<32u?height:32u) ||
            svt_av2_coeff_tx_width(size)!=width || svt_av2_coeff_tx_height(size)!=height ||
            svt_av2_coeff_tx_sz_ctx(size)!=context ||
            svt_av2_coeff_tx_size_from_dims(width,height,&mapped)!=0 || mapped!=size) return 1;
        for (size_t k=0u;k<count;++k) {
            if (actual[k] != (uint16_t)av2_scan_orders[expected][DCT_DCT].scan[k]) {
                fprintf(stderr,"FAIL shape=%u index=%zu actual=%u expected=%d\n",i,k,actual[k],av2_scan_orders[expected][DCT_DCT].scan[k]);
                return 1;
            }
            ++positions;
        }
    }
    printf("PASS fixed AVM scan: %u shapes / %zu positions\n",SVT_AV2_TX_SIZE_COUNT,positions);
    return 0;
}
