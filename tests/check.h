/* Minimalni testovaci makra: CHECK, CHECK_EQ, CHECK_DONE. */
#ifndef PICOPAD_TESTS_CHECK_H
#define PICOPAD_TESTS_CHECK_H

#include <stdio.h>

static int check_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, \
                    #cond);                                                  \
            check_failures++;                                                \
        }                                                                    \
    } while (0)

#define CHECK_EQ(a, b)                                                       \
    do {                                                                     \
        long long check_a_ = (long long)(a);                                 \
        long long check_b_ = (long long)(b);                                 \
        if (check_a_ != check_b_) {                                          \
            fprintf(stderr, "%s:%d: CHECK_EQ failed: %s != %s (%lld vs %lld)\n", \
                    __FILE__, __LINE__, #a, #b, check_a_, check_b_);         \
            check_failures++;                                                \
        }                                                                    \
    } while (0)

#define CHECK_DONE() (check_failures ? 1 : 0)

#endif
