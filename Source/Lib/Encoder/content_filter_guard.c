#include "content_filter_guard.h"
#include <string.h>

static int source_pixel(const uint8_t *p, size_t stride, unsigned bits,
                        unsigned x, unsigned y) {
    p += y*stride+x*(bits>8 ? 2u : 1u);
    return bits>8 ? (int)p[0] | ((int)p[1]<<8) : p[0];
}

static uint64_t square(int64_t v) { return (uint64_t)(v*v); }

static int flat_patch(const uint8_t *p, size_t stride, unsigned bits,
                      unsigned x, unsigned y, int limit) {
    for (unsigned j=y-2; j<=y+2; ++j)
        for (unsigned i=x-2; i<=x+2; ++i) {
            int v=source_pixel(p,stride,bits,i,j);
            if (i<x+2) {
                int d=source_pixel(p,stride,bits,i+1,j)-v;
                if (d>limit || d< -limit) return 0;
            }
            if (j<y+2) {
                int d=source_pixel(p,stride,bits,i,j+1)-v;
                if (d>limit || d< -limit) return 0;
            }
        }
    return 1;
}

int m6_content_filter_accept(const uint8_t *source, size_t stride,
    unsigned bits, const uint16_t *base, const uint16_t *trial,
    unsigned width, unsigned height, unsigned x0, unsigned y0,
    unsigned rw, unsigned rh, M6ContentFilterGuardStats *result) {
    M6ContentFilterGuardStats stats;
    memset(&stats,0,sizeof(stats));
    if (result) *result=stats;
    if (!source || !base || !trial || (bits!=8 && bits!=10) ||
        x0>=width || y0>=height || rw==0 || rh==0 || rw>384 || rh>384 ||
        stride<(size_t)width*(bits>8 ? 2u : 1u)) return 0;
    unsigned x1=rw>width-x0 ? width : x0+rw;
    unsigned y1=rh>height-y0 ? height : y0+rh;
    int scale=1<<(bits-8), threshold=12*scale;
    for (unsigned y=y0; y<y1; ++y)
        for (unsigned x=x0; x<x1; ++x) {
            size_t k=(size_t)y*width+x;
            int v=source_pixel(source,stride,bits,x,y);
            if (x+1<x1 && y+1<y1) {
                int gx=source_pixel(source,stride,bits,x+1,y)-v;
                int gy=source_pixel(source,stride,bits,x,y+1)-v;
                if (square(gx)+square(gy)>=(uint64_t)threshold*threshold) {
                    stats.edge_base_sse+=square((int)base[k+1]-base[k]-gx)
                        +square((int)base[k+width]-base[k]-gy);
                    stats.edge_trial_sse+=square((int)trial[k+1]-trial[k]-gx)
                        +square((int)trial[k+width]-trial[k]-gy);
                    ++stats.edge_samples;
                }
            }
            if (x>=x0+2 && x+2<x1 && y>=y0+2 && y+2<y1 &&
                flat_patch(source,stride,bits,x,y,2*scale)) {
                stats.flat_base_sse+=square((int)base[k]-v);
                stats.flat_trial_sse+=square((int)trial[k]-v);
                ++stats.flat_samples;
            }
        }
    if (result) *result=stats;
    /* Equal masks/counts make RMSE ratios equal sqrt(SSE ratios). RUs up to
     * 384x384 at <=10 bits keep these products below 1.3e16 (uint64_t).
     * This includes PCW's merged 320/376-row final RUs. Empty masks
     * do not veto. A zero base SSE permits only a zero candidate SSE. */
    return !(stats.edge_samples && stats.edge_trial_sse*10000>stats.edge_base_sse*10404)
        && !(stats.flat_samples && stats.flat_trial_sse*10000>stats.flat_base_sse*10404);
}
