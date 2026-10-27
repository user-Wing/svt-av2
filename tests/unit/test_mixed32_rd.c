/* Reuse the complete true-J, reversal, capacity and transform-fault audit. */
int test_m6_rd_original_main(int argc, char** argv);
#define main test_m6_rd_original_main
#include "test_m6_rd.c"
#undef main

int main(int argc, char** argv) {
    const uint32_t format=argc>1?(uint32_t)strtoul(argv[1],NULL,10):0u;
    const uint32_t bd=format?10u:8u;
    const SvtAv2ChromaFormat chroma=format==2u?SVT_AV2_CHROMA_444:SVT_AV2_CHROMA_420;
    frame_test(64u,64u,bd,chroma,0,4u,1u,5u,2u,1u,1u);
    frame_test(72u,72u,bd,chroma,0,4u,0u,5u,2u,1u,1u);
    SVT_TEST_REPORT("test_mixed32_rd");
}
