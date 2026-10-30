#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "transform_path.h"
#include "quantization.h"
#include "transform.h"
#undef M6_TX_MEMO_IDS
#undef M6_TX_MEMO_BUCKETS
#undef M6_TX_MEMO_BYTES
#define M6_TX_MEMO_IDS 32u
#define M6_TX_MEMO_BUCKETS 64u
#define M6_TX_MEMO_BYTES 32768u
#include "rd_transform_memo.inc"
#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr,"FAIL memo1 line=%d expression=%s\n",__LINE__,#x); return 0; \
} } while (0)

static int memo_check(const RdTransformCache* c) {
    uint8_t lru[M6_TX_MEMO_IDS]={0}, hash[M6_TX_MEMO_IDS]={0};
    uint8_t available[M6_TX_MEMO_IDS]={0};
    uint32_t count=0u, bytes=0u, previous=0u;
    if (c->arena_used>c->arena_capacity || c->live_bytes>c->arena_used) return 0;
    for (uint32_t id=c->lru_head; id;) {
        if (id>M6_TX_MEMO_IDS || count>=M6_TX_MEMO_IDS || lru[id-1u]) return 0;
        const RdTransformCacheEntry* e=&c->entries[id-1u];
        if (!e->bytes || e->lprev!=previous || e->offset>c->arena_used ||
            e->bytes>c->arena_used-e->offset || (e->offset&7u)) return 0;
        uint32_t record[4];
        memcpy(record,c->arena+e->offset,sizeof(record));
        if (record[0]!=e->bytes || record[1]!=id || record[2]!=e->generation) return 0;
        lru[id-1u]=1u; bytes+=e->bytes; ++count; previous=id; id=e->lnext;
    }
    if (previous!=c->lru_tail || count!=c->live_count || bytes!=c->live_bytes) return 0;
    uint32_t hash_count=0u;
    for (uint32_t bucket=0u;bucket<M6_TX_MEMO_BUCKETS;++bucket) {
        previous=0u;
        for (uint32_t id=c->buckets[bucket];id;) {
            if (id>M6_TX_MEMO_IDS || hash_count>=M6_TX_MEMO_IDS || hash[id-1u]) return 0;
            const RdTransformCacheEntry* e=&c->entries[id-1u];
            if (!e->bytes || e->hprev!=previous || rd_tx_memo_bucket(e->hash)!=bucket) return 0;
            hash[id-1u]=1u; ++hash_count; previous=id; id=e->hnext;
        }
    }
    if (hash_count!=count) return 0;
    uint32_t free_count=0u;
    for (uint32_t id=c->free_head;id;) {
        if (id>M6_TX_MEMO_IDS || free_count>=M6_TX_MEMO_IDS || available[id-1u]) return 0;
        const RdTransformCacheEntry* e=&c->entries[id-1u];
        if (e->bytes || lru[id-1u] || hash[id-1u]) return 0;
        available[id-1u]=1u; ++free_count; id=e->hnext;
    }
    if (free_count+count!=M6_TX_MEMO_IDS) return 0;
    for (uint32_t i=0u;i<M6_TX_MEMO_IDS;++i)
        if (c->entries[i].bytes ? !(lru[i]&&hash[i]&&!available[i]) : !available[i]) return 0;
    return c->integrity_resets==0u;
}

static uint64_t memo_digest(const RdTransformCache* c,uint32_t id) {
    const RdTransformCacheEntry* e=&c->entries[id-1u];
    const uint8_t* p=rd_tx_memo_data(c,id);
    uint64_t h=UINT64_C(14695981039346656037);
    for (uint32_t i=0u;i<e->bytes-RD_TX_RECORD_HEADER;++i)
        h=(h^p[i])*UINT64_C(1099511628211);
    return h;
}
static int storage_stress(void) {
    RdTransformCache* c=rd_tx_memo_create();
    CHECK(c!=NULL);
    CHECK(c->allocation_bytes==M6_TX_MEMO_BYTES);
    _Static_assert(sizeof(RdTransformCacheEntry)==64u,"memo1 entry layout must be 64 bytes");
    int16_t hot[80u*4u], residual[80u*64u];
    int32_t coeff[1032], qcoeff[1032], got_c[1032], got_q[1032], got_d[1032];
    for (uint32_t i=0u;i<1032u;++i) {
        coeff[i]=(int32_t)(i%17u)-8;
        qcoeff[i]=(i%19u)==0u?1:0;
    }
    for (uint32_t i=0u;i<80u*4u;++i) hot[i]=7;
    const uint64_t forced_hash=UINT64_C(42);
    const uint32_t hot_key[6]={0u,4u,4u,0u,100u,8u};
    CHECK(rd_tx_memo_store(c,forced_hash,hot_key,hot,80u,coeff,qcoeff,0));
    CHECK(memo_check(c));
    uint32_t wrong_key[6]; memcpy(wrong_key,hot_key,sizeof(wrong_key));
    wrong_key[0]=1u;
    CHECK(rd_tx_memo_find(c,forced_hash,wrong_key,hot,80u)==0u);
    for (uint32_t iteration=1u;iteration<=200u;++iteration) {
        const uint32_t key[6]={0u,8u,8u,0u,100u,8u};
        for (uint32_t y=0u;y<64u;++y)
            for (uint32_t x=0u;x<80u;++x)
                residual[y*80u+x]=(int16_t)(x<8u&&y<8u?iteration:1234u);
        CHECK(rd_tx_memo_find(c,forced_hash,key,residual,80u)==0u);
        CHECK(rd_tx_memo_store(c,forced_hash,key,residual,80u,coeff,qcoeff,0));
        uint32_t id=rd_tx_memo_find(c,forced_hash,key,residual,80u);
        CHECK(id!=0u);
        for (uint32_t i=0u;i<1032u;++i) got_c[i]=got_q[i]=got_d[i]=123456789;
        int all_zero=-1;
        CHECK(rd_tx_memo_restore(c,id,got_c,got_q,got_d,&all_zero));
        CHECK(all_zero==0);
        CHECK(memcmp(got_c,coeff,64u*sizeof(*coeff))==0);
        CHECK(memcmp(got_q,qcoeff,64u*sizeof(*qcoeff))==0);
        for (uint32_t i=64u;i<1032u;++i)
            CHECK(got_c[i]==123456789 && got_q[i]==123456789 && got_d[i]==123456789);
        residual[7u*80u+7u]++;
        CHECK(rd_tx_memo_find(c,forced_hash,key,residual,80u)==0u);
        residual[7u*80u+7u]--;
        residual[7u*80u+79u]++;
        CHECK(rd_tx_memo_find(c,forced_hash,key,residual,80u)!=0u);
        if (iteration%5u==0u) {
            const uint32_t large[6]={0u,64u,64u,0u,100u,8u};
            for (uint32_t y=0u;y<64u;++y)
                for (uint32_t x=0u;x<64u;++x)
                    residual[y*80u+x]=-(int16_t)iteration;
            CHECK(rd_tx_memo_store(c,forced_hash,large,residual,80u,coeff,qcoeff,0));
            CHECK(rd_tx_memo_find(c,forced_hash,large,residual,80u)!=0u);
        }
        id=rd_tx_memo_find(c,forced_hash,hot_key,hot,80u);
        CHECK(id!=0u);
        CHECK(rd_tx_memo_restore(c,id,got_c,got_q,got_d,&all_zero));
        CHECK(memcmp(got_c,coeff,16u*sizeof(*coeff))==0);
        CHECK(memcmp(got_q,qcoeff,16u*sizeof(*qcoeff))==0);
        hot[79]++;
        CHECK(memo_check(c));
        if (iteration%7u==0u) {
            uint64_t digests[M6_TX_MEMO_IDS]={0};
            for (uint32_t i=0u;i<M6_TX_MEMO_IDS;++i)
                if (c->entries[i].bytes) digests[i]=memo_digest(c,i+1u);
            CHECK(rd_tx_memo_compact(c));
            CHECK(memo_check(c));
            for (uint32_t i=0u;i<M6_TX_MEMO_IDS;++i)
                if (c->entries[i].bytes) CHECK(digests[i]==memo_digest(c,i+1u));
        }
    }
    CHECK(c->collisions>0u && c->replaces>0u && c->compactions>0u);
    CHECK(c->moved_bytes>0u && c->integrity_resets==0u);
    rd_tx_memo_report(c);
    free(c);
    return 1;
}
static int production_roundtrip(void) {
    static const uint32_t dims[][2]={
        {4u,4u},{4u,8u},{8u,4u},{8u,8u},{8u,16u},{16u,8u},
        {16u,16u},{16u,32u},{32u,16u},{32u,32u},{32u,64u},
        {64u,32u},{64u,64u},{8u,64u},{64u,8u},{16u,64u},{64u,16u}
    };
    static const uint32_t user_q[3]={1u,100u,255u};
    RdTransformCache* c=rd_tx_memo_create();
    CHECK(c!=NULL);
    int16_t residual[80u*64u];
    int32_t expected_c[1032],expected_q[1032],expected_d[1032];
    int32_t actual_c[1032],actual_q[1032],actual_d[1032];
    uint32_t cases=0u;
    for (uint32_t depth=8u;depth<=10u;depth+=2u) {
        for (uint32_t qi=0u;qi<3u;++qi) {
            const uint32_t qindex=user_q[qi]+(depth-8u)*24u;
            SvtAv2TransformPathState state;
            SvtAv2IntraTransformPath path;
            CHECK(svt_av2_transform_path_init_bd(&state,&path,qindex,depth)==SVT_AV2_ERROR_NONE);
            CHECK(svt_av2_transform_path_cacheable(&path,qindex,depth));
            for (uint32_t plane=0u;plane<3u;++plane) {
                for (size_t di=0u;di<sizeof(dims)/sizeof(dims[0]);++di) {
                    const uint32_t w=dims[di][0],h=dims[di][1];
                    if (!svt_av2_transform_path_supports(w,h)) continue;
                    const size_t count=(size_t)(w<32u?w:32u)*(h<32u?h:32u);
                    for (uint32_t y=0u;y<64u;++y)
                        for (uint32_t x=0u;x<80u;++x)
                            residual[y*80u+x]=(int16_t)(x<w&&y<h?
                                (int32_t)((x*17u+y*29u+plane*11u+qi*5u)%63u)-31:3210);
                    const uint32_t tx_count=((w==h&&w<=16u)||
                        (w==8u&&h==16u)||(w==16u&&h==8u))?9u:1u;
                    for (uint32_t tx=0u;tx<tx_count;++tx) {
                        for (uint32_t i=0u;i<1032u;++i)
                            expected_c[i]=expected_q[i]=expected_d[i]=-9876543;
                        int expected_zero=-1,actual_zero=-1;
                        const uint64_t before=state.forward_calls;
                        const SvtAv2Error status=tx?
                            path.forward_type(path.user,plane,w,h,residual,80u,
                                expected_c,expected_q,expected_d,&expected_zero,tx):
                            path.forward(path.user,plane,w,h,residual,80u,
                                expected_c,expected_q,expected_d,&expected_zero);
                        if (status!=SVT_AV2_ERROR_NONE) {
                            CHECK(tx!=0u && status==SVT_AV2_ERROR_UNSUPPORTED);
                            continue;
                        }
                        CHECK(state.forward_calls==before+1u);
                        const uint32_t key[6]={plane,w,h,tx,qindex,depth};
                        const uint64_t hash=UINT64_C(0x13572468);
                        CHECK(rd_tx_memo_store(c,hash,key,residual,80u,
                            expected_c,expected_q,expected_zero));
                        const uint32_t id=rd_tx_memo_find(c,hash,key,residual,80u);
                        CHECK(id!=0u);
                        for (uint32_t i=0u;i<1032u;++i)
                            actual_c[i]=actual_q[i]=actual_d[i]=123456789;
                        const uint64_t before_hit=state.forward_calls;
                        CHECK(rd_tx_memo_restore(c,id,actual_c,actual_q,actual_d,&actual_zero));
                        CHECK(state.forward_calls==before_hit);
                        CHECK(actual_zero==expected_zero);
                        CHECK(memcmp(actual_c,expected_c,count*sizeof(*actual_c))==0);
                        CHECK(memcmp(actual_q,expected_q,count*sizeof(*actual_q))==0);
                        CHECK(memcmp(actual_d,expected_d,count*sizeof(*actual_d))==0);
                        for (size_t i=count;i<1032u;++i) {
                            CHECK(expected_c[i]==-9876543 && expected_q[i]==-9876543 && expected_d[i]==-9876543);
                            CHECK(actual_c[i]==123456789 && actual_q[i]==123456789 && actual_d[i]==123456789);
                        }
                        CHECK(memo_check(c));
                        ++cases;
                    }
                }
            }
        }
    }
    CHECK(cases>=100u && c->integrity_resets==0u);
    printf("PASS memo1 production roundtrip cases=%u\n",cases);
    free(c);
    return 1;
}
int main(void) {
    if (!storage_stress() || !production_roundtrip()) return 1;
    puts("PASS memo1_internal");
    return 0;
}
