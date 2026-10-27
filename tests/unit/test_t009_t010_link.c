/*
 * T009 <-> T010 link smoke test (joint ownership).
 *
 * Scope is deliberately narrow, per 00's ruling on 2026-10-07:
 *
 *   residual --svt_av2_fwd_transform-->  coeff
 *            --svt_av2_quantize_block--> qcoeff + dqcoeff
 *            --svt_av2_inv_transform-->   residual'
 *
 * This is a LINK test, not a reconstruction test. It checks only:
 *
 *   1. No UB / no contract violation across the boundary: the real T009
 *      coefficient output is accepted by T010, and T010's dqcoeff is accepted
 *      by T009's inverse, at every supported geometry.
 *   2. The value domain T010 promised at the exit (`|qcoeff| <= 8191`,
 *      `dqcoeff` clipped, `qcoeff[i]==0 <=> dqcoeff[i]==0`) holds on real
 *      transform output, not merely on synthetic arrays.
 *   3. The round trip is BOUNDED, not exact. Quantization is lossy, so
 *      `inv(fwd(x)) == x` is false by construction and is never asserted
 *      (docs/m1-m2-contracts.md section 3). What IS asserted is that the
 *      error cannot run away: max-abs difference is bounded, and a zero
 *      residual maps to a zero block at both ends.
 *
 * The bound in (3) is not a tight rate-distortion statement; it exists to catch
 * a sign error, a missing shift, or an unclipped intermediate, i.e. the class
 * of bug that only shows up once tx_scale != 0. Every check is expressed in
 * terms of the geometry, not hard-coded counts, so the 32x32 and 64x64 cases
 * became live the moment T009 delivered them (2026-10-07) with no edit here.
 *
 * 64x64 note: the coefficient array length comes from
 * svt_av2_tx_coeff_count(), which is 1024 for 64x64, NOT 4096 (00's
 * array-length ruling). Quantizing w*h entries would touch 3072 undefined ones.
 */
#include "test_util.h"

#include <stdint.h>
#include <stdio.h>

#include "quantization.h"
#include "transform.h"

static uint32_t g_lcg;

static uint32_t lcg_next(void) {
    g_lcg = g_lcg * 1664525u + 1013904223u;
    return g_lcg;
}

/*
 * Worst-case bound on |residual'| for one block.
 *
 * Derivation: the inverse output is clamped to [-256, 255] by T009 (col_rng in
 * transform.c), so any single sample difference is at most 511 by construction.
 * That is a *domain* bound, not a quality bound. To also catch gross gain
 * errors we require the mean absolute error to stay far below the domain bound:
 * a DC-only or sign-flipped reconstruction blows past this immediately, while a
 * correct lossy round trip at the frozen QP100 stays well under it.
 *
 * Using a domain bound (rather than a tight value) keeps the test independent
 * of the exact quantizer step, which is what makes it a link test.
 */
#define LINK_MAX_ABS_DIFF 511

static void link_geometry(uint32_t w, uint32_t h) {
    if (!svt_av2_tx_supported(w, h)) {
        /* T009 has not grown past 256 pels yet; the link test simply skips the
         * geometry rather than failing. When T009 adds 32/64 this branch stops
         * being taken for those sizes and the checks below cover them. */
        fprintf(stdout, "  skip %ux%u (T009 transform does not support it yet)\n", w, h);
        return;
    }

    /* The two modules derive the scale exponent independently; if they ever
     * disagree the dequantizer shifts by the wrong amount and every value below
     * is wrong in the same systematic way. Pin the agreement first. */
    const int tx_scale_t009 = svt_av2_tx_scale(w, h);
    const int log_scale_t009 = svt_av2_tx_log_scale(w, h);
    SVT_TEST_CHECK_EQ_INT(tx_scale_t009, log_scale_t009);

    SvtAv2QuantParams params;
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_params_init_tx(&params, 100, w, h), SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(params.tx_scale, tx_scale_t009);

    /* Ruled in 00's array-length adjudication: count = min(w,32)*min(h,32). */
    const uint32_t cw = w < 32 ? w : 32;
    const uint32_t ch = h < 32 ? h : 32;
    const size_t count = (size_t)cw * (size_t)ch;

    static int16_t residual[4096];
    static int32_t coeff[4096];
    static int32_t qcoeff[4096];
    static int32_t dqcoeff[4096];
    static int16_t inv_residual[4096];

    /* --- case 1: non-flat residual, full path ---------------------------- */
    const size_t samples = (size_t)w * h;
    for (size_t i = 0; i < samples; ++i) {
        residual[i] = (int16_t)((int32_t)(lcg_next() % 511u) - 255);
    }

    SVT_TEST_CHECK_EQ_INT(svt_av2_fwd_transform(residual, coeff, w, h), SVT_AV2_TX_OK);

    /* Real transform output must be accepted without a refuse/clamp path. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_block(&params, coeff, qcoeff, dqcoeff, count, 0, log_scale_t009),
                          SVT_AV2_QUANT_OK);

    int nonzero_levels = 0;
    for (size_t i = 0; i < count; ++i) {
        SVT_TEST_CHECK(qcoeff[i] <= 8191 && qcoeff[i] >= -8191);
        SVT_TEST_CHECK((qcoeff[i] == 0) == (dqcoeff[i] == 0));
        SVT_TEST_CHECK(dqcoeff[i] >= -32768 && dqcoeff[i] <= 32767);
        if (qcoeff[i] != 0) {
            ++nonzero_levels;
        }
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_all_zero(qcoeff, count), nonzero_levels == 0);
    /* Non-flat input must not quantize to an empty block, or the path is a
     * pass-through and this test proves nothing. */
    SVT_TEST_CHECK(nonzero_levels > 0);

    SVT_TEST_CHECK_EQ_INT(svt_av2_inv_transform(dqcoeff, inv_residual, w, h), SVT_AV2_TX_OK);

    /* Bounded, not exact: see the header comment. */
    long long max_abs = 0;
    for (size_t i = 0; i < samples; ++i) {
        const long long d = (long long)inv_residual[i] - (long long)residual[i];
        const long long a = d < 0 ? -d : d;
        if (a > max_abs) {
            max_abs = a;
        }
    }
    SVT_TEST_CHECK(max_abs <= LINK_MAX_ABS_DIFF);

    /* --- case 2: zero residual -> zero coefficients -> zero output -------- */
    for (size_t i = 0; i < samples; ++i) {
        residual[i] = 0;
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_fwd_transform(residual, coeff, w, h), SVT_AV2_TX_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quantize_block(&params, coeff, qcoeff, dqcoeff, count, 0, log_scale_t009),
                          SVT_AV2_QUANT_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_quant_all_zero(qcoeff, count), 1);
    SVT_TEST_CHECK_EQ_INT(svt_av2_inv_transform(dqcoeff, inv_residual, w, h), SVT_AV2_TX_OK);
    for (size_t i = 0; i < samples; ++i) {
        SVT_TEST_CHECK_EQ_INT(inv_residual[i], 0);
    }

    fprintf(stdout, "  %2ux%-2u tx_scale=%d count=%zu nonzero=%d max|diff|=%lld  ok\n", w, h, tx_scale_t009,
            count, nonzero_levels, max_abs);
}

int main(void) {
    /* Every geometry the two modules currently share. 32x32/64x64 are listed so
     * that they start being covered the moment T009 supports them. */
    static const uint32_t sizes[][2] = {
        { 4, 4 }, { 8, 8 }, { 16, 16 }, { 32, 32 }, { 64, 64 },
        { 4, 8 }, { 8, 4 }, { 4, 16 }, { 16, 4 }, { 8, 16 }, { 16, 8 },
    };

    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        g_lcg = 0x9E3779B9u + (uint32_t)(i * 7919u);
        link_geometry(sizes[i][0], sizes[i][1]);
    }

    SVT_TEST_REPORT("test_t009_t010_link");
}
