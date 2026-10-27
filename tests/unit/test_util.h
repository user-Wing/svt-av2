/*
 * Minimal, dependency-free test helpers shared by the SVT-AV2 unit tests.
 *
 * Each test translation unit is built as its own executable and has its own
 * main(), so a single header with file-scope counters is sufficient. No test
 * framework is pulled in: the project deliberately avoids third-party runtime
 * dependencies (plan section 4.2).
 */
#ifndef SVT_AV2_TEST_UTIL_H
#define SVT_AV2_TEST_UTIL_H

#include <stdio.h>
#include <string.h>

static int g_svt_av2_test_checks = 0;
static int g_svt_av2_test_failures = 0;

#define SVT_TEST_CHECK(cond)                                                                       \
    do {                                                                                           \
        ++g_svt_av2_test_checks;                                                                   \
        if (!(cond)) {                                                                             \
            ++g_svt_av2_test_failures;                                                             \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                        \
        }                                                                                          \
    } while (0)

#define SVT_TEST_CHECK_EQ_INT(actual, expected)                                                    \
    do {                                                                                           \
        long long svt_test_a = (long long)(actual);                                                \
        long long svt_test_e = (long long)(expected);                                              \
        ++g_svt_av2_test_checks;                                                                   \
        if (svt_test_a != svt_test_e) {                                                            \
            ++g_svt_av2_test_failures;                                                             \
            fprintf(stderr,                                                                        \
                    "FAIL %s:%d: %s == %s (got %lld, expected %lld)\n",                            \
                    __FILE__,                                                                      \
                    __LINE__,                                                                      \
                    #actual,                                                                       \
                    #expected,                                                                     \
                    svt_test_a,                                                                    \
                    svt_test_e);                                                                   \
        }                                                                                          \
    } while (0)

#define SVT_TEST_CHECK_EQ_STR(actual, expected)                                                    \
    do {                                                                                           \
        const char* svt_test_a = (actual);                                                         \
        const char* svt_test_e = (expected);                                                       \
        ++g_svt_av2_test_checks;                                                                   \
        if (svt_test_a == NULL || svt_test_e == NULL || strcmp(svt_test_a, svt_test_e) != 0) {     \
            ++g_svt_av2_test_failures;                                                             \
            fprintf(stderr,                                                                        \
                    "FAIL %s:%d: %s == %s (got \"%s\", expected \"%s\")\n",                        \
                    __FILE__,                                                                      \
                    __LINE__,                                                                      \
                    #actual,                                                                       \
                    #expected,                                                                     \
                    svt_test_a ? svt_test_a : "(null)",                                            \
                    svt_test_e ? svt_test_e : "(null)");                                           \
        }                                                                                          \
    } while (0)

/* Prints the summary and returns the process exit code. */
#define SVT_TEST_REPORT(name)                                                                      \
    do {                                                                                           \
        fprintf(stdout,                                                                           \
                "%s: %d checks, %d failures\n",                                                    \
                (name),                                                                            \
                g_svt_av2_test_checks,                                                             \
                g_svt_av2_test_failures);                                                          \
        return (g_svt_av2_test_failures == 0) ? 0 : 1;                                             \
    } while (0)

#endif /* SVT_AV2_TEST_UTIL_H */
