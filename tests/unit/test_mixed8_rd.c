/* Exercise extended-plan IDs through the existing complete true-J,
 * old-domain seed, reverse-order and second-finalist fault audit. */
#define SVT_AV2_TEST_MIXED8 1
int test_tree_finalists_original_main(int argc,char** argv);
#define main test_tree_finalists_original_main
#include "test_tree_finalists.c"
#undef main

int main(int argc,char** argv) {
    const uint32_t format=argc>1?(uint32_t)strtoul(argv[1],NULL,10):0u;
    const uint32_t bd=format?10u:8u;
    const SvtAv2ChromaFormat chroma=format==2u?SVT_AV2_CHROMA_444:SVT_AV2_CHROMA_420;
    tree_limit=4u;
    if (argc>2) export_folder=argv[2];
    frame_test(64u,64u,bd,chroma,0,4u,1u,6u,2u,1u,1u);
    frame_test(72u,72u,bd,chroma,0,4u,0u,6u,2u,1u,1u);
    SVT_TEST_REPORT("test_mixed8_rd");
}
