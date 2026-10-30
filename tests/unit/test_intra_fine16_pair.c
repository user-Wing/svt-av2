/* Reuse only the old forced-encoding fixture; the new geometry oracle and
 * prefix/rollback checks below do not call the production selector. */
#ifndef M6_FINE16
#define M6_FINE16 0
#endif
#if M6_FINE16
int fine16_legacy_test_main(int argc,char** argv);
#else
int fine16_legacy_test_main(void);
#endif
#define main fine16_legacy_test_main
#include "test_intra_fine16.c"
#undef main
#include "block_geometry.h"
#include "tile_writer.h"

#if M6_FINE16_PAIR
static const uint32_t pair_quadrants[6][2]={{0u,1u},{0u,2u},{0u,3u},{1u,2u},{1u,3u},{2u,3u}};

static uint16_t pair_plan(uint32_t pair,uint32_t low,uint32_t high,uint32_t others) {
    return (uint16_t)(11096u+pair*4900u+(low-1u)*350u+(high-1u)*25u+others);
}

static void pair_geometry_tests(void) {
    static const uint32_t widths[5]={16u,8u,16u,8u,32u};
    static const uint32_t heights[5]={16u,8u,8u,16u,32u};
    SvtAv2FrameGeometry g;
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init_ex(&g,64u,64u,64u,SVT_AV2_CHROMA_420),0);
    uint32_t checked=0u,min_leaves=64u,max_leaves=0u,max_events=0u;
    for (uint32_t pair=0u;pair<6u;++pair) for (uint32_t low=1u;low<15u;++low)
        for (uint32_t high=1u;high<15u;++high) for (uint32_t others=0u;others<25u;++others) {
            const uint16_t decision=pair_plan(pair,low,high,others);
            SVT_TEST_CHECK_EQ_INT(decision,11096u+checked);
            const uint32_t lo=pair_quadrants[pair][0],hi=pair_quadrants[pair][1];
            uint32_t shapes[4]={0u},digits=others,expected=0u;
            for (uint32_t q=0u;q<4u;++q) {
                if (q==lo || q==hi) {
                    const uint32_t mask=q==lo?low:high;
                    expected+=4u;
                    for (uint32_t cell=0u;cell<4u;++cell) expected+=3u*((mask>>cell)&1u);
                } else {
                    shapes[q]=digits%5u;digits/=5u;
                    expected+=1024u/(widths[shapes[q]]*heights[shapes[q]]);
                }
            }
            Walk walk={0},tree={0};uint32_t events=0u;
            const SvtAv2BlockView root={0u,0u,64u,64u,NULL};
            SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_sb_plan(&g,0u,&decision,1u,collect,&walk),0);
            SVT_TEST_CHECK_EQ_INT(fine16_events(&g,&root,&decision,&tree,&events),0);
            SVT_TEST_CHECK_EQ_INT(walk.count,expected);
            SVT_TEST_CHECK_EQ_INT(tree.count,walk.count);
            SVT_TEST_CHECK(walk.count>=16u && walk.count<=58u && events<=115u);
            SVT_TEST_CHECK_EQ_INT(events,2u*walk.count-1u);
            uint8_t covered[64]={0};uint32_t chroma_area=0u;
            for (size_t i=0u;i<walk.count;++i) {
                const SvtAv2BlockView* b=walk.blocks+i;
                const int valid=b->w>=8u && b->h>=8u && b->x+b->w<=64u && b->y+b->h<=64u &&
                    !(b->x%8u) && !(b->y%8u) && !(b->w%8u) && !(b->h%8u);
                SVT_TEST_CHECK(valid);if (!valid) continue;
                SVT_TEST_CHECK_EQ_INT(b->x,tree.blocks[i].x);SVT_TEST_CHECK_EQ_INT(b->y,tree.blocks[i].y);
                SVT_TEST_CHECK_EQ_INT(b->w,tree.blocks[i].w);SVT_TEST_CHECK_EQ_INT(b->h,tree.blocks[i].h);
                const uint32_t q=b->y/32u*2u+b->x/32u;
                uint32_t w=widths[shapes[q]],h=heights[shapes[q]];
                if (q==lo || q==hi) {
                    const uint32_t cell=(b->y%32u/16u)*2u+b->x%32u/16u;
                    w=h=((q==lo?low:high)&(1u<<cell))?8u:16u;
                }
                SVT_TEST_CHECK_EQ_INT(b->w,w);SVT_TEST_CHECK_EQ_INT(b->h,h);
                SVT_TEST_CHECK_EQ_INT(b->x%w,0u);SVT_TEST_CHECK_EQ_INT(b->y%h,0u);
                /* Every top/left sample required by causal prediction is ready. */
                if (b->y) for (uint32_t x=b->x;x<b->x+b->w;x+=8u)
                    SVT_TEST_CHECK_EQ_INT(covered[(b->y/8u-1u)*8u+x/8u],1u);
                if (b->x) for (uint32_t y=b->y;y<b->y+b->h;y+=8u)
                    SVT_TEST_CHECK_EQ_INT(covered[y/8u*8u+b->x/8u-1u],1u);
                if (b->x && b->y) SVT_TEST_CHECK_EQ_INT(covered[(b->y/8u-1u)*8u+b->x/8u-1u],1u);
                for (uint32_t y=b->y;y<b->y+b->h;y+=8u) for (uint32_t x=b->x;x<b->x+b->w;x+=8u) {
                    const uint32_t at=y/8u*8u+x/8u;
                    SVT_TEST_CHECK_EQ_INT(covered[at],0u);covered[at]=1u;
                }
                SvtAv2BlockView uv;
                SVT_TEST_CHECK_EQ_INT(svt_av2_chroma_view(&g,b,&uv),0);
                SVT_TEST_CHECK_EQ_INT(uv.w,b->w/2u);SVT_TEST_CHECK_EQ_INT(uv.h,b->h/2u);
                SVT_TEST_CHECK(uv.w>=4u && uv.h>=4u);chroma_area+=uv.w*uv.h;
            }
            for (uint32_t i=0u;i<64u;++i) SVT_TEST_CHECK_EQ_INT(covered[i],1u);
            SVT_TEST_CHECK_EQ_INT(chroma_area,1024u);
            min_leaves=SVT_AV2_MIN(min_leaves,(uint32_t)walk.count);
            max_leaves=SVT_AV2_MAX(max_leaves,(uint32_t)walk.count);
            max_events=SVT_AV2_MAX(max_events,events);++checked;
        }
    SVT_TEST_CHECK_EQ_INT(checked,29400u);SVT_TEST_CHECK_EQ_INT(min_leaves,16u);
    SVT_TEST_CHECK_EQ_INT(max_leaves,58u);SVT_TEST_CHECK_EQ_INT(max_events,115u);
    const uint16_t invalid[3]={4095u,40496u,65535u};Walk untouched={0};
    for (uint32_t i=0u;i<3u;++i)
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_sb_plan(&g,0u,invalid+i,1u,collect,&untouched),SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(untouched.count,0u);
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&g,72u,72u,64u),0);
    const uint16_t partial[4]={11096u,0u,0u,40495u};
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_coded_plan(&g,partial,4u,collect,&untouched),SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(untouched.count,0u);
    /* A full second SB must use local 64x64 coordinates, not frame origin. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&g,128u,128u,64u),0);
    const uint16_t offset[4]={0u,40495u,0u,0u};Walk shifted={0};
    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_sb_plan(&g,1u,offset,4u,collect,&shifted),0);
    for (size_t i=0u;i<shifted.count;++i) {
        SVT_TEST_CHECK(shifted.blocks[i].x>=64u && shifted.blocks[i].x+shifted.blocks[i].w<=128u);
        SVT_TEST_CHECK(shifted.blocks[i].y+shifted.blocks[i].h<=64u);
    }
    printf("FINE16_PAIR_GEOMETRY plans=%u min_leaves=%u max_leaves=%u max_events=%u\n",
        checked,min_leaves,max_leaves,max_events);
}

static void pair_transaction_test(Forced* f,uint16_t decision,uint32_t adaptive) {
    SvtAv2FrameHeaderConfig cfg;
    svt_av2_frame_header_config_default(&cfg,64u,64u);
    cfg.bit_depth=f->source.bit_depth;cfg.chroma_format=(uint32_t)f->source.chroma_format;
    cfg.base_q_idx=100u+(cfg.bit_depth==10u?48u:0u);cfg.m5_intra_modes=4u;
    cfg.reduced_tx_set=0u;cfg.entropy_adaptation=adaptive;
    SvtAv2TileRateSession* session=NULL;SvtAv2TileRatePrefix* prefix=NULL;
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_create(&cfg,&session),0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_prefix_create(&prefix),0);
    if (!session || !prefix) {svt_av2_tile_rate_free(session);svt_av2_tile_rate_prefix_free(prefix);return;}
    uint64_t original=0u,repeat=0u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_trial(session,0u,decision,f->blocks,f->count,NULL,0u,0,&original),0);
    size_t leaves=0u;
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_prefix_begin(prefix,session,0u,decision,&leaves),0);
    SVT_TEST_CHECK_EQ_INT(leaves,f->count/3u);
    for (size_t leaf=0u;leaf<leaves;++leaf) {
        SvtAv2TileCoeffBlock* blocks=f->blocks+leaf*3u;
        uint64_t inlet=0u,after_error=0u;
        SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_prefix_trial(prefix,leaf,blocks,0,&inlet),0);
        ++blocks[1].x;
        SVT_TEST_CHECK(svt_av2_tile_rate_prefix_trial(prefix,leaf,blocks,1,&after_error)!=0);
        --blocks[1].x;
        SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_prefix_trial(prefix,leaf,blocks,0,&after_error),0);
        SVT_TEST_CHECK_EQ_INT(after_error,inlet);
        SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_prefix_trial(prefix,leaf,blocks,1,&repeat),0);
        SVT_TEST_CHECK_EQ_INT(repeat,inlet);
    }
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_prefix_finish(prefix,&repeat),0);
    SVT_TEST_CHECK_EQ_INT(repeat,original);
    ++f->blocks[1].x;
    SVT_TEST_CHECK(svt_av2_tile_rate_trial(session,0u,decision,f->blocks,f->count,NULL,0u,1,&repeat)!=0);
    --f->blocks[1].x;
    SVT_TEST_CHECK(svt_av2_tile_rate_trial(session,0u,40496u,f->blocks,f->count,NULL,0u,1,&repeat)!=0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_trial(session,0u,decision,f->blocks,f->count,NULL,0u,0,&repeat),0);
    SVT_TEST_CHECK_EQ_INT(repeat,original);
    SVT_TEST_CHECK_EQ_INT(svt_av2_tile_rate_trial(session,0u,decision,f->blocks,f->count,NULL,0u,1,&repeat),0);
    SVT_TEST_CHECK_EQ_INT(repeat,original);
    svt_av2_tile_rate_prefix_free(prefix);svt_av2_tile_rate_free(session);
}

int main(int argc,char** argv) {
    pair_geometry_tests();
    if (g_svt_av2_test_failures) SVT_TEST_REPORT("test_intra_fine16_pair");
    size_t emitted=0u,forced=0u;
    for (uint32_t format=0u;format<3u;++format) for (uint32_t geometry=0u;geometry<2u;++geometry) {
        if (geometry && format!=1u) continue;
        Forced* f=(Forced*)calloc(1u,sizeof(*f));
        SVT_TEST_CHECK(f!=NULL);if (!f) return 1;
        const uint32_t w=geometry?128u:64u,h=geometry?72u:64u;
        const uint32_t bd=format?10u:8u;
        const SvtAv2ChromaFormat chroma=format==2u?SVT_AV2_CHROMA_444:SVT_AV2_CHROMA_420;
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&f->source,w,h,bd,chroma),0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_picture_alloc_ex(&f->recon,w,h,bd,chroma),0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init_ex(&f->geometry,w,h,64u,chroma),0);
        SVT_TEST_CHECK_EQ_INT(svt_av2_recon_pool_init(&f->scratch,64u,64u),0);
        for (uint32_t p=0u;p<3u;++p) for (uint32_t y=0u;y<svt_av2_picture_plane_height(&f->source,p);++y)
            for (uint32_t x=0u;x<svt_av2_picture_plane_width(&f->source,p);++x)
                svt_av2_sample_store(f->source.planes[p]+(size_t)y*f->source.strides[p]+x*(bd>8u?2u:1u),bd,
                    (uint16_t)(((x*y*17u+x*43u+y*29u+p*53u)&255u)<<(bd-8u)));
        SvtAv2TransformPathState state;const uint32_t q=100u+(bd==10u?48u:0u);
        SVT_TEST_CHECK_EQ_INT(svt_av2_transform_path_init_bd(&state,&f->path,q,bd),0);
        for (uint32_t pair=0u;pair<6u;++pair) {
            uint32_t low=format==0u?1u:format==1u?7u:14u;
            uint32_t high=format==0u?14u:format==1u?8u:1u;
            uint32_t others=(pair*7u+format*11u)%25u;
            if (pair==0u && format==0u) {low=high=1u;others=0u;}
            if (pair==5u && format==2u) {low=high=14u;others=24u;}
            if (pair==2u && format==1u) {low=high=14u;others=6u;}
            const uint16_t plan=pair_plan(pair,low,high,others);
            for (uint32_t adaptive=0u;adaptive<2u;++adaptive) {
                uint16_t decisions[4]={plan,geometry?plan:0u,0u,0u};
                const size_t sb_count=(size_t)f->geometry.sb_cols*f->geometry.sb_rows;
                f->count=f->used=0u;
                SVT_TEST_CHECK_EQ_INT(svt_av2_recon_reset_frame(&f->recon,(uint16_t)(1u<<(bd-1u))),0);
                for (size_t sb=0u;sb<sb_count;++sb)
                    SVT_TEST_CHECK_EQ_INT(svt_av2_partition_traverse_sb_plan(&f->geometry,sb,decisions,sb_count,encode_leaf,f),0);
                if (!geometry) pair_transaction_test(f,plan,adaptive);
                uint8_t stream[262144];size_t bytes=0u;
                SVT_TEST_CHECK_EQ_INT(svt_av2_encode_still_picture_tx(w,h,bd,(uint32_t)chroma,q,
                    f->blocks,f->count,4u,decisions,sb_count,adaptive,2u,stream,sizeof(stream),&bytes),0);
                SVT_TEST_CHECK(bytes>0u);++forced;
                if (argc>1 && bytes) {
                    char label[96];snprintf(label,sizeof(label),"pair-f%u-s%u-plan%u-a%u-t2",format,geometry,(unsigned)plan,adaptive);
                    write_file(argv[1],label,"av2",stream,bytes);
                    uint8_t raw[98304];size_t raw_bytes=0u;
                    SVT_TEST_CHECK_EQ_INT(svt_av2_picture_export_raw(&f->recon,raw,sizeof(raw),&raw_bytes),0);
                    write_file(argv[1],label,"yuv",raw,raw_bytes);++emitted;
                }
            }
        }
        svt_av2_recon_pool_free(&f->scratch);svt_av2_picture_free(&f->source);svt_av2_picture_free(&f->recon);free(f);
    }
    SVT_TEST_CHECK_EQ_INT(forced,48u);
    if (argc>1) {SVT_TEST_CHECK_EQ_INT(emitted,48u);printf("EMITTED,%zu\n",emitted);}
    printf("FINE16_PAIR_FORCED count=%zu\n",forced);
    SVT_TEST_REPORT("test_intra_fine16_pair");
}
#else
int main(void) {
    SvtAv2FrameGeometry g;
    SVT_TEST_CHECK_EQ_INT(svt_av2_frame_geometry_init(&g,64u,64u,64u),0);
    for (uint32_t plan=11096u;plan<=40495u;++plan)
        SVT_TEST_CHECK_EQ_INT(svt_av2_partition_plan_validate_sb(&g,0u,(uint16_t)plan),SVT_AV2_ERROR_INVALID_PARAMETER);
    puts("FINE16_PAIR_DISABLED_REJECT plans=29400");
    SVT_TEST_REPORT("test_intra_fine16_pair");
}
#endif
