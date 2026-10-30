#if M6_FINE16_PAIR
/* Add a second selective quadrant without replacing any existing route.
 * All trials start at the same SB entry; the old complete winner owns ties. */
static void fine16_pair_reset(RdWork* work) {
    fine16_reset(work);
    work->rdoq_rate_active=work->rdoq_rate_ready=work->rdoq_use_context=0u;
    work->memo_active=work->memo_prefix=0u;work->uv_y_size=0u;
    work->joint_y_tx=work->skip_mhccp_refinement=0u;
}

static SvtAv2Error fine16_pair_evaluate(RdWork* work,const RdWork* entry,size_t sb,
    uint16_t plan,RdFine16Search* search) {
    if (plan<11096u || plan>40495u || search->seen_count>=64u)
        return SVT_AV2_ERROR_INVALID_STATE;
    for (uint32_t i=0u;i<search->seen_count;++i)
        if (search->seen[i].decision==plan) return SVT_AV2_ERROR_INVALID_STATE;
    const SvtAv2RdFrameStats aggregate=work->stats;
    *work=*entry;work->stats=aggregate;region_copy(work,1);fine16_pair_reset(work);
    SvtAv2Error error=geometry_for_sb(work,sb,plan);
    if (error!=SVT_AV2_ERROR_NONE) return error;
    memset(work->uv_ids,0,sizeof(work->uv_ids));
    uint64_t d=0u,r=0u,j=0u;uint32_t path=0u;int valid=0;
    error=build_path(work,0u,1,1,NULL,&valid,&d,&path);
    if (error==SVT_AV2_ERROR_NONE && (!valid || svt_av2_tile_rate_trial(work->rate,sb,plan,
        work->blocks,work->leaf_count*3u,NULL,0u,0,&r)!=0)) error=SVT_AV2_ERROR_INVALID_STATE;
    if (error==SVT_AV2_ERROR_NONE) error=svt_av2_rd_cost(d,work->stats.lambda_q8,r,&j);
    if (error!=SVT_AV2_ERROR_NONE) return error;
    RdBasicChoice choice={j,d,r,path_hash(work),plan_candidate_id(plan,0u,path)};
    memcpy(work->best_ids,work->ids,work->leaf_count);
    ++work->stats.candidate_count;
    if (work->config->candidate_hook)
        work->config->candidate_hook(work->config->candidate_hook_user,sb,choice.id,d,r,j,0);
    error=refine_basic(work,sb,0,plan,&choice);
    if (error!=SVT_AV2_ERROR_NONE) return error;
    RdTreeSeed* seed=search->seen+search->seen_count++;
    seed->choice=choice;seed->decision=plan;seed->leaf_count=work->leaf_count;
    memcpy(seed->ids,work->best_ids,work->leaf_count);memcpy(seed->uv,work->uv_ids,work->leaf_count);
    fine16_offer(search,seed);
    return SVT_AV2_ERROR_NONE;
}

static SvtAv2Error fine16_pair_prepare(RdWork* work,size_t sb,uint16_t old_plan,
    uint16_t anchor,RdTreeSeed seeds[4],uint32_t* count) {
    *count=0u;
    if (anchor<4096u || anchor>11095u) return SVT_AV2_ERROR_INVALID_STATE;
    RdWork* entry=(RdWork*)malloc(sizeof(*entry));
    RdFine16Search* search=(RdFine16Search*)calloc(1u,sizeof(*search));
    if (!entry || !search) {free(entry);free(search);return SVT_AV2_ERROR_OUT_OF_MEMORY;}
    *entry=*work;
    const uint32_t code=(uint32_t)anchor-4096u;
    const uint32_t first=code/125u/14u,first_mask=code/125u%14u+1u;
    uint32_t digits=code%125u,shapes[4]={0u};
    for (uint32_t q=0u;q<4u;++q) if (q!=first) {shapes[q]=digits%5u;digits/=5u;}
    static const uint32_t pairs[6][2]={{0u,1u},{0u,2u},{0u,3u},{1u,2u},{1u,3u},{2u,3u}};
    SvtAv2Error error=SVT_AV2_ERROR_NONE;
    for (uint32_t second=0u;second<4u && error==SVT_AV2_ERROR_NONE;++second) {
        if (second==first) continue;
        const uint32_t lo=SVT_AV2_MIN(first,second),hi=SVT_AV2_MAX(first,second);
        uint32_t pair=0u,others=0u,power=1u;
        while (pair<6u && (pairs[pair][0]!=lo || pairs[pair][1]!=hi)) ++pair;
        if (pair==6u) {error=SVT_AV2_ERROR_INVALID_STATE;break;}
        for (uint32_t q=0u;q<4u;++q) if (q!=lo && q!=hi) {others+=shapes[q]*power;power*=5u;}
        for (uint32_t i=0u;i<14u;++i) {
            const uint32_t mask=M6_FINE16_REVERSE_CHOICES?14u-i:1u+i;
            const uint32_t low_mask=lo==first?first_mask:mask;
            const uint32_t high_mask=hi==first?first_mask:mask;
            const uint16_t plan=(uint16_t)(11096u+((pair*14u+low_mask-1u)*14u+high_mask-1u)*25u+others);
            error=fine16_pair_evaluate(work,entry,sb,plan,search);
            if (error!=SVT_AV2_ERROR_NONE) break;
        }
    }
    if (error==SVT_AV2_ERROR_NONE && search->seen_count!=42u) error=SVT_AV2_ERROR_INVALID_STATE;
    if (error==SVT_AV2_ERROR_NONE) {
        *count=search->best_count;memcpy(seeds,search->best,*count*sizeof(*seeds));
    }
    const uint32_t trials=search->seen_count;
    const SvtAv2RdFrameStats aggregate=work->stats;
    *work=*entry;work->stats=aggregate;region_copy(work,1);
    const SvtAv2Error restore=geometry_for_sb(work,sb,old_plan);
    if (error==SVT_AV2_ERROR_NONE) error=restore;
    fprintf(stderr,"FINE16_PAIR_BASIC sb=%zu anchor=%u trials=%u finalists=%u error=%u\n",
        sb,(unsigned)anchor,trials,*count,(unsigned)error);
    free(search);free(entry);return error;
}

static SvtAv2Error fine16_pair_refine(RdWork* work,size_t sb,const RdTreeSeed seeds[4],
    uint32_t count,uint16_t* decision,RdBasicChoice* best) {
    if (!count || count>4u) return SVT_AV2_ERROR_INVALID_PARAMETER;
    RdWork* states=(RdWork*)malloc(2u*sizeof(*states));
    if (!states) return SVT_AV2_ERROR_OUT_OF_MEMORY;
    states[0]=states[1]=*work;
    const uint16_t old_plan=*decision;const RdBasicChoice old=*best;
    uint16_t winner_plan=old_plan;RdBasicChoice winner=old;
    uint32_t trials=0u,completed=0u,better=0u;
    SvtAv2Error error=SVT_AV2_ERROR_NONE;
    for (uint32_t i=0u;i<count*2u;++i) {
        const SvtAv2RdFrameStats aggregate=work->stats;
        *work=states[0];work->stats=aggregate;region_copy(work,1);fine16_pair_reset(work);
        const RdTreeSeed* seed=seeds+i/2u;
        if (seed->decision<11096u || seed->decision>40495u) {error=SVT_AV2_ERROR_INVALID_STATE;break;}
        error=geometry_for_sb(work,sb,seed->decision);
        if (error!=SVT_AV2_ERROR_NONE || work->leaf_count!=seed->leaf_count) {
            error=SVT_AV2_ERROR_INVALID_STATE;break;
        }
        memcpy(work->best_ids,seed->ids,work->leaf_count);memcpy(work->uv_ids,seed->uv,work->leaf_count);
        RdBasicChoice trial=seed->choice;
        ++trials;
        error=fine16_verify(work,sb,seed->decision,&trial);
        work->tool_rounds=1u+i%2u;
        if (error==SVT_AV2_ERROR_NONE) error=refine_tools(work,sb,0,seed->decision,&trial);
        if (error==SVT_AV2_ERROR_NONE) error=fine16_verify(work,sb,seed->decision,&trial);
        if (error!=SVT_AV2_ERROR_NONE) break;
        ++completed;better+=(uint32_t)(trial.cost<old.cost);
        if (trial.cost<winner.cost || (winner_plan!=old_plan && trial.cost==winner.cost &&
            (seed->decision<winner_plan || (seed->decision==winner_plan && trial.hash<winner.hash)))) {
            winner=trial;winner_plan=seed->decision;states[1]=*work;
        }
    }
    const SvtAv2RdFrameStats aggregate=work->stats;
    if (error==SVT_AV2_ERROR_NONE) *work=states[1];
    else {*work=states[0];winner=old;winner_plan=old_plan;}
    work->stats=aggregate;work->defer_rdoq=0u;work->tree_rdoq=work->tool_rounds=0u;
    const uint32_t leaf_count=work->leaf_count;
    const SvtAv2Error restore=geometry_for_sb(work,sb,winner_plan);
    if (error==SVT_AV2_ERROR_NONE) error=restore;
    if (error==SVT_AV2_ERROR_NONE && work->leaf_count!=leaf_count) error=SVT_AV2_ERROR_INVALID_STATE;
    if (error==SVT_AV2_ERROR_NONE) error=fine16_verify(work,sb,winner_plan,&winner);
    const uint32_t accepted=(uint32_t)(error==SVT_AV2_ERROR_NONE && winner.cost<old.cost);
    fprintf(stderr,"FINE16_PAIR_SB sb=%zu full_trials=%u completed=%u better_trials=%u accepted=%u old_plan=%u winner_plan=%u oldD=%llu newD=%llu oldRq12=%llu newRq12=%llu oldJ=%llu newJ=%llu error=%u\n",
        sb,trials,completed,better,accepted,(unsigned)old_plan,(unsigned)winner_plan,
        (unsigned long long)old.distortion,(unsigned long long)winner.distortion,
        (unsigned long long)old.rate,(unsigned long long)winner.rate,
        (unsigned long long)old.cost,(unsigned long long)winner.cost,(unsigned)error);
    region_copy(work,1);free(states);
    if (error==SVT_AV2_ERROR_NONE) {*best=winner;*decision=winner_plan;}
    return error;
}
#endif
