#if M6_SATD_ANGLES
#if !M6_DEBLOCK
#error SATD_packet_gate_requires_actual_packet_cost_helpers
#endif
static SvtAv2Error satd_dump(const SvtAv2EncoderContext* c,size_t bytes) {
    if (!getenv("SVT_AV2_TEST_SATD_DUMP")) return SVT_AV2_ERROR_NONE;
    FILE* file=fopen("satd-candidate.obu","wb");
    if (!file) return SVT_AV2_ERROR_INVALID_STATE;
    const size_t written=fwrite(c->packet,1u,bytes,file);
    const int closed=fclose(file);
    if (written!=bytes || closed) return SVT_AV2_ERROR_INVALID_STATE;
    file=fopen("satd-candidate.yuv","wb");
    if (!file) return SVT_AV2_ERROR_INVALID_STATE;
    int failed=0;
    const uint32_t step=c->config.bit_depth>8u?2u:1u;
    for (uint32_t p=0u;p<3u;++p) {
        const uint32_t w=c->frame_width>>(p?1u:0u),h=c->frame_height>>(p?1u:0u);
        for (uint32_t y=0u;y<h;++y)
            failed|=fwrite(c->recon.planes[p]+(size_t)y*c->recon.strides[p],step,w,file)!=w;
    }
    failed|=fclose(file)!=0;
    return failed?SVT_AV2_ERROR_INVALID_STATE:SVT_AV2_ERROR_NONE;
}
/* Run the unchanged frame first. The extra branch owns its reconstruction,
 * packet and coded records; source, scratch and reference remain borrowed. */
static SvtAv2Error satd_packet(SvtAv2EncoderContext* c,size_t* bytes) {
    SvtAv2EncoderContext trial=*c;
    memset(&trial.recon,0,sizeof(trial.recon));trial.packet=NULL;trial.packet_capacity=0u;
    trial.coded_blocks=NULL;trial.coded_modes=NULL;trial.coded_block_capacity=0u;
    trial.coded_coeffs=NULL;trial.coded_coeff_capacity=0u;
    trial.partition_decisions=NULL;trial.partition_decision_capacity=0u;
    trial.satd_angles=1u;
    SvtAv2Error error=svt_av2_picture_alloc_ex(&trial.recon,c->frame_width,c->frame_height,
        (uint32_t)c->config.bit_depth,c->config.chroma_format);
    size_t new_bytes=0u;
    uint64_t old_cost=0u,new_cost=0u;
    if (error==SVT_AV2_ERROR_NONE) error=db_current_cost(c,*bytes,&old_cost);
    if (error==SVT_AV2_ERROR_NONE) error=encode_reconstruction(&trial,c->frame_width,c->frame_height);
    if (error==SVT_AV2_ERROR_NONE) error=ccso_auto_encode(&trial,&new_bytes);
    if (error==SVT_AV2_ERROR_NONE) error=db_current_cost(&trial,new_bytes,&new_cost);
    if (error==SVT_AV2_ERROR_NONE) error=satd_dump(&trial,new_bytes);
    if (error==SVT_AV2_ERROR_NONE) {
        const int accept=new_cost<old_cost;
        fprintf(stderr,"SATD_PACKET accepted=%d old_bytes=%zu new_bytes=%zu oldJ=%llu newJ=%llu old_trials=%llu new_trials=%llu\n",
            accept,*bytes,new_bytes,(unsigned long long)old_cost,(unsigned long long)new_cost,
            (unsigned long long)c->rd_stats.candidate_count,(unsigned long long)trial.rd_stats.candidate_count);
        if (accept) {
            svt_av2_picture_free(&c->recon);free(c->packet);free(c->coded_blocks);free(c->coded_modes);
            free(c->coded_coeffs);free(c->partition_decisions);trial.satd_angles=0u;*c=trial;*bytes=new_bytes;
            return SVT_AV2_ERROR_NONE;
        }
    }
    svt_av2_picture_free(&trial.recon);free(trial.packet);free(trial.coded_blocks);free(trial.coded_modes);
    free(trial.coded_coeffs);free(trial.partition_decisions);return error;
}
#endif
