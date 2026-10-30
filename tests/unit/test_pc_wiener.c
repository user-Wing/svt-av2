/* Expected hashes were produced by fixed AVM full-frame PC_WIENER, not by
 * this implementation. See the independent kernel-matrix/results.json. */
#include "pc_wiener.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct Vector {unsigned w,h,bd,q,seed;uint64_t hash;} Vector;
static const Vector vectors[] = {
    {64u,64u,8u,1u,1u,UINT64_C(15700497455391098141)},
    {64u,64u,8u,129u,1u,UINT64_C(17777055512084487123)},
    {64u,64u,8u,130u,1u,UINT64_C(4245682045066117140)},
    {64u,64u,8u,189u,1u,UINT64_C(1927415048941700563)},
    {64u,64u,8u,190u,1u,UINT64_C(13965924441102381546)},
    {64u,64u,8u,219u,1u,UINT64_C(12645678773851027432)},
    {64u,64u,8u,220u,1u,UINT64_C(14865765880853007726)},
    {64u,64u,8u,255u,1u,UINT64_C(5292270323986271167)},
    {80u,80u,10u,49u,1u,UINT64_C(17789911500642594097)},
    {80u,80u,10u,129u,1u,UINT64_C(17894466468436704044)},
    {80u,80u,10u,130u,1u,UINT64_C(9311277360739892632)},
    {80u,80u,10u,189u,1u,UINT64_C(10823395787830607936)},
    {80u,80u,10u,190u,1u,UINT64_C(8559893264589736983)},
    {80u,80u,10u,219u,1u,UINT64_C(8368050570090091592)},
    {80u,80u,10u,220u,1u,UINT64_C(6216208075069808487)},
    {80u,80u,10u,303u,1u,UINT64_C(17063344117464870740)},
};
int main(void) {
    unsigned checks=0u,failures=0u;
    for (size_t c=0u;c<sizeof(vectors)/sizeof(vectors[0]);++c) {
        const Vector v=vectors[c];
        const size_t ss=v.w+3u,ds=v.w+5u,ts=v.w/4u+2u;
        uint16_t *src=(uint16_t*)calloc(ss*v.h,sizeof(*src));
        uint16_t *dst=(uint16_t*)malloc(ds*v.h*sizeof(*dst));
        uint8_t *skip=(uint8_t*)calloc(ts*(v.h/4u),1u);
        if (!src || !dst || !skip) return 2;
        for (size_t i=0u;i<ds*v.h;++i) dst[i]=65535u;
        for (unsigned y=0u;y<v.h;++y) for (unsigned x=0u;x<v.w;++x)
            src[y*ss+x]=(uint16_t)((17u*x+23u*y+13u*((x*y)%19u)+v.seed*37u)&((1u<<v.bd)-1u));
        for (unsigned y=0u;y<v.h/4u;++y) for (unsigned x=0u;x<v.w/4u;++x)
            skip[y*ts+x]=(uint8_t)((x+y/3u+v.seed)%3u==0u);
        const int status=svt_av2_pcwiener_filter_y(src,(ptrdiff_t)ss,dst,(ptrdiff_t)ds,
            v.w,v.h,v.bd,v.q,skip,(ptrdiff_t)ts);
        ++checks;if(status) ++failures;
        uint64_t hash=UINT64_C(14695981039346656037);
        for (unsigned y=0u;y<v.h;++y) {
            for(unsigned x=0u;x<v.w;++x) {hash^=dst[y*ds+x];hash*=UINT64_C(1099511628211);}
            for(size_t x=v.w;x<ds;++x) {++checks;if(dst[y*ds+x]!=65535u) ++failures;}
        }
        ++checks;if(hash!=v.hash) {++failures;printf("FAIL vector=%zu got=%llu expected=%llu\n",c,(unsigned long long)hash,(unsigned long long)v.hash);}
        free(skip);free(dst);free(src);
    }
    printf("test_pc_wiener: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
