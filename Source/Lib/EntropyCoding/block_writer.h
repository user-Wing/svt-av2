/*
 * SVT-AV2 - M1 minimal intra block writer (T005).
 *
 * Scope: the single, smallest legal M1 coding unit. For a 64x64 CLOSED_LOOP_KEY
 * frame with one 64x64 superblock, decode_tile() evaluates partition_implied()
 * at (0,0) which is false, so exactly one do_split symbol is read. A value of 0
 * selects PARTITION_NONE, yielding a single 64x64 intra block. That block then
 * produces:
 *   - one y_mode_set symbol (0)
 *   - one y_mode_index symbol (0)     -> YMode = DC_PRED
 *   - one uv_mode symbol (0)          -> UVMode = DC_PRED
 *   - three all_zero symbols (1)      -> luma TX_64X64 and U/V TX_32X32 have no
 *                                        coefficients
 * = 7 entropy symbols and no residual values.
 *
 * This module is a thin, testable policy layer over the T005 entropy encoder:
 * it owns the symbol choice and the CDF selection (section 8.3.2 contexts), it
 * does not own the range coder.
 *
 * Spec references (AV2 Bitstream & Decoding Process Specification v1.0.0):
 *   - 6.19.3.2 read_partition() do_split and its context
 *   - 5.20.5.5 read_intra_y_mode()/read_intra_uv_mode() and their contexts
 *   - 6.19.7.23 coeffs() all_zero and its context / txSzCtx
 *   - 8.3.2 cdf_selection_process()
 */
#ifndef SVT_AV2_BLOCK_WRITER_H
#define SVT_AV2_BLOCK_WRITER_H

#include <stdint.h>

#include "entropy_encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Number of entropy symbols written for the M1 minimal 64x64 intra block. */
#define SVT_AV2_BLOCK_MIN_SYMBOLS 7u

/* Writes the 7 symbols of the M1 minimal intra block into enc. `base_q_idx`
 * selects the coefficient CDF context (spec 6.17 init_coeff_cdfs) and must be
 * the same value the frame header carried. Returns an SvtAv2EntropyStatus. */
int svt_av2_block_write_minimal_intra(SvtAv2EntropyEncoder* enc,
                                      uint32_t              base_q_idx);

#ifdef __cplusplus
}
#endif

#endif /* SVT_AV2_BLOCK_WRITER_H */
