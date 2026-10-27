/* Link this harness with freshly compiled frozen AVM transform sources only. */
#include "transform.h"
#include "av2/common/idct.h"
#include <stdio.h>
#include <string.h>

extern void fwd_txfm_c(const int16_t*, tran_low_t*, int, TxfmParam*);
typedef void (*Inverse1d)(const int*, int*, int, int, int, int, int, int);
extern void inv_txfm_adst_size4_c(const int*, int*, int, int, int, int, int, int);
extern void inv_txfm_dct2_size4_c(const int*, int*, int, int, int, int, int, int);
extern void inv_txfm_adst_size8_c(const int*, int*, int, int, int, int, int, int);
extern void inv_txfm_dct2_size8_c(const int*, int*, int, int, int, int, int, int);
extern void inv_txfm_adst_size16_c(const int*, int*, int, int, int, int, int, int);
extern void inv_txfm_dct2_size16_c(const int*, int*, int, int, int, int, int, int);

int main(void) {
    uint32_t random = 104729u;
    uint64_t cases = 0u, samples = 0u;
    for (uint32_t bd = 8u; bd <= 10u; bd += 2u)
        for (uint32_t side = 4u, size = 0u; side <= 16u; side *= 2u, ++size)
            for (uint32_t type = 1u; type <= 3u; ++type)
            for (uint32_t trial = 0u; trial < 1200u; ++trial) {
                int16_t residual[256], actual_inverse[256];
                int32_t actual[256], expected[256], dqcoeff[256];
                uint16_t decoded[256];
                const uint32_t maximum = (1u << bd) - 1u;
                const int middle = 1 << (bd - 1u);
                for (uint32_t i = 0u; i < side * side; ++i) {
                    random = random * 1664525u + 1013904223u;
                    residual[i] = (int16_t)((int)((random >> 10u) % (2u * maximum + 1u)) - (int)maximum);
                    if (trial < 6u)
                        residual[i] = (int16_t)(trial == 0u ? 0 : trial == 1u ? (int)maximum :
                            trial == 2u ? -(int)maximum : trial == 3u ? (i & 1u ? (int)maximum : -(int)maximum) :
                            trial == 4u ? (i == 0u ? (int)maximum : 0) : (i == side * side - 1u ? -(int)maximum : 0));
                }
                TxfmParam param = {0};
                param.tx_type = (TX_TYPE)type; param.tx_size = (TX_SIZE)size;
                param.bd = (int)bd;
                fwd_txfm_c(residual, expected, (int)side, &param);
                if (svt_av2_fwd_transform_type_bd(residual, actual, side, side, bd, type) != 0 ||
                    memcmp(actual, expected, side * side * sizeof(*actual)) != 0) {
                    printf("FAIL forward bd=%u side=%u type=%u trial=%u\n", bd, side, type, trial); return 1;
                }
                for (uint32_t i = 0u; i < side * side; ++i) {
                    random = random * 1664525u + 1013904223u;
                    dqcoeff[i] = (int32_t)((random >> 10u) % 524289u) - 262144;
                    if (trial < 7u)
                        dqcoeff[i] = trial == 0u ? 0 : trial == 1u ? 262144 : trial == 2u ? -262144 :
                            trial == 3u ? (i == 0u ? 12345 : 0) : trial == 4u ? (i == 1u ? -12345 : 0) :
                            trial == 5u ? (i == side * side - 1u ? 4197 : 0) : (i & 1u ? 131071 : -131072);
                    decoded[i] = (uint16_t)middle;
                }
                inv_txfm_c(dqcoeff, decoded, (int)side, &param);
                if (svt_av2_inv_transform_type_bd(dqcoeff, actual_inverse, side, side, bd, type) != 0) return 1;
                int clamped[256], tmp[256], inverse[256], first, second;
                const int lo = -(1 << (bd + 7u)), hi = (1 << (bd + 7u)) - 1;
                for (uint32_t i = 0u; i < side * side; ++i)
                    clamped[i] = dqcoeff[i] < lo ? lo : dqcoeff[i] > hi ? hi : dqcoeff[i];
                svt_av2_tx_inv_shifts(side, side, &first, &second);
                const Inverse1d adst = size == 0u ? inv_txfm_adst_size4_c :
                    size == 1u ? inv_txfm_adst_size8_c : inv_txfm_adst_size16_c;
                const Inverse1d dct = size == 0u ? inv_txfm_dct2_size4_c :
                    size == 1u ? inv_txfm_dct2_size8_c : inv_txfm_dct2_size16_c;
                const Inverse1d horizontal = type == 1u ? dct : adst;
                const Inverse1d vertical = type == 2u ? dct : adst;
                horizontal(clamped, tmp, first, (int)side, 0, 0, lo, hi);
                vertical(tmp, inverse, second, (int)side, 0, 0, -(1 << bd), (1 << bd) - 1);
                for (uint32_t i = 0u; i < side * side; ++i) {
                    if (actual_inverse[i] != inverse[i]) {
                        printf("FAIL inverse residual bd=%u side=%u trial=%u sample=%u\n", bd, side, trial, i);
                        return 1;
                    }
                    const int sample = middle + actual_inverse[i];
                    const uint16_t clipped = (uint16_t)(sample < 0 ? 0 : sample > (int)maximum ? maximum : (uint32_t)sample);
                    if (clipped != decoded[i]) {
                        printf("FAIL inverse bd=%u side=%u trial=%u sample=%u expected=%u got=%u\n",
                            bd, side, trial, i, decoded[i], clipped); return 1;
                    }
                }
                cases += 2u; samples += 2u * side * side;
            }
    printf("PASS primary TX: %llu cases, %llu samples\n", (unsigned long long)cases, (unsigned long long)samples);
    return 0;
}
