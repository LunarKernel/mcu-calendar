/* app 层端到端：假 port + 脚本化的 ESP8266，应答经环形缓冲 -> 状态机 -> 写 RTC。 */
#include "app.h"
#include "check.h"
#include "port.h"

#include <string.h>

/* ---- 假 port ---- */
static uint32_t g_now_ms;
static char g_rx[512];
static size_t g_rx_head, g_rx_tail;
static char g_last_tx[200];
static mc_datetime_t g_rtc;
static int g_rtc_set_count;
static bool g_rtc_fail;
static uint8_t g_fired_mask;

uint32_t port_millis(void)
{
    return g_now_ms;
}

bool port_rtc_get(mc_datetime_t *out)
{
    *out = g_rtc;
    return true;
}

bool port_rtc_set(const mc_datetime_t *dt)
{
    g_rtc_set_count++;
    if (g_rtc_fail) {
        return false;
    }
    g_rtc = *dt;
    return true;
}

void port_esp_write(const char *data, size_t len)
{
    if (len >= sizeof g_last_tx) {
        len = sizeof g_last_tx - 1;
    }
    memcpy(g_last_tx, data, len);
    g_last_tx[len] = '\0';
}

int port_esp_read_byte(void)
{
    if (g_rx_head == g_rx_tail) {
        return -1;
    }
    return (unsigned char)g_rx[g_rx_head++];
}

void port_on_countdown_fired(uint8_t channel)
{
    g_fired_mask |= (uint8_t)(1u << channel);
}

static void esp_reply(const char *text)
{
    size_t n = strlen(text);
    if (g_rx_head == g_rx_tail) {
        g_rx_head = g_rx_tail = 0;
    }
    memcpy(g_rx + g_rx_tail, text, n);
    g_rx_tail += n;
}

/* 模拟模块：看到什么指令回什么应答 */
static void esp_respond_to_last_tx(const char *time_line)
{
    if (strcmp(g_last_tx, "AT+CIPSNTPTIME?\r\n") == 0) {
        esp_reply(time_line);
        esp_reply("\r\nOK\r\n");
    } else if (strncmp(g_last_tx, "AT+CWJAP=", 9) == 0) {
        esp_reply("WIFI CONNECTED\r\nWIFI GOT IP\r\n\r\nOK\r\n");
    } else if (g_last_tx[0] != '\0') {
        esp_reply("\r\nOK\r\n");
    }
    g_last_tx[0] = '\0';
}

static void reset_world(void)
{
    g_now_ms = 0;
    g_rx_head = g_rx_tail = 0;
    g_last_tx[0] = '\0';
    memset(&g_rtc, 0, sizeof g_rtc);
    g_rtc_set_count = 0;
    g_rtc_fail = false;
    g_fired_mask = 0;
    app_init();
}

static void test_sync_writes_rtc(void)
{
    reset_world();
    CHECK_EQ(app_sync_state(), APP_SYNC_NONE);
    app_request_sync();
    CHECK_EQ(app_sync_state(), APP_SYNC_RUNNING);

    for (int i = 0; i < 50 && app_sync_state() == APP_SYNC_RUNNING; ++i) {
        esp_respond_to_last_tx("+CIPSNTPTIME:Sun Sep 20 19:00:05 2026");
        g_now_ms += 10;
        app_poll();
    }
    CHECK_EQ(app_sync_state(), APP_SYNC_OK);
    CHECK_EQ(g_rtc_set_count, 1);
    CHECK_EQ(g_rtc.date.year, 2026);
    CHECK_EQ(g_rtc.date.month, 9);
    CHECK_EQ(g_rtc.date.day, 20);
    CHECK_EQ(g_rtc.hour, 19);
    CHECK_EQ(g_rtc.second, 5);

    /* 完成后继续轮询不得重复写 RTC */
    app_poll();
    app_poll();
    CHECK_EQ(g_rtc_set_count, 1);
}

static void test_unsynced_never_touches_rtc(void)
{
    reset_world();
    app_request_sync();
    for (int i = 0; i < 5000 && app_sync_state() == APP_SYNC_RUNNING; ++i) {
        esp_respond_to_last_tx("+CIPSNTPTIME:Thu Jan 01 08:00:00 1970");
        g_now_ms += 10;
        app_poll();
    }
    CHECK_EQ(app_sync_state(), APP_SYNC_FAILED);
    CHECK_EQ(app_sync_error(), MC_SNTP_ERR_NOT_SYNCED);
    CHECK_EQ(g_rtc_set_count, 0);
}

static void test_rtc_write_failure(void)
{
    reset_world();
    g_rtc_fail = true;
    app_request_sync();
    for (int i = 0; i < 50 && app_sync_state() == APP_SYNC_RUNNING; ++i) {
        esp_respond_to_last_tx("+CIPSNTPTIME:Sun Sep 20 19:00:05 2026");
        g_now_ms += 10;
        app_poll();
    }
    CHECK_EQ(app_sync_state(), APP_SYNC_RTC_FAIL);
}

static void test_countdown_notifies_port(void)
{
    reset_world();
    mc_timer_bank_t *bank = app_timers();
    mc_countdown_set(&bank->countdown[2], 100);
    mc_countdown_start(&bank->countdown[2], port_millis());
    g_now_ms = 99;
    app_poll();
    CHECK_EQ(g_fired_mask, 0);
    g_now_ms = 100;
    app_poll();
    CHECK_EQ(g_fired_mask, 0x04);
}

int main(void)
{
    test_sync_writes_rtc();
    test_unsynced_never_touches_rtc();
    test_rtc_write_failure();
    test_countdown_notifies_port();
    CHECK_REPORT();
}
