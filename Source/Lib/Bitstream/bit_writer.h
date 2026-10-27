/*
 * SVT-AV2 - BitWriter and LEB128 primitives (T003).
 *
 * Scope: the raw bit/byte writing layer shared by every AV2 syntax writer
 * (sequence header, frame header, tile payload). It is deliberately NOT an
 * entropy coder: arithmetic/range coding for tile data is a separate module
 * owned by T005 and must not be faked with raw bits.
 *
 * Spec references (AV2 Bitstream & Decoding Process Specification v1.0.0):
 *   - 4.11.2 f(n)    : unsigned n-bit number, bits written highest first.
 *   - 4.11.6 leb128(): byte-aligned, <= 2^32-1, at most 8 bytes, and the MSB
 *                      of the 8th byte must be 0.
 *   - 5.2.1 General OBU syntax / 5.2.3 trailing_bits(nbBits).
 *
 * Contract (frozen by T003; see docs/m0-m1-contracts.md section 5):
 *   - Bit order: MSB-first within each byte.
 *   - Capacity: caller owns a fixed buffer; the writer never allocates or
 *     grows. Writes that would exceed the buffer fail with NO_SPACE and leave
 *     the writer unchanged (no partial bit is committed).
 *   - Errors: functions return 0 (SVT_AV2_BW_OK) or a negative status.
 *   - finalize(): requires the writer to be byte aligned; afterwards further
 *     writes are rejected with STATE. Use it to fix the output length.
 *   - OBU payload handoff: build the payload with this writer, finish it with
 *     svt_av2_obu_payload_finalize(), finalize, then wrap the resulting bytes
 *     with svt_av2_obu_write() (leb128 size prefix + header + payload).
 */
#ifndef SVT_AV2_BIT_WRITER_H
#define SVT_AV2_BIT_WRITER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- status codes --------------------------------------------------------- */

typedef enum SvtAv2BitWriterStatus {
    SVT_AV2_BW_OK = 0,
    SVT_AV2_BW_ERROR_NULL = -1,         /* NULL writer/buffer/output pointer */
    SVT_AV2_BW_ERROR_INVALID_ARG = -2,  /* bit width / type out of range */
    SVT_AV2_BW_ERROR_NO_SPACE = -3,     /* fixed output buffer exhausted */
    SVT_AV2_BW_ERROR_UNALIGNED = -4,    /* byte-aligned op at a bit offset */
    SVT_AV2_BW_ERROR_RANGE = -5,        /* value out of the allowed range */
    SVT_AV2_BW_ERROR_STATE = -6,        /* already finalized */
    SVT_AV2_BW_ERROR_UNSUPPORTED = -7   /* syntax element outside the M1 subset */
} SvtAv2BitWriterStatus;

/* leb128() conformance limits (spec 4.11.6). */
#define SVT_AV2_BIT_WRITER_MAX_LEB128_BYTES 8u
#define SVT_AV2_BIT_WRITER_MAX_LEB128_VALUE 0xFFFFFFFFu

/* uvlc() conformance limit (spec 4.11.3): the largest returned value is
 * (1 << 32) - 2, because leadingZeros must be < 32 when the function returns. */
#define SVT_AV2_BIT_WRITER_MAX_UVLC_VALUE 0xFFFFFFFEu

/* --- writer --------------------------------------------------------------- */

typedef struct SvtAv2BitWriter {
    uint8_t* buffer;    /* caller owned; never reallocated by the writer */
    size_t   capacity;  /* size of buffer in bytes */
    size_t   bit_count; /* number of bits successfully written */
    int      finalized; /* set by svt_av2_bit_writer_finalize() */
} SvtAv2BitWriter;

/* Binds the writer to a caller buffer. buffer may be NULL only when capacity
 * is 0 (a pure counting writer). The buffer is zeroed on init so that unused
 * low bits of the last byte are deterministic. */
void svt_av2_bit_writer_init(SvtAv2BitWriter* bw, uint8_t* buffer, size_t capacity);

/* Writes a single bit (0 or 1). Returns INVALID_ARG for any other value. */
int svt_av2_bit_writer_write_bit(SvtAv2BitWriter* bw, uint32_t bit);

/* Writes the low num_bits of value, most significant bit first (f(n)).
 * num_bits may be 0 (no-op) and at most 32. */
int svt_av2_bit_writer_write_bits(SvtAv2BitWriter* bw, uint32_t value, uint32_t num_bits);

/* 1 when the next bit starts a new byte. */
int svt_av2_bit_writer_byte_aligned(const SvtAv2BitWriter* bw);

/* uvlc() variable-length unsigned number (spec 4.11.3): value = (1 << n) - 1 + n
 * extra bits, where n is the number of leading zeros. Valid for
 * value <= 2^32 - 2 (SVT_AV2_BIT_WRITER_MAX_UVLC_VALUE). */
int svt_av2_bit_writer_write_uvlc(SvtAv2BitWriter* bw, uint32_t value);

/* ns(n): non-symmetric code for 0..n-1, n >= 1 (spec 4.11.8). */
int svt_av2_bit_writer_write_ns(SvtAv2BitWriter* bw, uint32_t n, uint32_t value);

/* byte_alignment() (spec 5.2.4): pads with zero bits to the next byte boundary.
 * No-op when already aligned. This is NOT trailing_bits(): it writes only 0s and
 * adds nothing when aligned, whereas trailing_bits() writes a 1 bit. */
int svt_av2_bit_writer_byte_alignment(SvtAv2BitWriter* bw);

/* Byte-aligned helpers; fail with UNALIGNED when bit_count % 8 != 0. */
int svt_av2_bit_writer_write_byte(SvtAv2BitWriter* bw, uint8_t byte);
int svt_av2_bit_writer_write_bytes(SvtAv2BitWriter* bw, const uint8_t* data, size_t size);

/* leb128() of a value in [0, 2^32-1]; byte aligned; canonical (shortest form).
 * Fails with UNALIGNED when the writer is not byte aligned. */
int svt_av2_bit_writer_write_leb128(SvtAv2BitWriter* bw, uint32_t value);

/* Number of bytes leb128() uses for value (1..5 for the valid range). */
size_t svt_av2_leb128_size(uint32_t value);

/* trailing_bits(nb_bits): one 1 bit then (nb_bits - 1) zero bits (spec 5.2.3).
 * nb_bits must be >= 1. */
int svt_av2_bit_writer_write_trailing_bits(SvtAv2BitWriter* bw, uint32_t nb_bits);

/* Pads from the current position to the next byte boundary using the trailing
 * pattern: writes a single 1 bit then zero bits. When already byte aligned it
 * writes a full 0x80 byte, which the spec mandates for a payload that happens
 * to end exactly on a byte boundary. */
int svt_av2_bit_writer_write_trailing_bits_align(SvtAv2BitWriter* bw);

/* Requires byte alignment, then marks the writer finalized. */
int svt_av2_bit_writer_finalize(SvtAv2BitWriter* bw);

/* Bits/bytes written so far. Bytes are rounded up (a partial last byte counts). */
size_t svt_av2_bit_writer_bits_written(const SvtAv2BitWriter* bw);
size_t svt_av2_bit_writer_bytes_written(const SvtAv2BitWriter* bw);

#ifdef __cplusplus
}
#endif

#endif /* SVT_AV2_BIT_WRITER_H */
