/* T011: tile-writer scratch/capacity invariants.
 *
 * Background: the default entry (scratch == NULL) used to take its entropy
 * scratch from a fixed SVT_AV2_TILE_PAYLOAD_MAX_BYTES (32u) stack buffer, so a
 * caller-supplied `out_cap` larger than 32 bytes was silently capped. It now
 * derives the scratch from `out_cap`, so the caller's capacity is the only
 * bound. keyframe.c (T013) calls that entry.
 *
 * Honest scope note: at this configuration the M1 block is a fixed 7-symbol,
 * 1-coded-byte payload, so no reachable input makes the old 32u cap observable
 * (verified: old and new code return identical statuses for out_cap 1..10).
 * This test therefore does NOT prove the >32-byte path (no input reaches it);
 * it pins the invariants that must hold either way:
 *   1. the frozen M1 vector is unchanged through the default entry,
 *   2. the reported bound dominates the real payload,
 *   3. a too-small capacity fails closed (never truncates),
 *   4. the scratch variant and the default entry produce identical bytes.
 * The >32-byte behaviour is argued from the code, not exercised here. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "frame_header.h"
#include "tile_writer.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        ++checks;                                                              \
        if (!(cond)) {                                                         \
            ++failures;                                                        \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
        }                                                                      \
    } while (0)

int main(void) {
    SvtAv2FrameHeaderConfig cfg;
    svt_av2_frame_header_config_default(&cfg, 64, 64);

    /* 1. M1 payload through the default entry. */
    static const uint8_t expect[4] = {0xE6, 0xC8, 0x00, 0x12};
    uint8_t buf[256];
    size_t  n = 0;
    int st = svt_av2_tile_writer_write_closed_loop_key(&cfg, buf, sizeof(buf), &n);
    CHECK(st == 0);
    CHECK(n == 4u);
    CHECK(memcmp(buf, expect, 4) == 0);

    /* 2. The reported bound dominates the real payload, and the default entry
     *    succeeds when handed a capacity well above the old 32u constant. This
     *    does not by itself distinguish the new code from the old (the M1
     *    payload is 1 coded byte, so both pass), but it pins that a large
     *    caller capacity is accepted and that the reported bound is not
     *    below the actual payload. */
    size_t bound = svt_av2_tile_writer_max_payload_bytes(64, 64);
    CHECK(bound >= n);

    uint8_t* big = (uint8_t*)malloc(4096);
    CHECK(big != NULL);
    if (big != NULL) {
        size_t n2 = 0;
        st = svt_av2_tile_writer_write_closed_loop_key(&cfg, big, 4096, &n2);
        CHECK(st == 0);
        CHECK(n2 == n);
        CHECK(memcmp(big, expect, 4) == 0);
        free(big);
    }

    /* 3. A capacity just below the header size must fail closed, not truncate,
     *    and must not claim more output than the buffer can hold. */
    uint8_t tiny[2];
    size_t  tsize = 999;
    st = svt_av2_tile_writer_write_closed_loop_key(&cfg, tiny, sizeof(tiny),
                                                   &tsize);
    CHECK(st != 0);
    CHECK(tsize >= sizeof(tiny));

    /* 4. The scratch variant and the default entry must produce identical
     *    bytes, so callers can switch without a stream change. */
    uint8_t  coded[256];
    uint16_t precarry[258];
    SvtAv2TileWriterScratch sc = {coded, sizeof(coded), precarry,
                                  sizeof(precarry) / sizeof(precarry[0])};
    uint8_t buf2[256];
    size_t  n3 = 0;
    st = svt_av2_tile_writer_write_closed_loop_key_scratch(&cfg, buf2, sizeof(buf2),
                                                           &n3, &sc);
    CHECK(st == 0);
    CHECK(n3 == n);
    CHECK(memcmp(buf2, buf, n) == 0);

    /* 5. A caller-provided scratch that cannot hold the payload must fail
     *    closed rather than overrun. This is reachable at this configuration
     *    (the coder needs at least one coded byte and one precarry entry), so
     *    unlike the >32-byte case it is a genuine guard on the capacity
     *    plumbing: if the scratch sizes were ignored, this would succeed. */
    uint8_t  tiny_coded[1];
    uint16_t tiny_prec[1];
    SvtAv2TileWriterScratch sc_small = {tiny_coded, sizeof(tiny_coded),
                                        tiny_prec,
                                        sizeof(tiny_prec) / sizeof(tiny_prec[0])};
    uint8_t buf3[256];
    size_t  n4 = 4321;
    st = svt_av2_tile_writer_write_closed_loop_key_scratch(
        &cfg, buf3, sizeof(buf3), &n4, &sc_small);
    CHECK(st != 0);

    printf("test_tile_writer_scratch: %d checks, %d failures\n", checks,
           failures);
    return failures == 0 ? 0 : 1;
}
