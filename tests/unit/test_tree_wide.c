/* Apply the existing full-J, reversal and second-finalist fault audit to
 * the preserved old-domain seeds plus the expanded rectangular tree pool. */
int test_tree_finalists_original_main(int argc, char** argv);
#define main test_tree_finalists_original_main
#include "test_tree_finalists.c"
#undef main

int main(int argc, char** argv) {
    tree_limit=4u;
    if (argc>1) export_folder=argv[1];
    frame_test(64u,64u,8u,SVT_AV2_CHROMA_420,0,4u,1u,5u,2u,1u,1u);
    frame_test(72u,72u,10u,SVT_AV2_CHROMA_420,0,4u,0u,5u,2u,1u,1u);
    frame_test(64u,64u,10u,SVT_AV2_CHROMA_444,0,4u,1u,5u,2u,1u,1u);
    SVT_TEST_REPORT("test_tree_wide");
}
