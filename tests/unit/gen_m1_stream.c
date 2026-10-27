/*
 * T005 minimal-stream generator (M1: 64x64 / 8-bit / 4:2:0 / single frame).
 *
 * Drives the library entry point svt_av2_encode_keyframe_64x64() and writes the
 * resulting Annex B elementary stream
 *   TEMPORAL_DELIMITER + SEQUENCE_HEADER(64x64,L0) + CLOSED_LOOP_KEY(tile_group)
 * to argv[1] (default frame_min_64x64.bin).
 *
 * This generator deliberately calls the same library function the application
 * will call, so "what the self-test runs" == "what the library entry runs"
 * (00 decision (a)). It carries no OBU/bitstream logic of its own.
 *
 * Standalone build (see docs/task-reports/T005.md):
 *   cl /nologo /W3 /O2 /I Source/Lib/Bitstream /I Source/Lib/EntropyCoding ^
 *      /Fe:gen_m1_stream.exe tests/unit/gen_m1_stream.c ^
 *      <build>/lib/svt_av2_bitstream.lib <build>/lib/svt_av2_entropy.lib
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "keyframe.h"

#define M1_W 64u
#define M1_H 64u
#define M1_BASE_Q_IDX 100u   /* frozen M1 value; maps to COEFF_CDF_Q_CTXS = 1 */

int main(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1] : "frame_min_64x64.bin";

    /* Input planes are accepted but unused by the M1 encoder (DC_PRED, zero
     * residual). Pass zero-filled tightly packed 4:2:0 planes so the call is
     * also valid once residual coding lands. */
    size_t y_sz = M1_W * M1_H;
    size_t c_sz = (M1_W / 2) * (M1_H / 2);
    uint8_t* y = (uint8_t*)calloc(1, y_sz);
    uint8_t* u = (uint8_t*)calloc(1, c_sz);
    uint8_t* v = (uint8_t*)calloc(1, c_sz);
    if (!y || !u || !v) { printf("FAIL alloc\n"); return 1; }

    uint8_t out[4096];
    size_t out_size = 0;
    int st = svt_av2_encode_keyframe_64x64(
        y, M1_W, u, M1_W / 2, v, M1_W / 2,
        M1_BASE_Q_IDX, out, sizeof(out), &out_size);
    free(y); free(u); free(v);

    if (st != 0 || out_size == 0) { printf("FAIL encode st=%d\n", st); return 1; }

    FILE* f = fopen(path, "wb");
    if (!f) { printf("FAIL open %s\n", path); return 1; }
    fwrite(out, 1, out_size, f);
    fclose(f);

    printf("OK wrote %zu bytes to %s\n  stream:", out_size, path);
    for (size_t i = 0; i < out_size; ++i) printf(" %02X", out[i]);
    printf("\n");
    return 0;
}
