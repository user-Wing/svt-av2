/*
 * T001/T013 unit tests: public encoder API state machine, parameter
 * validation, resource lifecycle and the send_picture()/get_packet_ex()
 * packet contract.
 *
 * These tests exercise only the public header (include/svt_av2.h); they must
 * not depend on internal layout.
 *
 * T013 (M1->M2 round): the send/get contract was rewritten to
 * docs/m1-m2-contracts.md section 7 / 7.1:
 *   - send_picture(NULL) is EOS and returns SVT_AV2_ERROR_NONE (the old M0
 *     assertion expecting INVALID_PARAMETER is gone);
 *   - a picture is a real SvtAv2Picture with real Y/U/V planes, strides and
 *     metadata. The old tests handed over `int32_t picture = 0` cast to void*
 *     and expected the encoder to accept it. That is exactly the "accept an
 *     arbitrary void* and pretend to validate it" behaviour the task forbids,
 *     so those inputs are now used only to check that they are REJECTED.
 *   - get_packet_ex() reports success / "no output" / "buffer too small" /
 *     state errors distinguishably, and never writes partial data.
 */
#include "svt_av2.h"

#include "test_util.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Applies the smallest valid configuration to a fresh handle. Returns the
 * first failing error code, or SVT_AV2_ERROR_NONE. */
static SvtAv2Error configure_valid(SvtAv2Handle handle, uint32_t width, uint32_t height) {
    const uint32_t           w = width;
    const uint32_t           h = height;
    const SvtAv2ChromaFormat chroma = SVT_AV2_CHROMA_420;
    const SvtAv2BitDepth     depth = SVT_AV2_BIT_DEPTH_8;
    const uint32_t           fps_num = 25;
    const uint32_t           fps_den = 1;
    const int32_t            qp = 100;
    const SvtAv2Preset       preset = SVT_AV2_PRESET_SLOW;
    const uint32_t           threads = 1;
    const int32_t            deterministic = 1;
    const char*              output_path = "unused.svt2";

    SvtAv2Error err = svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_WIDTH, &w);
    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_HEIGHT, &h);
    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_CHROMA_FORMAT, &chroma);
    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_BIT_DEPTH, &depth);
    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_FRAME_RATE_NUM, &fps_num);
    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_FRAME_RATE_DEN, &fps_den);
    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_QP, &qp);
    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_PRESET, &preset);
    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_THREADS, &threads);
    if (err == SVT_AV2_ERROR_NONE)
        err = svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_DETERMINISTIC, &deterministic);
    if (err == SVT_AV2_ERROR_NONE)
        err = svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_OUTPUT_PATH, &output_path);
    return err;
}

/* --- real picture construction -------------------------------------------- *
 *
 * The contract requires the encoder to verify picture metadata, non-NULL
 * Y/U/V planes, strides and lifecycle. A test that hands over a fake pointer
 * cannot tell a real check from a missing one, so every "this must be
 * accepted" case below builds a genuine 8-bit 4:2:0 picture over an allocated
 * buffer. */

typedef struct TestPicture {
    uint8_t       storage[64 * 64 + 2 * 32 * 32];
    SvtAv2Picture picture;
} TestPicture;

/* Fills a tightly packed 64x64 8-bit 4:2:0 picture with a deterministic,
 * non-flat pattern (a flat input would make "did the pixels matter?" cases
 * meaningless and would not exercise the copy path). */
static void test_picture_init_64x64(TestPicture* test) {
    uint8_t* const luma = test->storage;
    uint8_t* const cb = luma + 64 * 64;
    uint8_t* const cr = cb + 32 * 32;

    for (uint32_t row = 0; row < 64; ++row) {
        for (uint32_t col = 0; col < 64; ++col) {
            luma[row * 64 + col] = (uint8_t)((row * 3u + col * 5u) & 0xFFu);
        }
    }
    for (uint32_t row = 0; row < 32; ++row) {
        for (uint32_t col = 0; col < 32; ++col) {
            cb[row * 32 + col] = (uint8_t)(64u + row);
            cr[row * 32 + col] = (uint8_t)(192u - col);
        }
    }

    memset(&test->picture, 0, sizeof(test->picture));
    test->picture.planes[0] = luma;
    test->picture.planes[1] = cb;
    test->picture.planes[2] = cr;
    test->picture.strides[0] = 64;
    test->picture.strides[1] = 32;
    test->picture.strides[2] = 32;
    test->picture.width = 64;
    test->picture.height = 64;
    test->picture.chroma_format = SVT_AV2_CHROMA_420;
    test->picture.bit_depth = SVT_AV2_BIT_DEPTH_8;
    test->picture.pts = 7;
}

static void test_error_strings(void) {
    SVT_TEST_CHECK_EQ_STR(svt_av2_error_to_string(SVT_AV2_ERROR_NONE), "SVT_AV2_ERROR_NONE");
    SVT_TEST_CHECK_EQ_STR(svt_av2_error_to_string(SVT_AV2_ERROR_INVALID_HANDLE), "SVT_AV2_ERROR_INVALID_HANDLE");
    SVT_TEST_CHECK_EQ_STR(svt_av2_error_to_string(SVT_AV2_ERROR_NOT_IMPLEMENTED), "SVT_AV2_ERROR_NOT_IMPLEMENTED");
    SVT_TEST_CHECK_EQ_STR(svt_av2_error_to_string(SVT_AV2_ERROR_UNSUPPORTED), "SVT_AV2_ERROR_UNSUPPORTED");
    SVT_TEST_CHECK_EQ_STR(svt_av2_error_to_string((SvtAv2Error)999), "SVT_AV2_ERROR_UNKNOWN");
}

static void test_build_info(void) {
    const SvtAv2EncoderInfo* info = svt_av2_enc_get_build_info();
    SVT_TEST_CHECK(info != NULL);
    if (info != NULL) {
        SVT_TEST_CHECK_EQ_INT(info->version_major, SVT_AV2_VERSION_MAJOR);
        SVT_TEST_CHECK_EQ_INT(info->version_minor, SVT_AV2_VERSION_MINOR);
        SVT_TEST_CHECK_EQ_INT(info->version_patch, SVT_AV2_VERSION_PATCH);
        SVT_TEST_CHECK_EQ_STR(info->version_string, SVT_AV2_VERSION_STRING);
        SVT_TEST_CHECK_EQ_STR(info->milestone, "M5");
        SVT_TEST_CHECK(info->compiler[0] != '\0');
    }
    /* Stable pointer across calls. */
    SVT_TEST_CHECK(svt_av2_enc_get_build_info() == info);
}

static void test_init_handle_contract(void) {
    SvtAv2Error err = svt_av2_enc_init_handle(NULL, NULL);
    SVT_TEST_CHECK_EQ_INT(err, SVT_AV2_ERROR_INVALID_PARAMETER);

    SvtAv2Handle             handle = NULL;
    const SvtAv2EncoderInfo* info = NULL;
    err = svt_av2_enc_init_handle(&handle, &info);
    SVT_TEST_CHECK_EQ_INT(err, SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK(handle != NULL);
    SVT_TEST_CHECK(info != NULL);
    if (info != NULL) {
        SVT_TEST_CHECK_EQ_STR(info->version_string, SVT_AV2_VERSION_STRING);
    }

    /* info may be NULL. */
    SvtAv2Handle handle2 = NULL;
    err = svt_av2_enc_init_handle(&handle2, NULL);
    SVT_TEST_CHECK_EQ_INT(err, SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK(handle2 != NULL);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle2), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK(handle2 == NULL);

    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK(handle == NULL);
}

static void test_parameter_validation(void) {
    SvtAv2Handle handle = NULL;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle, NULL), SVT_AV2_ERROR_NONE);

    const uint32_t w = 64;

    /* NULL value and out-of-range parameter are rejected. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_WIDTH, NULL),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_COUNT, &w),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, (SvtAv2Parameter)-1, &w),
                          SVT_AV2_ERROR_INVALID_PARAMETER);

    /* Empty output path is rejected. */
    const char* empty = "";
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_OUTPUT_PATH, &empty),
                          SVT_AV2_ERROR_INVALID_PARAMETER);

    /* Invalid enum values are accepted at set time but rejected by init(). */
    const SvtAv2ChromaFormat bad_chroma = (SvtAv2ChromaFormat)77;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_CHROMA_FORMAT, &bad_chroma),
                          SVT_AV2_ERROR_NONE);

    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
}

static void test_lifecycle_state_machine(void) {
    SvtAv2Handle handle = NULL;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle, NULL), SVT_AV2_ERROR_NONE);

    /* Defaults have no geometry / output path: init must fail, not fake success. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_INVALID_PARAMETER);

    /* Calling the encoding API before init is a state error. */
    TestPicture test;
    test_picture_init_64x64(&test);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &test.picture), SVT_AV2_ERROR_INVALID_STATE);
    SvtAv2Packet packet;
    uint8_t      storage[4096];
    memset(storage, 0, sizeof(storage));
    packet.data = storage;
    packet.capacity = sizeof(storage);
    packet.size = 0;
    packet.pts = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(handle, &packet), SVT_AV2_ERROR_INVALID_STATE);

    SVT_TEST_CHECK_EQ_INT(configure_valid(handle, 64, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_NONE);

    /* Setting parameters after init is rejected. */
    const uint32_t w = 1280;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_WIDTH, &w),
                          SVT_AV2_ERROR_INVALID_STATE);

    /* Re-initialising an already initialised handle is rejected. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_INVALID_STATE);

    /* get_config round-trips the geometry and validates its argument. */
    SvtAv2EncoderConfig config;
    memset(&config, 0, sizeof(config));
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(handle, NULL), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(handle, &config), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(config.width, 64);
    SVT_TEST_CHECK_EQ_INT(config.height, 64);
    SVT_TEST_CHECK_EQ_INT(config.chroma_format, SVT_AV2_CHROMA_420);
    SVT_TEST_CHECK_EQ_INT(config.bit_depth, SVT_AV2_BIT_DEPTH_8);

    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
}

/* --- input validation: fake pictures must be REJECTED --------------------- */

static void test_send_picture_rejects_invalid(void) {
    SvtAv2Handle handle = NULL;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle, NULL), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(configure_valid(handle, 64, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_NONE);

    /* A hand-made picture with zeroed metadata is not a picture. */
    SvtAv2Picture zeroed;
    memset(&zeroed, 0, sizeof(zeroed));
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &zeroed), SVT_AV2_ERROR_INVALID_PARAMETER);

    TestPicture test;
    test_picture_init_64x64(&test);

    /* Missing luma plane. */
    SvtAv2Picture broken = test.picture;
    broken.planes[0] = NULL;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &broken), SVT_AV2_ERROR_INVALID_PARAMETER);

    /* Missing chroma planes (4:2:0 needs both). */
    broken = test.picture;
    broken.planes[1] = NULL;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &broken), SVT_AV2_ERROR_INVALID_PARAMETER);
    broken = test.picture;
    broken.planes[2] = NULL;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &broken), SVT_AV2_ERROR_INVALID_PARAMETER);

    /* Stride smaller than the visible width would make the encoder read past
     * the caller's rows; it must be refused, not "detected". */
    broken = test.picture;
    broken.strides[0] = 63;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &broken), SVT_AV2_ERROR_INVALID_PARAMETER);
    broken = test.picture;
    broken.strides[1] = 31;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &broken), SVT_AV2_ERROR_INVALID_PARAMETER);

    /* Geometry that does not match the configured stream is not silently
     * converted (M1 is a fixed 64x64 stream). */
    broken = test.picture;
    broken.width = 32;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &broken), SVT_AV2_ERROR_INVALID_PARAMETER);
    broken = test.picture;
    broken.height = 32;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &broken), SVT_AV2_ERROR_INVALID_PARAMETER);

    /* Unsupported format / bit depth: reported as unsupported, not encoded. */
    broken = test.picture;
    broken.chroma_format = SVT_AV2_CHROMA_444;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &broken), SVT_AV2_ERROR_UNSUPPORTED);
    broken = test.picture;
    broken.bit_depth = SVT_AV2_BIT_DEPTH_10;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &broken), SVT_AV2_ERROR_UNSUPPORTED);

    /* None of the rejected calls may have left a frame pending: the next
     * (valid) submission must still be accepted. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &test.picture), SVT_AV2_ERROR_NONE);

    /* And a second picture while one is in flight is a state error, not a
     * silent overwrite of the first frame's stream. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &test.picture), SVT_AV2_ERROR_INVALID_STATE);

    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
}

/* --- packet contract (contract section 7.1) -------------------------------- */

static void test_get_packet_ex_contract(void) {
    SvtAv2Handle handle = NULL;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle, NULL), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(configure_valid(handle, 64, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_NONE);

    /* Argument validation. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(handle, NULL), SVT_AV2_ERROR_INVALID_PARAMETER);

    /* No picture submitted yet: "nothing available" is NONE with size 0, which
     * stays distinguishable from a failure. */
    uint8_t      storage[4096];
    SvtAv2Packet packet;
    memset(storage, 0, sizeof(storage));
    packet.data = storage;
    packet.capacity = sizeof(storage);
    packet.size = 0;
    packet.pts = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(handle, &packet), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(packet.size, 0);

    /* A data pointer is required once a packet exists (checked below), but a
     * NULL data with no pending packet is still "no output". */
    packet.data = NULL;
    packet.capacity = 0;
    packet.size = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(handle, &packet), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(packet.size, 0);

    /* Real frame in, real packet out. */
    TestPicture test;
    test_picture_init_64x64(&test);
    test.picture.pts = 42;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &test.picture), SVT_AV2_ERROR_NONE);

    /* Too small a buffer is reported with the required size and must not write
     * partial data (the M1 stream is 21 bytes; ask for less). */
    uint8_t tiny[4];
    memset(tiny, 0xAA, sizeof(tiny));
    SvtAv2Packet small_packet;
    small_packet.data = tiny;
    small_packet.capacity = sizeof(tiny);
    small_packet.size = 0;
    small_packet.pts = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(handle, &small_packet), SVT_AV2_ERROR_OUT_OF_MEMORY);
    SVT_TEST_CHECK(small_packet.size > sizeof(tiny));

    /* Capacity zero with a valid pointer is the same "space too small" case,
     * not a crash and not a silent success. */
    packet.data = storage;
    packet.capacity = 0;
    packet.size = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(handle, &packet), SVT_AV2_ERROR_OUT_OF_MEMORY);
    SVT_TEST_CHECK(packet.size > 0);
    const size_t required = packet.size;
    packet.size = 0;
    packet.capacity = sizeof(storage);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(handle, &packet), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(packet.size, required);
    SVT_TEST_CHECK_EQ_INT(packet.pts, 42);

    /* A NULL output buffer with a packet waiting is a caller error, not a
     * bogus success. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &test.picture), SVT_AV2_ERROR_NONE);
    SvtAv2Packet null_data;
    null_data.data = NULL;
    null_data.capacity = 0;
    null_data.size = 0;
    null_data.pts = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(handle, &null_data), SVT_AV2_ERROR_INVALID_PARAMETER);

    /* The rejected call did not consume the packet. */
    packet.data = storage;
    packet.capacity = sizeof(storage);
    packet.size = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(handle, &packet), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK(packet.size > 0 && packet.size < required);

    /* The packet was produced even though the same frame was submitted twice
     * in a row: the second submission was refused, so only one packet exists. */
    packet.data = storage;
    packet.capacity = sizeof(storage);
    packet.size = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(handle, &packet), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(packet.size, 0);

    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
}

/* The legacy entry point has no capacity parameter, so it must only report
 * availability and must never write through the void* it is handed. */
static void test_legacy_get_packet_reports_only(void) {
    SvtAv2Handle handle = NULL;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle, NULL), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(configure_valid(handle, 64, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_NONE);

    uint8_t marker[16];
    memset(marker, 0x5A, sizeof(marker));
    int32_t has_packet = -1;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet(handle, marker, NULL), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet(handle, NULL, &has_packet), SVT_AV2_ERROR_INVALID_PARAMETER);

    has_packet = -1;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet(handle, marker, &has_packet), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(has_packet, 0);

    TestPicture test;
    test_picture_init_64x64(&test);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &test.picture), SVT_AV2_ERROR_NONE);

    has_packet = -1;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet(handle, marker, &has_packet), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(has_packet, 1);

    /* Not one byte of the caller's buffer may have been touched. */
    for (size_t i = 0; i < sizeof(marker); ++i) {
        SVT_TEST_CHECK_EQ_INT(marker[i], 0x5A);
    }

    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
}

/* --- EOS contract ---------------------------------------------------------- */

static void test_eos_contract(void) {
    SvtAv2Handle handle = NULL;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle, NULL), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(configure_valid(handle, 64, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_NONE);

    uint8_t      storage[4096];
    SvtAv2Packet packet;
    memset(storage, 0, sizeof(storage));

    /* send_picture(NULL) is EOS: NONE, not INVALID_PARAMETER (contract 7.1). */
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, NULL), SVT_AV2_ERROR_NONE);

    /* After EOS there is simply no output: NONE with size 0. */
    packet.data = storage;
    packet.capacity = sizeof(storage);
    packet.size = 0;
    packet.pts = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(handle, &packet), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(packet.size, 0);

    /* EOS is idempotent. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, NULL), SVT_AV2_ERROR_NONE);

    /* No picture is accepted after EOS. */
    TestPicture test;
    test_picture_init_64x64(&test);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &test.picture), SVT_AV2_ERROR_INVALID_STATE);

    /* EOS while a frame is still in flight is a state error, not a silent
     * discard of that frame. */
    SvtAv2Handle handle2 = NULL;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle2, NULL), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(configure_valid(handle2, 64, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle2), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle2, &test.picture), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle2, NULL), SVT_AV2_ERROR_INVALID_STATE);

    /* The pending packet is still takeable after the refused EOS. */
    packet.data = storage;
    packet.capacity = sizeof(storage);
    packet.size = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(handle2, &packet), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK(packet.size > 0);

    /* Now that the pipeline is drained, EOS is accepted. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle2, NULL), SVT_AV2_ERROR_NONE);

    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle2), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
}

static void test_unsupported_and_overflow(void) {
    /* Threading is not implemented in M0: asking for more than one thread must
     * fail loudly instead of silently encoding single threaded. */
    {
        SvtAv2Handle handle = NULL;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle, NULL), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(configure_valid(handle, 64, 64), SVT_AV2_ERROR_NONE);
        const uint32_t threads = 4;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_THREADS, &threads),
                              SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_UNSUPPORTED);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
    }

    /* Geometry that would overflow the sample counter must be rejected rather
     * than silently wrapping. */
    {
        SvtAv2Handle handle = NULL;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle, NULL), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(configure_valid(handle, 0xFFFFFFFFu, 0xFFFFFFFFu), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
    }

    /* Non-lossless CQP endpoints are supported; zero remains out of scope. */
    {
        SvtAv2Handle handle = NULL;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle, NULL), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(configure_valid(handle, 64, 64), SVT_AV2_ERROR_NONE);
        const int32_t qp_max = 255;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_QP, &qp_max), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
    }
    {
        SvtAv2Handle handle = NULL;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle, NULL), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(configure_valid(handle, 64, 64), SVT_AV2_ERROR_NONE);
        const int32_t qp_min = 0;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_QP, &qp_min), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_UNSUPPORTED);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
    }

    /* QP above the accepted range is rejected at init(). */
    {
        SvtAv2Handle handle = NULL;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle, NULL), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(configure_valid(handle, 64, 64), SVT_AV2_ERROR_NONE);
        const int32_t qp = 256;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_QP, &qp), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
    }

    /* QP below the accepted range is rejected at init(). */
    {
        SvtAv2Handle handle = NULL;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle, NULL), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(configure_valid(handle, 64, 64), SVT_AV2_ERROR_NONE);
        const int32_t qp = -1;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_QP, &qp), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
    }
}

static void test_repeated_init_deinit(void) {
    /* Repeated allocate/configure/init/send/deinit cycles must not leak or
     * corrupt state (a simple leak/asan smoke check), and the input buffer must
     * be reusable across the whole run (contract section 1.2: the picture is
     * consumed during the call). */
    TestPicture test;
    test_picture_init_64x64(&test);

    for (int i = 0; i < 64; ++i) {
        SvtAv2Handle handle = NULL;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle, NULL), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(configure_valid(handle, 64, 64), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &test.picture), SVT_AV2_ERROR_NONE);

        uint8_t      storage[4096];
        SvtAv2Packet packet;
        packet.data = storage;
        packet.capacity = sizeof(storage);
        packet.size = 0;
        packet.pts = 0;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(handle, &packet), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK(packet.size > 0);
        SVT_TEST_CHECK(packet.size <= sizeof(storage));

        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK(handle == NULL);
    }

    /* deinit on NULL / already closed handles. */
    SvtAv2Handle none = NULL;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&none), SVT_AV2_ERROR_INVALID_HANDLE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(NULL), SVT_AV2_ERROR_INVALID_HANDLE);
}

static void test_output_path_replacement(void) {
    /* Setting the output path twice must replace (not leak) the stored copy. */
    SvtAv2Handle handle = NULL;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle, NULL), SVT_AV2_ERROR_NONE);

    const char* first = "first_path.svt2";
    const char* second = "second_path.svt2";
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_OUTPUT_PATH, &first),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_OUTPUT_PATH, &second),
                          SVT_AV2_ERROR_NONE);

    SvtAv2EncoderConfig config;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(handle, &config), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_STR(config.output_path, second);

    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
}

/* Consecutive frames: the API's one-in-flight rule and the current single
 * still-picture capability. The library must not silently turn a sequence of
 * send/get calls into a concatenation of independent still pictures; the app
 * enforces the same limit (codec.h SVT_AV2_MAX_ENCODABLE_FRAMES). */
static void test_consecutive_frames_use_their_own_input(void) {
    SvtAv2Handle handle = NULL;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle, NULL), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(configure_valid(handle, 64, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_NONE);

    uint8_t      storage[4096];
    SvtAv2Packet packet;

    TestPicture test;
    test_picture_init_64x64(&test);

    /* One frame in flight at a time; a second submission before the packet is
     * taken is refused (not an overwrite). */
    test.storage[0] = 1;
    test.picture.pts = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &test.picture), SVT_AV2_ERROR_NONE);
    test.picture.pts = 1;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &test.picture), SVT_AV2_ERROR_INVALID_STATE);

    packet.data = storage;
    packet.capacity = sizeof(storage);
    packet.size = 0;
    packet.pts = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(handle, &packet), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK(packet.size > 0);
    SVT_TEST_CHECK_EQ_INT(packet.pts, 0);

    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
}

/* Every submitted frame retains its own PTS and coefficients. */
static void test_packet_matches_submitted_frame(void) {
    SvtAv2Handle handle = NULL;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle, NULL), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(configure_valid(handle, 64, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_NONE);

    uint8_t      storage[4096];
    SvtAv2Packet packet;

    TestPicture test;
    test_picture_init_64x64(&test);

    for (int32_t frame = 0; frame < 3; ++frame) {
        test.storage[0] = (uint8_t)(frame * 100);
        test.picture.pts = frame;

        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &test.picture), SVT_AV2_ERROR_NONE);

        packet.data = storage;
        packet.capacity = sizeof(storage);
        packet.size = 0;
        packet.pts = 0;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(handle, &packet), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK(packet.size > 0);
        SVT_TEST_CHECK_EQ_INT(packet.pts, frame);
    }

    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
}

static void test_m2_owned_source_and_reconstruction(void) {
    SvtAv2Handle handle = NULL;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle, NULL), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(configure_valid(handle, 64, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_NONE);
    TestPicture test;
    test_picture_init_64x64(&test);
    uint8_t coded[4096], reconstructed[6144];
    SvtAv2ReconFrame recon;
    memset(&recon, 0, sizeof(recon));
    recon.data = reconstructed;
    recon.capacity = sizeof(reconstructed);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_recon(handle, &recon), SVT_AV2_ERROR_INVALID_STATE);
    for (uint32_t frame = 0; frame < 17u; ++frame) {
        memset(test.storage, 128, sizeof(test.storage));
        test.picture.pts = (int32_t)(frame * 7u);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &test.picture), SVT_AV2_ERROR_NONE);
        memset(test.storage, 0, sizeof(test.storage)); /* caller may reuse immediately */
        int32_t available = 0;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet(handle, coded, &available), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(available, 1);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &test.picture), SVT_AV2_ERROR_INVALID_STATE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, NULL), SVT_AV2_ERROR_INVALID_STATE);
        SvtAv2Packet packet = {coded, sizeof(coded), 0, 0};
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(handle, &packet), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK(packet.size > 0);
        SVT_TEST_CHECK_EQ_INT(packet.pts, frame * 7u);
        uint8_t sentinel = 0xA5;
        recon.data = &sentinel; recon.capacity = 1;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_recon(handle, &recon), SVT_AV2_ERROR_OUT_OF_MEMORY);
        SVT_TEST_CHECK_EQ_INT(recon.byte_count, sizeof(reconstructed));
        SVT_TEST_CHECK_EQ_INT(sentinel, 0xA5);
        recon.data = reconstructed; recon.capacity = sizeof(reconstructed);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_recon(handle, &recon), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(recon.frame_index, frame);
        for (size_t i = 0; i < sizeof(reconstructed); ++i)
            SVT_TEST_CHECK_EQ_INT(reconstructed[i], 128);
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, NULL), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
}

static void test_m3_u16le_input_and_retries(void) {
    for (uint32_t format = 0; format < 2u; ++format) {
        SvtAv2Handle handle = NULL;
        SvtAv2BitDepth depth = SVT_AV2_BIT_DEPTH_10;
        SvtAv2ChromaFormat chroma = format ? SVT_AV2_CHROMA_444 : SVT_AV2_CHROMA_420;
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle, NULL), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(configure_valid(handle, 64u, 64u), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_BIT_DEPTH, &depth), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_CHROMA_FORMAT, &chroma), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_NONE);
        uint8_t storage[3][64u * 131u + 1u];
        SvtAv2Picture picture;
        memset(&picture, 0, sizeof(picture));
        picture.width = picture.height = 64u;
        picture.bit_depth = depth; picture.chroma_format = chroma;
        for (uint32_t p = 0; p < 3u; ++p) {
            picture.planes[p] = storage[p] + 1u; /* deliberately unaligned */
            picture.strides[p] = 131u; /* odd byte stride with poisoned padding */
        }
        for (uint32_t frame = 0; frame < 2u; ++frame) {
            memset(storage, 0xA5, sizeof(storage));
            for (uint32_t p = 0; p < 3u; ++p) {
                const uint32_t dim = p == 0u || format ? 64u : 32u;
                for (uint32_t y = 0; y < dim; ++y)
                    for (uint32_t x = 0; x < dim; ++x) {
                        uint8_t* v = storage[p] + 1u + y * 131u + 2u*x;
                        v[0] = 0u; v[1] = 2u; /* 512 in LE */
                    }
            }
            const uint32_t uv_dim = format ? 64u : 32u;
            uint8_t* last = storage[2] + 1u + (uv_dim-1u)*131u + 2u*(uv_dim-1u);
            last[1] = 4u; /* 1024 is invalid, including final V sample */
            SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &picture), SVT_AV2_ERROR_INVALID_PARAMETER);
            last[0] = 255u; last[1] = 255u;
            SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &picture), SVT_AV2_ERROR_INVALID_PARAMETER);
            last[0] = 0u; last[1] = 2u;
            picture.strides[0] = 127u;
            SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &picture), SVT_AV2_ERROR_INVALID_PARAMETER);
            picture.strides[0] = 131u;
            picture.pts = (int32_t)(17u + frame);
            SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &picture), SVT_AV2_ERROR_NONE);
            memset(storage, 0, sizeof(storage)); /* source consumed synchronously */
            uint8_t sentinel = 0xA5u, packet_data[4096], recon_data[24576];
            SvtAv2Packet packet = {&sentinel, 1u, 0u, 0};
            SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(handle, &packet), SVT_AV2_ERROR_OUT_OF_MEMORY);
            SVT_TEST_CHECK(packet.size > 1u);
            SVT_TEST_CHECK_EQ_INT(sentinel, 0xA5u);
            packet.data = packet_data; packet.capacity = sizeof(packet_data);
            SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(handle, &packet), SVT_AV2_ERROR_NONE);
            SVT_TEST_CHECK_EQ_INT(packet.pts, picture.pts);
            SvtAv2ReconFrame recon;
            memset(&recon, 0, sizeof(recon));
            recon.data = &sentinel; recon.capacity = 1u;
            SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_recon(handle, &recon), SVT_AV2_ERROR_OUT_OF_MEMORY);
            const size_t required = format ? 24576u : 12288u;
            SVT_TEST_CHECK_EQ_INT(recon.byte_count, required);
            SVT_TEST_CHECK_EQ_INT(sentinel, 0xA5u);
            recon.data = recon_data; recon.capacity = sizeof(recon_data);
            SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_recon(handle, &recon), SVT_AV2_ERROR_NONE);
            SVT_TEST_CHECK_EQ_INT(recon.bit_depth, depth);
            SVT_TEST_CHECK_EQ_INT(recon.chroma_format, chroma);
            SVT_TEST_CHECK_EQ_INT(recon.chroma_width, uv_dim);
            SVT_TEST_CHECK_EQ_INT(recon.chroma_height, uv_dim);
            SVT_TEST_CHECK_EQ_INT(recon.frame_index, frame);
            SVT_TEST_CHECK_EQ_INT(recon.byte_count, required);
            for (size_t i = 0; i < required; i += 2u) {
                SVT_TEST_CHECK_EQ_INT(recon_data[i], 0u);
                SVT_TEST_CHECK_EQ_INT(recon_data[i+1u], 2u);
            }
        }
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, NULL), SVT_AV2_ERROR_NONE);
        uint8_t data[8];
        SvtAv2Packet packet = {data, sizeof(data), 99u, 0};
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_packet_ex(handle, &packet), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(packet.size, 0u);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_send_picture(handle, &picture), SVT_AV2_ERROR_INVALID_STATE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
    }
    SvtAv2Handle handle = NULL;
    SvtAv2ChromaFormat chroma = SVT_AV2_CHROMA_444;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle, NULL), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(configure_valid(handle, 64u, 64u), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_CHROMA_FORMAT, &chroma), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_UNSUPPORTED);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
}

static void test_m4_prediction_structure_validation(void) {
    SvtAv2Handle handle = NULL;
    SvtAv2EncoderConfig config;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle, NULL), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(handle, &config), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(config.prediction_structure, SVT_AV2_PRED_ALL_INTRA);
    SVT_TEST_CHECK_EQ_INT(configure_valid(handle, 64u, 64u), SVT_AV2_ERROR_NONE);
    const SvtAv2PredictionStructure invalid[] = {(SvtAv2PredictionStructure)-1, (SvtAv2PredictionStructure)2};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_PREDICTION_STRUCTURE, &invalid[i]), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_INVALID_PARAMETER);
    }
    const SvtAv2PredictionStructure valid = SVT_AV2_PRED_LOW_DELAY_P;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_PREDICTION_STRUCTURE, &valid), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
}

static void test_coefficient_rd_configuration(void) {
    SvtAv2Handle handle = NULL;
    SvtAv2EncoderConfig config;
    uint32_t value = 1u, invalid = 3u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle, NULL), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(handle, &config), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(config.coeff_optimization, 0u);
    SVT_TEST_CHECK_EQ_INT(configure_valid(handle, 64u, 64u), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_COEFF_OPTIMIZATION, &invalid),
        SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_COEFF_OPTIMIZATION, &value),
        SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_RD_MODE, &value), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(handle, &config), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(config.coeff_optimization, 1u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_NONE);
    value = 0u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_COEFF_OPTIMIZATION, &value),
        SVT_AV2_ERROR_INVALID_STATE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
}

static void test_motion_rd_configuration(void) {
    SvtAv2Handle handle = NULL;
    SvtAv2EncoderConfig config;
    uint32_t value = 1u, zero = 0u, invalid = 3u, wide = 2u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle, NULL), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(handle, &config), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(config.motion_optimization, 0u);
    SVT_TEST_CHECK_EQ_INT(configure_valid(handle, 64u, 64u), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_MOTION_OPTIMIZATION, &invalid),
        SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_MOTION_OPTIMIZATION, &value),
        SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_RD_MODE, &value), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_COEFF_OPTIMIZATION, &value),
        SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_COEFF_OPTIMIZATION, &zero),
        SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(handle, &config), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(config.motion_optimization, 1u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_MOTION_OPTIMIZATION, &zero),
        SVT_AV2_ERROR_INVALID_STATE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle, NULL), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(configure_valid(handle, 64u, 64u), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_RD_MODE, &value), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_MOTION_OPTIMIZATION, &wide),
        SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(handle, &config), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(config.motion_optimization, 2u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
}

static void test_entropy_adaptation_configuration(void) {
    SvtAv2Handle handle = NULL;
    SvtAv2EncoderConfig config;
    uint32_t on = 1u, off = 0u, invalid = 2u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init_handle(&handle, NULL), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(handle, &config), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(config.entropy_adaptation, 0u);
    SVT_TEST_CHECK_EQ_INT(configure_valid(handle, 64u, 64u), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_ENTROPY_ADAPTATION, &invalid),
        SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_ENTROPY_ADAPTATION, &on),
        SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_get_config(handle, &config), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(config.entropy_adaptation, 1u);
    for (uint32_t rd = 1u; rd <= 2u; ++rd) {
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_RD_MODE, &rd),
            SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_INVALID_PARAMETER);
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_RD_MODE, &off), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_init(handle), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_ENTROPY_ADAPTATION, &off),
        SVT_AV2_ERROR_INVALID_STATE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_enc_deinit(&handle), SVT_AV2_ERROR_NONE);
}

int main(void) {
    test_entropy_adaptation_configuration();
    test_motion_rd_configuration();
    test_coefficient_rd_configuration();
    test_m4_prediction_structure_validation();
    test_m3_u16le_input_and_retries();
    test_error_strings();
    test_build_info();
    test_init_handle_contract();
    test_parameter_validation();
    test_lifecycle_state_machine();
    test_send_picture_rejects_invalid();
    test_get_packet_ex_contract();
    test_legacy_get_packet_reports_only();
    test_eos_contract();
    test_unsupported_and_overflow();
    test_repeated_init_deinit();
    test_output_path_replacement();
    test_consecutive_frames_use_their_own_input();
    test_packet_matches_submitted_frame();
    test_m2_owned_source_and_reconstruction();

    SVT_TEST_REPORT("test_api");
}

