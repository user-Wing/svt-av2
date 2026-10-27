#ifndef M6_ROOT_ASYM
#define M6_ROOT_ASYM 0
#endif

/*
 * SVT-AV2 - M1 closed loop key frame writer (T005).
 * See tile_writer.h for the bit layout and the spec references.
 *
 * Layout note: the tile group payload is a header region written with the raw
 * BitWriter (is_first_tile_group + frame_header, then byte_alignment) followed
 * by the arithmetic-coded symbol bytes. The two regions are built in separate
 * buffers and concatenated because they use different coders; the boundary is
 * byte aligned by construction (byte_alignment() forces it).
 */
#include "tile_writer.h"

#include <stdlib.h>

#include "bit_writer.h"
#include "block_writer.h"
#include "entropy_encoder.h"
#include "entropy_models.h"
#include "coeff_syntax.h"
#include "cdf_tables.h"
#include "block_geometry.h"
#include "intra_mode_map.h"
#include "intra_prediction.h"
#include <string.h>

/* Forced BO4 profile can need 65 header bits before byte_alignment: 9 bytes. */
#define SVT_AV2_TILE_HEADER_MAX_BYTES 16u

/* Forward boundaries from AVM entropy_inits_modes.h, shared-plane rows. */
static const uint16_t split_cdf[64] = {
    28084,23755,23634,19368,24961,14941,16154,5905,
    21934,10440,11984,3474,20492,6963,8099,1529,
    24117,7871,23604,8429,27356,22441,8897,6811,
    17592,5648,5339,1082,26143,11379,20142,7401,
    26235,23674,12441,10482,20663,4192,5274,713,
    28255,27370,23527,20990,26727,21187,25324,17838,
    26136,16591,19838,10605,22914,12609,11341,4556,
    24218,13059,15378,5858,21644,7767,8309,1687
};
static const uint16_t rect_cdf[64] = {
    14644,10173,18529,16071,20263,12813,26612,23277,
    10594,7000,20002,12889,13854,10750,18380,17505,
    14430,11554,20078,19097,15278,10137,21921,14621,
    19330,15921,26218,24318,16384,16384,16384,16384,
    16384,16384,16384,16384,16066,9225,22849,14817,
    16384,16384,16384,16384,16384,16384,16384,16384,
    18543,13210,24367,18417,24701,18911,29590,27778,
    3400,935,10365,1723,16384,16384,16384,16384
};

typedef struct TileTree {
    SvtAv2EntropyEncoder* enc;
    SvtAv2FrameGeometry geometry;
    const SvtAv2TileCoeffBlock* blocks;
    size_t count, next;
    uint32_t chroma_shift, q_ctx;
    const SvtAv2TileBlockMode* modes;
    size_t mode_count;
    const uint16_t* partition_decisions;
    size_t partition_decision_count;
    uint32_t m5_intra_modes;
    uint32_t reduced_tx_set;
    uint32_t enable_mrls;
    uint32_t enable_mhccp;
    const SvtAv2Ccso64Plan* ccso64;
    uint16_t ccso_cdf[2][4][6]; /* Canonical inverse boundary, terminal, count, AVM_PARA2. */
    uint8_t mrl_above[SVT_AV2_COEFF_CTX_UNITS];
    uint8_t mrl_left[SVT_AV2_COEFF_CTX_UNITS];
    uint8_t m5_above_mode[SVT_AV2_COEFF_CTX_UNITS];
    uint8_t m5_left_mode[SVT_AV2_COEFF_CTX_UNITS];
    uint8_t cfl_above[SVT_AV2_COEFF_CTX_UNITS];
    uint8_t cfl_left[SVT_AV2_COEFF_CTX_UNITS];
    SvtAv2MotionSyntax motion;
    uint8_t above_partition[SVT_AV2_COEFF_CTX_UNITS];
    uint8_t left_partition[SVT_AV2_COEFF_CTX_UNITS];
    SvtAv2CoeffNeighbours neighbours[3];
} TileTree;

/* M5: the v1.0.0 decoder's joint-mode ordering, reduced to modes 0/1/2
 * that the current selector may emit (DC/V/H, angle delta zero).
 * See fixed reconintra.h: default_mode_list_y and reconintra.c:391. */
static const uint8_t m5_default_mode_list[56] = {
    17,45,3,10,24,31,38,52,15,19,43,47,1,5,8,12,
    22,26,29,33,36,40,50,54,16,18,44,46,2,4,9,11,
    23,25,30,32,37,39,51,53,14,20,42,48,0,6,7,13,
    21,27,28,34,35,41,49,55
};
static const uint16_t m5_y_cdf[3][7] = {
    {15175,20075,21728,24098,26405,27655,28860},
    {10114,14957,16815,19127,20147,25583,27169},
    {5636,9004,10456,12122,12744,20325,25607}
};
static const uint16_t m5_y_offset_cdf[3][5] = {
    {12743,18172,20194,23648,26419},
    {8976,16084,20827,24595,28496},
    {8784,14556,19710,24903,28724}
};
static void m5_add_joint(uint8_t* list, uint8_t* present, uint32_t* size,
                         uint32_t joint) {
    if (!present[joint]) {
        list[(*size)++] = (uint8_t)joint;
        present[joint] = 1u;
    }
}
static int m5_write_intra_y_mode(TileTree* tree, const SvtAv2BlockView* b,
                               uint32_t mode, uint32_t uv_mode) {
    if (mode > (tree->m5_intra_modes >= 3u ? 61u : 7u) || mode == 3u ||
        (uv_mode > 7u && !(tree->m5_intra_modes == 4u && uv_mode == 14u)) || uv_mode == 3u ||
        (tree->m5_intra_modes < 2u && (mode > 2u || uv_mode != 0u)))
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    const uint32_t x = b->x / 4u, y = b->y / 4u;
    const uint32_t w = b->w / 4u, h = b->h / 4u;
    if (w == 0u || h == 0u || x + w > SVT_AV2_COEFF_CTX_UNITS ||
        y + h > SVT_AV2_COEFF_CTX_UNITS)
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    const uint32_t left_mode = b->x ? tree->m5_left_mode[y + h - 1u] : 0u;
    const uint32_t above_mode = b->y ? tree->m5_above_mode[x + w - 1u] : 0u;
    const uint32_t left = svt_av2_intra_mode_joint(left_mode);
    const uint32_t above = svt_av2_intra_mode_joint(above_mode);
    const uint32_t context = (left >= 5u) + (above >= 5u);
    uint32_t directional[2] = {left, above};
    uint32_t count = context;
    if (count == 2u && left == above) count = 1u;
    if (count == 1u && left < 5u) directional[0] = above;
    uint8_t list[61] = {0}, present[61] = {0};
    uint32_t list_size = 0u;
    for (uint32_t j = 0; j < 5u; ++j)
        m5_add_joint(list, present, &list_size, j);
    if (b->w >= 8u && b->h >= 8u) {
        for (uint32_t j = 0; j < count; ++j)
            m5_add_joint(list, present, &list_size, directional[j]);
        if (b->w * b->h > 64u) {
            for (uint32_t offset = 0; offset < 4u; ++offset)
                for (uint32_t j = 0; j < count; ++j) {
                    const uint32_t n = directional[j];
                    m5_add_joint(list, present, &list_size,
                                 (n - offset + 50u) % 56u + 5u);
                    m5_add_joint(list, present, &list_size,
                                 (n + offset - 4u) % 56u + 5u);
                }
        }
    }
    for (uint32_t j = 0; j < 56u; ++j)
        m5_add_joint(list, present, &list_size, (uint32_t)m5_default_mode_list[j] + 5u);
    const uint32_t goal = svt_av2_intra_mode_joint(mode);
    uint32_t index = 0u;
    while (index < list_size && list[index] != goal) ++index;
    if (index >= list_size) return SVT_AV2_ENTROPY_ERROR_RANGE;

    const uint32_t set = index < 13u ? 0u : 1u + (index - 13u) / 16u;
    int st = svt_av2_entropy_encode_model(tree->enc, SVT_AV2_MODEL_Y_SET, svt_av2_cdf_y_mode_set, 4u, set);
    if (st != 0) return st;
    if (set == 0u) {
        const uint32_t symbol = index < 7u ? index : 7u;
        st = svt_av2_entropy_encode_model(tree->enc, SVT_AV2_MODEL_Y_IDX + context, m5_y_cdf[context], 8u, symbol);
        if (st == 0 && symbol == 7u)
            st = svt_av2_entropy_encode_model(tree->enc, SVT_AV2_MODEL_Y_OFFSET + context, m5_y_offset_cdf[context],
                                                6u, index - 7u);
    } else {
        st = svt_av2_entropy_encode_literal(tree->enc, (index - 13u) % 16u, 4);
    }
    const uint32_t mrl = tree->enable_mrls ? tree->blocks[tree->next].mrl : 0u;
    const uint32_t directional_mode = svt_av2_intra_mode_joint(mode) >= 5u;
    if (mrl > 7u || mrl == 4u || (mrl && (!tree->enable_mrls || !directional_mode ||
        (b->w!=b->h && b->w!=2u*b->h && b->h!=2u*b->w &&
            !((b->w==16u && b->h==64u) || (b->w==64u && b->h==16u))) ||
        b->w<8u || b->h<8u))) return SVT_AV2_ENTROPY_ERROR_RANGE;
    if (tree->enable_mrls && directional_mode) {
        static const uint16_t mrl_cdf[3][3] = {
            {29573u,31193u,32023u},{21812u,27066u,30279u},{16076u,23806u,28762u}};
        static const uint16_t multi_cdf[3] = {19678u,12287u,9574u};
        /* fetch_spatial_neighbors selects BL, AR, L, A and excludes above
         * at the SB top. With only one side it can count that side twice. */
        uint32_t neighbors[2] = {0u,0u}, neighbor_count = 0u;
        if (b->x) neighbors[neighbor_count++] = tree->mrl_left[y+h-1u];
        if (b->y % 64u && neighbor_count < 2u) neighbors[neighbor_count++] = tree->mrl_above[x+w-1u];
        if (b->x && neighbor_count < 2u) neighbors[neighbor_count++] = tree->mrl_left[y];
        if (b->y % 64u && neighbor_count < 2u) neighbors[neighbor_count++] = tree->mrl_above[x];
        const uint32_t ctx = ((neighbors[0]&3u)!=0u) + ((neighbors[1]&3u)!=0u);
        st = svt_av2_entropy_encode_model(tree->enc,SVT_AV2_MODEL_MRL+ctx,mrl_cdf[ctx],4u,mrl&3u);
        if (st == 0 && (mrl&3u)) {
            const uint32_t multi_ctx = (neighbors[0]>>2u) + (neighbors[1]>>2u);
            st = svt_av2_entropy_encode_model(tree->enc,SVT_AV2_MODEL_MULTI_MRL+multi_ctx,
                multi_cdf+multi_ctx,2u,mrl>>2u);
        }
        if (st != 0) return st;
    }
    memset(tree->mrl_above+x,(int)mrl,w);
    memset(tree->mrl_left+y,(int)mrl,h);
    return st;
}

static int m5_write_intra_uv_mode(TileTree* tree,const SvtAv2BlockView* b,
    uint32_t mode,uint32_t uv_mode) {
    const uint32_t x=b->x/4u,y=b->y/4u,w=b->w/4u,h=b->h/4u;
    int st=0;
    /* AVM get_uv_intra_mode_set: directional co-located Y first, followed by
     * DC/SMOOTH/SMOOTH_V/SMOOTH_H/PAETH and the remaining V/H/... modes. */
    const uint32_t base_mode = svt_av2_intra_mode_base(mode);
    const uint32_t uv_context = svt_av2_intra_mode_joint(mode) >= 5u;
    uint32_t uv_index = uv_mode == 0u ? 0u : uv_mode >= 4u ? uv_mode - 3u : uv_mode + 4u;
    if (uv_context) {
        if (uv_mode == base_mode) uv_index = 0u;
        else if (uv_mode == 2u && base_mode == 1u) uv_index = 6u;
        else ++uv_index;
    }
    if (st == 0 && tree->m5_intra_modes == 4u) {
        static const uint16_t cfl_cdf[3] = {20441u,11610u,4643u};
        const uint32_t cfl_context = (b->x ? tree->cfl_left[y] : 0u) + (b->y ? tree->cfl_above[x] : 0u);
        st = svt_av2_entropy_encode_model(tree->enc, SVT_AV2_MODEL_CFL + cfl_context,
            cfl_cdf + cfl_context, 2u, uv_mode == 14u);
        if (st == 0 && uv_mode == 14u) {
            static const uint16_t cfl_index_cdf[1] = {12507u};
            const SvtAv2TileCoeffBlock* input = tree->blocks + tree->next;
            const int mhccp_allowed = tree->enable_mhccp &&
                svt_av2_intra_mhccp_allowed(b->w,b->h,tree->geometry.chroma_format);
            if (input->cfl_explicit > 1u || input->mhccp > 3u ||
                (input->mhccp && (!mhccp_allowed || input->cfl_explicit))) return SVT_AV2_ENTROPY_ERROR_RANGE;
            if (mhccp_allowed) {
                static const uint16_t mhccp_cdf[1] = {15499u};
                st = svt_av2_entropy_encode_model(tree->enc,SVT_AV2_MODEL_MHCCP,mhccp_cdf,2u,input->mhccp != 0u);
            }
            if (st == 0 && input->mhccp) {
                static const uint16_t dir_cdf[4][2] = {{10923u,21845u},{8795u,15105u},{10433u,15974u},{17085u,21689u}};
                const uint32_t group = svt_av2_intra_mhccp_size_group(b->w,b->h);
                st = svt_av2_entropy_encode_model(tree->enc,SVT_AV2_MODEL_MHCCP_DIR+group,dir_cdf[group],3u,input->mhccp-1u);
            } else if (st == 0) st = svt_av2_entropy_encode_model(tree->enc, SVT_AV2_MODEL_CFL_INDEX,
                cfl_index_cdf, 2u, input->cfl_explicit ? 0u : 1u);
            if (st == 0 && input->cfl_explicit) {
                static const uint16_t sign_cdf[7] = {2421u,4332u,11256u,12766u,21386u,28725u,32087u};
                static const uint16_t alpha_cdf[6][7] = {
                    {21679u,25305u,30646u,31512u,32537u,32646u,32696u},
                    {8262u,16302u,24082u,29422u,31398u,32286u,32525u},
                    {17235u,26166u,30378u,31305u,32373u,32549u,32668u},
                    {17618u,25732u,27865u,30338u,31125u,31522u,32238u},
                    {17542u,23066u,27907u,28728u,30702u,31165u,31435u},
                    {17675u,24802u,30468u,30783u,31841u,32264u,32422u}
                };
                const uint32_t joint=input->cfl_alpha_signs, idx=input->cfl_alpha_idx;
                if (joint >= 8u || idx >= 64u) return SVT_AV2_ENTROPY_ERROR_RANGE;
                const uint32_t usign=(joint+1u)/3u, vsign=(joint+1u)%3u;
                if ((!usign && (idx>>3u)) || (!vsign && (idx&7u)))
                    return SVT_AV2_ENTROPY_ERROR_RANGE;
                st=svt_av2_entropy_encode_model(tree->enc,SVT_AV2_MODEL_CFL_SIGN,sign_cdf,8u,joint);
                if (st==0 && usign) {
                    const uint32_t ctx=joint+1u-3u;
                    st=svt_av2_entropy_encode_model(tree->enc,SVT_AV2_MODEL_CFL_ALPHA+ctx,alpha_cdf[ctx],8u,idx>>3u);
                }
                if (st==0 && vsign) {
                    const uint32_t ctx=vsign*3u+usign-3u;
                    st=svt_av2_entropy_encode_model(tree->enc,SVT_AV2_MODEL_CFL_ALPHA+ctx,alpha_cdf[ctx],8u,idx&7u);
                }
            }
        }
    }
    if (st == 0 && uv_mode != 14u)
        st = svt_av2_entropy_encode_model(tree->enc, SVT_AV2_MODEL_UV + uv_context,
            uv_context ? svt_av2_cdf_uv_mode_cfl_not_allowed_1 :
                          svt_av2_cdf_uv_mode_cfl_not_allowed_0, 8u, uv_index);
    if (st == 0 && uv_mode != 14u && uv_index == 7u) st = svt_av2_entropy_encode_literal(tree->enc, 0u, 3);
    if (st == 0) {
        memset(tree->m5_above_mode + x, (int)mode, w);
        memset(tree->m5_left_mode + y, (int)mode, h);
        if (tree->m5_intra_modes == 4u) {
            memset(tree->cfl_above + x, uv_mode == 14u, w);
            memset(tree->cfl_left + y, uv_mode == 14u, h);
        }
    }
    return st;
}

static int m5_write_intra_mode(TileTree* tree,const SvtAv2BlockView* b,
    uint32_t mode,uint32_t uv_mode) {
    const int st=m5_write_intra_y_mode(tree,b,mode,uv_mode);
    return st==0?m5_write_intra_uv_mode(tree,b,mode,uv_mode):st;
}

static uint32_t log2_dim(uint32_t d) {
    uint32_t n = 0;
    while ((1u << n) < d) ++n;
    return n;
}

static int tile_partition_write(TileTree* tree, const SvtAv2BlockView* b,
    SvtAv2PartitionType part, int implied) {
    const uint32_t lw = log2_dim(b->w), lh = log2_dim(b->h);
    const uint32_t above = (tree->above_partition[b->x / 4u] >> (lw - 3u)) & 1u;
    const uint32_t left = (tree->left_partition[b->y / 4u] >> (lh - 3u)) & 1u;
    const uint32_t raw = 2u * left + above;
    int st = 0;
    if (!implied) {
        const int aspect4 = b->w == 4u * b->h || b->h == 4u * b->w;
        const uint32_t size_map = aspect4 ?
            10u + 2u * ((lw < lh ? lw : lh) - 2u) + (b->w > b->h) : (lw > lh ? lw : lh) - 3u;
        /* Missing half-block rows/columns forbid NONE. In that case do_split
         * is implied even when the binary orientation itself is signaled. */
        if (b->x + b->w / 2u < tree->geometry.visible_width &&
            b->y + b->h / 2u < tree->geometry.visible_height) {
            st = svt_av2_entropy_encode_model(tree->enc, SVT_AV2_MODEL_SPLIT + size_map * 4u + raw, split_cdf + size_map * 4u + raw,
                                              2u, part != SVT_AV2_PARTITION_NONE);
        }
        if (st != 0) return st;
        if (part != SVT_AV2_PARTITION_NONE) {
            uint32_t rect_map;
            if (aspect4) rect_map = b->h > b->w ? 13u : 14u;
            else if (b->w == b->h) rect_map = b->w == 64u ? 6u : (b->w == 32u ? 3u : 0u);
            else if (b->h > b->w) rect_map = b->w == 32u ? 4u : 1u;
            else rect_map = b->h == 32u ? 5u : 2u;
            st = svt_av2_entropy_encode_model(tree->enc, SVT_AV2_MODEL_RECT + rect_map * 4u + raw, rect_cdf + rect_map * 4u + raw,
                                              2u, part == SVT_AV2_PARTITION_VERT);
            if (st != 0) return st;
        }
    }
    return 0;
}

static int tile_coeff_plane_write(TileTree* tree,const SvtAv2BlockView* b,
    uint32_t p,uint32_t is_inter,uint32_t skip_txfm,uint32_t m5_mode,uint8_t* u_nonzero) {
    int st=0;
    const SvtAv2TileCoeffBlock* input = tree->blocks + tree->next++;
    const uint32_t width = b->w >> (p == 0u ? 0u : tree->chroma_shift);
    const uint32_t height = b->h >> (p == 0u ? 0u : tree->chroma_shift);
    const uint32_t x = b->x >> (p == 0u ? 0u : tree->chroma_shift);
    const uint32_t y = b->y >> (p == 0u ? 0u : tree->chroma_shift);
    if (input->plane != p || input->x != x || input->y != y ||
        input->width != width || input->height != height || input->qcoeff == NULL)
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    SvtAv2TxSize tx_size;
    if (svt_av2_coeff_tx_size_from_dims(width,height,&tx_size) != 0)
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    SvtAv2CoeffPlaneInfo pi = {p, tx_size, x / 4u, y / 4u,
        tree->geometry.visible_width / (4u << (p == 0u ? 0u : tree->chroma_shift)),
        tree->geometry.visible_height / (4u << (p == 0u ? 0u : tree->chroma_shift)), width, height};
#if M6_FLIP_TX
    if (p==0u) m5_mode=svt_av2_intra_tx_mode(input->intra_mode,width,height,input->mrl);
#endif
    if (skip_txfm) {
        memset(tree->neighbours[p].above_level + x/4u, 0, width/4u);
        memset(tree->neighbours[p].above_dc + x/4u, 0, width/4u);
        memset(tree->neighbours[p].left_level + y/4u, 0, height/4u);
        memset(tree->neighbours[p].left_dc + y/4u, 0, height/4u);
    } else st = svt_av2_coeff_write_block_transform_intra(tree->enc, &pi, *u_nonzero, input->qcoeff, tree->neighbours + p, tree->q_ctx, is_inter, tree->reduced_tx_set, tree->reduced_tx_set <= 1u ? input->tx_type : 0u, m5_mode);
    if (st != SVT_AV2_COEFF_OK) {
        st = st == SVT_AV2_COEFF_ERR_NO_SPACE ? SVT_AV2_ENTROPY_ERROR_NO_SPACE
                                             : SVT_AV2_ENTROPY_ERROR_RANGE;
    }
    if (p == 1u) {
        const size_t count = svt_av2_coeff_coeff_count(pi.tx_size);
        for (size_t c = 0; c < count; ++c) *u_nonzero |= input->qcoeff[c] != 0;
    }
    return st;
}

/* Inline rows follow every TileTree trial/commit; failed trials retain no updates. */
static void tile_ccso64_init(TileTree* tree, const SvtAv2Ccso64Plan* plan) {
    tree->ccso64=plan;
    if (plan) svt_av2_ccso64_cdf_init(tree->ccso_cdf);
}

static int tile_ccso64_write(TileTree* tree, const SvtAv2BlockView* b) {
    if (tree->ccso64 == NULL || (b->x & 63u) || (b->y & 63u)) return 0;
    const size_t unit = (size_t)(b->y / 64u) * tree->geometry.sb_cols + b->x / 64u;
    const SvtAv2Ccso64Plan* plan = tree->ccso64;
    const uint8_t* flags = plan->unit_flags ? plan->unit_flags[unit] : plan->block_flag;
    for (uint32_t p = 0u; p < 2u; ++p) if (plan->enabled[p]) {
        const uint32_t context=svt_av2_ccso64_context(plan,unit,tree->geometry.sb_cols,p);
        const int st=svt_av2_ccso64_symbol(tree->enc,tree->ccso_cdf[p][context],flags[p],
            tree->enc->models!=NULL);
        if (st != 0) return st;
    }
    return 0;
}

static int tile_leaf_write(TileTree* tree, const SvtAv2BlockView* b) {
    int st=0;
    if (tree->next + 3u > tree->count) return SVT_AV2_ENTROPY_ERROR_RANGE;
    uint32_t is_inter = 0, skip_txfm = 0;
    if (tree->modes) {
        const size_t idx = tree->next / 3u;
        if (idx >= tree->mode_count) return SVT_AV2_ENTROPY_ERROR_RANGE;
        is_inter = tree->modes[idx].is_inter;
        skip_txfm = is_inter;
        for (uint32_t p = 0; p < 3u; ++p) {
            const SvtAv2TileCoeffBlock* in = tree->blocks + tree->next + p;
            if (in->qcoeff == NULL) return SVT_AV2_ENTROPY_ERROR_NULL;
            const uint32_t dim = in->width < 32u ? in->width : 32u;
            const uint32_t height = in->height < 32u ? in->height : 32u;
            for (uint32_t c = 0; c < dim * height; ++c) if (in->qcoeff[c] != 0) skip_txfm = 0;
        }
        st = svt_av2_motion_write_block(&tree->motion, tree->enc, b, tree->modes + idx, skip_txfm);
    }
    /* Fixed AVM blockd.c av2_get_joint_mode: an inter neighbour contributes
     * DC to intra mode ordering, replacing any earlier directional block. */
    if (is_inter && tree->m5_intra_modes && st == 0) {
        memset(tree->m5_above_mode + b->x / 4u, 0, b->w / 4u);
        memset(tree->m5_left_mode + b->y / 4u, 0, b->h / 4u);
    }
    if (st == 0) st = tile_ccso64_write(tree, b);
    uint32_t m5_mode = 0u;
    if (!is_inter && tree->m5_intra_modes) {
        /* M5 syntax is enabled explicitly by tile tree configuration. */
        m5_mode = tree->blocks[tree->next].intra_mode;
        if (m5_mode > (tree->m5_intra_modes >= 3u ? 61u : 7u) || m5_mode == 3u || (m5_mode > 2u && tree->m5_intra_modes < 2u))
            return SVT_AV2_ENTROPY_ERROR_RANGE;
    }
    if (!is_inter && tree->m5_intra_modes >= 2u &&
        tree->blocks[tree->next + 1u].intra_mode != tree->blocks[tree->next + 2u].intra_mode)
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    if (!is_inter && tree->m5_intra_modes && st == 0)
        st = m5_write_intra_mode(tree, b, m5_mode,
            tree->m5_intra_modes >= 2u ? tree->blocks[tree->next + 1u].intra_mode : 0u);
    if (!is_inter && !tree->m5_intra_modes && st == 0)
        st = svt_av2_entropy_encode_model(tree->enc, SVT_AV2_MODEL_Y_SET, svt_av2_cdf_y_mode_set, 4u, 0u);
    /* AVM alphabets have eight symbols; the M1 seven-symbol approximation is
     * retained only in block_write_minimal_intra's frozen regression path. */
    if (!is_inter && !tree->m5_intra_modes && st == 0)
        st = svt_av2_entropy_encode_model(tree->enc, SVT_AV2_MODEL_Y_IDX, svt_av2_cdf_y_mode_index_0, 8u, 0u);
    if (!is_inter && !tree->m5_intra_modes && st == 0) st = svt_av2_entropy_encode_model(tree->enc, SVT_AV2_MODEL_UV + (m5_mode != 0u),
        m5_mode ? svt_av2_cdf_uv_mode_cfl_not_allowed_1 : svt_av2_cdf_uv_mode_cfl_not_allowed_0,
        8u, m5_mode ? 1u : 0u);
    if (!is_inter && tree->modes && m5_mode == 0u && st == 0) {
        const uint16_t palette_cdf = 30045u;
        st = svt_av2_entropy_encode_model(tree->enc, SVT_AV2_MODEL_PALETTE, &palette_cdf, 2u, 0u);
    }
    uint8_t u_nonzero = 0u;
    for (uint32_t p = 0; p < 3u && st == 0; ++p)
        st=tile_coeff_plane_write(tree,b,p,is_inter,skip_txfm,m5_mode,&u_nonzero);
    memset(tree->above_partition + b->x / 4u, (int)(64u - b->w / 4u), b->w / 4u);
    memset(tree->left_partition + b->y / 4u, (int)(64u - b->h / 4u), b->h / 4u);
    return st;
}

static int tile_tree_write(TileTree* tree, const SvtAv2BlockView* b) {
    if (b->x >= tree->geometry.visible_width || b->y >= tree->geometry.visible_height) return 0;
    SvtAv2PartitionType part;
    int implied;
    if (svt_av2_partition_coded_type_plan_validated(&tree->geometry, b,
        tree->partition_decisions, &part, &implied) != 0)
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    int st=tile_partition_write(tree,b,part,implied);
    if (st!=0) return st;
    if (part != SVT_AV2_PARTITION_NONE) {
        SvtAv2BlockView child = *b;
        if (part == SVT_AV2_PARTITION_HORZ) child.h /= 2u;
        else if (part == SVT_AV2_PARTITION_VERT) child.w /= 2u;
        else return SVT_AV2_ENTROPY_ERROR_RANGE;
        st = tile_tree_write(tree, &child);
        if (st != 0) return st;
        if (part == SVT_AV2_PARTITION_HORZ) child.y += child.h;
        else child.x += child.w;
        return tile_tree_write(tree, &child);
    }
    return tile_leaf_write(tree,b);
}

static int tile_write_coeffs(SvtAv2EntropyEncoder* enc, const SvtAv2FrameHeaderConfig* cfg,
                            const SvtAv2TileCoeffBlock* blocks, size_t count,
                            const SvtAv2TileBlockMode* modes, size_t mode_count) {
    if (svt_av2_ccso64_validate(cfg->ccso64, cfg->frame_width, cfg->frame_height,
        cfg->bit_depth, cfg->chroma_format) != 0 ||
        (cfg->ccso64 && (cfg->multi_frame || cfg->inter_frame || cfg->sb_shift != 4u)))
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    if ((cfg->base_q_idx < (cfg->bit_depth == 10u ? 49u : 1u) || cfg->base_q_idx > (cfg->bit_depth == 10u ? 303u : 255u)) || cfg->frame_width > 1920u || cfg->frame_height > 1080u)
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    TileTree tree;
    memset(&tree, 0, sizeof(tree));
    tree.enc = enc; tree.blocks = blocks; tree.count = count;
    tree.modes = modes; tree.mode_count = mode_count;
    tree.partition_decisions = cfg->partition_decisions;
    tree.partition_decision_count = cfg->partition_decision_count;
    tree.m5_intra_modes = cfg->m5_intra_modes;
    tree.reduced_tx_set = cfg->reduced_tx_set;
    tree.enable_mrls = cfg->enable_mrls;
    tree.enable_mhccp = cfg->enable_mhccp;
    tile_ccso64_init(&tree, cfg->ccso64);
    tree.chroma_shift = cfg->chroma_format == 3u ? 0u : 1u;
    tree.q_ctx = svt_av2_coeff_q_context(cfg->base_q_idx);
    if (svt_av2_frame_geometry_init_ex(&tree.geometry, cfg->frame_width, cfg->frame_height, 64u,
        (SvtAv2ChromaFormat)cfg->chroma_format) != 0)
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    if (svt_av2_partition_plan_validate(&tree.geometry,
        tree.partition_decisions, tree.partition_decision_count) != 0)
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    if (modes && svt_av2_motion_syntax_init(&tree.motion, cfg->frame_width, cfg->frame_height) != 0)
        return SVT_AV2_ENTROPY_ERROR_NO_SPACE;
    for (uint32_t y = 0; y < cfg->frame_height; y += 64u) {
        for (uint32_t x = 0; x < cfg->frame_width; x += 64u) {
            SvtAv2BlockView sb = {0};
            sb.x = x; sb.y = y; sb.w = sb.h = 64u;
            const int st = tile_tree_write(&tree, &sb);
            if (st != 0) { svt_av2_motion_syntax_free(&tree.motion); return st; }
        }
    }
    svt_av2_motion_syntax_free(&tree.motion);
    return tree.next == count ? 0 : SVT_AV2_ENTROPY_ERROR_RANGE;
}

struct SvtAv2TileRateSession {
    TileTree tree;
    uint16_t* plan;
    size_t cursor, sb_count;
    uint32_t inter_frame;
    SvtAv2MotionCell saved_motion[16u * 16u];
    uint32_t adaptive;
    SvtAv2EntropyModels committed_models, scratch_models;
};

int svt_av2_tile_rate_create(const SvtAv2FrameHeaderConfig* cfg,
                             SvtAv2TileRateSession** session) {
    if (session == NULL) return SVT_AV2_ENTROPY_ERROR_NULL;
    *session = NULL;
    if (cfg == NULL) return SVT_AV2_ENTROPY_ERROR_NULL;
    if (svt_av2_ccso64_validate(cfg->ccso64, cfg->frame_width, cfg->frame_height,
        cfg->bit_depth, cfg->chroma_format) != 0 ||
        (cfg->ccso64 && (cfg->multi_frame || cfg->inter_frame || cfg->sb_shift != 4u)))
        return SVT_AV2_ENTROPY_ERROR_RANGE;

    if (cfg->entropy_adaptation > 1u ||
        (cfg->entropy_adaptation && (cfg->multi_frame || cfg->inter_frame)))
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    const uint32_t qmin = cfg->bit_depth == 10u ? 49u : 1u;
    const uint32_t qmax = cfg->bit_depth == 10u ? 303u : 255u;
    if (cfg->base_q_idx < qmin || cfg->base_q_idx > qmax ||
        cfg->frame_width > 1920u || cfg->frame_height > 1080u ||
        (cfg->bit_depth != 8u && cfg->bit_depth != 10u) ||
        (cfg->chroma_format != 1u && cfg->chroma_format != 3u) ||
        (cfg->bit_depth == 8u && cfg->chroma_format == 3u) || cfg->inter_frame > 1u)
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    SvtAv2TileRateSession* s = (SvtAv2TileRateSession*)calloc(1u, sizeof(*s));
    if (s == NULL) return SVT_AV2_ENTROPY_ERROR_NO_SPACE;
    if (svt_av2_frame_geometry_init_ex(&s->tree.geometry, cfg->frame_width,
        cfg->frame_height, 64u, (SvtAv2ChromaFormat)cfg->chroma_format) != 0) {
        free(s); return SVT_AV2_ENTROPY_ERROR_RANGE;
    }
    s->sb_count = (size_t)s->tree.geometry.sb_cols * s->tree.geometry.sb_rows;
    s->plan = (uint16_t*)calloc(s->sb_count, sizeof(*s->plan));
    if (s->plan == NULL) { free(s); return SVT_AV2_ENTROPY_ERROR_NO_SPACE; }
    s->tree.partition_decisions = s->plan;
    s->tree.partition_decision_count = s->sb_count;
    s->tree.m5_intra_modes = cfg->m5_intra_modes;
    s->tree.reduced_tx_set = cfg->reduced_tx_set;
    s->tree.enable_mrls = cfg->enable_mrls;
    s->tree.enable_mhccp = cfg->enable_mhccp;
    tile_ccso64_init(&s->tree, cfg->ccso64);
    s->tree.chroma_shift = cfg->chroma_format == 3u ? 0u : 1u;
    s->tree.q_ctx = svt_av2_coeff_q_context(cfg->base_q_idx);
    s->inter_frame = cfg->inter_frame;
    s->adaptive = cfg->entropy_adaptation;
    if (s->adaptive && svt_av2_entropy_models_init(&s->committed_models, s->tree.q_ctx) != 0) {
        svt_av2_tile_rate_free(s); return SVT_AV2_ENTROPY_ERROR_RANGE;
    }
    if (s->inter_frame && svt_av2_motion_syntax_init(&s->tree.motion,
        cfg->frame_width, cfg->frame_height) != 0) {
        svt_av2_tile_rate_free(s); return SVT_AV2_ENTROPY_ERROR_NO_SPACE;
    }
    *session = s;
    return 0;
}

void svt_av2_tile_rate_free(SvtAv2TileRateSession* session) {
    if (session == NULL) return;
    svt_av2_motion_syntax_free(&session->tree.motion);
    free(session->plan);
    free(session);
}

int svt_av2_tile_rate_trial(SvtAv2TileRateSession* s, size_t sb_index,
    uint16_t partition, const SvtAv2TileCoeffBlock* blocks, size_t block_count,
    const SvtAv2TileBlockMode* modes, size_t mode_count, int commit,
    uint64_t* rate_q12) {
    if (s == NULL || blocks == NULL || rate_q12 == NULL)
        return SVT_AV2_ENTROPY_ERROR_NULL;
    *rate_q12 = 0u;
    if (sb_index != s->cursor || sb_index >= s->sb_count || partition > (M6_ROOT_ASYM ? 3288u : 3284u) ||
        (commit != 0 && commit != 1) || block_count == 0u || block_count % 3u ||
        (s->inter_frame ? modes == NULL || mode_count != block_count / 3u :
                         modes != NULL || mode_count != 0u))
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    const uint16_t old_plan = s->plan[sb_index];
    s->plan[sb_index] = partition;
    if (svt_av2_partition_plan_validate_sb(&s->tree.geometry, sb_index, partition) != 0) {
        s->plan[sb_index] = old_plan;
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    }
    SvtAv2BlockView sb = {0};
    sb.x = (uint32_t)(sb_index % s->tree.geometry.sb_cols) * 64u;
    sb.y = (uint32_t)(sb_index / s->tree.geometry.sb_cols) * 64u;
    sb.w = sb.h = 64u;
    const uint32_t x4 = sb.x / 4u, y4 = sb.y / 4u;
    uint32_t width4 = 0u, height4 = 0u;
    if (s->inter_frame) {
        width4 = s->tree.motion.cols - x4;
        height4 = s->tree.motion.rows - y4;
        if (width4 > 16u) width4 = 16u;
        if (height4 > 16u) height4 = 16u;
        for (uint32_t row = 0u; row < height4; ++row)
            memcpy(s->saved_motion + row * 16u,
                s->tree.motion.cells + (size_t)(y4 + row) * s->tree.motion.cols + x4,
                width4 * sizeof(*s->saved_motion));
    }
    TileTree trial = s->tree;
    SvtAv2EntropyEncoder enc;
    svt_av2_entropy_rate_init(&enc);
    if (s->adaptive) {
        /* Every candidate starts from the committed prefix. Failed and losing
         * trials cannot mutate it, including equal raw CDF boundaries. */
        s->scratch_models = s->committed_models;
        enc.models = &s->scratch_models;
    }
    trial.enc = &enc;
    trial.blocks = blocks; trial.count = block_count; trial.next = 0u;
    trial.modes = modes; trial.mode_count = mode_count;
    int st = tile_tree_write(&trial, &sb);
    if (st == 0 && trial.next != block_count) st = SVT_AV2_ENTROPY_ERROR_RANGE;
    if (st == 0) *rate_q12 = enc.rate_q12;
    if (st == 0 && commit) {
        if (s->adaptive) s->committed_models = s->scratch_models;
        s->tree = trial;
        s->tree.enc = NULL; s->tree.blocks = NULL; s->tree.modes = NULL;
        ++s->cursor;
    } else {
        s->plan[sb_index] = old_plan;
        for (uint32_t row = 0u; row < height4; ++row)
            memcpy(s->tree.motion.cells + (size_t)(y4 + row) * s->tree.motion.cols + x4,
                s->saved_motion + row * 16u, width4 * sizeof(*s->saved_motion));
    }
    return st;
}

/* Rate-only intra transaction. Partition symbols are consumed in the original
 * preorder; partition neighbours change only after a complete coding leaf. */
#define TILE_PREFIX_NODES 127u
#define TILE_PREFIX_SBS 510u
typedef struct TilePrefixEvent {
    SvtAv2BlockView block;
    SvtAv2PartitionType part;
    int implied;
} TilePrefixEvent;

struct SvtAv2TileRatePrefix {
    TileTree tree, trial;
    SvtAv2EntropyEncoder enc;
    SvtAv2EntropyModels models, scratch_models;
    TilePrefixEvent events[TILE_PREFIX_NODES], pending[TILE_PREFIX_NODES];
    uint16_t plan[TILE_PREFIX_SBS];
    size_t count, pos, leaf, leaves;
    uint32_t adaptive, active;
};

static int tile_prefix_flatten(const SvtAv2FrameGeometry* geometry,
    const uint16_t* plan, const SvtAv2BlockView* b, TilePrefixEvent* events,
    size_t* count, size_t* leaves) {
    if (b->x>=geometry->visible_width || b->y>=geometry->visible_height) return 0;
    if (*count>=TILE_PREFIX_NODES) return SVT_AV2_ENTROPY_ERROR_RANGE;
    TilePrefixEvent* event=events+*count;
    event->block=*b;
    if (svt_av2_partition_coded_type_plan_validated(geometry,b,plan,
        &event->part,&event->implied)!=0) return SVT_AV2_ENTROPY_ERROR_RANGE;
    ++*count;
    if (event->part==SVT_AV2_PARTITION_NONE) {++*leaves;return 0;}
    SvtAv2BlockView child=*b;
    if (event->part==SVT_AV2_PARTITION_HORZ) child.h/=2u;
    else if (event->part==SVT_AV2_PARTITION_VERT) child.w/=2u;
    else return SVT_AV2_ENTROPY_ERROR_RANGE;
    int st=tile_prefix_flatten(geometry,plan,&child,events,count,leaves);
    if (st!=0) return st;
    if (event->part==SVT_AV2_PARTITION_HORZ) child.y+=child.h;
    else child.x+=child.w;
    return tile_prefix_flatten(geometry,plan,&child,events,count,leaves);
}

static int tile_prefix_advance(TileTree* tree, const TilePrefixEvent* events,
    size_t count, size_t* pos) {
    while (*pos<count) {
        const TilePrefixEvent* event=events+(*pos)++;
        const int st=tile_partition_write(tree,&event->block,event->part,event->implied);
        if (st!=0) return st;
        if (event->part==SVT_AV2_PARTITION_NONE) break;
    }
    return 0;
}

int svt_av2_tile_rate_prefix_create(SvtAv2TileRatePrefix** prefix) {
    if (prefix==NULL) return SVT_AV2_ENTROPY_ERROR_NULL;
    *prefix=(SvtAv2TileRatePrefix*)calloc(1u,sizeof(**prefix));
    return *prefix!=NULL?0:SVT_AV2_ENTROPY_ERROR_NO_SPACE;
}

void svt_av2_tile_rate_prefix_free(SvtAv2TileRatePrefix* prefix) {free(prefix);}

int svt_av2_tile_rate_prefix_begin(SvtAv2TileRatePrefix* p,
    const SvtAv2TileRateSession* s, size_t sb_index, uint16_t partition,
    size_t* leaf_count) {
    if (p==NULL || s==NULL || leaf_count==NULL) return SVT_AV2_ENTROPY_ERROR_NULL;
    *leaf_count=0u;
    if (s->inter_frame || sb_index!=s->cursor || sb_index>=s->sb_count ||
        s->sb_count>TILE_PREFIX_SBS ||
        svt_av2_partition_plan_validate_sb(&s->tree.geometry,sb_index,partition)!=0)
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    memcpy(p->plan,s->plan,s->sb_count*sizeof(*p->plan));
    p->plan[sb_index]=partition;
    SvtAv2BlockView sb={0};
    sb.x=(uint32_t)(sb_index%s->tree.geometry.sb_cols)*64u;
    sb.y=(uint32_t)(sb_index/s->tree.geometry.sb_cols)*64u;sb.w=sb.h=64u;
    size_t count=0u,leaves=0u,pos=0u;
    int st=tile_prefix_flatten(&s->tree.geometry,p->plan,&sb,p->pending,&count,&leaves);
    if (st!=0 || leaves==0u) return SVT_AV2_ENTROPY_ERROR_RANGE;
    p->trial=s->tree;
    p->trial.partition_decisions=NULL;p->trial.partition_decision_count=0u;
    p->trial.blocks=NULL;p->trial.count=p->trial.next=0u;
    p->trial.modes=NULL;p->trial.mode_count=0u;
    SvtAv2EntropyEncoder enc;
    svt_av2_entropy_rate_init(&enc);
    if (s->adaptive) {p->scratch_models=s->committed_models;enc.models=&p->scratch_models;}
    p->trial.enc=&enc;
    st=tile_prefix_advance(&p->trial,p->pending,count,&pos);
    if (st!=0) return st;
    p->tree=p->trial;p->tree.enc=NULL;
    p->enc=enc;p->adaptive=s->adaptive;
    if (p->adaptive) {p->models=p->scratch_models;p->enc.models=&p->models;}
    memcpy(p->events,p->pending,count*sizeof(*p->events));
    p->count=count;p->pos=pos;p->leaf=0u;p->leaves=leaves;p->active=1u;
    *leaf_count=leaves;
    return 0;
}

int svt_av2_tile_rate_prefix_trial(SvtAv2TileRatePrefix* p, size_t leaf,
    const SvtAv2TileCoeffBlock blocks[3], int commit, uint64_t* rate_q12) {
    if (p==NULL || blocks==NULL || rate_q12==NULL) return SVT_AV2_ENTROPY_ERROR_NULL;
    *rate_q12=0u;
    if (!p->active || leaf!=p->leaf || leaf>=p->leaves || p->pos==0u ||
        (commit!=0 && commit!=1)) return SVT_AV2_ENTROPY_ERROR_RANGE;
    p->trial=p->tree;
    SvtAv2EntropyEncoder enc=p->enc;
    if (p->adaptive) {p->scratch_models=p->models;enc.models=&p->scratch_models;}
    p->trial.enc=&enc;p->trial.blocks=blocks;p->trial.count=3u;p->trial.next=0u;
    int st=tile_leaf_write(&p->trial,&p->events[p->pos-1u].block);
    if (st==0 && p->trial.next!=3u) st=SVT_AV2_ENTROPY_ERROR_RANGE;
    const uint64_t candidate_rate=enc.rate_q12;
    size_t pos=p->pos;
    if (st==0 && commit) st=tile_prefix_advance(&p->trial,p->events,p->count,&pos);
    if (st!=0) return st;
    if (commit) {
        p->tree=p->trial;p->tree.enc=NULL;p->tree.blocks=NULL;
        p->tree.count=p->tree.next=0u;
        p->enc=enc;
        if (p->adaptive) {p->models=p->scratch_models;p->enc.models=&p->models;}
        p->pos=pos;++p->leaf;
    }
    *rate_q12=candidate_rate;
    return 0;
}

/* Rate-only luma subset. UV syntax uses disjoint model rows, so omitting
 * it cannot alter Y coefficient costs. Never commit or advance this subset. */
int svt_av2_tile_rate_prefix_trial_y(SvtAv2TileRatePrefix* p,size_t leaf,
    const SvtAv2TileCoeffBlock* y,uint64_t* rate_q12) {
    if (p==NULL || y==NULL || rate_q12==NULL) return SVT_AV2_ENTROPY_ERROR_NULL;
    *rate_q12=0u;
    if (!p->active || leaf!=p->leaf || leaf>=p->leaves || p->pos==0u ||
        p->tree.m5_intra_modes<3u) return SVT_AV2_ENTROPY_ERROR_RANGE;
    p->trial=p->tree;
    SvtAv2EntropyEncoder enc=p->enc;
    if (p->adaptive) {p->scratch_models=p->models;enc.models=&p->scratch_models;}
    p->trial.enc=&enc;p->trial.blocks=y;p->trial.count=1u;p->trial.next=0u;
    const SvtAv2BlockView* b=&p->events[p->pos-1u].block;
    int st=tile_ccso64_write(&p->trial,b);
    if (st==0) st=m5_write_intra_y_mode(&p->trial,b,y->intra_mode,0u);
    uint8_t unused=0u;
    if (st==0) st=tile_coeff_plane_write(&p->trial,b,0u,0u,0u,y->intra_mode,&unused);
    if (st==0 && p->trial.next!=1u) st=SVT_AV2_ENTROPY_ERROR_RANGE;
    if (st==0) *rate_q12=enc.rate_q12;
    return st;
}

int svt_av2_tile_rate_prefix_finish(const SvtAv2TileRatePrefix* p, uint64_t* rate_q12) {
    if (p==NULL || rate_q12==NULL) return SVT_AV2_ENTROPY_ERROR_NULL;
    *rate_q12=0u;
    if (!p->active || p->leaf!=p->leaves || p->pos!=p->count)
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    *rate_q12=p->enc.rate_q12;
    return 0;
}

int svt_av2_tile_rate_frame(const SvtAv2FrameHeaderConfig* cfg,
    const SvtAv2TileCoeffBlock* blocks, size_t block_count,
    const SvtAv2TileBlockMode* modes, size_t mode_count, uint64_t* rate_q12) {
    if (cfg == NULL || blocks == NULL || rate_q12 == NULL)
        return SVT_AV2_ENTROPY_ERROR_NULL;
    if (cfg->entropy_adaptation > 1u ||
        (cfg->entropy_adaptation && (cfg->multi_frame || cfg->inter_frame)))
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    if (cfg->inter_frame ? modes == NULL || mode_count * 3u != block_count :
                           modes != NULL || mode_count != 0u)
        return SVT_AV2_ENTROPY_ERROR_RANGE;
    SvtAv2EntropyEncoder enc;
    svt_av2_entropy_rate_init(&enc);
    SvtAv2EntropyModels models;
    if (cfg->entropy_adaptation) {
        const int status = svt_av2_entropy_models_init(&models, svt_av2_coeff_q_context(cfg->base_q_idx));
        if (status != 0) return status;
        enc.models = &models;
    }
    const int st = tile_write_coeffs(&enc, cfg, blocks, block_count, modes, mode_count);
    *rate_q12 = st == 0 ? enc.rate_q12 : 0u;
    return st;
}

/* One payload byte per luma sample plus the header allowance: a safe bound for
 * this configuration. Returned to callers so they never size by hand. */
size_t svt_av2_tile_writer_max_payload_bytes(uint32_t width, uint32_t height) {
    size_t samples = (size_t)width * (size_t)height;
    return SVT_AV2_TILE_HEADER_MAX_BYTES + samples + 2u;
}

/* Shared implementation. `scratch` may be NULL, in which case the entropy
 * scratch is derived from `out_cap` (the caller's real payload capacity) rather
 * than from a fixed stack bound, so a frame is never capped by a hidden
 * constant. */
static int tile_writer_write(const SvtAv2FrameHeaderConfig* cfg, uint8_t* out,
                             size_t out_cap, size_t* out_size,
                             const SvtAv2TileWriterScratch* scratch,
                             const SvtAv2TileCoeffBlock* blocks, size_t block_count,
                             const SvtAv2TileBlockMode* modes, size_t mode_count) {
    if (cfg == NULL || out == NULL || out_size == NULL) {
        return SVT_AV2_BW_ERROR_NULL;
    }

    if (cfg->entropy_adaptation > 1u || (cfg->entropy_adaptation && blocks == NULL))
        return SVT_AV2_ENTROPY_ERROR_RANGE;

    /* --- header region: is_first_tile_group + frame_header + alignment --- */
    uint8_t         header[SVT_AV2_TILE_HEADER_MAX_BYTES];
    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, header, sizeof(header));

    int st = svt_av2_bit_writer_write_bit(&bw, 1u); /* is_first_tile_group */
    if (st != SVT_AV2_BW_OK) {
        return st;
    }
    st = svt_av2_frame_header_write(&bw, cfg);
    if (st != SVT_AV2_BW_OK) {
        return st;
    }
    st = svt_av2_bit_writer_byte_alignment(&bw);
    if (st != SVT_AV2_BW_OK) {
        return st;
    }
    size_t header_size = svt_av2_bit_writer_bytes_written(&bw);
    if (header_size > out_cap) {
        *out_size = header_size;
        return SVT_AV2_BW_ERROR_NO_SPACE;
    }

    /* --- entropy scratch -------------------------------------------------
     * The coded bytes are produced into `coded` and copied into `out` at the
     * end, because the final length is only known once the coder is done. The
     * coded region can therefore be at most `out_cap - header_size` bytes; the
     * precarry needs one uint16_t per coded byte. When the caller does not
     * supply scratch we allocate exactly that much, so the effective bound is
     * the caller's own capacity and never a fixed constant. */
    uint8_t*  coded_owned    = NULL;
    uint16_t* precarry_owned = NULL;
    uint8_t*  coded          = NULL;
    size_t    coded_cap      = 0u;
    uint16_t* precarry       = NULL;
    size_t    precarry_cap   = 0u;

    if (scratch != NULL) {
        coded        = scratch->coded;
        coded_cap    = scratch->coded_cap;
        precarry     = scratch->precarry;
        precarry_cap = scratch->precarry_cap;
        if (coded == NULL || precarry == NULL) {
            return SVT_AV2_ENTROPY_ERROR_NULL;
        }
    } else {
        coded_cap = out_cap - header_size;
        if (coded_cap == 0u) {
            *out_size = header_size + 1u;
            return SVT_AV2_BW_ERROR_NO_SPACE;
        }
        coded_owned = (uint8_t*)malloc(coded_cap);
        /* One uint16_t per coded byte plus the coder's carry slack (see
         * entropy_encoder.h). */
        precarry_cap = coded_cap + 2u;
        precarry_owned = (uint16_t*)malloc(precarry_cap * sizeof(uint16_t));
        if (coded_owned == NULL || precarry_owned == NULL) {
            free(coded_owned);
            free(precarry_owned);
            return SVT_AV2_BW_ERROR_NO_SPACE;
        }
        coded    = coded_owned;
        precarry = precarry_owned;
    }

    /* --- symbol region: the entropy-coded tile payload --- */
    SvtAv2EntropyEncoder enc;
    svt_av2_entropy_encoder_init(&enc, coded, coded_cap, precarry,
                                 precarry_cap);

    SvtAv2EntropyModels models;
    if (cfg->entropy_adaptation) {
        st = svt_av2_entropy_models_init(&models, svt_av2_coeff_q_context(cfg->base_q_idx));
        if (st != 0) { free(coded_owned); free(precarry_owned); return st; }
        enc.models = &models;
    }
    if (blocks == NULL) {
        st = svt_av2_block_write_minimal_intra(&enc, cfg->base_q_idx);
    } else {
        st = tile_write_coeffs(&enc, cfg, blocks, block_count, modes, mode_count);
    }
    if (st != SVT_AV2_ENTROPY_OK) {
        if (st == SVT_AV2_ENTROPY_ERROR_NO_SPACE) {
            *out_size = svt_av2_tile_writer_max_payload_bytes(cfg->frame_width, cfg->frame_height);
            st = SVT_AV2_BW_ERROR_NO_SPACE;
        }
        free(coded_owned);
        free(precarry_owned);
        return st;
    }
    uint8_t* coded_bytes = NULL;
    size_t   coded_size  = 0u;
    st = svt_av2_entropy_encoder_done(&enc, &coded_bytes, &coded_size);
    if (st != SVT_AV2_ENTROPY_OK) {
        /* The entropy scratch was too small for the payload this block
         * produced. The exact byte count is not available from the coder, so
         * report the bound for the geometry the caller must retry with. */
        *out_size = svt_av2_tile_writer_max_payload_bytes(cfg->frame_width,
                                                          cfg->frame_height);
        free(coded_owned);
        free(precarry_owned);
        return (st == SVT_AV2_ENTROPY_ERROR_NO_SPACE)
                   ? SVT_AV2_BW_ERROR_NO_SPACE
                   : st;
    }
    if (header_size + coded_size > out_cap) {
        /* Exact requirement; nothing has been written to out yet. */
        *out_size = header_size + coded_size;
        free(coded_owned);
        free(precarry_owned);
        return SVT_AV2_BW_ERROR_NO_SPACE;
    }

    /* --- concatenate --- */
    for (size_t i = 0; i < header_size; ++i) {
        out[i] = header[i];
    }
    for (size_t i = 0; i < coded_size; ++i) {
        out[header_size + i] = coded_bytes[i];
    }
    *out_size = header_size + coded_size;
    free(coded_owned);
    free(precarry_owned);
    return 0;
}

int svt_av2_tile_writer_write_closed_loop_key(const SvtAv2FrameHeaderConfig* cfg,
                                              uint8_t* out,
                                              size_t   out_cap,
                                              size_t*  out_size) {
    return tile_writer_write(cfg, out, out_cap, out_size, NULL, NULL, 0u, NULL, 0u);
}

int svt_av2_tile_writer_write_closed_loop_key_scratch(
    const SvtAv2FrameHeaderConfig* cfg, uint8_t* out, size_t out_cap,
    size_t* out_size, const SvtAv2TileWriterScratch* scratch) {
    return tile_writer_write(cfg, out, out_cap, out_size, scratch, NULL, 0u, NULL, 0u);
}

int svt_av2_tile_writer_write_closed_loop_key_coeffs(
    const SvtAv2FrameHeaderConfig* cfg, const SvtAv2TileCoeffBlock* blocks,
    size_t block_count, uint8_t* out, size_t out_cap, size_t* out_size,
    const SvtAv2TileWriterScratch* scratch) {
    if (blocks == NULL) return SVT_AV2_BW_ERROR_NULL;
    return tile_writer_write(cfg, out, out_cap, out_size, scratch, blocks, block_count, NULL, 0u);
}

int svt_av2_tile_writer_write_inter_coeffs(
    const SvtAv2FrameHeaderConfig* cfg, const SvtAv2TileCoeffBlock* blocks,
    size_t block_count, const SvtAv2TileBlockMode* modes, size_t mode_count,
    uint8_t* out, size_t out_cap, size_t* out_size) {
    if (blocks == NULL || modes == NULL) return SVT_AV2_BW_ERROR_NULL;
    if (!cfg || !cfg->inter_frame || mode_count * 3u != block_count) return SVT_AV2_BW_ERROR_RANGE;
    return tile_writer_write(cfg, out, out_cap, out_size, NULL, blocks, block_count, modes, mode_count);
}


uint64_t svt_av2_tile_rate_prefix_current_rate(const SvtAv2TileRatePrefix* prefix) {
    return prefix->enc.rate_q12;
}
