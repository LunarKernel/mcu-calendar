#include "app.h"

#include "app_config.h"
#include "port.h"

static mc_sntp_t s_sntp;
static mc_timer_bank_t s_timers;
static app_sync_state_t s_sync_state;

static void esp_write(void *ctx, const char *data, size_t len)
{
    (void)ctx;
    port_esp_write(data, len);
}

void app_init(void)
{
    static const mc_sntp_config_t k_cfg = {
        APP_WIFI_SSID,
        APP_WIFI_PASSWORD,
        APP_NTP_SERVER,
        APP_TZ_HOURS,
    };
    mc_sntp_init(&s_sntp, &k_cfg, esp_write, NULL);
    mc_timer_bank_init(&s_timers);
    s_sync_state = APP_SYNC_NONE;
}

void app_request_sync(void)
{
    if (s_sync_state == APP_SYNC_RUNNING) {
        return;
    }
    s_sync_state = APP_SYNC_RUNNING;
    mc_sntp_start(&s_sntp, port_millis());
}

static void poll_sync(uint32_t now_ms)
{
    int byte;
    while ((byte = port_esp_read_byte()) >= 0) {
        mc_sntp_feed(&s_sntp, (uint8_t)byte, now_ms);
    }
    mc_sntp_poll(&s_sntp, now_ms);

    if (s_sync_state != APP_SYNC_RUNNING) {
        return;
    }
    mc_datetime_t dt;
    if (mc_sntp_result(&s_sntp, &dt)) {
        s_sync_state = port_rtc_set(&dt) ? APP_SYNC_OK : APP_SYNC_RTC_FAIL;
    } else if (mc_sntp_status(&s_sntp) == MC_SNTP_FAILED) {
        s_sync_state = APP_SYNC_FAILED;
    }
}

void app_poll(void)
{
    uint32_t now_ms = port_millis();

    poll_sync(now_ms);

    uint8_t fired = mc_timer_bank_poll(&s_timers, now_ms);
    for (uint8_t ch = 0; ch < MC_COUNTDOWN_CHANNELS; ++ch) {
        if (fired & (1u << ch)) {
            port_on_countdown_fired(ch);
        }
    }
}

app_sync_state_t app_sync_state(void)
{
    return s_sync_state;
}

mc_sntp_error_t app_sync_error(void)
{
    return mc_sntp_error(&s_sntp);
}

mc_timer_bank_t *app_timers(void)
{
    return &s_timers;
}
