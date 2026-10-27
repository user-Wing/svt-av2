/*
 * T003 unit tests: BitWriter and LEB128.
 *
 * The expected bytes are hand-derived from the AV2 spec descriptors (4.11.2
 * f(n) MSB-first, 4.11.6 leb128) and cross-checked against an independent
 * accumulator-based reference model implemented below. The production writer
 * uses in-place bit addressing, so the two implementations do not share code.
 */
#include "bit_writer.h"

#include "test_util.h"

#include <stdint.h>
#include <string.h>

/* --- independent reference model ----------------------------------------- */

typedef struct {
    uint8_t  data[512];
    size_t   length;
    uint32_t accumulator;
    uint32_t nbits;
} RefWriter;

static void ref_write_bits(RefWriter* rw, uint32_t value, uint32_t num_bits) {
    for (uint32_t i = 0; i < num_bits; ++i) {
        const uint32_t shift = num_bits - 1u - i;
        rw->accumulator = (rw->accumulator << 1) | ((value >> shift) & 1u);
        if (++rw->nbits == 8u) {
            rw->data[rw->length++] = (uint8_t)rw->accumulator;
            rw->accumulator = 0;
            rw->nbits = 0;
        }
    }
}

static void ref_flush(RefWriter* rw) {
    if (rw->nbits != 0u) {
        rw->data[rw->length++] = (uint8_t)(rw->accumulator << (8u - rw->nbits));
        rw->accumulator = 0;
        rw->nbits = 0;
    }
}

static void ref_write_leb128(RefWriter* rw, uint32_t value) {
    uint32_t remaining = value;
    do {
        uint8_t byte = (uint8_t)(remaining & 0x7Fu);
        remaining >>= 7;
        if (remaining != 0u) {
            byte |= 0x80u;
        }
        rw->data[rw->length++] = byte;
    } while (remaining != 0u);
}

/* uvlc() (spec 4.11.3): leadingZeros zeros, a one, then leadingZeros bits of
 * (value - (1 << leadingZeros) + 1). */
static void ref_write_uvlc(RefWriter* rw, uint32_t value) {
    uint32_t leading_zeros = 0;
    uint32_t probe = value + 1u;
    while (probe > 1u) {
        probe >>= 1;
        ++leading_zeros;
    }
    for (uint32_t i = 0; i < leading_zeros; ++i) {
        ref_write_bits(rw, 0u, 1u);
    }
    ref_write_bits(rw, 1u, 1u);
    const uint32_t extra = value - (1u << leading_zeros) + 1u;
    for (uint32_t i = 0; i < leading_zeros; ++i) {
        ref_write_bits(rw, (extra >> (leading_zeros - 1u - i)) & 1u, 1u);
    }
}

/* ns(n) (spec 4.11.13): w = FloorLog2(n)+1, m = (1<<w)-n; if value < m write
 * value in w-1 bits, else write value+m in w bits. */
static void ref_write_ns(RefWriter* rw, uint32_t n, uint32_t value) {
    uint32_t w = 1;
    while ((n - 1u) >> w) {
        ++w;
    }
    const uint32_t m = (1u << w) - n;
    if (value < m) {
        ref_write_bits(rw, value, w - 1u);
    } else {
        ref_write_bits(rw, value + m, w);
    }
}

static void check_bytes(const uint8_t* actual, const uint8_t* expected, size_t size) {
    SVT_TEST_CHECK(memcmp(actual, expected, size) == 0);
}

/* --- known-answer tests --------------------------------------------------- */

static void test_single_bits(void) {
    uint8_t       buffer[4];
    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));

    /* 1 0 1 1 0 0 0 1 -> 0xB1 */
    const uint32_t bits[8] = { 1, 0, 1, 1, 0, 0, 0, 1 };
    for (int i = 0; i < 8; ++i) {
        SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bit(&bw, bits[i]), SVT_AV2_BW_OK);
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bytes_written(&bw), 1);
    SVT_TEST_CHECK_EQ_INT(buffer[0], 0xB1);

    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bit(&bw, 2), SVT_AV2_BW_ERROR_INVALID_ARG);
}

static void test_msb_first_partial(void) {
    uint8_t       buffer[4];
    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));

    /* 6 bits 101100 -> 0b10110000 = 0xB0 */
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bits(&bw, 0x2Cu, 6u), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(buffer[0], 0xB0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), 6);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bytes_written(&bw), 1);
}

static void test_cross_byte(void) {
    uint8_t       buffer[4];
    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));

    /* 4 bits 1010 then 8 bits 01011010 -> 1010 0101 1010 0000 = A5 A0 */
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bits(&bw, 0xAu, 4u), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bits(&bw, 0x5Au, 8u), SVT_AV2_BW_OK);
    const uint8_t expected[] = { 0xA5, 0xA0 };
    check_bytes(buffer, expected, sizeof(expected));
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bytes_written(&bw), 2);
}

static void test_full_word(void) {
    uint8_t       buffer[4];
    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));

    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bits(&bw, 0xDEADBEEFu, 32u), SVT_AV2_BW_OK);
    const uint8_t expected[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    check_bytes(buffer, expected, sizeof(expected));
}

static void test_zero_bits_is_noop(void) {
    uint8_t       buffer[2];
    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));

    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bits(&bw, 0xFFFFu, 0u), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), 0);
}

static void test_capacity_boundary(void) {
    uint8_t       buffer[1];
    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));

    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bits(&bw, 0xABu, 8u), SVT_AV2_BW_OK);
    /* The next bit does not fit; the writer must be left unchanged. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bit(&bw, 1u), SVT_AV2_BW_ERROR_NO_SPACE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), 8);
    SVT_TEST_CHECK_EQ_INT(buffer[0], 0xAB);

    /* A 4-bit write that straddles the end also fails atomically. */
    SvtAv2BitWriter bw2;
    svt_av2_bit_writer_init(&bw2, buffer, sizeof(buffer));
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bits(&bw2, 0u, 6u), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bits(&bw2, 0xFu, 4u), SVT_AV2_BW_ERROR_NO_SPACE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw2), 6);
}

static void test_alignment_and_bytes(void) {
    uint8_t       buffer[4];
    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));

    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_byte_aligned(&bw), 1);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bits(&bw, 0x1u, 3u), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_byte_aligned(&bw), 0);

    /* Byte-aligned ops at a non byte boundary are rejected. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_byte(&bw, 0x00u), SVT_AV2_BW_ERROR_UNALIGNED);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bytes(&bw, (const uint8_t*)"\x00", 1u),
                          SVT_AV2_BW_ERROR_UNALIGNED);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_leb128(&bw, 5u), SVT_AV2_BW_ERROR_UNALIGNED);

    /* Finish the byte, then bytes work. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bits(&bw, 0x1Fu, 5u), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(buffer[0], 0x3F);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bytes(&bw, (const uint8_t*)"\x12\x34", 2u), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(buffer[1], 0x12);
    SVT_TEST_CHECK_EQ_INT(buffer[2], 0x34);
}

static void test_leb128_known_values(void) {
    static const struct {
        uint32_t value;
        uint8_t  bytes[SVT_AV2_BIT_WRITER_MAX_LEB128_BYTES];
        size_t   size;
    } cases[] = {
        { 0u, { 0x00 }, 1 },
        { 1u, { 0x01 }, 1 },
        { 127u, { 0x7F }, 1 },
        { 128u, { 0x80, 0x01 }, 2 },
        { 255u, { 0xFF, 0x01 }, 2 },
        { 16383u, { 0xFF, 0x7F }, 2 },
        { 16384u, { 0x80, 0x80, 0x01 }, 3 },
        { 0x1FFFFFu, { 0xFF, 0xFF, 0x7F }, 3 },
        { 0x200000u, { 0x80, 0x80, 0x80, 0x01 }, 4 },
        { 0x0FFFFFFFu, { 0xFF, 0xFF, 0xFF, 0x7F }, 4 },
        { 0x10000000u, { 0x80, 0x80, 0x80, 0x80, 0x01 }, 5 },
        { 0xFFFFFFFFu, { 0xFF, 0xFF, 0xFF, 0xFF, 0x0F }, 5 },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        uint8_t          buffer[8];
        SvtAv2BitWriter  bw;
        svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));

        SVT_TEST_CHECK_EQ_INT(svt_av2_leb128_size(cases[i].value), (int)cases[i].size);
        SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_leb128(&bw, cases[i].value), SVT_AV2_BW_OK);
        SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bytes_written(&bw), (int)cases[i].size);
        check_bytes(buffer, cases[i].bytes, cases[i].size);
    }
}

static void test_trailing_bits(void) {
    uint8_t       buffer[4];
    SvtAv2BitWriter bw;

    /* trailing_bits(1) writes a single 1 bit (at the MSB here). */
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_trailing_bits(&bw, 1u), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(buffer[0], 0x80);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), 1);

    /* trailing_bits(4) -> 1 0 0 0 = 0x80. */
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_trailing_bits(&bw, 4u), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(buffer[0], 0x80);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), 4);

    /* After 3 payload bits, alignment writes 1 then 4 zeros: 1111 0000. */
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bits(&bw, 0x7u, 3u), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_trailing_bits_align(&bw), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(buffer[0], 0xF0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), 8);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_byte_aligned(&bw), 1);

    /* Already byte aligned: a full 0x80 byte is written (spec 5.2.3), i.e. the
     * trailing-bit pattern still emits its leading 1 even on a byte boundary. */
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bits(&bw, 0xABu, 8u), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_trailing_bits_align(&bw), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(buffer[0], 0xAB);
    SVT_TEST_CHECK_EQ_INT(buffer[1], 0x80);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), 16);

    /* Unaligned by 1 bit: 1 then 7 zeros completes the byte (0x80). */
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bits(&bw, 0x1u, 1u), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_trailing_bits_align(&bw), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(buffer[0], 0xC0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), 8);

    /* trailing_bits(0) is invalid. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_trailing_bits(&bw, 0u), SVT_AV2_BW_ERROR_INVALID_ARG);
}

static void test_finalize(void) {
    uint8_t       buffer[4];
    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));

    /* Unaligned finalize fails. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bits(&bw, 0x1u, 1u), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_finalize(&bw), SVT_AV2_BW_ERROR_UNALIGNED);

    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bits(&bw, 0x0u, 7u), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_finalize(&bw), SVT_AV2_BW_OK);

    /* A finalized writer rejects further writes. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bit(&bw, 0u), SVT_AV2_BW_ERROR_STATE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bytes(&bw, (const uint8_t*)"\x00", 1u),
                          SVT_AV2_BW_ERROR_STATE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_finalize(&bw), SVT_AV2_BW_ERROR_STATE);
}

static void test_byte_alignment(void) {
    uint8_t         buffer[4];
    SvtAv2BitWriter bw;

    /* Already aligned: no-op. */
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_byte_alignment(&bw), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), 0);

    /* 5 bits 10101 -> pad 3 zeros -> 1010 1000 = 0xA8. */
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bits(&bw, 0x15u, 5u), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_byte_alignment(&bw), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(buffer[0], 0xA8);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), 8);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_byte_aligned(&bw), 1);

    /* byte_alignment pads with zeros, unlike trailing_bits which writes a 1. */
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bits(&bw, 0x0u, 5u), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_byte_alignment(&bw), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(buffer[0], 0x00);
}

static void test_uvlc_known_values(void) {
    static const struct {
        uint32_t value;
        uint8_t  first_byte;
        uint32_t bits;
    } cases[] = {
        { 0u, 0x80, 1 },  /* 1 */
        { 1u, 0x40, 3 },  /* 010 */
        { 2u, 0x60, 3 },  /* 011 */
        { 3u, 0x20, 5 },  /* 00100 */
        { 4u, 0x28, 5 },  /* 00101 */
        { 5u, 0x30, 5 },  /* 00110 */
        { 6u, 0x38, 5 },  /* 00111 */
        { 7u, 0x10, 7 },  /* 0001000 */
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        uint8_t         buffer[8];
        SvtAv2BitWriter bw;
        svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));

        SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_uvlc(&bw, cases[i].value), SVT_AV2_BW_OK);
        SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), (int)cases[i].bits);
        SVT_TEST_CHECK_EQ_INT(buffer[0], cases[i].first_byte);
    }

    /* Out of range. */
    uint8_t         buffer[16];
    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_uvlc(&bw, 0xFFFFFFFFu), SVT_AV2_BW_ERROR_RANGE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), 0);
}

static void test_uvlc_capacity_atomic(void) {
    uint8_t         buffer[1];
    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));

    /* uvlc(8) is 7 bits and fits. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_uvlc(&bw, 8u), SVT_AV2_BW_OK);
    /* uvlc(1) needs 3 more bits; only 1 remains. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_uvlc(&bw, 1u), SVT_AV2_BW_ERROR_NO_SPACE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), 7);
}

/* --- randomized cross-check against the reference model ------------------- */

static void test_random_cross_check(void) {
    uint32_t state = 0x12345678u;
    for (int iteration = 0; iteration < 2000; ++iteration) {
        uint8_t         actual[128];
        SvtAv2BitWriter bw;
        RefWriter       rw;
        memset(&rw, 0, sizeof(rw));

        svt_av2_bit_writer_init(&bw, actual, sizeof(actual));

        const uint32_t total_bits = (state % 512u) + 1u;
        uint32_t       written = 0;
        while (written < total_bits) {
            state = state * 1103515245u + 12345u;
            uint32_t num_bits = (state >> 16) % 33u;
            if (num_bits == 0u) {
                num_bits = 1u;
            }
            if (num_bits > total_bits - written) {
                num_bits = total_bits - written;
            }
            state = state * 1103515245u + 12345u;
            const uint32_t value = state;

            SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bits(&bw, value, num_bits), SVT_AV2_BW_OK);
            ref_write_bits(&rw, value, num_bits);
            written += num_bits;
        }

        ref_flush(&rw);
        SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bytes_written(&bw), (int)rw.length);
        check_bytes(actual, rw.data, rw.length);
    }
}

/* --- LEB128 randomized cross-check ---------------------------------------- */

static void test_random_leb128(void) {
    uint32_t state = 0xA5A5A5A5u;
    for (int iteration = 0; iteration < 5000; ++iteration) {
        state = state * 1664525u + 1013904223u;
        const uint32_t value = state;

        uint8_t          actual[8];
        SvtAv2BitWriter  bw;
        RefWriter        rw;
        memset(&rw, 0, sizeof(rw));
        svt_av2_bit_writer_init(&bw, actual, sizeof(actual));

        SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_leb128(&bw, value), SVT_AV2_BW_OK);
        ref_write_leb128(&rw, value);
        SVT_TEST_CHECK_EQ_INT(svt_av2_leb128_size(value), (int)rw.length);
        check_bytes(actual, rw.data, rw.length);
    }
}

static void test_random_uvlc(void) {
    uint32_t state = 0xC0FFEE01u;
    for (int iteration = 0; iteration < 5000; ++iteration) {
        state = state * 1664525u + 1013904223u;
        /* Keep within the conformant uvlc range [0, 2^32 - 2]. */
        const uint32_t value = state % 0xFFFFFFFEu;

        uint8_t         actual[16];
        SvtAv2BitWriter bw;
        RefWriter       rw;
        memset(&rw, 0, sizeof(rw));
        svt_av2_bit_writer_init(&bw, actual, sizeof(actual));

        SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_uvlc(&bw, value), SVT_AV2_BW_OK);
        ref_write_uvlc(&rw, value);
        ref_flush(&rw);
        SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bytes_written(&bw), (int)rw.length);
        check_bytes(actual, rw.data, rw.length);
    }
}

static void test_ns_known_values(void) {
    /* ns(n) value range is 0..n-1. n = 4: w = 3, m = 4, so v < m always and
     * every value uses w-1 = 2 bits. n = 5: m = 3, values 3..4 need 3 bits. */
    static const struct {
        uint32_t n;
        uint32_t value;
        uint8_t  first_byte;
        uint32_t bits;
    } cases[] = {
        { 4u, 0u, 0x00, 2 }, /* 00 */
        { 4u, 1u, 0x40, 2 }, /* 01 */
        { 4u, 2u, 0x80, 2 }, /* 10 */
        { 4u, 3u, 0xC0, 2 }, /* 11 */
        { 5u, 0u, 0x00, 2 }, /* m = 3, 00 */
        { 5u, 2u, 0x80, 2 }, /* 10 */
        { 5u, 3u, 0xC0, 3 }, /* 6 = 110 */
        { 5u, 4u, 0xE0, 3 }, /* 7 = 111 */
        { 3u, 0u, 0x00, 1 }, /* m = 1, 0 */
        { 3u, 1u, 0x80, 2 }, /* 2 = 10 */
        { 3u, 2u, 0xC0, 2 }, /* value+m = 3 = 11 */
        { 2u, 0u, 0x00, 1 }, /* 0 */
        { 2u, 1u, 0x80, 1 }, /* 1 */
        { 1u, 0u, 0x00, 0 }, /* n = 1: no bits */
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        uint8_t         buffer[8];
        SvtAv2BitWriter bw;
        svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));

        SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_ns(&bw, cases[i].n, cases[i].value), SVT_AV2_BW_OK);
        SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), (int)cases[i].bits);
        if (cases[i].bits > 0u) {
            SVT_TEST_CHECK_EQ_INT(buffer[0], cases[i].first_byte);
        }
    }
}

static void test_ns_rejections(void) {
    uint8_t         buffer[4];
    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));

    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_ns(&bw, 0u, 0u), SVT_AV2_BW_ERROR_INVALID_ARG);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_ns(&bw, 4u, 4u), SVT_AV2_BW_ERROR_INVALID_ARG);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_ns(NULL, 4u, 0u), SVT_AV2_BW_ERROR_NULL);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&bw), 0);
}

static void test_random_ns(void) {
    uint32_t state = 0x5EED1234u;
    for (int iteration = 0; iteration < 4000; ++iteration) {
        state = state * 1664525u + 1013904223u;
        const uint32_t n = (state % 255u) + 2u; /* 2..256 */
        state = state * 1664525u + 1013904223u;
        const uint32_t value = state % n;

        uint8_t         actual[16];
        SvtAv2BitWriter bw;
        RefWriter       rw;
        memset(&rw, 0, sizeof(rw));
        svt_av2_bit_writer_init(&bw, actual, sizeof(actual));

        SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_ns(&bw, n, value), SVT_AV2_BW_OK);
        ref_write_ns(&rw, n, value);
        ref_flush(&rw);
        SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bytes_written(&bw), (int)rw.length);
        check_bytes(actual, rw.data, rw.length);
    }
}

int main(void) {
    test_single_bits();
    test_msb_first_partial();
    test_cross_byte();
    test_full_word();
    test_zero_bits_is_noop();
    test_capacity_boundary();
    test_alignment_and_bytes();
    test_byte_alignment();
    test_leb128_known_values();
    test_uvlc_known_values();
    test_uvlc_capacity_atomic();
    test_ns_known_values();
    test_ns_rejections();
    test_trailing_bits();
    test_finalize();
    test_random_cross_check();
    test_random_leb128();
    test_random_uvlc();
    test_random_ns();

    SVT_TEST_REPORT("test_bit_writer");
}
