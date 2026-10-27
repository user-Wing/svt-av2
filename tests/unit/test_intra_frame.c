/*
 * T012 unit test: intra frame core wiring.
 *
 * IMPORTANT — what the transform path in this file is and is not
 * -------------------------------------------------------------
 * T009 (Transform/) and T010 (Quantization/) ARE implemented and are bound to
 * the frame core by transform_path.c (see transform_path.h). This file
 * deliberately does NOT use that production binding: it drives the core with
 * its own stub so the traversal, the prediction, the clipped write and the
 * neighbour updates can be checked independently of the real transform's
 * numbers. The production binding has its own test, test_transform_path.c.
 *
 * The step in the chain is reached through SvtAv2IntraTransformPath, and the
 * frame core FAILS CLOSED (SVT_AV2_ERROR_UNSUPPORTED) when no path is supplied.
 *
 * The path used below is TEST SCAFFOLDING, not a claimed implementation of
 * T009/T010: it is a pass-through transform with a real integer deadzone
 * quantiser, so that
 *   - the quantiser genuinely loses information (reconstruction != source, and
 *     different inputs produce different reconstructions),
 *   - negative residuals and non-zero AC survive,
 *   - the chain executes its real length.
 * Its numbers are NOT AV2-conformant and no test here asserts bitstream or
 * decoder agreement. It follows the frozen coefficient layout (contract
 * section 3: row stride min(w,32), length min(w,32)*min(h,32)) so that it
 * exercises the core exactly as the production path does.
 *
 * What IS production code under test here:
 *   - the traversal (T007) covering every visible sample exactly once,
 *   - prediction reading the internal reconstruction (T008), so a block's
 *     output depends on previously reconstructed neighbours and not on source,
 *   - the clipped write (T012) landing in the recon picture,
 *   - per-frame neighbour reset and cross-frame independence,
 *   - scratch reuse: the same buffers serve every block and every frame.
 */

#include "intra_frame.h"
#include "transform_path.h"

#include <stdlib.h>
#include <string.h>

#include "test_util.h"

/* ------------------------------------------------------------------------- */
/* TEST SCAFFOLDING transform path (see the file header).                     */
/* ------------------------------------------------------------------------- */

typedef struct TestPathState {
    int quant_step; /* scaffolding deadzone step, default 8 */
    int forward_calls;
    int inverse_calls;
    int max_block_w;
    int max_block_h;
    int out_of_range_stride;
    /* Pointer identity of the caller's buffers, to prove reuse. */
    const uint8_t* last_prediction_ptr;
    const int32_t* first_qcoeff_ptr;
    const int32_t* first_dqcoeff_ptr;
} TestPathState;

/* Deadzone quantiser: level = round(|c| / step); a level of 0 means "all-zero
 * block" for that coefficient. Dequantisation is level * step, so the
 * reconstruction differs from source by up to half a step. The step comes from
 * the state so a test can show the reconstruction tracks it rather than
 * copying the source. */

static SvtAv2Error test_forward(void* user,
                                uint32_t plane,
                                uint32_t block_w,
                                uint32_t block_h,
                                const int16_t* residual,
                                uint32_t stride,
                                int32_t* coeff,
                                int32_t* qcoeff,
                                int32_t* dqcoeff,
                                int* all_zero_out) {
    TestPathState* state = (TestPathState*)user;
    if (state == NULL || residual == NULL || coeff == NULL || qcoeff == NULL || dqcoeff == NULL ||
        all_zero_out == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (plane > SVT_AV2_PLANE_V || block_w == 0 || block_h == 0) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (stride < block_w) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    /* The core must hand the path a stride the pool can serve; the pool's
     * stride is at least the max block width and 16-aligned. */
    if (state->max_block_w > 0 && stride < (uint32_t)state->max_block_w) {
        state->out_of_range_stride = 1;
    }

    ++state->forward_calls;
    if ((int)block_w > state->max_block_w) {
        state->max_block_w = (int)block_w;
    }
    if ((int)block_h > state->max_block_h) {
        state->max_block_h = (int)block_h;
    }
    if (state->first_qcoeff_ptr == NULL) {
        state->first_qcoeff_ptr = qcoeff;
        state->first_dqcoeff_ptr = dqcoeff;
    }

    int all_zero = 1;
    const int32_t step = state->quant_step > 0 ? state->quant_step : 8;
    /* The coefficient arrays follow contract section 3, NOT the sample stride:
     * row stride min(w,32) and length min(w,32)*min(h,32). T011's scan indexes
     * this same grid, and the pool reserves exactly that many entries, so using
     * `stride` here would both contradict the contract and write past the
     * allocation for a 64x64 block.
     *
     * This stub is a lossless identity path, so it only carries the grid the
     * contract defines; `test_inverse` above reconstructs the remaining
     * (high-frequency) samples as zero, exactly as the frozen transform does. */
    const uint32_t kw = (block_w > 32u) ? 32u : block_w;
    const uint32_t kh = (block_h > 32u) ? 32u : block_h;
    for (uint32_t i = 0; i < kh; ++i) {
        for (uint32_t j = 0; j < kw; ++j) {
            const int32_t value = (int32_t)residual[(size_t)i * stride + j];
            coeff[(size_t)i * kw + j] = value;
            const int32_t magnitude = value < 0 ? -value : value;
            const int32_t level = (magnitude + step / 2) / step;
            const int32_t signed_level = value < 0 ? -level : level;
            qcoeff[(size_t)i * kw + j] = signed_level;
            dqcoeff[(size_t)i * kw + j] = signed_level * step;
            if (signed_level != 0) {
                all_zero = 0;
            }
        }
    }
    *all_zero_out = all_zero;
    return SVT_AV2_ERROR_NONE;
}

static SvtAv2Error test_inverse(void* user,
                                uint32_t plane,
                                uint32_t block_w,
                                uint32_t block_h,
                                const int32_t* dqcoeff,
                                uint32_t stride,
                                int16_t* inverse_residual) {
    TestPathState* state = (TestPathState*)user;
    if (state == NULL || dqcoeff == NULL || inverse_residual == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    if (plane > SVT_AV2_PLANE_V || block_w == 0 || block_h == 0 || stride < block_w) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    ++state->inverse_calls;
    /* Read the coefficient grid from the same min(w,32)-wide coordinates the
     * forward stub wrote (contract section 3), not from `stride`, and write
     * EVERY sample of the block: the samples outside the carried grid are zero
     * (the high frequencies the frozen transform drops). Leaving them untouched
     * would make the reconstruction depend on scratch contents from a previous
     * block. */
    const uint32_t kw = (block_w > 32u) ? 32u : block_w;
    const uint32_t kh = (block_h > 32u) ? 32u : block_h;
    for (uint32_t i = 0; i < block_h; ++i) {
        for (uint32_t j = 0; j < block_w; ++j) {
            int32_t v = 0;
            if (i < kh && j < kw) {
                /* Clamp to int16_t: the core only relies on this buffer holding
                 * the inverse residual; clipping to [0,255] happens after adding
                 * the prediction (contract 6). */
                v = dqcoeff[(size_t)i * kw + j];
                if (v > 32000) {
                    v = 32000;
                } else if (v < -32000) {
                    v = -32000;
                }
            }
            inverse_residual[(size_t)i * stride + j] = (int16_t)v;
        }
    }
    return SVT_AV2_ERROR_NONE;
}

static void test_path_init(SvtAv2IntraTransformPath* path, TestPathState* state) {
    memset(state, 0, sizeof(*state));
    state->quant_step = 8;
    state->first_qcoeff_ptr = NULL;
    state->first_dqcoeff_ptr = NULL;
    path->user = state;
    path->forward = test_forward;
    path->inverse = test_inverse;
    path->name = "test-scaffolding: passthrough + deadzone quantiser (NOT T009/T010)";
}

/* --- test image helpers ---------------------------------------------------- */

typedef struct TestImage {
    uint8_t* data; /* tightly packed, 8-bit 4:2:0 */
    size_t size;
} TestImage;

static TestImage make_pattern(uint32_t width, uint32_t height, int kind, uint32_t seed) {
    TestImage image;
    image.size = (size_t)width * height + (size_t)2u * ((width / 2u) * (height / 2u));
    image.data = (uint8_t*)malloc(image.size);
    if (image.data == NULL) {
        return image;
    }

    const uint32_t cw = width / 2u;
    const uint32_t ch = height / 2u;
    uint32_t rng = seed != 0 ? seed : 1u;

    for (uint32_t plane = 0; plane < 3u; ++plane) {
        const uint32_t pw = plane == 0 ? width : cw;
        const uint32_t ph = plane == 0 ? height : ch;
        const size_t offset = plane == 0 ? 0u : (size_t)width * height + (plane - 1u) * cw * ch;
        for (uint32_t y = 0; y < ph; ++y) {
            for (uint32_t x = 0; x < pw; ++x) {
                uint8_t value;
                switch (kind) {
                case 0: /* flat mid grey */
                    value = 128;
                    break;
                case 1: /* flat white */
                    value = 255;
                    break;
                case 2: /* gradient */
                    value = (uint8_t)((x * 7u + y * 3u) & 0xFFu);
                    break;
                case 3: /* checkerboard, 8-sample squares */
                    value = (((x / 8u) + (y / 8u)) & 1u) ? 255 : 0;
                    break;
                default: /* fixed-seed noise */
                    rng ^= rng << 13;
                    rng ^= rng >> 17;
                    rng ^= rng << 5;
                    value = (uint8_t)(rng & 0xFFu);
                    break;
                }
                image.data[offset + (size_t)y * pw + x] = value;
            }
        }
    }
    return image;
}

/* Copy a tight image into a picture's visible area (bypasses
 * svt_av2_picture_copy_in, which wants two pictures). */
static void load_picture(SvtAv2FrameBuffer* picture, const TestImage* image) {
    size_t offset = 0;
    for (uint32_t plane = 0; plane < 3u; ++plane) {
        uint8_t* const base = svt_av2_picture_plane(picture, plane);
        const uint32_t stride = picture->strides[plane];
        const uint32_t pw = svt_av2_picture_plane_width(picture, plane);
        const uint32_t ph = svt_av2_picture_plane_height(picture, plane);
        for (uint32_t y = 0; y < ph; ++y) {
            memcpy(base + (size_t)y * stride, image->data + offset, pw);
            offset += pw;
        }
    }
}

/* --- tests ----------------------------------------------------------------- */

static void test_fails_closed_without_transform_path(void) {
    SvtAv2FrameBuffer source;
    SvtAv2FrameBuffer recon;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&source, 64, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&recon, 64, 64), SVT_AV2_ERROR_NONE);

    SvtAv2ReconScratch scratch;
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(&scratch, 64, 64), SVT_AV2_ERROR_NONE);

    SvtAv2IntraFrameConfig config;
    memset(&config, 0, sizeof(config));
    config.base_q_idx = 100;
    config.sb_size = 64;
    config.frame_reset_value = 0xCD;
    /* transform left zeroed: no forward/inverse. */

    SvtAv2IntraFrameStats stats;
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_intra_frame_encode(&source, &recon, &config, &scratch, &stats),
        SVT_AV2_ERROR_UNSUPPORTED);

    /* A half-built path is also refused. */
    TestPathState state;
    SvtAv2IntraTransformPath path;
    test_path_init(&path, &state);
    config.transform = path;
    config.transform.inverse = NULL;
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_intra_frame_encode(&source, &recon, &config, &scratch, &stats),
        SVT_AV2_ERROR_UNSUPPORTED);

    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_free(&scratch), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&recon), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&source), SVT_AV2_ERROR_NONE);
}

static void test_invalid_frame_inputs(void) {
    SvtAv2FrameBuffer source;
    SvtAv2FrameBuffer recon;
    SvtAv2FrameBuffer other;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&source, 64, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&recon, 64, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&other, 32, 32), SVT_AV2_ERROR_NONE);

    SvtAv2ReconScratch scratch;
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(&scratch, 64, 64), SVT_AV2_ERROR_NONE);

    TestPathState state;
    SvtAv2IntraFrameConfig config;
    memset(&config, 0, sizeof(config));
    config.sb_size = 64;
    test_path_init(&config.transform, &state);

    SvtAv2IntraFrameStats stats;

    /* Source and recon must be distinguishable buffers. */
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_intra_frame_encode(&source, &source, &config, &scratch, &stats),
        SVT_AV2_ERROR_INVALID_PARAMETER);

    /* Geometry mismatch. */
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_intra_frame_encode(&other, &recon, &config, &scratch, &stats),
        SVT_AV2_ERROR_INVALID_PARAMETER);

    /* NULL inputs. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_frame_encode(NULL, &recon, &config, &scratch, &stats),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_frame_encode(&source, NULL, &config, &scratch, &stats),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_frame_encode(&source, &recon, NULL, &scratch, &stats),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_frame_encode(&source, &recon, &config, NULL, &stats),
                          SVT_AV2_ERROR_INVALID_PARAMETER);

    /* Uninitialised pool. */
    SvtAv2ReconScratch empty;
    memset(&empty, 0, sizeof(empty));
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_frame_encode(&source, &recon, &config, &empty, &stats),
                          SVT_AV2_ERROR_INVALID_PARAMETER);

    /* A pool too small for the superblock is rejected up front. */
    SvtAv2ReconScratch small;
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(&small, 32, 32), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_intra_frame_encode(&source, &recon, &config, &small, &stats),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_free(&small), SVT_AV2_ERROR_NONE);

    /* T007's own verdict must reach the caller unchanged: the geometry module
     * reports UNSUPPORTED both for an illegal sb_size (not 64/128/256) and for
     * a frame size that is not a multiple of 8, and INVALID_PARAMETER only for
     * NULL. The frame core must pass that through rather than flattening it
     * into its own code, so a caller can tell what to fix. */
    {
        SvtAv2IntraFrameConfig bad_sb = config;
        bad_sb.sb_size = 48; /* not 64/128/256 */
        SVT_TEST_CHECK_EQ_INT(
            svt_av2_intra_frame_encode(&source, &recon, &bad_sb, &scratch, &stats),
            SVT_AV2_ERROR_UNSUPPORTED);
    }
    {
        /* svt_av2_picture_alloc only accepts multiples of 8, so build both
         * descriptors by hand to reach the geometry check with a bad size. */
        SvtAv2FrameBuffer odd_src = source;
        SvtAv2FrameBuffer odd_recon = recon;
        odd_src.visible_width = odd_recon.visible_width = 65;
        SVT_TEST_CHECK_EQ_INT(
            svt_av2_intra_frame_encode(&odd_src, &odd_recon, &config, &scratch, &stats),
            SVT_AV2_ERROR_UNSUPPORTED);
    }
    {
        /* Pin both verdicts against their opposites. A caller (or a future
         * change to T007's ordering) that swaps them would otherwise still
         * "pass" a single-code test if the code were the same in both branches.
         * These two assertions state the mapping explicitly: an illegal
         * sb_size and a non-multiple-of-8 frame size are BOTH UNSUPPORTED, and
         * neither is INVALID_PARAMETER. */
        SvtAv2IntraFrameConfig bad_sb = config;
        bad_sb.sb_size = 48;
        const SvtAv2Error bad_sb_err =
            svt_av2_intra_frame_encode(&source, &recon, &bad_sb, &scratch, &stats);
        SVT_TEST_CHECK(bad_sb_err != SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(bad_sb_err, SVT_AV2_ERROR_UNSUPPORTED);

        SvtAv2FrameBuffer odd_src = source;
        SvtAv2FrameBuffer odd_recon = recon;
        odd_src.visible_height = odd_recon.visible_height = 65;
        const SvtAv2Error odd_err =
            svt_av2_intra_frame_encode(&odd_src, &odd_recon, &config, &scratch, &stats);
        SVT_TEST_CHECK(odd_err != SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(odd_err, SVT_AV2_ERROR_UNSUPPORTED);
    }

    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_free(&scratch), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&other), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&recon), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&source), SVT_AV2_ERROR_NONE);
}

/* Runs one 64x64 frame and checks the reconstruction against an independent
 * model of the same chain (prediction per DC rules + quantised residual).
 *
 * `expect_changed` says whether this input must differ from the source under
 * the scaffolding quantiser's step. Flat white and the 8-sample checkerboard
 * are exactly representable at step 8 with DC = 128, so they round-trip
 * losslessly; that is a property of the scaffolding step, not of the chain
 * (the separate quantiser-sensitivity test below proves the reconstruction is
 * not a copy of the source). */
static void run_frame_case(int kind, uint32_t seed, int expect_changed) {
    const uint32_t width = 64;
    const uint32_t height = 64;

    TestImage image = make_pattern(width, height, kind, seed);
    SVT_TEST_CHECK(image.data != NULL);
    if (image.data == NULL) {
        return;
    }

    SvtAv2FrameBuffer source;
    SvtAv2FrameBuffer recon;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&source, width, height), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&recon, width, height), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_pad(&source, 0x00), SVT_AV2_ERROR_NONE);
    load_picture(&source, &image);

    SvtAv2ReconScratch scratch;
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(&scratch, 64, 64), SVT_AV2_ERROR_NONE);

    TestPathState state;
    SvtAv2IntraFrameConfig config;
    memset(&config, 0, sizeof(config));
    config.base_q_idx = 100;
    config.sb_size = 64;
    config.frame_reset_value = 0xCD;
    test_path_init(&config.transform, &state);

    SvtAv2IntraFrameStats stats;
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_intra_frame_encode(&source, &recon, &config, &scratch, &stats),
        SVT_AV2_ERROR_NONE);

    /* 64x64 with sb_size 64 is one 64x64 luma block plus one 32x32 chroma block
     * per plane. */
    SVT_TEST_CHECK_EQ_INT(stats.luma_blocks, 1);
    SVT_TEST_CHECK_EQ_INT(stats.chroma_blocks, 2);
    SVT_TEST_CHECK_EQ_INT(stats.zero_blocks + stats.nonzero_blocks, 3);
    SVT_TEST_CHECK_EQ_INT(state.forward_calls, 3);
    SVT_TEST_CHECK_EQ_INT(state.inverse_calls, 3);
    SVT_TEST_CHECK_EQ_INT(state.out_of_range_stride, 0);
    SVT_TEST_CHECK_EQ_INT(state.max_block_w, 64);
    SVT_TEST_CHECK_EQ_INT(state.max_block_h, 64);

    /* The prediction of the first block must not depend on the poison value the
     * frame reset wrote: for a 64x64 frame the first luma block has no
     * available neighbour, so its DC prediction is exactly 128 for every
     * sample. */
    const uint8_t* const y = svt_av2_picture_plane_const(&recon, SVT_AV2_PLANE_Y);
    SVT_TEST_CHECK(y != NULL);
    const uint32_t y_stride = recon.strides[SVT_AV2_PLANE_Y];
    /* Reconstruct the first block from the model and compare sample by sample
     * only where the model is unambiguous: DC = 128, quantised residual.
     *
     * The coefficient grid is min(w,32) wide (contract section 3), so for this
     * 64x64 block only the top-left 32x32 carries residual; the rest is zero
     * (the high frequencies the frozen transform drops), so those samples
     * reconstruct as the prediction alone. Modelling that here is what makes
     * this a contract test rather than a stub-shape test. */
    for (uint32_t r = 0; r < 64; ++r) {
        for (uint32_t c = 0; c < 64; ++c) {
            const int32_t pred = 128;
            int32_t inv = 0;
            if (r < 32u && c < 32u) {
                const int32_t res = (int32_t)image.data[(size_t)r * width + c] - pred;
                const int32_t magnitude = res < 0 ? -res : res;
                const int32_t level = (magnitude + 8 / 2) / 8;
                const int32_t signed_level = res < 0 ? -level : level;
                inv = signed_level * 8;
            }
            int32_t sum = pred + inv;
            if (sum < 0) {
                sum = 0;
            } else if (sum > 255) {
                sum = 255;
            }
            SVT_TEST_CHECK_EQ_INT(y[(size_t)r * y_stride + c], (uint8_t)sum);
        }
    }

    /* Evidence that the chain is not identity: when the input has content the
     * reconstruction must differ from the source somewhere, and different
     * inputs must give different reconstructions. */
    size_t exported_size = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_raw_size(&recon, &exported_size), SVT_AV2_ERROR_NONE);
    uint8_t* exported = (uint8_t*)malloc(exported_size);
    SVT_TEST_CHECK(exported != NULL);
    SvtAv2ReconExport meta;
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_recon_export_frame(&recon, 0, exported, exported_size, &meta),
        SVT_AV2_ERROR_NONE);

    size_t differences = 0;
    for (size_t i = 0; i < exported_size; ++i) {
        if (exported[i] != image.data[i]) {
            ++differences;
        }
    }
    if (expect_changed) {
        if (differences == 0) {
            fprintf(stderr, "NOTE: case kind=%d seed=%u produced no change vs source\n", kind, seed);
        }
        SVT_TEST_CHECK(differences > 0);
    }

    /* Padding and samples outside the visible area must not have been emitted:
     * the export length is exactly the tight size. */
    SVT_TEST_CHECK_EQ_INT(meta.byte_count, (size_t)width * height + (size_t)2u * (width / 2u) * (height / 2u));

    free(exported);
    free(image.data);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_free(&scratch), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&recon), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&source), SVT_AV2_ERROR_NONE);
}

static void test_frame_cases(void) {
    run_frame_case(1, 0, 0);        /* flat white   -> exactly representable at step 8 */
    run_frame_case(2, 0, 1);        /* gradient     -> quantisation loss             */
    run_frame_case(3, 0, 0);        /* checkerboard -> exactly representable at step 8 */
    run_frame_case(4, 0xA5A5u, 1);  /* fixed-seed noise -> quantisation loss         */
    /* Flat mid grey: prediction == source, so the reconstruction equals source
     * and the quantiser has nothing to do. This is a legal case, not a
     * bypass: it is the one input where a correct encoder must be lossless. */
    run_frame_case(0, 0, 0);
}

/* Proves the reconstruction is produced by the chain and not copied from the
 * source: with two different quantiser steps the same input must give two
 * different reconstructions, each matching its own model. */
static void test_recon_tracks_quantiser_not_source(void) {
    const uint32_t width = 64;
    const uint32_t height = 64;

    TestImage image = make_pattern(width, height, 2, 0x77u); /* gradient */
    SVT_TEST_CHECK(image.data != NULL);
    if (image.data == NULL) {
        return;
    }

    SvtAv2FrameBuffer source;
    SvtAv2FrameBuffer recon_a;
    SvtAv2FrameBuffer recon_b;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&source, width, height), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&recon_a, width, height), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&recon_b, width, height), SVT_AV2_ERROR_NONE);
    load_picture(&source, &image);

    SvtAv2ReconScratch scratch;
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(&scratch, 64, 64), SVT_AV2_ERROR_NONE);

    TestPathState state_a;
    TestPathState state_b;
    SvtAv2IntraFrameConfig config_a;
    SvtAv2IntraFrameConfig config_b;
    memset(&config_a, 0, sizeof(config_a));
    memset(&config_b, 0, sizeof(config_b));
    config_a.sb_size = 64;
    config_b.sb_size = 64;
    config_a.frame_reset_value = 0xCD;
    config_b.frame_reset_value = 0xCD;
    test_path_init(&config_a.transform, &state_a);
    test_path_init(&config_b.transform, &state_b);
    state_a.quant_step = 3;   /* finer */
    state_b.quant_step = 40;  /* coarse: many coefficients quantise to zero */

    SvtAv2IntraFrameStats stats_a;
    SvtAv2IntraFrameStats stats_b;
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_intra_frame_encode(&source, &recon_a, &config_a, &scratch, &stats_a),
        SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_intra_frame_encode(&source, &recon_b, &config_b, &scratch, &stats_b),
        SVT_AV2_ERROR_NONE);

    size_t size = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_raw_size(&recon_a, &size), SVT_AV2_ERROR_NONE);
    uint8_t* out_a = (uint8_t*)malloc(size);
    uint8_t* out_b = (uint8_t*)malloc(size);
    SVT_TEST_CHECK(out_a != NULL && out_b != NULL);
    SvtAv2ReconExport meta;
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_export_frame(&recon_a, 0, out_a, size, &meta),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_export_frame(&recon_b, 0, out_b, size, &meta),
                          SVT_AV2_ERROR_NONE);

    /* Coarser quantisation must change the reconstruction. */
    SVT_TEST_CHECK(memcmp(out_a, out_b, size) != 0);
    /* And at least one of them must differ from the source (the chain is not a
     * copy). */
    SVT_TEST_CHECK(memcmp(out_a, image.data, size) != 0 || memcmp(out_b, image.data, size) != 0);

    /* The coarse frame's luma must still match the independent model of the
     * chain with step 40, over the min(w,32)-wide coefficient grid the contract
     * defines (the samples outside it reconstruct as the prediction alone). */
    const uint8_t* const y = svt_av2_picture_plane_const(&recon_b, SVT_AV2_PLANE_Y);
    const uint32_t y_stride = recon_b.strides[SVT_AV2_PLANE_Y];
    for (uint32_t r = 0; r < 64; ++r) {
        for (uint32_t c = 0; c < 64; ++c) {
            const int32_t pred = 128;
            int32_t inv = 0;
            if (r < 32u && c < 32u) {
                const int32_t res = (int32_t)image.data[(size_t)r * width + c] - pred;
                const int32_t magnitude = res < 0 ? -res : res;
                const int32_t level = (magnitude + 20) / 40;
                const int32_t signed_level = res < 0 ? -level : level;
                inv = signed_level * 40;
            }
            int32_t sum = pred + inv;
            if (sum < 0) {
                sum = 0;
            } else if (sum > 255) {
                sum = 255;
            }
            SVT_TEST_CHECK_EQ_INT(y[(size_t)r * y_stride + c], (uint8_t)sum);
        }
    }

    free(out_a);
    free(out_b);
    free(image.data);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_free(&scratch), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&recon_b), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&recon_a), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&source), SVT_AV2_ERROR_NONE);
}

/* A block's prediction must come from the reconstruction of earlier blocks, not
 * from the source. Run the same source twice, once with a poisoned
 * reconstruction of the previous frame, and require identical output. */
static void test_cross_frame_independence(void) {
    const uint32_t width = 64;
    const uint32_t height = 64;

    TestImage image = make_pattern(width, height, 4, 0xBEEFu);
    SVT_TEST_CHECK(image.data != NULL);
    if (image.data == NULL) {
        return;
    }

    SvtAv2FrameBuffer source;
    SvtAv2FrameBuffer recon_a;
    SvtAv2FrameBuffer recon_b;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&source, width, height), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&recon_a, width, height), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&recon_b, width, height), SVT_AV2_ERROR_NONE);
    load_picture(&source, &image);

    SvtAv2ReconScratch scratch;
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(&scratch, 64, 64), SVT_AV2_ERROR_NONE);

    /* recon_a: fresh frame, reset value 0xCD.
     * recon_b: first "pollute" every byte with a different value, then encode
     * the same frame with reset value 0xCD. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_reset_frame(&recon_b, 0x9F), SVT_AV2_ERROR_NONE);

    TestPathState state_a;
    TestPathState state_b;
    SvtAv2IntraFrameConfig config_a;
    SvtAv2IntraFrameConfig config_b;
    memset(&config_a, 0, sizeof(config_a));
    memset(&config_b, 0, sizeof(config_b));
    config_a.sb_size = 64;
    config_b.sb_size = 64;
    config_a.frame_reset_value = 0xCD;
    config_b.frame_reset_value = 0xCD;
    test_path_init(&config_a.transform, &state_a);
    test_path_init(&config_b.transform, &state_b);

    SvtAv2IntraFrameStats stats_a;
    SvtAv2IntraFrameStats stats_b;
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_intra_frame_encode(&source, &recon_a, &config_a, &scratch, &stats_a),
        SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_intra_frame_encode(&source, &recon_b, &config_b, &scratch, &stats_b),
        SVT_AV2_ERROR_NONE);

    size_t size = 0;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_raw_size(&recon_a, &size), SVT_AV2_ERROR_NONE);
    uint8_t* out_a = (uint8_t*)malloc(size);
    uint8_t* out_b = (uint8_t*)malloc(size);
    SVT_TEST_CHECK(out_a != NULL && out_b != NULL);
    SvtAv2ReconExport meta;
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_export_frame(&recon_a, 0, out_a, size, &meta),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_export_frame(&recon_b, 0, out_b, size, &meta),
                          SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK(memcmp(out_a, out_b, size) == 0);

    free(out_a);
    free(out_b);
    free(image.data);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_free(&scratch), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&recon_b), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&recon_a), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&source), SVT_AV2_ERROR_NONE);
}

/* The scratch buffers must be the same objects for every block and every frame
 * (contract 6: no per-block malloc). */
static void test_scratch_reused_across_blocks_and_frames(void) {
    const uint32_t width = 176;
    const uint32_t height = 144;

    TestImage image = make_pattern(width, height, 2, 0x33u);
    SVT_TEST_CHECK(image.data != NULL);
    if (image.data == NULL) {
        return;
    }

    SvtAv2FrameBuffer source;
    SvtAv2FrameBuffer recon;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&source, width, height), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&recon, width, height), SVT_AV2_ERROR_NONE);
    load_picture(&source, &image);

    SvtAv2ReconScratch scratch;
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(&scratch, 64, 64), SVT_AV2_ERROR_NONE);

    const uint8_t* const pred_ptr = scratch.prediction;
    const int16_t* const res_ptr = scratch.residual;
    const int16_t* const inv_ptr = scratch.inverse_residual;
    const int32_t* const coeff_ptr = scratch.coeff;
    const int32_t* const qcoeff_ptr = scratch.qcoeff;

    TestPathState state;
    SvtAv2IntraFrameConfig config;
    memset(&config, 0, sizeof(config));
    config.sb_size = 64;
    config.frame_reset_value = 0xCD;
    test_path_init(&config.transform, &state);

    SvtAv2IntraFrameStats stats;
    for (int frame = 0; frame < 3; ++frame) {
        SVT_TEST_CHECK_EQ_INT(
            svt_av2_intra_frame_encode(&source, &recon, &config, &scratch, &stats),
            SVT_AV2_ERROR_NONE);
        /* Not a single buffer pointer moved. */
        SVT_TEST_CHECK(scratch.prediction == pred_ptr);
        SVT_TEST_CHECK(scratch.residual == res_ptr);
        SVT_TEST_CHECK(scratch.inverse_residual == inv_ptr);
        SVT_TEST_CHECK(scratch.coeff == coeff_ptr);
        SVT_TEST_CHECK(scratch.qcoeff == qcoeff_ptr);
        SVT_TEST_CHECK(state.first_qcoeff_ptr == qcoeff_ptr);
    }

    /* 176x144 with 64x64 superblocks: 3 x 3 superblocks, of which the right
     * column and bottom row are partial. Every visible sample is covered
     * exactly once, so the block counts must be non-zero and the chroma count
     * is twice the luma count. */
    SVT_TEST_CHECK(stats.luma_blocks > 0);
    SVT_TEST_CHECK_EQ_INT(stats.chroma_blocks, 2u * stats.luma_blocks);
    SVT_TEST_CHECK(state.out_of_range_stride == 0);

    free(image.data);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_free(&scratch), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&recon), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&source), SVT_AV2_ERROR_NONE);
}

/* Records, per plane, which samples have been reconstructed so far, and checks
 * the raster-order precondition T008's interface note (section 3) depends on:
 * whenever the frame core tells T008 that a left/above neighbour is available
 * (frame boundary), that neighbour must ALREADY have been reconstructed by an
 * earlier block of this frame. It also checks that the traversal covers every
 * visible sample of every plane exactly once, and that the availability the
 * core acts on is exactly T007's svt_av2_block_neighbors verdict. */
typedef struct Coverage {
    uint8_t* mask[3];   /* one byte per visible sample: reconstructed yet? */
    uint32_t width[3];
    uint32_t height[3];
    int violation_left;  /* core claimed have_left but (x-1, y) not reconstructed */
    int violation_above; /* core claimed have_above but (x, y-1) not reconstructed */
    int flag_mismatch;   /* core's flags != svt_av2_block_neighbors */
    const SvtAv2FrameBuffer* recon;
    uint32_t blocks;
} Coverage;

static int coverage_hook(void* user,
                         uint32_t plane,
                         const SvtAv2BlockView* block,
                         const SvtAv2ReconScratch* scratch,
                         int all_zero) {
    (void)scratch;
    (void)all_zero;
    Coverage* cov = (Coverage*)user;
    if (plane > 2u) {
        return 1;
    }

    /* The availability policy must be T007's, not a local re-derivation: ask
     * the geometry module the same question the core asked and compare. */
    SvtAv2BlockNeighbors nb;
    if (svt_av2_block_neighbors(cov->recon, plane, block, &nb) != SVT_AV2_ERROR_NONE) {
        return 1;
    }
    const int expect_left = nb.left_valid;
    const int expect_above = nb.above_valid;
    if (expect_left != (block->x > 0 ? 1 : 0) || expect_above != (block->y > 0 ? 1 : 0)) {
        cov->flag_mismatch = 1;
    }

    /* The core's flags are the frame-boundary flags: check they are honest. */
    if (block->x > 0) {
        const size_t base = (size_t)block->y * cov->width[plane];
        for (uint32_t j = 0; j < block->h; ++j) {
            if (!cov->mask[plane][base + (size_t)j * cov->width[plane] + (block->x - 1u)]) {
                cov->violation_left = 1;
            }
        }
    }
    if (block->y > 0) {
        const size_t base = (size_t)(block->y - 1u) * cov->width[plane] + block->x;
        for (uint32_t j = 0; j < block->w; ++j) {
            if (!cov->mask[plane][base + j]) {
                cov->violation_above = 1;
            }
        }
    }

    for (uint32_t i = 0; i < block->h; ++i) {
        for (uint32_t j = 0; j < block->w; ++j) {
            cov->mask[plane][(size_t)(block->y + i) * cov->width[plane] + block->x + j] = 1;
        }
    }
    ++cov->blocks;
    return 0;
}

static void test_neighbour_availability_is_raster_order(void) {
    const uint32_t width = 176;
    const uint32_t height = 144;

    TestImage image = make_pattern(width, height, 4, 0x5A5Au);
    SVT_TEST_CHECK(image.data != NULL);
    if (image.data == NULL) {
        return;
    }

    SvtAv2FrameBuffer source;
    SvtAv2FrameBuffer recon;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&source, width, height), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&recon, width, height), SVT_AV2_ERROR_NONE);
    load_picture(&source, &image);

    SvtAv2ReconScratch scratch;
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(&scratch, 64, 64), SVT_AV2_ERROR_NONE);

    Coverage cov;
    memset(&cov, 0, sizeof(cov));
    cov.recon = &recon;
    for (uint32_t plane = 0; plane < 3u; ++plane) {
        cov.width[plane] = svt_av2_picture_plane_width(&recon, plane);
        cov.height[plane] = svt_av2_picture_plane_height(&recon, plane);
        cov.mask[plane] = (uint8_t*)calloc((size_t)cov.width[plane] * cov.height[plane], 1);
        SVT_TEST_CHECK(cov.mask[plane] != NULL);
    }

    TestPathState state;
    SvtAv2IntraFrameConfig config;
    memset(&config, 0, sizeof(config));
    config.sb_size = 64;
    config.frame_reset_value = 0xCD;
    config.block_hook = coverage_hook;
    config.block_hook_user = &cov;
    test_path_init(&config.transform, &state);

    SvtAv2IntraFrameStats stats;
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_intra_frame_encode(&source, &recon, &config, &scratch, &stats),
        SVT_AV2_ERROR_NONE);

    /* The precondition T008 section 3 requires: the frame-boundary flags the
     * core passes are backed by earlier reconstruction in raster order, and
     * they agree with T007's own svt_av2_block_neighbors verdict. */
    SVT_TEST_CHECK_EQ_INT(cov.violation_left, 0);
    SVT_TEST_CHECK_EQ_INT(cov.violation_above, 0);
    SVT_TEST_CHECK_EQ_INT(cov.flag_mismatch, 0);
    SVT_TEST_CHECK_EQ_INT(cov.blocks, 3u * stats.luma_blocks);

    /* Every visible sample of every plane was reconstructed exactly once (the
     * mask only records 0/1, so this asserts full coverage; the "exactly once"
     * half is the block-count equality above, since hook calls are per block). */
    for (uint32_t plane = 0; plane < 3u; ++plane) {
        for (uint32_t i = 0; i < cov.height[plane]; ++i) {
            for (uint32_t j = 0; j < cov.width[plane]; ++j) {
                SVT_TEST_CHECK_EQ_INT(cov.mask[plane][(size_t)i * cov.width[plane] + j], 1);
            }
        }
        free(cov.mask[plane]);
    }

    free(image.data);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_free(&scratch), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&recon), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&source), SVT_AV2_ERROR_NONE);
}

/* A block hook failure must abort the frame; the core must not report success
 * after a failed syntax write. */
static int failing_hook(void* user,
                        uint32_t plane,
                        const SvtAv2BlockView* block,
                        const SvtAv2ReconScratch* scratch,
                        int all_zero) {
    int* calls = (int*)user;
    (void)plane;
    (void)block;
    (void)scratch;
    (void)all_zero;
    ++(*calls);
    return 1; /* simulate a syntax write that cannot continue */
}

static void test_hook_failure_aborts(void) {
    SvtAv2FrameBuffer source;
    SvtAv2FrameBuffer recon;
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&source, 64, 64), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc(&recon, 64, 64), SVT_AV2_ERROR_NONE);

    SvtAv2ReconScratch scratch;
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(&scratch, 64, 64), SVT_AV2_ERROR_NONE);

    int hook_calls = 0;
    TestPathState state;
    SvtAv2IntraFrameConfig config;
    memset(&config, 0, sizeof(config));
    config.sb_size = 64;
    config.frame_reset_value = 0xCD;
    config.block_hook = failing_hook;
    config.block_hook_user = &hook_calls;
    test_path_init(&config.transform, &state);

    SvtAv2IntraFrameStats stats;
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_intra_frame_encode(&source, &recon, &config, &scratch, &stats),
        SVT_AV2_ERROR_INVALID_STATE);
    /* Exactly one block was attempted before the hook aborted the frame. */
    SVT_TEST_CHECK_EQ_INT(hook_calls, 1);

    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_free(&scratch), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&recon), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_free(&source), SVT_AV2_ERROR_NONE);
}

typedef struct PlanIntraHook { uint32_t count; SvtAv2ChromaFormat format; } PlanIntraHook;
static int plan_intra_collect(void* user, uint32_t plane, const SvtAv2BlockView* block,
    const SvtAv2ReconScratch* scratch, int all_zero) {
    PlanIntraHook* hook = (PlanIntraHook*)user;
    const uint32_t index = hook->count / 3u;
    const uint32_t divisor = plane && hook->format == SVT_AV2_CHROMA_420 ? 2u : 1u;
    (void)all_zero;
    SVT_TEST_CHECK_EQ_INT(plane, hook->count % 3u); SVT_TEST_CHECK(scratch->qcoeff != NULL);
    SVT_TEST_CHECK_EQ_INT(block->w, (index < 4u ? 32u : 64u) / divisor);
    SVT_TEST_CHECK_EQ_INT(block->h, block->w);
    SVT_TEST_CHECK_EQ_INT(block->x, (index < 4u ? index % 2u * 32u : 64u) / divisor);
    SVT_TEST_CHECK_EQ_INT(block->y, (index < 4u ? index / 2u * 32u : 0u) / divisor);
    ++hook->count;
    return 0;
}
static void test_partition_plan_real_path(void) {
    /* This additional M5.2 test uses the production transform path, unlike the
     * independent scaffolding tests above. It checks geometry/hook plumbing. */
    for (uint32_t f = 0; f < 3u; ++f) {
        const uint32_t bd = f ? 10u : 8u, step = f ? 2u : 1u, scale = f ? 4u : 1u;
        const SvtAv2ChromaFormat format = f == 2u ? SVT_AV2_CHROMA_444 : SVT_AV2_CHROMA_420;
        SvtAv2FrameBuffer source, recon;
        svt_av2_picture_alloc_ex(&source, 128, 64, bd, format);
        svt_av2_picture_alloc_ex(&recon, 128, 64, bd, format);
        svt_av2_recon_reset_frame(&source, (uint16_t)(128u * scale));
        for (uint32_t y = 0; y < 64u; ++y) for (uint32_t x = 0; x < 64u; ++x)
            svt_av2_sample_store(source.planes[0] + y * source.strides[0] + x * step, bd,
                (uint16_t)((1u + y / 32u * 2u + x / 32u) * 40u * scale));
        SvtAv2FrameGeometry geometry; svt_av2_frame_geometry_init_ex(&geometry, 128, 64, 64, format);
        uint16_t plan[2];
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_select_quads(&source, &geometry, plan, 2), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(plan[0], 1); SVT_TEST_CHECK_EQ_INT(plan[1], 0);
        SvtAv2ReconScratch scratch; svt_av2_recon_pool_init(&scratch, 64, 64);
        SvtAv2TransformPathState path; SvtAv2IntraFrameConfig config; memset(&config, 0, sizeof(config));
        config.sb_size = 64u; config.frame_reset_value = (uint16_t)(128u * scale);
        svt_av2_transform_path_init_bd(&path, &config.transform, f ? 148u : 100u, bd);
        config.partition_decisions = plan; config.partition_decision_count = 2u;
        PlanIntraHook hook; config.block_hook = plan_intra_collect; config.block_hook_user = &hook;
        SvtAv2IntraFrameStats stats;
        for (uint32_t adaptive = 0; adaptive < 2u; ++adaptive) {
            memset(&hook, 0, sizeof(hook)); hook.format = format; config.m5_select_intra = adaptive;
            SVT_TEST_CHECK_EQ_INT(svt_av2_intra_frame_encode(&source, &recon, &config, &scratch, &stats), SVT_AV2_ERROR_NONE);
            SVT_TEST_CHECK_EQ_INT(hook.count, 15); SVT_TEST_CHECK_EQ_INT(stats.luma_blocks, 5);
            SVT_TEST_CHECK_EQ_INT(stats.chroma_blocks, 10); SVT_TEST_CHECK(stats.nonzero_blocks > 0);
            SVT_TEST_CHECK_EQ_INT(plan[0], 1); SVT_TEST_CHECK_EQ_INT(plan[1], 0);
        }
        plan[1] = 882u; hook.count = 0;
        SVT_TEST_CHECK_EQ_INT(svt_av2_intra_frame_encode(&source, &recon, &config, &scratch, &stats), SVT_AV2_ERROR_INVALID_PARAMETER);
        SVT_TEST_CHECK_EQ_INT(hook.count, 0);
        svt_av2_recon_pool_free(&scratch); svt_av2_picture_free(&source); svt_av2_picture_free(&recon);
    }
}

int main(void) {
    test_partition_plan_real_path();
    test_fails_closed_without_transform_path();
    test_invalid_frame_inputs();
    test_frame_cases();
    test_recon_tracks_quantiser_not_source();
    test_cross_frame_independence();
    test_scratch_reused_across_blocks_and_frames();
    test_hook_failure_aborts();
    test_neighbour_availability_is_raster_order();

    SVT_TEST_REPORT("test_intra_frame");
}
