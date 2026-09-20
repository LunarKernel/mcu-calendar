#include "check.h"
#include "mc/mc_sntp.h"

#include <string.h>

/* 记录模块发出的最后一条指令与发送总次数 */
static char g_last_tx[200];
static int g_tx_count;

static void capture_write(void *ctx, const char *data, size_t len)
{
    (void)ctx;
    if (len >= sizeof g_last_tx) {
        len = sizeof g_last_tx - 1;
    }
    memcpy(g_last_tx, data, len);
    g_last_tx[len] = '\0';
    g_tx_count++;
}

static void feed(mc_sntp_t *s, const char *text, uint32_t now_ms)
{
    for (; *text != '\0'; ++text) {
        mc_sntp_feed(s, (uint8_t)*text, now_ms);
    }
}

static const mc_sntp_config_t k_cfg = {"my,phone", "p\"w\\d", "ntp.aliyun.com", 8};

static void begin(mc_sntp_t *s, uint32_t now_ms)
{
    g_tx_count = 0;
    g_last_tx[0] = '\0';
    mc_sntp_init(s, &k_cfg, capture_write, NULL);
    mc_sntp_start(s, now_ms);
}

/* 走到 QUERY 步之前的公共前段 */
static void run_until_query(mc_sntp_t *s)
{
    begin(s, 0);
    CHECK(strcmp(g_last_tx, "AT\r\n") == 0);
    feed(s, "AT\r\r\n\r\nOK\r\n", 10); /* 回显未关时的真实形态 */
    CHECK(strcmp(g_last_tx, "ATE0\r\n") == 0);
    feed(s, "ATE0\r\r\n\r\nOK\r\n", 20);
    CHECK(strcmp(g_last_tx, "AT+CWMODE=1\r\n") == 0);
    feed(s, "\r\nOK\r\n", 30);
    CHECK(strcmp(g_last_tx, "AT+CWJAP=\"my\\,phone\",\"p\\\"w\\\\d\"\r\n") == 0);
    feed(s, "WIFI CONNECTED\r\nWIFI GOT IP\r\n\r\nOK\r\n", 5000);
    CHECK(strcmp(g_last_tx, "AT+CIPSNTPCFG=1,8,\"ntp.aliyun.com\"\r\n") == 0);
    feed(s, "\r\nOK\r\n", 5010);
    CHECK(strcmp(g_last_tx, "AT+CIPSNTPTIME?\r\n") == 0);
    CHECK_EQ(mc_sntp_status(s), MC_SNTP_BUSY);
}

static void test_parse(void)
{
    mc_datetime_t dt;
    CHECK(mc_sntp_parse_time("+CIPSNTPTIME:Sun Sep 20 12:34:56 2026", &dt));
    CHECK_EQ(dt.date.year, 2026);
    CHECK_EQ(dt.date.month, 9);
    CHECK_EQ(dt.date.day, 20);
    CHECK_EQ(dt.hour, 12);
    CHECK_EQ(dt.minute, 34);
    CHECK_EQ(dt.second, 56);

    /* asctime：一位数的日前面补空格 */
    CHECK(mc_sntp_parse_time("+CIPSNTPTIME:Sat Sep  5 01:02:03 2026", &dt));
    CHECK_EQ(dt.date.day, 5);
    CHECK(mc_sntp_parse_time("+CIPSNTPTIME:Thu Jan 01 08:00:00 1970", &dt));
    CHECK_EQ(dt.date.year, 1970);

    CHECK(!mc_sntp_parse_time("CIPSNTPTIME:Sun Sep 20 12:34:56 2026", &dt));
    CHECK(!mc_sntp_parse_time("+CIPSNTPTIME:Sun Foo 20 12:34:56 2026", &dt));
    CHECK(!mc_sntp_parse_time("+CIPSNTPTIME:Sun Feb 30 12:34:56 2026", &dt));
    CHECK(!mc_sntp_parse_time("+CIPSNTPTIME:Sun Sep 20 25:34:56 2026", &dt));
    CHECK(!mc_sntp_parse_time("+CIPSNTPTIME:Sun Sep 20 12:34 2026", &dt));
    CHECK(!mc_sntp_parse_time("+CIPSNTPTIME:Sun Sep 20 12:34:56", &dt));
    CHECK(!mc_sntp_parse_time("+CIPSNTPTIME:", &dt));
    CHECK(!mc_sntp_parse_time("+CIPSNTPTIME:Su", &dt));
}

static void test_happy_path(void)
{
    mc_sntp_t s;
    mc_datetime_t dt;
    run_until_query(&s);
    CHECK(!mc_sntp_result(&s, &dt));
    feed(&s, "+CIPSNTPTIME:Sun Sep 20 12:34:56 2026\r\nOK\r\n", 5020);
    CHECK_EQ(mc_sntp_status(&s), MC_SNTP_DONE);
    CHECK(mc_sntp_result(&s, &dt));
    CHECK_EQ(dt.date.year, 2026);
    CHECK_EQ(dt.second, 56);
    CHECK_EQ(g_tx_count, 6);
}

static void test_not_synced_then_ok(void)
{
    /* 配置刚生效时 ESP 返回 1970，应等 1 s 重问，而不是把 1970 写进 RTC */
    mc_sntp_t s;
    mc_datetime_t dt;
    run_until_query(&s);
    int tx_before = g_tx_count;
    feed(&s, "+CIPSNTPTIME:Thu Jan 01 08:00:00 1970\r\nOK\r\n", 6000);
    CHECK_EQ(mc_sntp_status(&s), MC_SNTP_BUSY);
    mc_sntp_poll(&s, 6999);
    CHECK_EQ(g_tx_count, tx_before); /* 未到重试时刻 */
    feed(&s, "OK\r\n", 6500);        /* 等待期内的杂散应答不得推进状态 */
    CHECK_EQ(mc_sntp_status(&s), MC_SNTP_BUSY);
    mc_sntp_poll(&s, 7000);
    CHECK_EQ(g_tx_count, tx_before + 1);
    CHECK(strcmp(g_last_tx, "AT+CIPSNTPTIME?\r\n") == 0);
    feed(&s, "+CIPSNTPTIME:Sun Sep 20 12:34:58 2026\r\nOK\r\n", 7010);
    CHECK_EQ(mc_sntp_status(&s), MC_SNTP_DONE);
    CHECK(mc_sntp_result(&s, &dt));
    CHECK_EQ(dt.second, 58);
}

static void test_never_synced(void)
{
    mc_sntp_t s;
    run_until_query(&s);
    uint32_t now = 6000;
    for (int i = 0; i < 40 && mc_sntp_status(&s) == MC_SNTP_BUSY; ++i) {
        feed(&s, "+CIPSNTPTIME:Thu Jan 01 08:00:00 1970\r\nOK\r\n", now);
        now += 1000;
        mc_sntp_poll(&s, now);
    }
    CHECK_EQ(mc_sntp_status(&s), MC_SNTP_FAILED);
    CHECK_EQ(mc_sntp_error(&s), MC_SNTP_ERR_NOT_SYNCED);
}

static void test_no_module(void)
{
    mc_sntp_t s;
    begin(&s, 0);
    mc_sntp_poll(&s, 999);
    CHECK_EQ(g_tx_count, 1);
    mc_sntp_poll(&s, 1000);
    CHECK_EQ(g_tx_count, 2);
    mc_sntp_poll(&s, 2000);
    CHECK_EQ(g_tx_count, 3);
    mc_sntp_poll(&s, 3000);
    CHECK_EQ(mc_sntp_status(&s), MC_SNTP_FAILED);
    CHECK_EQ(mc_sntp_error(&s), MC_SNTP_ERR_NO_MODULE);

    /* 失败后可重新发起 */
    mc_sntp_start(&s, 4000);
    CHECK_EQ(mc_sntp_status(&s), MC_SNTP_BUSY);
    CHECK_EQ(g_tx_count, 4);
}

static void test_wifi_fail(void)
{
    mc_sntp_t s;
    begin(&s, 0);
    feed(&s, "OK\r\n", 1);
    feed(&s, "OK\r\n", 2);
    feed(&s, "OK\r\n", 3);
    feed(&s, "+CWJAP:3\r\n\r\nFAIL\r\n", 8000);
    CHECK_EQ(mc_sntp_status(&s), MC_SNTP_FAILED);
    CHECK_EQ(mc_sntp_error(&s), MC_SNTP_ERR_WIFI);

    /* CWJAP 无应答超时同样判 WIFI */
    begin(&s, 0);
    feed(&s, "OK\r\nOK\r\nOK\r\n", 1);
    mc_sntp_poll(&s, 20000);
    CHECK_EQ(mc_sntp_status(&s), MC_SNTP_BUSY);
    mc_sntp_poll(&s, 20001);
    CHECK_EQ(mc_sntp_status(&s), MC_SNTP_FAILED);
    CHECK_EQ(mc_sntp_error(&s), MC_SNTP_ERR_WIFI);
}

static void test_cmd_error_and_overflow(void)
{
    mc_sntp_t s;
    begin(&s, 0);
    feed(&s, "OK\r\nOK\r\n", 1);
    feed(&s, "ERROR\r\n", 2); /* CWMODE 被拒 */
    CHECK_EQ(mc_sntp_status(&s), MC_SNTP_FAILED);
    CHECK_EQ(mc_sntp_error(&s), MC_SNTP_ERR_CMD);

    /* 超长行整行丢弃：行尾恰为 "OK" 也不得被当成应答 */
    begin(&s, 0);
    for (int i = 0; i < 300; ++i) {
        mc_sntp_feed(&s, 'x', 1);
    }
    feed(&s, "\r\n", 1);
    CHECK_EQ(g_tx_count, 1);
    feed(&s, "OK\r\n", 2); /* 丢弃之后恢复正常收行 */
    CHECK_EQ(g_tx_count, 2);
}

static void test_tick_wraparound(void)
{
    mc_sntp_t s;
    begin(&s, 0xFFFFFE00u); /* 期限落在回绕之后 */
    mc_sntp_poll(&s, 0xFFFFFFFFu);
    CHECK_EQ(g_tx_count, 1);
    mc_sntp_poll(&s, 0x000001E8u); /* 0xFFFFFE00 + 1000 */
    CHECK_EQ(g_tx_count, 2);
}

int main(void)
{
    test_parse();
    test_happy_path();
    test_not_synced_then_ok();
    test_never_synced();
    test_no_module();
    test_wifi_fail();
    test_cmd_error_and_overflow();
    test_tick_wraparound();
    CHECK_REPORT();
}
