/*
 * SVT-AV2 - AV2 symbol (range) encoder for tile payloads (T005, M1).
 *
 * Scope: the arithmetic encoder that is the exact inverse of the AV2 symbol
 * decoding process (spec 8.2). It is the ONLY entropy coder in the tree; it is
 * deliberately separate from T003's raw BitWriter (see bit_writer.h: "普通
 * BitWriter ≠ 熵编码器").
 *
 * Spec references (AV2 Bitstream & Decoding Process Specification v1.0.0):
 *   - 8.2.2 init_symbol(sz)
 *   - 8.2.6 symbol_decoding_process (read_symbol)
 *   - 8.2.4 exit_process_for_symbol_decoder (exit_symbol)
 *   - 8.3.2 cdf_selection_process
 *   - Constants: EC_PROB_SHIFT = 7; Prob_Inc[][] (attachment prob_inc.h).
 *
 * The encoder state machine mirrors the reference implementation in the
 * read-only AVM oracle (avm/avm_dsp/entenc.c), whose decoder counterpart
 * (avm/avm_dsp/entdec.c avm_od_ec_decode_cdf_q15_c) is provably identical to
 * spec 8.2.6 / 8.2.3. Coding with these equations therefore produces a stream
 * that the fixed avmdec oracle decodes to the same symbols.
 *
 * Output convention: the encoder emits the arithmetic-coded payload bytes with
 * byte 0 first. The spec's init_symbol() raw bits are the first Min(sz*8,15)
 * bits of this payload; they are not a separate field, matching the AVM
 * reference (avm_dsp/bitwriter.c avm_stop_encode copies the
 * avm_od_ec_enc_done() bytes to offset 0).
 */
#ifndef SVT_AV2_ENTROPY_ENCODER_H
#define SVT_AV2_ENTROPY_ENCODER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Status codes for the entropy encoder. */
typedef enum SvtAv2EntropyStatus {
    SVT_AV2_ENTROPY_OK = 0,
    SVT_AV2_ENTROPY_ERROR_NULL = -1,
    SVT_AV2_ENTROPY_ERROR_RANGE = -2,
    SVT_AV2_ENTROPY_ERROR_NO_SPACE = -3
} SvtAv2EntropyStatus;

/* A CDF array passed to the encoder carries exactly N uint16_t entries, one per
 * symbol (N == nsyms): cdf[k] is the cumulative frequency of symbols 0..k scaled
 * to 32768, so cdf[0] is the boundary between symbol 0 and symbol 1. The
 * redundant trailing CDF_PROB_TOP (32768) that the spec's own tables append is
 * omitted; the encoder derives the last symbol's upper boundary itself. This
 * mirrors cdf_tables.h. Adaptation is not modelled because the frozen M1 frame
 * header sets disable_cdf_update = 1. */

typedef struct SvtAv2EntropyEncoder {
    uint8_t*  buf;            /* caller-owned output buffer */
    size_t    storage;        /* output capacity in bytes */
    uint16_t* precarry;       /* caller-owned carry-flag byte buffer */
    size_t    precarry_size;  /* capacity of precarry, in entries */
    size_t    offs;           /* coded bytes emitted into precarry */
    uint64_t  low;            /* low end of the current range */
    uint32_t  rng;            /* number of values in the current range */
    int32_t   cnt;            /* bits of data in the current value */
    int       error;          /* nonzero once an error occurred */
    uint32_t  rate_only;      /* M6: shared syntax, without arithmetic output */
    uint64_t  rate_q12;       /* sum of estimated -log2(CDF probability), 4096/bit */
    uint64_t  rate_symbols, rate_literal_bits;
    struct SvtAv2EntropyModels* models; /* Adaptive formal/rate bank; NULL is frozen off. */
} SvtAv2EntropyEncoder;

/* Deterministic integer cost; zero probability is an error. This estimates
 * entropy from the forward CDF, not finalized range-coder byte length. */
int svt_av2_entropy_probability_cost_q12(uint32_t probability, uint32_t* cost);
/* AVM position-adjusted p15 for reachable adaptive models (N=2..8).
 * Accepts N-1 forward boundaries, including equal raw boundaries. */
int svt_av2_entropy_adjusted_probability(const uint16_t* cdf, uint32_t nsyms,
    uint32_t symbol, uint32_t* probability);
void svt_av2_entropy_rate_init(SvtAv2EntropyEncoder* enc);
/* Initializes the encoder over caller-owned buffers. `buf`/`size` receive the
 * final payload bytes; `precarry`/`precarry_size` hold one uint16_t per coded
 * byte plus carry slack (pass at least size entries). */
void svt_av2_entropy_encoder_init(SvtAv2EntropyEncoder* enc, uint8_t* buf,
                                  size_t size, uint16_t* precarry,
                                  size_t precarry_size);

/* Encodes symbol `s` (0 <= s < nsyms) using the forward CDF `cdf` (nsyms
 * entries, monotone increasing, cdf[nsyms-1] < 32768). Returns an
 * SvtAv2EntropyStatus. */
int svt_av2_entropy_encode_symbol(SvtAv2EntropyEncoder* enc, const uint16_t* cdf,
                                  uint32_t nsyms, uint32_t s);

/* Encodes `n_bits` raw (bypass) bits of `value`, most significant first, with no
 * probability model: every bit costs exactly one bit of range. This is the
 * L(n) / literal syntax element the coefficient layer interleaves with its
 * symbols (eob_pt_*_extra, eob_extra_bit, sign, read_quant()).
 *
 * `n_bits` is 0..16 (AVM chunks wider literals into <=8-bit calls, see
 * avm/avm_dsp/bitreader.h avm_read_literal_); `value` must fit in `n_bits`.
 * Returns an SvtAv2EntropyStatus. The arithmetic is the exact inverse of
 * od_ec_decode_literal_bypass() (avm/avm_dsp/entdec.c) and matches
 * od_ec_encode_literal_bypass() (avm/avm_dsp/entenc.c:223). */
int svt_av2_entropy_encode_literal(SvtAv2EntropyEncoder* enc, uint32_t value,
                                   int n_bits);

/* Encodes a single raw bit. Equivalent to svt_av2_entropy_encode_literal(enc,
 * bit, 1) and to AVM's od_ec_encode_bool_bypass(). */
int svt_av2_entropy_encode_bit(SvtAv2EntropyEncoder* enc, uint32_t bit);

/* Finalizes the stream. On success *out_bytes points at the first payload byte
 * and *out_size is its length; the range coder's two leading raw-bit bytes are
 * inserted here. Returns an SvtAv2EntropyStatus. `enc->buf` must have had at
 * least 2 bytes of slack beyond the coded bytes. */
int svt_av2_entropy_encoder_done(SvtAv2EntropyEncoder* enc, uint8_t** out_bytes,
                                 size_t* out_size);

#ifdef __cplusplus
}
#endif

#endif /* SVT_AV2_ENTROPY_ENCODER_H */
