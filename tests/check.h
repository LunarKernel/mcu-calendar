/* 极简测试宏：失败不中断，main 返回失败计数，供 CTest 判定。 */
#ifndef CHECK_H
#define CHECK_H

#include <stdio.h>

static int g_check_failures = 0;

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);   \
            g_check_failures++;                                               \
        }                                                                     \
    } while (0)

#define CHECK_EQ(actual, expected)                                            \
    do {                                                                      \
        long long a_ = (long long)(actual);                                   \
        long long e_ = (long long)(expected);                                 \
        if (a_ != e_) {                                                       \
            printf("%s:%d: %s = %lld, expected %lld\n", __FILE__, __LINE__,   \
                   #actual, a_, e_);                                          \
            g_check_failures++;                                               \
        }                                                                     \
    } while (0)

#define CHECK_REPORT()                                                        \
    do {                                                                      \
        printf("%s: %d failure(s)\n", __FILE__, g_check_failures);            \
        return g_check_failures == 0 ? 0 : 1;                                 \
    } while (0)

#endif
