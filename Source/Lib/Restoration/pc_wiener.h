#ifndef SVT_AV2_PC_WIENER_H
#define SVT_AV2_PC_WIENER_H

#include <stddef.h>
#include <stdint.h>

/* Fixed AVM scalar PC_WIENER: one tile, SB64, RU256, all Y units enabled.
 * src/dst are distinct u16 sample planes; strides count samples. Width and
 * height are multiples of 8. tskip is a 4x4 binary grid, stride in entries.
 * Input is deblocked Y, with CDEF/Y-CCSO disabled; no delta-Q or lossless.
 * base_q_idx is the bitstream index, including the 10-bit +48 mapping.
 * Returns zero on success. UV is not read or written. */
int svt_av2_pcwiener_filter_y(const uint16_t* src, ptrdiff_t src_stride,
    uint16_t* dst, ptrdiff_t dst_stride, uint32_t width, uint32_t height,
    uint32_t bit_depth, uint32_t base_q_idx, const uint8_t* tskip,
    ptrdiff_t tskip_stride);

#endif
