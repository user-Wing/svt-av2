/* memo1: exact pre-RDOQ memo; one bounded allocation. */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "quantization.h"
#ifndef M6_TX_MEMO_IDS
#define M6_TX_MEMO_IDS 262144u
#endif
#ifndef M6_TX_MEMO_BUCKETS
#define M6_TX_MEMO_BUCKETS (2u * M6_TX_MEMO_IDS)
#endif
#ifndef M6_TX_MEMO_BYTES
#define M6_TX_MEMO_BYTES 168173752u
#endif
#if M6_TX_MEMO_IDS == 0 || M6_TX_MEMO_BUCKETS == 0 || \
    (M6_TX_MEMO_BUCKETS & (M6_TX_MEMO_BUCKETS - 1u)) != 0
#error Invalid_memo1_directory_size
#endif
typedef struct RdTransformCacheEntry {
    uint64_t hash;
    uint32_t key[6];
    uint32_t hprev, hnext, lprev, lnext;
    uint32_t offset, bytes;
    int all_zero;
    uint32_t generation;
} RdTransformCacheEntry;
typedef struct RdTransformCache {
    RdTransformCacheEntry* entries;
    uint32_t* buckets;
    uint8_t* arena;
    size_t allocation_bytes;
    uint32_t arena_capacity, arena_used, live_bytes, live_count;
    uint32_t free_head, lru_head, lru_tail;
    uint64_t clock;
    uint64_t hits, misses, replaces, collisions;
    uint64_t bound_trials, bound_pruned;
    uint64_t rdoq_route_trials[3],rdoq_route_wins[3],rdoq_live_rate_calls;
    uint64_t treeq_trials, treeq_wins, treeq_changes, treeq_finalists;
    uint64_t treeq_plan_commits, treeq_plan_repairs;
    uint64_t shared_f, shared_g;
    uint64_t repeat_trials, repeat_wins, repeat_rounds, repeat_changes, repeat_after_joint;
    uint64_t repeat_pair_trials, repeat_pair_illegal, repeat_pair_default;
    uint64_t compactions, moved_bytes, integrity_resets, dequant_rebuilds;
    uint64_t requests_by_area[9], misses_by_area[9];
} RdTransformCache;
#define RD_TX_RECORD_HEADER 16u
static uint32_t rd_tx_memo_bucket(uint64_t hash) {
    return (uint32_t)(hash ^ (hash >> 32u)) & (M6_TX_MEMO_BUCKETS - 1u);
}
static void rd_tx_memo_clear(RdTransformCache* c) {
    memset(c->buckets, 0, (size_t)M6_TX_MEMO_BUCKETS * sizeof(*c->buckets));
    memset(c->entries, 0, (size_t)M6_TX_MEMO_IDS * sizeof(*c->entries));
    for (uint32_t i = 0; i < M6_TX_MEMO_IDS; ++i)
        c->entries[i].hnext = i + 1u < M6_TX_MEMO_IDS ? i + 2u : 0u;
    c->free_head = 1u;
    c->lru_head = c->lru_tail = 0u;
    c->arena_used = c->live_bytes = c->live_count = 0u;
}
static RdTransformCache* rd_tx_memo_create(void) {
    const size_t header = (sizeof(RdTransformCache) + 7u) & ~(size_t)7u;
    const size_t entries = (size_t)M6_TX_MEMO_IDS * sizeof(RdTransformCacheEntry);
    const size_t buckets = (size_t)M6_TX_MEMO_BUCKETS * sizeof(uint32_t);
    const size_t budget = (size_t)M6_TX_MEMO_BYTES;
    if (header > budget || entries > budget - header ||
        buckets >= budget - header - entries) return NULL;
    const size_t payload = budget - header - entries - buckets;
    if (payload > UINT32_MAX || payload < 176u) return NULL;
    RdTransformCache* c = (RdTransformCache*)calloc(1u, budget);
    if (!c) return NULL;
    c->allocation_bytes = budget;
    c->entries = (RdTransformCacheEntry*)((uint8_t*)c + header);
    c->buckets = (uint32_t*)((uint8_t*)c->entries + entries);
    c->arena = (uint8_t*)c->buckets + buckets;
    c->arena_capacity = (uint32_t)payload;
    rd_tx_memo_clear(c);
    return c;
}
static void rd_tx_memo_detach(RdTransformCache* c, uint32_t id) {
    RdTransformCacheEntry* e = &c->entries[id - 1u];
    if (e->lprev) c->entries[e->lprev - 1u].lnext = e->lnext;
    else c->lru_head = e->lnext;
    if (e->lnext) c->entries[e->lnext - 1u].lprev = e->lprev;
    else c->lru_tail = e->lprev;
    e->lprev = e->lnext = 0u;
}
static void rd_tx_memo_front(RdTransformCache* c, uint32_t id) {
    RdTransformCacheEntry* e = &c->entries[id - 1u];
    e->lprev = 0u; e->lnext = c->lru_head;
    if (c->lru_head) c->entries[c->lru_head - 1u].lprev = id;
    else c->lru_tail = id;
    c->lru_head = id;
}
static void rd_tx_memo_touch(RdTransformCache* c, uint32_t id) {
    if (id == c->lru_head) return;
    rd_tx_memo_detach(c, id);
    rd_tx_memo_front(c, id);
}
static void rd_tx_memo_evict(RdTransformCache* c, uint32_t id) {
    RdTransformCacheEntry* e = &c->entries[id - 1u];
    /* Unlink before reusing the stable ID. */
    if (e->hprev) c->entries[e->hprev - 1u].hnext = e->hnext;
    else c->buckets[rd_tx_memo_bucket(e->hash)] = e->hnext;
    if (e->hnext) c->entries[e->hnext - 1u].hprev = e->hprev;
    rd_tx_memo_detach(c, id);
    c->live_bytes -= e->bytes;
    --c->live_count; ++c->replaces;
    e->bytes = 0u;
    e->hprev = 0u; e->hnext = c->free_head;
    c->free_head = id;
}
static uint32_t rd_tx_memo_span(const uint32_t key[6]) {
    const uint32_t w = key[1], h = key[2];
    if (!w || !h || w > 64u || h > 64u) return 0u;
    const uint32_t kw = w < 32u ? w : 32u;
    const uint32_t kh = h < 32u ? h : 32u;
    return (RD_TX_RECORD_HEADER + 2u * w * h + 8u * kw * kh + 7u) & ~7u;
}
static const uint8_t* rd_tx_memo_data(const RdTransformCache* c, uint32_t id) {
    return c->arena + c->entries[id - 1u].offset + RD_TX_RECORD_HEADER;
}
static uint32_t rd_tx_memo_find(RdTransformCache* c, uint64_t hash,
    const uint32_t key[6], const int16_t* residual, uint32_t stride) {
    for (uint32_t id = c->buckets[rd_tx_memo_bucket(hash)]; id;
         id = c->entries[id - 1u].hnext) {
        const RdTransformCacheEntry* e = &c->entries[id - 1u];
        if (e->hash != hash || memcmp(e->key, key, sizeof(e->key)) != 0) continue;
        const uint8_t* packed = rd_tx_memo_data(c, id);
        uint32_t y = 0u;
        while (y < key[2] && memcmp(packed + (size_t)y * key[1] * sizeof(int16_t),
            residual + (size_t)y * stride, (size_t)key[1] * sizeof(int16_t)) == 0) ++y;
        if (y == key[2]) return id;
        ++c->collisions;
    }
    return 0u;
}
static int rd_tx_memo_restore(RdTransformCache* c, uint32_t id,
    int32_t* coeff, int32_t* qcoeff, int32_t* dqcoeff, int* all_zero) {
    const RdTransformCacheEntry* e = &c->entries[id - 1u];
    const uint32_t w = e->key[1], h = e->key[2];
    const size_t count = (size_t)(w < 32u ? w : 32u) * (h < 32u ? h : 32u);
    const uint8_t* raw = rd_tx_memo_data(c, id) + (size_t)w * h * sizeof(int16_t);
    memcpy(coeff, raw, count * sizeof(*coeff));
    memcpy(qcoeff, raw + count * sizeof(*coeff), count * sizeof(*qcoeff));
    SvtAv2QuantParams params;
    if (svt_av2_quant_params_init_tx_bd(&params, e->key[4], w, h, e->key[5]) !=
            SVT_AV2_QUANT_OK ||
        svt_av2_dequantize_block(&params, qcoeff, dqcoeff, count, (int)e->key[0]) !=
            SVT_AV2_QUANT_OK) return 0;
    /* Only the original valid prefix is written; scratch tails stay untouched. */
    *all_zero = e->all_zero;
    rd_tx_memo_touch(c, id);
    ++c->hits; ++c->dequant_rebuilds;
    return 1;
}
static int rd_tx_memo_compact(RdTransformCache* c) {
    const uint32_t old_used = c->arena_used;
    uint32_t read_pos = 0u, write_pos = 0u;
    ++c->compactions;
    while (read_pos < old_used) {
        uint32_t header[4];
        if (old_used - read_pos < sizeof(header)) goto corrupt;
        memcpy(header, c->arena + read_pos, sizeof(header));
        /* Cache scalars before memmove: the old header may be overwritten. */
        const uint32_t span = header[0], id = header[1], generation = header[2];
        if (span < RD_TX_RECORD_HEADER || (span & 7u) ||
            span > old_used - read_pos || !id || id > M6_TX_MEMO_IDS)
            goto corrupt;
        RdTransformCacheEntry* e = &c->entries[id - 1u];
        if (e->bytes == span && e->offset == read_pos && e->generation == generation) {
            if (write_pos != read_pos) {
                memmove(c->arena + write_pos, c->arena + read_pos, span);
                c->moved_bytes += span;
            }
            e->offset = write_pos;
            write_pos += span;
        }
        read_pos += span;
    }
    if (write_pos != c->live_bytes) goto corrupt;
    c->arena_used = write_pos;
    return 1;
corrupt:
    /* Never manufacture a cache result; acceptance requires this count zero. */
    ++c->integrity_resets;
    rd_tx_memo_clear(c);
    return 0;
}
static int rd_tx_memo_make_room(RdTransformCache* c, uint32_t span) {
    if (!span || span > c->arena_capacity) return 0;
    if (!c->free_head && c->lru_tail) rd_tx_memo_evict(c, c->lru_tail);
    if (!c->free_head) return 0;
    if (span <= c->arena_capacity - c->arena_used) return 1;
    uint32_t reserve = c->arena_capacity / 8u;
    if (reserve < span) reserve = span;
    const uint32_t target = c->arena_capacity - reserve;
    while (c->live_bytes > target && c->lru_tail)
        rd_tx_memo_evict(c, c->lru_tail);
    if (span > c->arena_capacity - c->live_bytes) return 0;
    if (!rd_tx_memo_compact(c)) return 0;
    return c->free_head && span <= c->arena_capacity - c->arena_used;
}
static int rd_tx_memo_store(RdTransformCache* c, uint64_t hash,
    const uint32_t key[6], const int16_t* residual, uint32_t stride,
    const int32_t* coeff, const int32_t* qcoeff, int all_zero) {
    const uint32_t span = rd_tx_memo_span(key);
    if (!rd_tx_memo_make_room(c, span)) return 0;
    const uint32_t id = c->free_head;
    RdTransformCacheEntry* e = &c->entries[id - 1u];
    c->free_head = e->hnext;
    if (++e->generation == 0u) e->generation = 1u;
    e->hash = hash;
    memcpy(e->key, key, sizeof(e->key));
    e->offset = c->arena_used; e->bytes = span; e->all_zero = all_zero;
    const uint32_t header[4] = {span, id, e->generation, 0u};
    uint8_t* dst = c->arena + e->offset;
    memcpy(dst, header, sizeof(header));
    dst += RD_TX_RECORD_HEADER;
    for (uint32_t y = 0; y < key[2]; ++y)
        memcpy(dst + (size_t)y * key[1] * sizeof(int16_t),
            residual + (size_t)y * stride, (size_t)key[1] * sizeof(int16_t));
    dst += (size_t)key[1] * key[2] * sizeof(int16_t);
    const size_t count = (size_t)(key[1] < 32u ? key[1] : 32u) *
                                (key[2] < 32u ? key[2] : 32u);
    memcpy(dst, coeff, count * sizeof(*coeff));
    memcpy(dst + count * sizeof(*coeff), qcoeff, count * sizeof(*qcoeff));
    c->arena_used += span; c->live_bytes += span; ++c->live_count;
    const uint32_t bucket = rd_tx_memo_bucket(hash);
    e->hprev = 0u; e->hnext = c->buckets[bucket];
    if (e->hnext) c->entries[e->hnext - 1u].hprev = id;
    c->buckets[bucket] = id;
    rd_tx_memo_front(c, id);
    return 1;
}
static void rd_tx_memo_report(const RdTransformCache* c) {
    fprintf(stderr, "txcache hits=%llu misses=%llu replacements=%llu collisions=%llu "
        "slots=%u bytes=%llu ways=0 policy=byte_lru arena=%u live_bytes=%u "
        "live_entries=%u compactions=%llu moved_bytes=%llu integrity_resets=%llu "
        "dequant_rebuilds=%llu\n",
        (unsigned long long)c->hits, (unsigned long long)c->misses,
        (unsigned long long)c->replaces, (unsigned long long)c->collisions,
        (unsigned)M6_TX_MEMO_IDS, (unsigned long long)c->allocation_bytes,
        c->arena_capacity, c->live_bytes, c->live_count,
        (unsigned long long)c->compactions, (unsigned long long)c->moved_bytes,
        (unsigned long long)c->integrity_resets, (unsigned long long)c->dequant_rebuilds);
    for (uint32_t i = 0; i < 9u; ++i)
        if (c->requests_by_area[i])
            fprintf(stderr, "txmemo_area pels=%u requests=%llu misses=%llu\n",
                16u << i, (unsigned long long)c->requests_by_area[i],
                (unsigned long long)c->misses_by_area[i]);
}
