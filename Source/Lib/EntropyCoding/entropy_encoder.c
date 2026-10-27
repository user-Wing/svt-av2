/*
 * SVT-AV2 - AV2 symbol (range) encoder for tile payloads (T005, M1).
 *
 * Scope: the arithmetic encoder that is the exact inverse of the AV2 symbol
 * decoding process (spec 8.2). It is the ONLY entropy coder in the tree; it is
 * deliberately separate from T003's raw BitWriter (see bit_writer.h: a raw
 * BitWriter is not an entropy coder).
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
 * that the fixed avmdec oracle decodes to the same symbols. The internal
 * precarry buffer holds one value per coded byte plus a carry flag (values may
 * exceed 255 until the final carry propagation pass).
 *
 * Output convention: the tile payload starts with the raw bits that
 * init_symbol() reads (Min(sz*8,15) bits); the range coder's bytes follow. The
 * caller passes a buffer and this module prepends the two raw-bit bytes by
 * shifting the coded bytes right by two, leaving the leading bytes zero
 * (the encoder starts at low = 0 and never makes them nonzero).
 */
#include "entropy_encoder.h"
#include "rate_cost_table.h"

/* Spec / AVM constants. CDF_SHIFT is 0 for the 15-bit CDFs used here. */
#define EC_PROB_SHIFT 7
#define EC_CDF_PROB_TOP 32768u

/* Prob_Inc[nsym-2][n] (attachment prob_inc.h / AVM av2_prob_inc_tbl). The -1
 * "unused" entries are read for the last symbol (n == nsym-1) and MUST be kept:
 * od_ec_prob_scale() evaluates them with signed arithmetic, so the negative
 * offset wraps to the correct unsigned range boundary. */
static const int16_t k_prob_inc[15][16] = {
    { 8,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {10,  5,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {12,  8,  4,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {12,  9,  6,  3,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {13, 10,  8,  5,  2,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {13, 11,  9,  6,  4,  2,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {14, 12, 10,  8,  6,  4,  2,  0, -1, -1, -1, -1, -1, -1, -1, -1},
    {14, 12, 10,  8,  7,  5,  3,  1,  0, -1, -1, -1, -1, -1, -1, -1},
    {14, 12, 11,  9,  8,  6,  4,  3,  1,  0, -1, -1, -1, -1, -1, -1},
    {14, 13, 11, 10,  8,  7,  5,  4,  2,  1,  0, -1, -1, -1, -1, -1},
    {14, 13, 12, 10,  9,  8,  6,  5,  4,  2,  1,  0, -1, -1, -1, -1},
    {14, 13, 12, 11,  9,  8,  7,  6,  4,  3,  2,  1,  0, -1, -1, -1},
    {14, 13, 12, 11, 10,  9,  8,  6,  5,  4,  3,  2,  1,  0, -1, -1},
    {14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0, -1},
    {15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0}
};

/* Number of bits needed to represent rng (rng != 0): floor(log2(rng)) + 1. */
static int ent_ilog_nz16(uint32_t rng) {
    int n = 0;
    uint32_t probe = rng >> 1;
    while (probe != 0u) {
        probe >>= 1;
        ++n;
    }
    return n + 1;
}

/* od_ec_prob_scale(f, r, n, nsym): same equation as spec 8.2.6 pp/cur terms
 * with the CDF complemented (f = 32768 - cdf). CDF_SHIFT is 0, so the AVM
 * shift count is 7 - EC_PROB_SHIFT - CDF_SHIFT + 1 + 6 = 7. The arithmetic is
 * intentionally signed, matching AVM: the -1 padding entry for the last symbol
 * wraps to the correct unsigned range boundary. */
static uint32_t ent_prob_scale(uint32_t f, uint32_t r, uint32_t n, uint32_t nsym) {
    int rr = (int)(r >> 8);
    int pp = (int)(f >> EC_PROB_SHIFT);
    pp <<= 4;
    pp += k_prob_inc[nsym - 2u][n];
    return (uint32_t)((rr * pp >> 7) << 3);
}

/* Renormalizes and flushes completed bytes into the precarry buffer. Mirrors
 * od_ec_enc_normalize() (avm/avm_dsp/entenc.c).
 *
 * `n_bypass` is the number of raw (bypass) bits the caller just consumed from
 * the range: 0 for the symbol path, so `d` is the renormalization shift derived
 * from rng; > 0 for a literal/bypass call, where the range is a power of two and
 * no shift is needed. This is the ONLY difference between the two paths, and it
 * matches AVM entenc.c:63-71 exactly. */
static void ent_normalize(SvtAv2EntropyEncoder* enc, uint64_t low, uint32_t rng,
                          int n_bypass) {
    int d;
    int c;
    int s;

    if (n_bypass > 0) {
        c = enc->cnt + n_bypass;
        d = 0;
    } else {
        c = enc->cnt;
        d = 16 - ent_ilog_nz16(rng);
    }
    s = c + d;

    if (s >= 0) {
        uint32_t m;
        if (enc->offs + 2u > enc->precarry_size) {
            enc->error = 1;
            return;
        }
        c += 16;
        m = (1u << c) - 1u;
        if (s >= 8) {
            enc->precarry[enc->offs++] = (uint16_t)(low >> c);
            low &= m;
            c -= 8;
            m >>= 8;
        }
        enc->precarry[enc->offs++] = (uint16_t)(low >> c);
        s = c + d - 24;
        low &= m;
    }
    enc->low = low << d;
    enc->rng = rng << d;
    enc->cnt = s;
}

void svt_av2_entropy_encoder_init(SvtAv2EntropyEncoder* enc, uint8_t* buf,
                                  size_t size, uint16_t* precarry,
                                  size_t precarry_size) {
    if (enc == NULL) {
        return;
    }
    enc->buf = buf;
    enc->storage = size;
    enc->precarry = precarry;
    enc->precarry_size = precarry_size;
    enc->offs = 0;
    enc->low = 0;
    enc->rng = 0x8000u;
    enc->cnt = -9;
    enc->error = 0;
    enc->rate_only = 0u;
    enc->models = NULL;
    enc->rate_q12 = 0u;
    enc->rate_symbols = enc->rate_literal_bits = 0u;
}

int svt_av2_entropy_probability_cost_q12(uint32_t probability, uint32_t* cost) {
    if (cost == NULL) return SVT_AV2_ENTROPY_ERROR_NULL;
    if (probability == 0u || probability > 32768u)
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    *cost = k_probability_cost_q12[probability];
    return 0;
}

int svt_av2_entropy_adjusted_probability(const uint16_t* cdf, uint32_t nsyms,
    uint32_t symbol, uint32_t* probability) {
    if (cdf == NULL || probability == NULL) return SVT_AV2_ENTROPY_ERROR_NULL;
    if (nsyms < 2u || nsyms > 8u || symbol >= nsyms)
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    /* For N=2..8, the position offsets strictly decrease to zero.
     * With a nondecreasing raw CDF, every adjusted interval is therefore
     * positive except when the first raw boundary is zero. Validate the
     * complete raw CDF, then compute only the selected interval's endpoints. */
    uint32_t last = 0u;
    for (uint32_t i = 0u; i + 1u < nsyms; ++i) {
        if (cdf[i] > 32768u || cdf[i] < last)
            return SVT_AV2_ENTROPY_ERROR_RANGE;
        last = cdf[i];
    }
    if (cdf[0] == 0u) return SVT_AV2_ENTROPY_ERROR_RANGE;
    const uint32_t previous = symbol == 0u ? 32768u :
        (((32768u - cdf[symbol - 1u]) >> EC_PROB_SHIFT) << EC_PROB_SHIFT) +
            8u * (uint32_t)k_prob_inc[nsyms - 2u][symbol - 1u];
    const uint32_t current = symbol + 1u == nsyms ? 0u :
        (((32768u - cdf[symbol]) >> EC_PROB_SHIFT) << EC_PROB_SHIFT) +
            8u * (uint32_t)k_prob_inc[nsyms - 2u][symbol];
    if (current >= previous) return SVT_AV2_ENTROPY_ERROR_RANGE;
    *probability = previous - current;
    return 0;
}

void svt_av2_entropy_rate_init(SvtAv2EntropyEncoder* enc) {
    svt_av2_entropy_encoder_init(enc, NULL, 0u, NULL, 0u);
    if (enc != NULL) enc->rate_only = 1u;
}

int svt_av2_entropy_encode_symbol(SvtAv2EntropyEncoder* enc, const uint16_t* cdf,
                                  uint32_t nsyms, uint32_t s) {
    if (enc == NULL || cdf == NULL) {
        return SVT_AV2_ENTROPY_ERROR_NULL;
    }
    if (nsyms < 2u || nsyms > 16u || s >= nsyms) {
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    }
    if (enc->error) {
        return SVT_AV2_ENTROPY_ERROR_NO_SPACE;
    }

    if (enc->rate_only) {
        const uint32_t lower = s == 0u ? 0u : cdf[s - 1u];
        const uint32_t upper = s + 1u == nsyms ? 32768u : cdf[s];
        uint32_t cost;
        if (lower >= upper || upper > 32768u ||
            svt_av2_entropy_probability_cost_q12(upper - lower, &cost) != 0)
            return SVT_AV2_ENTROPY_ERROR_RANGE;
        enc->rate_q12 += cost;
        ++enc->rate_symbols;
        return 0;
    }

    /* fl/fh in inverse-CDF terms (spec 8.2.6: f = 32768 - cdf[symbol], except
     * the last symbol whose upper boundary is the top of the range). The CDF
     * arrays passed in carry one boundary per symbol and omit the redundant
     * trailing CDF_PROB_TOP, so cdf[s] is only valid for s < nsyms-1. */
    uint32_t fl = (s > 0u) ? (EC_CDF_PROB_TOP - cdf[s - 1u]) : EC_CDF_PROB_TOP;
    uint32_t fh = (s + 1u < nsyms) ? (EC_CDF_PROB_TOP - cdf[s]) : 0u;

    uint64_t l = enc->low;
    uint32_t r = enc->rng;

    if (fl < EC_CDF_PROB_TOP) {
        uint32_t u = ent_prob_scale(fl, r, s - 1u, nsyms);
        uint32_t v = ent_prob_scale(fh, r, s, nsyms);
        l += (uint32_t)(r - u);
        r = u - v;
    } else {
        uint32_t v = ent_prob_scale(fh, r, s, nsyms);
        r -= v;
    }

    ent_normalize(enc, l, r, 0);
    if (enc->error) {
        return SVT_AV2_ENTROPY_ERROR_NO_SPACE;
    }
    return SVT_AV2_ENTROPY_OK;
}

int svt_av2_entropy_encode_literal(SvtAv2EntropyEncoder* enc, uint32_t value,
                                   int n_bits) {
    if (enc == NULL) {
        return SVT_AV2_ENTROPY_ERROR_NULL;
    }
    if (n_bits < 0 || n_bits > 16) {
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    }
    if (n_bits > 0 && value >= (1u << n_bits)) {
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    }
    if (enc->error) {
        return SVT_AV2_ENTROPY_ERROR_NO_SPACE;
    }
    if (n_bits == 0) {
        return SVT_AV2_ENTROPY_OK;
    }
    if (enc->rate_only) {
        enc->rate_q12 += (uint64_t)(uint32_t)n_bits * 4096u;
        enc->rate_literal_bits += (uint32_t)n_bits;
        return 0;
    }

    /* od_ec_encode_literal_bypass() (avm/avm_dsp/entenc.c:223-239): the range is
     * a power of two, so the value is packed MSB-first directly into low with no
     * probability scaling. Note low's low n_bits are live (unlike the symbol
     * path), which is exactly why ent_normalize() must be told n_bypass. */
    uint64_t l = enc->low << n_bits;
    l += (uint64_t)enc->rng * value;

    ent_normalize(enc, l, enc->rng, n_bits);
    if (enc->error) {
        return SVT_AV2_ENTROPY_ERROR_NO_SPACE;
    }
    return SVT_AV2_ENTROPY_OK;
}

int svt_av2_entropy_encode_bit(SvtAv2EntropyEncoder* enc, uint32_t bit) {
    if (bit > 1u) {
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    }
    return svt_av2_entropy_encode_literal(enc, bit, 1);
}

int svt_av2_entropy_encoder_done(SvtAv2EntropyEncoder* enc, uint8_t** out_bytes,
                                 size_t* out_size) {
    if (enc == NULL || out_bytes == NULL || out_size == NULL) {
        return SVT_AV2_ENTROPY_ERROR_NULL;
    }
    if (enc->rate_only) return SVT_AV2_ENTROPY_ERROR_RANGE;
    if (enc->error) {
        return SVT_AV2_ENTROPY_ERROR_NO_SPACE;
    }

    uint64_t l = enc->low;
    int c = enc->cnt;
    int s = 10 + c;
    uint64_t e = ((l + 0x3FFFu) & ~(uint64_t)0x3FFFu) | 0x4000u;

    if (s > 0) {
        uint32_t n = (uint32_t)(((uint64_t)1 << (c + 16)) - 1u);
        do {
            if (enc->offs + 2u > enc->precarry_size) {
                enc->error = 1;
                return SVT_AV2_ENTROPY_ERROR_NO_SPACE;
            }
            enc->precarry[enc->offs++] = (uint16_t)(e >> (c + 16));
            e &= n;
            s -= 8;
            c -= 8;
            n >>= 8;
        } while (s > 0);
    }

    /* The precarry values are the stream bytes, except that each may carry into
     * its predecessor. Carry propagates from the last coded byte towards the
     * first, but the bytes stay in order: precarry[0] is stream byte 0. */
    if (enc->offs > enc->storage) {
        enc->error = 1;
        return SVT_AV2_ENTROPY_ERROR_NO_SPACE;
    }
    unsigned carry = 0;
    for (size_t i = enc->offs; i > 0u; --i) {
        carry += enc->precarry[i - 1u];
        enc->buf[i - 1u] = (uint8_t)carry;
        carry >>= 8;
    }

    *out_bytes = enc->buf;
    *out_size = enc->offs;
    return SVT_AV2_ENTROPY_OK;
}
