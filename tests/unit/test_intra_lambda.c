#include "rd_frame.h"
#include "test_util.h"

int main(void) {
    /* Anchors from the independently compiled fixed AVM AI rdmult probe,
     * converted from its 16*SSE RD units to this encoder's 4*SSE units. */
    static const uint32_t anchors[][4] = {
        {8u,1u,40u,32u}, {8u,130u,1664u,54080u}, {8u,255u,61440u,73728000u},
        {10u,49u,160u,32u}, {10u,178u,6656u,54080u}, {10u,303u,245760u,73728000u}
    };
    for (size_t i=0u; i<sizeof(anchors)/sizeof(anchors[0]); ++i) {
        uint64_t lambda=0u; uint32_t qstep=0u;
        SVT_TEST_CHECK_EQ_INT(svt_av2_rd_lambda_ai(anchors[i][1],anchors[i][0],&lambda,&qstep),0);
        SVT_TEST_CHECK_EQ_INT(qstep,anchors[i][2]);
        SVT_TEST_CHECK_EQ_INT(lambda,anchors[i][3]);
    }
    for (uint32_t bd=8u; bd<=10u; bd+=2u) {
        uint64_t previous=0u;
        for (uint32_t qp=1u; qp<=255u; ++qp) {
            uint64_t original=0u, ai=0u; uint32_t q0=0u, q1=0u;
            const uint32_t qindex=qp+(bd==10u?48u:0u);
            SVT_TEST_CHECK_EQ_INT(svt_av2_rd_lambda(qindex,bd,&original,&q0),0);
            SVT_TEST_CHECK_EQ_INT(svt_av2_rd_lambda_ai(qindex,bd,&ai,&q1),0);
            SVT_TEST_CHECK_EQ_INT(q0,q1);
            SVT_TEST_CHECK(ai>=previous && ai>0u && original>=12u*ai && original<=14u*ai);
            uint64_t ai150=0u; uint32_t q150=0u;
            SVT_TEST_CHECK_EQ_INT(svt_av2_rd_lambda_ai150(qindex,bd,&ai150,&q150),0);
            SVT_TEST_CHECK_EQ_INT(q150,q1);
            SVT_TEST_CHECK_EQ_INT(ai150,(ai*6u+2u)/4u);
            previous=ai;
        }
    }
    uint64_t lambda=0u; uint32_t qstep=0u;
    SVT_TEST_CHECK(svt_av2_rd_lambda_ai(0u,8u,&lambda,&qstep)!=0);
    SVT_TEST_CHECK(svt_av2_rd_lambda_ai(1u,10u,&lambda,&qstep)!=0);
    SVT_TEST_CHECK(svt_av2_rd_lambda_ai(1u,12u,&lambda,&qstep)!=0);
    SVT_TEST_CHECK(svt_av2_rd_lambda_ai(1u,8u,NULL,&qstep)!=0);
    SVT_TEST_CHECK(svt_av2_rd_lambda_ai(1u,8u,&lambda,NULL)!=0);
    SVT_TEST_CHECK(svt_av2_rd_lambda_ai150(0u,8u,&lambda,&qstep)!=0);
    SVT_TEST_CHECK(svt_av2_rd_lambda_ai150(1u,8u,NULL,&qstep)!=0);
    SVT_TEST_CHECK(svt_av2_rd_lambda_ai150(1u,8u,&lambda,NULL)!=0);
    SVT_TEST_REPORT("test_intra_lambda");
}
