#include "rd_frame.h"
#include "coeff_syntax.h"
#include "keyframe.h"
#include "transform_path.h"
#include "test_util.h"

#include <stdlib.h>
#include <string.h>

typedef struct Audit {
    uint64_t minimum[16], seen[16], committed[16];
    uint64_t minimum_id[16];
    uint64_t lambda;
    uint64_t first_sb_costs[512];
    uint8_t first_sb_seen[512];
    const struct Audit* exhaustive;
    int basic;
    uint64_t mhccp_trials;
} Audit;

static void observe(void* user, size_t sb, uint64_t id, uint64_t d,
    uint64_t r, uint64_t cost, int committed) {
    Audit* audit = (Audit*)user;
    SVT_TEST_CHECK(sb < 16u);
    if (sb >= 16u) return;
    /* Small fixture range: independent ordinary multiplication is safe here. */
    SVT_TEST_CHECK(r < UINT64_MAX / audit->lambda);
    SVT_TEST_CHECK_EQ_INT(cost, d + (audit->lambda * r + 524288u) / 1048576u);
    if (!committed) {
        audit->mhccp_trials += (id & (UINT32_C(1) << 30u)) != 0u;
        if (sb == 0u && id < 512u) {
            audit->first_sb_costs[id] = cost; audit->first_sb_seen[id] = 1u;
            if (audit->exhaustive != NULL) {
                SVT_TEST_CHECK(audit->exhaustive->first_sb_seen[id] != 0u);
                SVT_TEST_CHECK_EQ_INT(cost, audit->exhaustive->first_sb_costs[id]);
            }
        }
        if (audit->seen[sb] == 0u || cost < audit->minimum[sb] ||
            (cost == audit->minimum[sb] && id < audit->minimum_id[sb])) {
            audit->minimum[sb] = cost; audit->minimum_id[sb] = id;
        }
        ++audit->seen[sb];
    } else {
        SVT_TEST_CHECK_EQ_INT(cost, audit->minimum[sb]);
        if (!audit->basic) SVT_TEST_CHECK_EQ_INT(id, audit->minimum_id[sb]);
        ++audit->committed[sb];
    }
}

static void numeric_tests(void) {
    uint64_t lambda = 0u, cost = 0u; uint32_t qstep = 0u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_rd_lambda(303u, 10u, &lambda, &qstep), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(qstep, 245760u);
    SVT_TEST_CHECK_EQ_INT(lambda, 943718400u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_rd_lambda(255u, 8u, &lambda, &qstep), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(qstep, 61440u);
    SVT_TEST_CHECK_EQ_INT(lambda, 943718400u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_rd_lambda(1u, 8u, &lambda, &qstep), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(qstep, 40u);
    SVT_TEST_CHECK_EQ_INT(lambda, 400u);
    SVT_TEST_CHECK(svt_av2_rd_lambda(0u, 8u, &lambda, &qstep) != SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK(svt_av2_rd_lambda(1u, 10u, &lambda, &qstep) != SVT_AV2_ERROR_NONE);
    const uint64_t sse444[3] = {UINT64_C(4096) * 1023u * 1023u,
        UINT64_C(4096) * 1023u * 1023u, UINT64_C(4096) * 1023u * 1023u};
    const uint64_t sse420[3] = {sse444[0], sse444[1] / 4u, sse444[2] / 4u};
    SVT_TEST_CHECK_EQ_INT(svt_av2_rd_distortion(sse444, 10u, SVT_AV2_CHROMA_444), UINT64_C(1607468544));
    SVT_TEST_CHECK_EQ_INT(svt_av2_rd_distortion(sse444, 10u, SVT_AV2_CHROMA_444),
        svt_av2_rd_distortion(sse420, 10u, SVT_AV2_CHROMA_420));
    /* Deliberately overflow a naive uint64 product, but the divided result fits. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_rd_cost(17u, 943718400u, UINT64_C(1) << 40u, &cost), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(cost, 17u + UINT64_C(943718400) * (UINT64_C(1) << 20u));
    SVT_TEST_CHECK_EQ_INT(svt_av2_rd_cost(0u, 1u, 524287u, &cost), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(cost, 0u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_rd_cost(0u, 1u, 524288u, &cost), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(cost, 1u);
    SVT_TEST_CHECK(svt_av2_rd_cost(UINT64_MAX, 1u, 1048576u, &cost) != SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK(svt_av2_rd_cost(0u, UINT64_MAX, UINT64_MAX, &cost) != SVT_AV2_ERROR_NONE);
    for (uint64_t a = 1u; a <= 1000u; a += 37u)
        for (uint64_t b = 524277u; b < 524301u; ++b) {
            SVT_TEST_CHECK_EQ_INT(svt_av2_rd_cost(19u, a, b, &cost), SVT_AV2_ERROR_NONE);
            SVT_TEST_CHECK_EQ_INT(cost, 19u + (a * b + 524288u) / 1048576u);
        }
}

static uint32_t source_pattern;
static void fill(SvtAv2FrameBuffer* picture) {
    const uint32_t step = picture->bit_depth > 8u ? 2u : 1u;
    for (uint32_t p = 0u; p < 3u; ++p)
        for (uint32_t y = 0u; y < svt_av2_picture_plane_height(picture, p); ++y)
            for (uint32_t x = 0u; x < svt_av2_picture_plane_width(picture, p); ++x) {
                const uint32_t scale=p && picture->chroma_format==SVT_AV2_CHROMA_420?2u:1u;
                const uint32_t lx=x*scale,ly=y*scale,q=(ly%64u/32u)*2u+lx%64u/32u;
                const uint32_t pool=q==0u?(lx%32u<16u?32u:224u):q==1u?(ly%32u<16u?64u:192u):q==2u?128u:96u;
                const uint32_t value = (source_pattern==2u?pool+(p==2u?7u:0u):
                    ((x * 11u + y * 7u + (x * y % 31u) + p * 27u) & 255u))
                    << (picture->bit_depth - 8u);
                svt_av2_sample_store(picture->planes[p] + (size_t)y * picture->strides[p] + x * step,
                    picture->bit_depth, (uint16_t)value);
            }
    if (source_pattern==1u) for(uint32_t p=1u;p<3u;++p)
        for(uint32_t y=0u;y<svt_av2_picture_plane_height(picture,p);++y)
            for(uint32_t x=0u;x<svt_av2_picture_plane_width(picture,p);++x) {
                const uint32_t scale=picture->chroma_format==SVT_AV2_CHROMA_420?2u:1u;
                uint32_t sum=0u;
                for(uint32_t dy=0u;dy<scale;++dy)for(uint32_t dx=0u;dx<scale;++dx)
                    sum+=svt_av2_sample_load(picture->planes[0]+(size_t)(y*scale+dy)*picture->strides[0]+(x*scale+dx)*step,picture->bit_depth);
                const uint32_t v=(sum/(scale*scale))>>(picture->bit_depth-8u);
                const uint32_t quadratic=(v*v+128u)>>8u;
                const uint32_t uv=p==1u?quadratic:(quadratic+255u-v)/2u;
                svt_av2_sample_store(picture->planes[p]+(size_t)y*picture->strides[p]+x*step,picture->bit_depth,
                    (uint16_t)(uv<<(picture->bit_depth-8u)));
            }
}

static uint64_t picture_hash(const SvtAv2FrameBuffer* picture) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (uint32_t p = 0u; p < 3u; ++p)
        for (size_t i = 0u; i < (size_t)picture->strides[p] * picture->aligned_height[p]; ++i)
            hash = (hash ^ picture->planes[p][i]) * UINT64_C(1099511628211);
    return hash;
}

static uint64_t independent_distortion(const SvtAv2FrameBuffer* source,
    const SvtAv2FrameBuffer* recon) {
    uint64_t sums[16] = {0u};
    const uint32_t cols = (source->visible_width + 63u) / 64u;
    const uint32_t step = source->bit_depth > 8u ? 2u : 1u;
    for (uint32_t p = 0u; p < 3u; ++p) {
        const uint32_t scale = p != 0u && source->chroma_format == SVT_AV2_CHROMA_420 ? 2u : 1u;
        const uint64_t weight = p == 0u || scale == 2u ? 4u : 1u;
        for (uint32_t y = 0u; y < svt_av2_picture_plane_height(source, p); ++y)
            for (uint32_t x = 0u; x < svt_av2_picture_plane_width(source, p); ++x) {
                const int a = svt_av2_sample_load(source->planes[p] + (size_t)y * source->strides[p] + x * step, source->bit_depth);
                const int b = svt_av2_sample_load(recon->planes[p] + (size_t)y * recon->strides[p] + x * step, recon->bit_depth);
                const size_t sb = (size_t)(y * scale / 64u) * cols + x * scale / 64u;
                sums[sb] += weight * (uint64_t)((a - b) * (a - b));
            }
    }
    uint64_t result = 0u;
    for (size_t i = 0u; i < 16u; ++i)
        result += source->bit_depth == 10u ? (sums[i] + 8u) / 16u : sums[i];
    return result;
}

typedef struct FaultPath { SvtAv2IntraTransformPath real; uint32_t calls; } FaultPath;
static SvtAv2Error fault_forward(void* user, uint32_t p, uint32_t w, uint32_t h,
    const int16_t* residual, uint32_t stride, int32_t* coeff, int32_t* qcoeff,
    int32_t* dqcoeff, int* zero) {
    FaultPath* fault = (FaultPath*)user;
    const SvtAv2Error error = fault->real.forward(fault->real.user, p, w, h,
        residual, stride, coeff, qcoeff, dqcoeff, zero);
    return ++fault->calls == 5u ? SVT_AV2_ERROR_INVALID_STATE : error;
}
static SvtAv2Error fault_inverse(void* user, uint32_t p, uint32_t w, uint32_t h,
    const int32_t* dqcoeff, uint32_t stride, int16_t* residual) {
    FaultPath* fault = (FaultPath*)user;
    return fault->real.inverse(fault->real.user, p, w, h, dqcoeff, stride, residual);
}

static void frame_test(uint32_t width, uint32_t height, uint32_t bd,
    SvtAv2ChromaFormat format, int inter, uint32_t intra_modes, uint32_t adaptive, uint32_t fine, uint32_t primary, uint32_t rdoq, uint32_t angles) {
    SvtAv2FrameBuffer source = {0}, recon = {0};
    SvtAv2ReferenceFrame reference = {0}; SvtAv2ReconScratch scratch = {0};
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&source, width, height, bd, format), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&recon, width, height, bd, format), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(&scratch, 64u, 64u), SVT_AV2_ERROR_NONE);
    fill(&source);
    if (inter) {
        SVT_TEST_CHECK_EQ_INT(svt_av2_reference_init(&reference, width, height, bd, format), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_reference_prepare(&reference, &source, 0u), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(svt_av2_reference_commit(&reference), SVT_AV2_ERROR_NONE);
    }
    const uint64_t source_hash = picture_hash(&source);
    const uint64_t reference_hash = inter ? picture_hash(&reference.committed) : 0u;
    const size_t count = (size_t)width * height;
    SvtAv2RdFrameOutput output = {0};
    output.block_capacity = count / 64u * 3u; output.mode_capacity = count / 64u;
    output.coeff_capacity = count * 3u; output.decision_capacity = ((width + 63u) / 64u) * ((height + 63u) / 64u);
    output.blocks = (SvtAv2TileCoeffBlock*)calloc(output.block_capacity, sizeof(*output.blocks));
    output.modes = (SvtAv2TileBlockMode*)calloc(output.mode_capacity, sizeof(*output.modes));
    output.qcoeff = (int32_t*)calloc(output.coeff_capacity, sizeof(*output.qcoeff));
    output.decisions = (uint16_t*)calloc(output.decision_capacity, sizeof(*output.decisions));
    SVT_TEST_CHECK(output.blocks != NULL && output.modes != NULL && output.qcoeff != NULL && output.decisions != NULL);
    SvtAv2RdFrameConfig config = {0};
    svt_av2_frame_header_config_default(&config.frame_header, width, height);
    config.frame_header.bit_depth = bd; config.frame_header.chroma_format = (uint32_t)format;
    config.frame_header.base_q_idx = 100u + (bd == 10u ? 48u : 0u);
    config.angle_search = angles;
    config.mhccp_search=1u;config.frame_header.enable_mhccp=1u;
    config.cfl_search=1u;config.frame_header.enable_mrls=1u;
    config.tree_finalists=4u;config.rd_lambda_model=1u;
    config.transform_selection = primary;
    config.frame_header.reduced_tx_set = primary == 2u ? 0u : primary ? 1u : 2u; config.frame_header.multi_frame = intra_modes < 2u;
    config.frame_header.m5_intra_modes = intra_modes; config.frame_header.inter_frame = inter != 0;
    config.frame_header.entropy_adaptation = adaptive;
    config.rd_mode = 1u; config.partition_selection = fine ? fine + 1u : 1u;
    config.coeff_optimization = rdoq ? 2u : 0u;
#ifdef SVT_AV2_TEST_CLASSIC_CFL
    config.cfl_search=1u;
#endif
    SvtAv2TransformPathState transform;
    SVT_TEST_CHECK_EQ_INT(svt_av2_transform_path_init_bd(&transform, &config.transform,
        config.frame_header.base_q_idx, bd), SVT_AV2_ERROR_NONE);
    Audit audit = {0}; uint32_t qstep;
    audit.basic = intra_modes >= 2u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_rd_lambda_ai(config.frame_header.base_q_idx, bd, &audit.lambda, &qstep), SVT_AV2_ERROR_NONE);
    config.candidate_hook = observe; config.candidate_hook_user = &audit;
    SvtAv2RdFrameStats first = {0}, reversed = {0}, pruned = {0};
    SVT_TEST_CHECK_EQ_INT(svt_av2_rd_frame_encode(&source, &recon, inter ? &reference : NULL,
        &config, &scratch, &output, &first), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(first.replay_checks, output.decision_count);
    const char* export_dir = getenv("SVT_AV2_MIXED32_RD_OUT");
    if (export_dir != NULL) {
        uint8_t stream[65536], raw[65536]; size_t bytes=0u,raw_bytes=0u;
        SVT_TEST_CHECK_EQ_INT(svt_av2_encode_still_picture_mhccp(width,height,bd,(uint32_t)format,
            config.frame_header.base_q_idx,output.blocks,output.block_count,intra_modes,
            output.decisions,output.decision_count,adaptive,primary,config.frame_header.enable_mrls,config.frame_header.enable_mhccp,
            stream,sizeof(stream),&bytes),0);
        char name[1024];
        snprintf(name,sizeof(name),"%s/%ux%u-%ub-%u.av2",export_dir,width,height,bd,(uint32_t)format);
        FILE* file=fopen(name,"wb"); SVT_TEST_CHECK(file!=NULL);
        if(file){SVT_TEST_CHECK_EQ_INT(fwrite(stream,1u,bytes,file),bytes);fclose(file);}
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_export_raw(&recon,raw,sizeof(raw),&raw_bytes),0);
        snprintf(name,sizeof(name),"%s/%ux%u-%ub-%u.yuv",export_dir,width,height,bd,(uint32_t)format);
        file=fopen(name,"wb");SVT_TEST_CHECK(file!=NULL);
        if(file){SVT_TEST_CHECK_EQ_INT(fwrite(raw,1u,raw_bytes,file),raw_bytes);fclose(file);}
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_export_raw(&source,raw,sizeof(raw),&raw_bytes),0);
        snprintf(name,sizeof(name),"%s/%ux%u-%ub-%u.source.yuv",export_dir,width,height,bd,(uint32_t)format);
        file=fopen(name,"wb");SVT_TEST_CHECK(file!=NULL);
        if(file){SVT_TEST_CHECK_EQ_INT(fwrite(raw,1u,raw_bytes,file),raw_bytes);fclose(file);}
    }
#ifdef SVT_AV2_TEST_CLASSIC_CFL
    size_t explicit_leaves=0u;
    for (size_t i=0u;i<output.block_count;i+=3u) explicit_leaves+=output.blocks[i].cfl_explicit!=0u;
    SVT_TEST_CHECK(first.cfl_trials>0u);
    printf("CLASSIC,%ux%u,bd=%u,fmt=%u,trials=%llu,accepts=%llu,leaves=%zu\n",width,height,bd,(uint32_t)format,
        (unsigned long long)first.cfl_trials,(unsigned long long)first.cfl_accepts,explicit_leaves);
#endif
    size_t mhccp_leaves=0u;
    for(size_t i=0u;i<output.block_count;i+=3u)mhccp_leaves+=output.blocks[i].mhccp!=0u;
    SVT_TEST_CHECK(first.mhccp_trials>0u);
    SVT_TEST_CHECK_EQ_INT(first.mhccp_trials,audit.mhccp_trials);
    printf("MHCCP,%ux%u,bd=%u,fmt=%u,trials=%llu,accepts=%llu,leaves=%zu,trees=%llu,RDOQ=%llu,replay=%llu\n",
        width,height,bd,(uint32_t)format,(unsigned long long)first.mhccp_trials,
        (unsigned long long)first.mhccp_accepts,mhccp_leaves,(unsigned long long)first.tree_finalist_trials,
        (unsigned long long)first.coeff_trials,(unsigned long long)first.replay_checks);
    if (rdoq) {
        size_t eligible = 0u;
        for (size_t i = 0u; i < output.block_count; i += 3u) {
            const SvtAv2TileCoeffBlock* b = output.blocks + i;
            if (b->width == b->height || (b->width <= 16u && b->height <= 16u)) eligible += 3u;
        }
        /* Four finalists aggregate trials from every seed; eligible counts only the winner. */
        SVT_TEST_CHECK(first.coeff_trials >= eligible);
        if (eligible && fine < 5u) SVT_TEST_CHECK(first.coeff_accepts > 0u);
        for (size_t i = 0u; i < output.block_count; ++i) {
            const SvtAv2TileCoeffBlock* block = output.blocks + i;
            if (block->tx_type == 0u) continue;
            SvtAv2TxSize tx; uint16_t scan[256];
            SVT_TEST_CHECK_EQ_INT(svt_av2_coeff_tx_size_from_dims(block->width, block->height, &tx), 0);
            const size_t n = svt_av2_coeff_scan_dct_dct(tx, scan);
            SVT_TEST_CHECK(svt_av2_coeff_find_eob(block->qcoeff, scan, n) > 1u);
        }
    }
    SVT_TEST_CHECK_EQ_INT(first.distortion, independent_distortion(&source, &recon));
    for (size_t i = 0u; i < output.decision_count; ++i) SVT_TEST_CHECK_EQ_INT(audit.committed[i], 1u);
    if (intra_modes < 2u && width == 64u && height == 64u) SVT_TEST_CHECK_EQ_INT(first.candidate_count, inter ? 74u : 13u);
    if (intra_modes < 2u && width == 128u && height == 128u)
        SVT_TEST_CHECK_EQ_INT(audit.seen[3], inter ? 260u : 84u);
    config.frame_header.partition_decisions = output.decisions;
    config.frame_header.partition_decision_count = output.decision_count;
    uint64_t replay_rate = 0u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_frame(&config.frame_header, output.blocks, output.block_count,
        inter ? output.modes : NULL, inter ? output.mode_count : 0u, &replay_rate), 0);
    SVT_TEST_CHECK_EQ_INT(replay_rate, first.rate_q12);
    size_t recon_bytes = 0u;
    for (uint32_t p = 0u; p < 3u; ++p) recon_bytes += (size_t)recon.strides[p] * recon.aligned_height[p];
    uint8_t* saved_recon = (uint8_t*)malloc(recon_bytes);
    int32_t* saved_coeff = (int32_t*)malloc(output.coeff_count * sizeof(int32_t));
    const size_t saved_count = output.coeff_count;
    memcpy(saved_recon, recon.buffer, recon_bytes);
    memcpy(saved_coeff, output.qcoeff, saved_count * sizeof(int32_t));
    config.frame_header.partition_decisions = NULL; config.frame_header.partition_decision_count = 0u;
    config.reverse_candidates = 1u; memset(&audit, 0, sizeof(audit));
    audit.basic = intra_modes >= 2u;
    svt_av2_rd_lambda_ai(config.frame_header.base_q_idx, bd, &audit.lambda, &qstep);
    SVT_TEST_CHECK_EQ_INT(svt_av2_rd_frame_encode(&source, &recon, inter ? &reference : NULL,
        &config, &scratch, &output, &reversed), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(first.winner_hash, reversed.winner_hash);
    SVT_TEST_CHECK_EQ_INT(first.cost, reversed.cost);
    SVT_TEST_CHECK_EQ_INT(first.rate_q12, reversed.rate_q12);
    SVT_TEST_CHECK_EQ_INT(output.coeff_count, saved_count);
    SVT_TEST_CHECK(memcmp(saved_coeff, output.qcoeff, saved_count * sizeof(int32_t)) == 0);
    SVT_TEST_CHECK(memcmp(saved_recon, recon.buffer, recon_bytes) == 0);
    config.reverse_candidates = 0u; config.rd_mode = 2u; config.candidate_hook = NULL;
    /* Reverse enumeration collected the complete original domain. On the first
     * SB, identical entry state permits exact per-candidate subset comparison. */
    Audit pruned_audit = {0}; pruned_audit.lambda = audit.lambda;
    pruned_audit.basic = intra_modes >= 2u;
    pruned_audit.exhaustive = intra_modes >= 2u ? NULL : &audit;
    config.candidate_hook = observe; config.candidate_hook_user = &pruned_audit;
    SVT_TEST_CHECK_EQ_INT(svt_av2_rd_frame_encode(&source, &recon, inter ? &reference : NULL,
        &config, &scratch, &output, &pruned), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK(pruned.candidate_count <= first.candidate_count);
    /* A single SB shares its entry state, so the pruned subset cannot beat full. */
    if (intra_modes < 2u && width <= 64u && height <= 64u) SVT_TEST_CHECK(pruned.cost >= first.cost);
    const size_t capacity = output.coeff_capacity; output.coeff_capacity = 0u;
    config.candidate_hook = NULL;
    SVT_TEST_CHECK(svt_av2_rd_frame_encode(&source, &recon, inter ? &reference : NULL,
        &config, &scratch, &output, &pruned) != SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(output.block_count, 0u); output.coeff_capacity = capacity;
    FaultPath fault = {config.transform, 0u};
    config.transform.user = &fault; config.transform.forward = fault_forward;
    config.transform.inverse = fault_inverse;
    SVT_TEST_CHECK(svt_av2_rd_frame_encode(&source, &recon, inter ? &reference : NULL,
        &config, &scratch, &output, &pruned) != SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(output.block_count, 0u);
    config.transform = fault.real;
    config.rd_mode = 1u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_rd_frame_encode(&source, &recon, inter ? &reference : NULL,
        &config, &scratch, &output, &reversed), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(first.winner_hash, reversed.winner_hash);
    SVT_TEST_CHECK(memcmp(saved_recon, recon.buffer, recon_bytes) == 0);
    SVT_TEST_CHECK_EQ_INT(picture_hash(&source), source_hash);
    if (inter) {
        SVT_TEST_CHECK_EQ_INT(picture_hash(&reference.committed), reference_hash);
        SVT_TEST_CHECK_EQ_INT(reference.frame_index, 0u);
    }
    free(saved_recon); free(saved_coeff); free(output.blocks); free(output.modes);
    free(output.qcoeff); free(output.decisions);
    svt_av2_reference_release(&reference); svt_av2_recon_pool_free(&scratch);
    svt_av2_picture_free(&source); svt_av2_picture_free(&recon);
}

int main(int argc,char** argv) {
    numeric_tests();
    const uint32_t format=argc>1?(uint32_t)strtoul(argv[1],NULL,10):0u;
    const uint32_t bd=format?10u:8u;
    const SvtAv2ChromaFormat chroma=format==2u?SVT_AV2_CHROMA_444:SVT_AV2_CHROMA_420;
    source_pattern=argc>2?(uint32_t)strtoul(argv[2],NULL,10):0u;
    frame_test(64u,64u,bd,chroma,0,4u,1u,source_pattern==1u?1u:5u,2u,1u,1u);
    if(!source_pattern)frame_test(72u,72u,bd,chroma,0,4u,0u,5u,2u,1u,1u);
    SVT_TEST_REPORT("test_mhccp_rd");
}
