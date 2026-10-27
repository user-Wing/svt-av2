/*
 * SVT-AV2 - BitWriter and LEB128 primitives (T003).
 *
 * See bit_writer.h for the frozen contract and the spec references.
 */
#include "bit_writer.h"

/* Verifies that num_bits can be written without touching the writer. */
static int bit_writer_can_write(const SvtAv2BitWriter* bw, size_t num_bits) {
    if (bw == NULL) {
        return SVT_AV2_BW_ERROR_NULL;
    }
    if (bw->finalized) {
        return SVT_AV2_BW_ERROR_STATE;
    }
    if (num_bits > bw->capacity * 8u - bw->bit_count) {
        return SVT_AV2_BW_ERROR_NO_SPACE;
    }
    return SVT_AV2_BW_OK;
}

void svt_av2_bit_writer_init(SvtAv2BitWriter* bw, uint8_t* buffer, size_t capacity) {
    if (bw == NULL) {
        return;
    }
    bw->buffer = buffer;
    bw->capacity = capacity;
    bw->bit_count = 0;
    bw->finalized = 0;

    if (buffer != NULL) {
        for (size_t i = 0; i < capacity; ++i) {
            buffer[i] = 0;
        }
    }
}

int svt_av2_bit_writer_write_bit(SvtAv2BitWriter* bw, uint32_t bit) {
    if (bit > 1u) {
        return SVT_AV2_BW_ERROR_INVALID_ARG;
    }
    return svt_av2_bit_writer_write_bits(bw, bit, 1u);
}

int svt_av2_bit_writer_write_bits(SvtAv2BitWriter* bw, uint32_t value, uint32_t num_bits) {
    if (num_bits > 32u) {
        return SVT_AV2_BW_ERROR_INVALID_ARG;
    }
    const int status = bit_writer_can_write(bw, num_bits);
    if (status != SVT_AV2_BW_OK) {
        return status;
    }

    for (uint32_t i = 0; i < num_bits; ++i) {
        const uint32_t shift = num_bits - 1u - i;
        const uint32_t bit = (value >> shift) & 1u;
        const size_t   byte_index = bw->bit_count >> 3;
        const uint32_t bit_in_byte = (uint32_t)(bw->bit_count & 7u);

        if (bit != 0u) {
            bw->buffer[byte_index] |= (uint8_t)(1u << (7u - bit_in_byte));
        } else {
            bw->buffer[byte_index] &= (uint8_t)~(1u << (7u - bit_in_byte));
        }
        ++bw->bit_count;
    }
    return SVT_AV2_BW_OK;
}

int svt_av2_bit_writer_byte_aligned(const SvtAv2BitWriter* bw) {
    if (bw == NULL) {
        return 0;
    }
    return (bw->bit_count & 7u) == 0u;
}

int svt_av2_bit_writer_write_uvlc(SvtAv2BitWriter* bw, uint32_t value) {
    if (value > SVT_AV2_BIT_WRITER_MAX_UVLC_VALUE) {
        return SVT_AV2_BW_ERROR_RANGE;
    }

    /* Leading zeros of (value + 1); at most 31 for a value <= 2^32 - 2. */
    uint32_t leading_zeros = 0;
    uint32_t probe = value + 1u;
    while (probe > 1u) {
        probe >>= 1;
        ++leading_zeros;
    }

    const size_t total_bits = (size_t)leading_zeros * 2u + 1u;
    const int    status = bit_writer_can_write(bw, total_bits);
    if (status != SVT_AV2_BW_OK) {
        return status;
    }

    for (uint32_t i = 0; i < leading_zeros; ++i) {
        (void)svt_av2_bit_writer_write_bit(bw, 0u);
    }
    (void)svt_av2_bit_writer_write_bit(bw, 1u);
    /* The trailing bits carry value - (1 << leading_zeros) + 1 (spec 4.11.3). */
    const uint32_t extra = value - (1u << leading_zeros) + 1u;
    for (uint32_t i = 0; i < leading_zeros; ++i) {
        const uint32_t bit = (extra >> (leading_zeros - 1u - i)) & 1u;
        (void)svt_av2_bit_writer_write_bit(bw, bit);
    }
    return SVT_AV2_BW_OK;
}

/* Bits needed to represent value, at least 1 (FloorLog2(value) + 1). */
static uint32_t bit_writer_bits_for_value(uint32_t value) {
    uint32_t bits = 1;
    while ((value >> bits) != 0u) {
        ++bits;
    }
    return bits;
}

int svt_av2_bit_writer_write_ns(SvtAv2BitWriter* bw, uint32_t n, uint32_t value) {
    if (bw == NULL) {
        return SVT_AV2_BW_ERROR_NULL;
    }
    if (n < 1u || value >= n) {
        return SVT_AV2_BW_ERROR_INVALID_ARG;
    }

    const uint32_t w = bit_writer_bits_for_value(n - 1u); /* FloorLog2(n) + 1 */
    const uint32_t m = (1u << w) - n;

    if (value < m) {
        return svt_av2_bit_writer_write_bits(bw, value, w - 1u);
    }
    /* value + m < 2^w by construction, so w bits hold it exactly. */
    return svt_av2_bit_writer_write_bits(bw, value + m, w);
}

int svt_av2_bit_writer_byte_alignment(SvtAv2BitWriter* bw) {
    if (bw == NULL) {
        return SVT_AV2_BW_ERROR_NULL;
    }
    if (bw->finalized) {
        return SVT_AV2_BW_ERROR_STATE;
    }

    const uint32_t used = (uint32_t)(bw->bit_count & 7u);
    if (used == 0u) {
        return SVT_AV2_BW_OK;
    }
    const uint32_t nb_bits = 8u - used;
    const int      status = bit_writer_can_write(bw, nb_bits);
    if (status != SVT_AV2_BW_OK) {
        return status;
    }
    for (uint32_t i = 0; i < nb_bits; ++i) {
        (void)svt_av2_bit_writer_write_bit(bw, 0u);
    }
    return SVT_AV2_BW_OK;
}

int svt_av2_bit_writer_write_byte(SvtAv2BitWriter* bw, uint8_t byte) {
    return svt_av2_bit_writer_write_bytes(bw, &byte, 1u);
}

int svt_av2_bit_writer_write_bytes(SvtAv2BitWriter* bw, const uint8_t* data, size_t size) {
    if (bw == NULL) {
        return SVT_AV2_BW_ERROR_NULL;
    }
    if (bw->finalized) {
        return SVT_AV2_BW_ERROR_STATE;
    }
    if (size != 0u && data == NULL) {
        return SVT_AV2_BW_ERROR_NULL;
    }
    if ((bw->bit_count & 7u) != 0u) {
        return SVT_AV2_BW_ERROR_UNALIGNED;
    }
    if (size > bw->capacity - (bw->bit_count >> 3)) {
        return SVT_AV2_BW_ERROR_NO_SPACE;
    }

    for (size_t i = 0; i < size; ++i) {
        bw->buffer[(bw->bit_count >> 3) + i] = data[i];
    }
    bw->bit_count += size * 8u;
    return SVT_AV2_BW_OK;
}

size_t svt_av2_leb128_size(uint32_t value) {
    size_t size = 1;
    while (value >= 0x80u) {
        value >>= 7;
        ++size;
    }
    return size;
}

int svt_av2_bit_writer_write_leb128(SvtAv2BitWriter* bw, uint32_t value) {
    if (bw == NULL) {
        return SVT_AV2_BW_ERROR_NULL;
    }
    if (bw->finalized) {
        return SVT_AV2_BW_ERROR_STATE;
    }
    if ((bw->bit_count & 7u) != 0u) {
        return SVT_AV2_BW_ERROR_UNALIGNED;
    }

    const size_t size = svt_av2_leb128_size(value);
    if (size > bw->capacity - (bw->bit_count >> 3)) {
        return SVT_AV2_BW_ERROR_NO_SPACE;
    }

    uint8_t bytes[SVT_AV2_BIT_WRITER_MAX_LEB128_BYTES];
    uint32_t remaining = value;
    for (size_t i = 0; i < size; ++i) {
        uint8_t byte = (uint8_t)(remaining & 0x7Fu);
        remaining >>= 7;
        if (remaining != 0u) {
            byte |= 0x80u;
        }
        bytes[i] = byte;
    }

    return svt_av2_bit_writer_write_bytes(bw, bytes, size);
}

int svt_av2_bit_writer_write_trailing_bits(SvtAv2BitWriter* bw, uint32_t nb_bits) {
    if (nb_bits < 1u) {
        return SVT_AV2_BW_ERROR_INVALID_ARG;
    }
    const int status = svt_av2_bit_writer_write_bit(bw, 1u);
    if (status != SVT_AV2_BW_OK) {
        return status;
    }
    for (uint32_t i = 1; i < nb_bits; ++i) {
        const int zero_status = svt_av2_bit_writer_write_bit(bw, 0u);
        if (zero_status != SVT_AV2_BW_OK) {
            return zero_status;
        }
    }
    return SVT_AV2_BW_OK;
}

int svt_av2_bit_writer_write_trailing_bits_align(SvtAv2BitWriter* bw) {
    if (bw == NULL) {
        return SVT_AV2_BW_ERROR_NULL;
    }
    if (bw->finalized) {
        return SVT_AV2_BW_ERROR_STATE;
    }

    const uint32_t used = (uint32_t)(bw->bit_count & 7u);
    const uint32_t nb_bits = (used == 0u) ? 8u : (8u - used);
    return svt_av2_bit_writer_write_trailing_bits(bw, nb_bits);
}

int svt_av2_bit_writer_finalize(SvtAv2BitWriter* bw) {
    if (bw == NULL) {
        return SVT_AV2_BW_ERROR_NULL;
    }
    if (bw->finalized) {
        return SVT_AV2_BW_ERROR_STATE;
    }
    if ((bw->bit_count & 7u) != 0u) {
        return SVT_AV2_BW_ERROR_UNALIGNED;
    }
    bw->finalized = 1;
    return SVT_AV2_BW_OK;
}

size_t svt_av2_bit_writer_bits_written(const SvtAv2BitWriter* bw) {
    return (bw == NULL) ? 0u : bw->bit_count;
}

size_t svt_av2_bit_writer_bytes_written(const SvtAv2BitWriter* bw) {
    return (bw == NULL) ? 0u : (bw->bit_count + 7u) / 8u;
}
