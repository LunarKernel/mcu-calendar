#include <string.h>

#include "check.h"
#include "mc/mc_text.h"

static bool parse(const char *s, mc_datetime_t *out)
{
    return mc_datetime_parse(s, strlen(s), out);
}

static void test_parse_ok(void)
{
    mc_datetime_t dt;
    CHECK(parse("2026-10-29 16:45:07", &dt));
    CHECK_EQ(dt.date.year, 2026);
    CHECK_EQ(dt.date.month, 10);
    CHECK_EQ(dt.date.day, 29);
    CHECK_EQ(dt.hour, 16);
    CHECK_EQ(dt.minute, 45);
    CHECK_EQ(dt.second, 7);

    CHECK(parse("2000-02-29 00:00:00", &dt)); /* 闰日 */
    CHECK(parse("1900-01-01 23:59:59", &dt));
}

static void test_parse_rejects(void)
{
    mc_datetime_t dt = {{2001, 1, 1}, 1, 1, 1};
    CHECK(!parse("2026-10-29 16:45:7", &dt));   /* 少一位 */
    CHECK(!parse("2026-10-29 16:45:077", &dt)); /* 多一位 */
    CHECK(!parse("2026/10/29 16:45:07", &dt));  /* 分隔符 */
    CHECK(!parse("2026-10-29T16:45:07", &dt));
    CHECK(!parse("2026-1a-29 16:45:07", &dt)); /* 非数字 */
    CHECK(!parse("2026-13-01 00:00:00", &dt)); /* 月越界 */
    CHECK(!parse("2026-02-29 00:00:00", &dt)); /* 平年无 2 月 29 日 */
    CHECK(!parse("2026-10-29 24:00:00", &dt));
    CHECK(!parse("2026-10-29 23:60:00", &dt));
    CHECK(!parse("2026-10-29 23:59:60", &dt));
    CHECK(!mc_datetime_parse("2026-10-29 16:45:07", 18, &dt)); /* 长度以参数为准 */
    /* 失败不改 *out */
    CHECK_EQ(dt.date.year, 2001);
    CHECK_EQ(dt.second, 1);
}

static void test_format(void)
{
    char buf[MC_DATETIME_TEXT_LEN + 1u];
    const mc_datetime_t a = {{2026, 1, 5}, 7, 3, 9};
    mc_datetime_format(&a, buf);
    CHECK(strcmp(buf, "2026-01-05 07:03:09") == 0);

    const mc_datetime_t b = {{-1, 12, 31}, 23, 59, 59};
    mc_datetime_format(&b, buf);
    CHECK(strcmp(buf, "-----12-31 23:59:59") == 0);
}

static void test_round_trip(void)
{
    /* 格式化的结果再解析回来必须原样，期望值是手写的字面量而不是被测函数的输出 */
    mc_datetime_t dt;
    char buf[MC_DATETIME_TEXT_LEN + 1u];
    CHECK(parse("2099-12-31 23:59:58", &dt));
    mc_datetime_format(&dt, buf);
    CHECK(strcmp(buf, "2099-12-31 23:59:58") == 0);
}

int main(void)
{
    test_parse_ok();
    test_parse_rejects();
    test_format();
    test_round_trip();
    CHECK_REPORT();
}
