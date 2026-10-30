#if M6_FINE16
#include <stdio.h>
/* Independent selective 16-to-8 geometry proposals. Every basic/full trial is a
 * complete SB transaction at the same causal entry. Old finalists are never
 * evicted; the complete old tool/RDOQ result wins every outer tie. */
typedef struct RdFine16Search {
    RdTreeSeed seen[64], best[4];
    uint32_t seen_count, best_count;
} RdFine16Search;
static void fine16_reset(RdWork* work) {
    work->basic_rate_active=work->prefix_active=work->prefix_leaf=0u;
    work->prefix_coeff_count=0u;
    work->coeff_policy=work->motion_policy=work->wide_stage=work->defer_rdoq=0u;
    work->rdoq_target=UINT32_MAX;work->rdoq_phase=0u;work->uv_y_cache=0u;
    work->tree_rdoq=work->tool_rounds=0u;
    memset(work->mrl,0,sizeof(work->mrl));memset(work->mhccp,0,sizeof(work->mhccp));
    memset(work->tx_types,0,sizeof(work->tx_types));
    memset(work->cfl_explicit,0,sizeof(work->cfl_explicit));
    memset(work->cfl_alpha_idx,0,sizeof(work->cfl_alpha_idx));
    memset(work->cfl_alpha_signs,0,sizeof(work->cfl_alpha_signs));
    memset(work->rdoq_saved,0,sizeof(work->rdoq_saved));
}
static int fine16_seed_better(const RdTreeSeed* a,const RdTreeSeed* b) {
    return a->choice.cost<b->choice.cost || (a->choice.cost==b->choice.cost &&
        (a->decision<b->decision || (a->decision==b->decision && a->choice.hash<b->choice.hash)));
}
static void fine16_offer(RdFine16Search* search,const RdTreeSeed* seed) {
    uint32_t slot=0u;
    while (slot<search->best_count && search->best[slot].decision!=seed->decision) ++slot;
    if (slot<search->best_count) {
        if (!fine16_seed_better(seed,search->best+slot)) return;
        for (uint32_t i=slot+1u;i<search->best_count;++i) search->best[i-1u]=search->best[i];
        --search->best_count;
    }
    slot=0u;
    while (slot<search->best_count && !fine16_seed_better(seed,search->best+slot)) ++slot;
    if (slot>=4u) return;
    if (search->best_count<4u) ++search->best_count;
    for (uint32_t i=search->best_count-1u;i>slot;--i) search->best[i]=search->best[i-1u];
    search->best[slot]=*seed;
}
static SvtAv2Error fine16_evaluate(RdWork* work,const RdWork* entry,size_t sb,
    uint16_t plan,RdFine16Search* search,RdBasicChoice* choice) {
    for (uint32_t i=0u;i<search->seen_count;++i)
        if (search->seen[i].decision==plan) {*choice=search->seen[i].choice;return SVT_AV2_ERROR_NONE;}
    if (search->seen_count>=64u) return SVT_AV2_ERROR_INVALID_STATE;
    const SvtAv2RdFrameStats aggregate=work->stats;
    *work=*entry;work->stats=aggregate;region_copy(work,1);fine16_reset(work);
    SvtAv2Error error=geometry_for_sb(work,sb,plan);
    if (error!=SVT_AV2_ERROR_NONE) return error;
    memset(work->uv_ids,0,sizeof(work->uv_ids));
    uint64_t d=0u,r=0u,j=0u;uint32_t path=0u;int valid=0;
    error=build_path(work,0u,1,1,NULL,&valid,&d,&path);
    if (error==SVT_AV2_ERROR_NONE && (!valid || svt_av2_tile_rate_trial(work->rate,sb,plan,
        work->blocks,work->leaf_count*3u,NULL,0u,0,&r)!=0)) error=SVT_AV2_ERROR_INVALID_STATE;
    if (error==SVT_AV2_ERROR_NONE) error=svt_av2_rd_cost(d,work->stats.lambda_q8,r,&j);
    if (error!=SVT_AV2_ERROR_NONE) return error;
    *choice=(RdBasicChoice){j,d,r,path_hash(work),plan_candidate_id(plan,0u,path)};
    memcpy(work->best_ids,work->ids,work->leaf_count);
    ++work->stats.candidate_count;
    if (work->config->candidate_hook)
        work->config->candidate_hook(work->config->candidate_hook_user,sb,choice->id,d,r,j,0);
    error=refine_basic(work,sb,0,plan,choice);
    if (error!=SVT_AV2_ERROR_NONE) return error;
    RdTreeSeed* seed=search->seen+search->seen_count++;
    seed->choice=*choice;seed->decision=plan;seed->leaf_count=work->leaf_count;
    memcpy(seed->ids,work->best_ids,work->leaf_count);memcpy(seed->uv,work->uv_ids,work->leaf_count);
    if (plan>=4096u && plan<=11095u) fine16_offer(search,seed);
    return SVT_AV2_ERROR_NONE;
}
/* Map the old geometry only to an independent basic seed. Geometry that
 * crosses 32x32 quadrants is clipped for this seed; old full routes are intact. */
static uint32_t fine16_initial_code(const RdWork* work) {
    static const uint32_t widths[5]={16u,8u,16u,8u,32u};
    static const uint32_t heights[5]={16u,8u,8u,16u,32u};
    uint32_t code=0u,power=1u;
    const uint32_t ox=work->region[0].x,oy=work->region[0].y;
    for (uint32_t q=0u;q<4u;++q) {
        const uint32_t x=ox+(q%2u)*32u,y=oy+(q/2u)*32u;
        uint32_t shape=0u;
        for (uint32_t leaf=0u;leaf<work->leaf_count;++leaf) {
            const SvtAv2BlockView* b=work->leaves+leaf;
            if (b->x<=x && b->y<=y && b->x+b->w>x && b->y+b->h>y) {
                const uint32_t w=SVT_AV2_MIN(b->x+b->w-x,32u);
                const uint32_t h=SVT_AV2_MIN(b->y+b->h-y,32u);
                for (uint32_t k=0u;k<5u;++k)
                    if (w==widths[k] && h==heights[k]) {shape=k;break;}
                break;
            }
        }
        code+=shape*power;power*=5u;
    }
    return code;
}
static SvtAv2Error fine16_prepare(RdWork* work,size_t sb,uint16_t old_plan,
    RdTreeSeed seeds[4],uint32_t* count) {
    *count=0u;
    RdWork* entry=(RdWork*)malloc(sizeof(*entry));
    RdFine16Search* search=(RdFine16Search*)calloc(1u,sizeof(*search));
    if (!entry || !search) {free(entry);free(search);return SVT_AV2_ERROR_OUT_OF_MEMORY;}
    *entry=*work;
    const uint32_t current=fine16_initial_code(work);
    SvtAv2Error error=SVT_AV2_ERROR_NONE;
    for (uint32_t q=0u;q<4u && error==SVT_AV2_ERROR_NONE;++q) {
        uint32_t digits=current,others=0u,power=1u;
        for (uint32_t k=0u;k<4u;++k) {
            const uint32_t shape=digits%5u;digits/=5u;
            if (k!=q) {others+=shape*power;power*=5u;}
        }
        for (uint32_t i=0u;i<14u;++i) {
            const uint32_t mask=M6_FINE16_REVERSE_CHOICES?14u-i:1u+i;
            const uint16_t plan=(uint16_t)(4096u+(q*14u+mask-1u)*125u+others);
            RdBasicChoice trial;
            error=fine16_evaluate(work,entry,sb,plan,search,&trial);
            if (error!=SVT_AV2_ERROR_NONE) break;
        }
    }
    if (error==SVT_AV2_ERROR_NONE) {
        *count=search->best_count;memcpy(seeds,search->best,*count*sizeof(*seeds));
        fprintf(stderr,"FINE16_BASIC sb=%zu trials=%u finalists=%u\n",sb,search->seen_count,*count);
    }
    const SvtAv2RdFrameStats aggregate=work->stats;
    *work=*entry;work->stats=aggregate;region_copy(work,1);
    if (error==SVT_AV2_ERROR_NONE) error=geometry_for_sb(work,sb,old_plan);
    free(search);free(entry);return error;
}

static SvtAv2Error fine16_verify(RdWork* work,size_t sb,uint16_t plan,const RdBasicChoice* expected) {
    region_copy(work,1);
    uint64_t d=0u,r=0u,j=0u;uint32_t path=0u;int valid=0;
    SvtAv2Error error=build_path(work,0u,0,0,work->best_ids,&valid,&d,&path);
    if (error==SVT_AV2_ERROR_NONE && (!valid || d!=expected->distortion ||
        path_hash(work)!=expected->hash || svt_av2_tile_rate_trial(work->rate,sb,plan,
        work->blocks,work->leaf_count*3u,NULL,0u,0,&r)!=0 || r!=expected->rate))
        error=SVT_AV2_ERROR_INVALID_STATE;
    if (error==SVT_AV2_ERROR_NONE) error=svt_av2_rd_cost(d,work->stats.lambda_q8,r,&j);
    if (error==SVT_AV2_ERROR_NONE && j!=expected->cost) error=SVT_AV2_ERROR_INVALID_STATE;
    region_copy(work,1);return error;
}
static SvtAv2Error fine16_refine(RdWork* work,size_t sb,const RdTreeSeed seeds[4],
    uint32_t count,uint16_t* decision,RdBasicChoice* best) {
    if (!count || count>4u) return SVT_AV2_ERROR_INVALID_PARAMETER;
    RdWork* states=(RdWork*)malloc(2u*sizeof(*states));
    if (!states) return SVT_AV2_ERROR_OUT_OF_MEMORY;
    states[0]=states[1]=*work;
    const uint16_t old_plan=*decision;const RdBasicChoice old=*best;
    uint16_t winner_plan=old_plan;RdBasicChoice winner=old;
    SvtAv2Error error=SVT_AV2_ERROR_NONE;
    for (uint32_t i=0u;i<count*2u;++i) {
        const SvtAv2RdFrameStats aggregate=work->stats;
        *work=states[0];work->stats=aggregate;region_copy(work,1);fine16_reset(work);
        const RdTreeSeed* seed=seeds+i/2u;
        error=geometry_for_sb(work,sb,seed->decision);
        if (error!=SVT_AV2_ERROR_NONE || work->leaf_count!=seed->leaf_count) {
            error=SVT_AV2_ERROR_INVALID_STATE;break;
        }
        memcpy(work->best_ids,seed->ids,work->leaf_count);memcpy(work->uv_ids,seed->uv,work->leaf_count);
        RdBasicChoice trial=seed->choice;
        error=fine16_verify(work,sb,seed->decision,&trial);
        work->tool_rounds=1u+i%2u;
        if (error==SVT_AV2_ERROR_NONE) error=refine_tools(work,sb,0,seed->decision,&trial);
        if (error==SVT_AV2_ERROR_NONE) error=fine16_verify(work,sb,seed->decision,&trial);
        if (error!=SVT_AV2_ERROR_NONE) break;
        if (trial.cost<winner.cost || (winner_plan!=old_plan && trial.cost==winner.cost &&
            (seed->decision<winner_plan || (seed->decision==winner_plan && trial.hash<winner.hash)))) {
            winner=trial;winner_plan=seed->decision;states[1]=*work;
        }
    }
    const SvtAv2RdFrameStats aggregate=work->stats;
    if (error==SVT_AV2_ERROR_NONE && !M6_FINE16_RETAIN_OLD) *work=states[1];
    else {*work=states[0];winner=old;winner_plan=old_plan;}
    work->stats=aggregate;work->defer_rdoq=0u;work->tree_rdoq=work->tool_rounds=0u;
    if (error==SVT_AV2_ERROR_NONE) {
        const uint32_t leaf_count=work->leaf_count;
        error=geometry_for_sb(work,sb,winner_plan);
        if (error==SVT_AV2_ERROR_NONE && work->leaf_count!=leaf_count) error=SVT_AV2_ERROR_INVALID_STATE;
        if (error==SVT_AV2_ERROR_NONE) error=fine16_verify(work,sb,winner_plan,&winner);
    }
    region_copy(work,1);free(states);
    if (error==SVT_AV2_ERROR_NONE) {
        fprintf(stderr,"FINE16_SB sb=%zu full=%u old_plan=%u winner_plan=%u oldJ=%llu newJ=%llu retained=%u\n",sb,count*2u,(unsigned)old_plan,(unsigned)winner_plan,(unsigned long long)old.cost,(unsigned long long)winner.cost,(unsigned)M6_FINE16_RETAIN_OLD);
        *best=winner;*decision=winner_plan;
    }
    return error;
}
#endif
