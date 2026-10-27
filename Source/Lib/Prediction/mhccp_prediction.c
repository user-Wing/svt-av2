/* Fixed-v1 MHCCP arithmetic; independent experimental implementation. */
#include "intra_prediction.h"

#define MHC_STRIDE 256
#define MHC_REF_PELS (MHC_STRIDE * 128)
typedef struct MhcReferences {
    uint16_t y[MHC_REF_PELS], uv[MHC_REF_PELS];
    int above, left, width, height, top_sb;
} MhcReferences;
#include <limits.h>
#include <string.h>

static int minimum(int a,int b) { return a<b?a:b; }
static int msb(uint32_t value) {
    int result=0;
    while (value>>1) { value>>=1; ++result; }
    return result;
}
static int32_t multiply(int32_t a,int32_t b,int shift) {
    const uint32_t ua=(uint32_t)(a<0?-a:a),ub=(uint32_t)(b<0?-b:b);
    int drop=msb(ua)+msb(ub)+2-29;
    if (drop<0) drop=0;
    const int s0=drop/2,s1=drop-s0,adjust=shift-drop;
    const int32_t product=(a>>s0)*(b>>s1);
    if (adjust<=0) return product;
    if (adjust>29) return 0;
    const uint32_t bias=1u<<(adjust-1);
    return product>=0 ? (int32_t)(((uint32_t)product+bias)>>adjust) :
        -(int32_t)(((uint32_t)(-product)+bias)>>adjust);
}
static int32_t reciprocal(uint32_t divisor,int* shift) {
    static const int w[8]={214,153,113,86,67,53,43,35};
    static const int o[8]={4822,5952,6624,6792,6408,5424,3792,1466};
    static const int b[8]={12784,12054,11670,11583,11764,12195,12870,13782};
    *shift=msb(divisor);
    const int delta=*shift-14;
    int32_t normalized=delta>=0 ? (int32_t)((divisor+(delta?1u<<(delta-1):0u))>>delta) :
        (int32_t)(divisor<<(-delta));
    if (normalized<1) normalized=1;
    if (normalized>32767) normalized=32767;
    const int fraction=normalized&16383,index=fraction>>11,d=fraction-o[index];
    return (((w[index]*((d*d)>>14))>>8)-(d>>1)+b[index])*4;
}

static uint16_t sample(const SvtAv2FrameBuffer* recon, uint32_t plane, int x, int y) {
    const uint32_t step = recon->bit_depth > 8u ? 2u : 1u;
    return svt_av2_sample_load(recon->planes[plane] + (size_t)y * recon->strides[plane] + (size_t)x * step,
        recon->bit_depth);
}

static void collect(const SvtAv2FrameBuffer* recon, const SvtAv2IntraBlock* block,
    uint32_t top_right, uint32_t bottom_left, MhcReferences* refs) {
    memset(refs, 0, sizeof(*refs));
    const int sub = recon->chroma_format == SVT_AV2_CHROMA_420;
    const int step = 1 << sub, x = (int)block->x * step, y = (int)block->y * step;
    const int w = (int)block->w * step, h = (int)block->h * step;
    const int above = y ? 2 * step : 0, left = x ? 2 * step : 0;
    int rw = minimum(128, left + w + (w > 4 ? (int)top_right * step : 0));
    int rh = minimum(128, above + h + (h > 4 ? (int)bottom_left * step : 0));
    rw = minimum(rw, (int)recon->visible_width - x + left);
    rh = minimum(rh, (int)recon->visible_height - y + above);
    refs->above = above >> sub; refs->left = left >> sub;
    refs->width = rw >> sub; refs->height = rh >> sub;
    refs->top_sb = y % 64 == 0;
    if (!above && !left) return;
    for (int j = 0; j < refs->height; ++j) for (int i = 0; i < refs->width; ++i) {
        if ((j >= refs->above && i >= refs->left + (int)block->w) ||
            (j >= refs->above + (int)block->h && i >= refs->left)) continue;
        int yy = y - above + j * step;
        if (refs->top_sb && above == 2 * step && j < 2) yy = y - 1;
        const int xx = x - left + i * step;
        if (sub) {
            const int bottom = refs->top_sb && above == 4 && j < 2 ? 0 : 1;
            refs->y[j * MHC_STRIDE + i] = (uint16_t)((sample(recon,0u,xx,yy) + sample(recon,0u,xx+1,yy) +
                sample(recon,0u,xx,yy+bottom) + sample(recon,0u,xx+1,yy+bottom)) * 2);
        } else refs->y[j * MHC_STRIDE + i] = (uint16_t)(sample(recon,0u,xx,yy) * 8u);
        /* Only training boundaries are causal chroma. Never read current or suffix UV. */
        if (j < refs->above || i < refs->left) {
            int cy = (int)block->y - refs->above + j, cx = (int)block->x - refs->left + i;
            if (refs->above == 2 && j == 0) ++cy;
            if (refs->top_sb && refs->above == 2 && j == 1) cy = (int)block->y - 1;
            if (refs->left == 2 && i == 0) ++cx;
            refs->uv[j * MHC_STRIDE + i] = sample(recon,block->plane,cx,cy);
        }
    }
}

static void derive(const MhcReferences* refs,int bd,int dir,int32_t params[3]) {
    int32_t c[3][4]={{0}};int count=0;
    const int mid=1<<(bd-1);
    for (int j=1;j<refs->height-1;++j) for (int i=1;i<refs->width-1;++i) {
        if (i>=refs->left&&j>=refs->above) continue;
        const int offset=refs->top_sb&&refs->above==2&&j<refs->above?refs->above-1-j:0;
        const int pos=(j+offset)*MHC_STRIDE+i;
        const int center=refs->y[pos]>>3;
        const int32_t a[3]={refs->y[pos-(dir==1?MHC_STRIDE:dir==2?1:0)]>>3,
            (center*center+mid)>>bd,mid};
        for (int row=0;row<3;++row) {
            for (int col=row;col<3;++col) c[row][col]+=a[row]*a[col];
            c[row][3]+=a[row]*refs->uv[pos];
        }
        ++count;
    }
    if (!count) {params[0]=params[1]=0;params[2]=65536;return;}
    const int ceil_log=msb((uint32_t)count)+(count!=(1<<msb((uint32_t)count)));
    const int matrix_shift=22-2*bd-ceil_log;
    for (int row=0;row<3;++row) for (int col=row;col<4;++col)
        c[row][col]=matrix_shift>0?c[row][col]*(1<<matrix_shift):c[row][col]>>(-matrix_shift);
    for (int row=0;row<3;++row) {
        for (int col=0;col<row;++col) c[row][col]=c[col][row];
        c[row][row]+=2<<(bd-8);
    }
    for (int row=0;row<3;++row) {
        const int32_t diag=c[row][row];
        uint32_t divisor=(uint32_t)(diag<0?-diag:diag);if (!divisor) divisor=1;
        int shift;const int32_t scale=reciprocal(divisor,&shift);
        for (int col=row+1;col<4;++col) c[row][col]=multiply(c[row][col],scale,shift);
        for (int next=row+1;next<3;++next) for (int col=row+1;col<4;++col)
            c[next][col]-=multiply(c[next][row],c[row][col],16);
    }
    for (int row=2;row>=0;--row) {
        params[row]=c[row][3];
        for (int col=row+1;col<3;++col) params[row]-=multiply(c[row][col],params[col],16);
    }
}

static void predict(const MhcReferences* refs,int width,int height,int bd,int dir,
    const int32_t params[3],uint16_t* dst,int stride) {
    const int mid=1<<(bd-1),limit=(1<<bd)-1;
    const uint16_t* l=refs->y+refs->above*MHC_STRIDE+refs->left;
    for (int y=0;y<height;++y) for (int x=0;x<width;++x) {
        const int center=l[y*MHC_STRIDE+x]>>3;
        const int tap=dir==1?(y||refs->above?-MHC_STRIDE:0):dir==2?(x||refs->left?-1:0):0;
        const int32_t v[3]={l[y*MHC_STRIDE+x+tap]>>3,(center*center+mid)>>bd,mid};
        int value=0;for (int i=0;i<3;++i) value+=multiply(params[i],v[i],16);
        if (value<INT16_MIN) value=INT16_MIN;
        if (value>INT16_MAX) value=INT16_MAX;
        dst[y*stride+x]=(uint16_t)(value<0?0:value>limit?limit:value);
    }
}

int svt_av2_intra_mhccp_allowed(uint32_t width, uint32_t height, uint32_t chroma_format) {
    if ((chroma_format != SVT_AV2_CHROMA_420 && chroma_format != SVT_AV2_CHROMA_444) ||
        width < 4u || height < 4u || width > 64u || height > 64u ||
        (width & (width - 1u)) || (height & (height - 1u)) ||
        width > 8u * height || height > 8u * width) return 0;
    const uint32_t shift = chroma_format == SVT_AV2_CHROMA_420 ? 1u : 0u;
    const uint32_t w = SVT_AV2_MAX(4u, width >> shift), h = SVT_AV2_MAX(4u, height >> shift);
    return w <= 32u && h <= 32u && (w != 4u || h != 4u);
}

uint32_t svt_av2_intra_mhccp_size_group(uint32_t width, uint32_t height) {
    const uint32_t side = width < height ? width : height;
    const uint32_t base = side >= 32u ? 3u : side >= 16u ? 2u : side >= 8u ? 1u : 0u;
    return base + (width == 8u * height || height == 8u * width);
}

SvtAv2Error svt_av2_intra_mhccp_predict(const SvtAv2FrameBuffer* recon,
    const SvtAv2IntraBlock* block, uint32_t direction, uint32_t top_right,
    uint32_t bottom_left, uint8_t* dst, uint32_t stride, int32_t* parameters) {
    if (recon == NULL || block == NULL || dst == NULL || recon->buffer == NULL ||
        block->plane < 1u || block->plane > 2u || direction > 2u ||
        (recon->bit_depth != 8u && recon->bit_depth != 10u) ||
        (recon->chroma_format != SVT_AV2_CHROMA_420 && recon->chroma_format != SVT_AV2_CHROMA_444))
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    const uint32_t sub = recon->chroma_format == SVT_AV2_CHROMA_420 ? 1u : 0u;
    const uint32_t bytes = recon->bit_depth > 8u ? 2u : 1u;
    const uint32_t plane_width = recon->visible_width >> sub, plane_height = recon->visible_height >> sub;
    if (block->w < 4u || block->h < 4u || block->w > 32u || block->h > 32u ||
        (block->x & 3u) || (block->y & 3u) ||
        block->x > plane_width || block->w > plane_width - block->x ||
        block->y > plane_height || block->h > plane_height - block->y ||
        !svt_av2_intra_mhccp_allowed(block->w << sub, block->h << sub, (uint32_t)recon->chroma_format) ||
        stride < block->w * bytes || top_right > block->w || bottom_left > block->h)
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    MhcReferences refs;
    int32_t params[3];
    uint16_t pixels[32u * 32u];
    collect(recon, block, top_right, bottom_left, &refs);
    derive(&refs, (int)recon->bit_depth, (int)direction, params);
    predict(&refs, (int)block->w, (int)block->h, (int)recon->bit_depth, (int)direction,
        params, pixels, (int)block->w);
    for (uint32_t y = 0u; y < block->h; ++y) for (uint32_t x = 0u; x < block->w; ++x)
        svt_av2_sample_store(dst + (size_t)y * stride + x * bytes, recon->bit_depth, pixels[y * block->w + x]);
    if (parameters != NULL) memcpy(parameters, params, sizeof(params));
    return SVT_AV2_ERROR_NONE;
}
