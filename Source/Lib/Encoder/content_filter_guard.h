#ifndef M6_CONTENT_FILTER_GUARD_H
#define M6_CONTENT_FILTER_GUARD_H
#include <stddef.h>
#include <stdint.h>

typedef struct M6ContentFilterGuardStats {
    uint64_t edge_samples, flat_samples;
    uint64_t edge_base_sse, edge_trial_sse;
    uint64_t flat_base_sse, flat_trial_sse;
} M6ContentFilterGuardStats;

/* All recon arrays use full-frame width as their uint16_t element stride.
 * Evaluate after ordinary SSE/rate selection. Return 0 to keep the base RU.
 * RU dimensions may reach 384, including PCW's merged 320/376-row endings.
 * Only source pixels select masks; neither path, labels nor future RUs enter.
 * Supported source storage: uint8_t, or little-endian 10-bit in uint16_t. */
int m6_content_filter_accept(const uint8_t *source, size_t source_stride,
    unsigned bit_depth, const uint16_t *base, const uint16_t *trial,
    unsigned width, unsigned height, unsigned x0, unsigned y0,
    unsigned ru_width, unsigned ru_height, M6ContentFilterGuardStats *stats);
#endif
