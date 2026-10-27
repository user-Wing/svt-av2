/*
 * T005 unit tests: M1 keyframe entry point (svt_av2_encode_keyframe_64x64).
 *
 * Pins the complete 21-byte Annex B stream that the read-only AVM oracle
 * (avmdec.exe) decodes to exactly one 64x64 frame, and checks the failure modes
 * (NULL output, unsupported quantizer, too-small buffer) fail closed instead of
 * emitting a truncated stream.
 */
#include "keyframe.h"

#include "test_util.h"

#include <stdint.h>
#include <string.h>

static const uint8_t k_expect[21] = {
    0x01, 0x08, 0x0C, 0x04, 0x82, 0x0A, 0x55, 0xFF, 0xF0, 0x00, 0x04,
    0x00, 0x1B, 0x80, 0x08, 0x05, 0x10, 0xE6, 0xC8, 0x00, 0x12
};

static void test_exact_stream(void) {
    uint8_t out[512];
    size_t  n = 0;
    int st = svt_av2_encode_keyframe_64x64(NULL, 0, NULL, 0, NULL, 0, 100, out,
                                           sizeof(out), &n);
    SVT_TEST_CHECK(st == 0);
    SVT_TEST_CHECK(n == sizeof(k_expect));
    SVT_TEST_CHECK(n == sizeof(k_expect) &&
                   memcmp(out, k_expect, sizeof(k_expect)) == 0);
}

static void test_null_output(void) {
    size_t n = 0;
    SVT_TEST_CHECK(svt_av2_encode_keyframe_64x64(NULL, 0, NULL, 0, NULL, 0, 100,
                                                 NULL, 64, &n) != 0);
    uint8_t out[64];
    SVT_TEST_CHECK(svt_av2_encode_keyframe_64x64(NULL, 0, NULL, 0, NULL, 0, 100,
                                                 out, sizeof(out), NULL) != 0);
}

static void test_unsupported_quantizer(void) {
    /* base_q_idx = 0 (lossless) and 40 (COEFF_CDF_Q_CTXS index 0) must fail
     * rather than code with a mismatched CDF. */
    uint8_t out[512];
    size_t  n = 0;
    SVT_TEST_CHECK(svt_av2_encode_keyframe_64x64(NULL, 0, NULL, 0, NULL, 0, 0,
                                                 out, sizeof(out), &n) != 0);
    SVT_TEST_CHECK(svt_av2_encode_keyframe_64x64(NULL, 0, NULL, 0, NULL, 0, 40,
                                                 out, sizeof(out), &n) != 0);
}

static void test_small_buffer_fails_closed(void) {
    uint8_t out[8]; /* smaller than the 21-byte stream */
    size_t  n = 123; /* poison */
    SVT_TEST_CHECK(svt_av2_encode_keyframe_64x64(NULL, 0, NULL, 0, NULL, 0, 100,
                                                 out, sizeof(out), &n) != 0);
}

int main(void) {
    test_exact_stream();
    test_null_output();
    test_unsupported_quantizer();
    test_small_buffer_fails_closed();

    SVT_TEST_REPORT("test_keyframe");
}
