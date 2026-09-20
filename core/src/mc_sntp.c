#include "mc/mc_sntp.h"

#include <stdio.h>
#include <string.h>

enum {
    STEP_AT,
    STEP_ECHO_OFF,
    STEP_MODE,
    STEP_JOIN,
    STEP_CFG,
    STEP_QUERY,
};

#define AT_TIMEOUT_MS 1000u
#define AT_MAX_TRIES 3u
#define CMD_TIMEOUT_MS 2000u
#define JOIN_TIMEOUT_MS 20000u
#define QUERY_RETRY_DELAY_MS 1000u
#define QUERY_MAX_TRIES 15u

#define TIME_PREFIX "+CIPSNTPTIME:"

static bool deadline_passed(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0; /* 回绕安全，要求间隔 < 2^31 ms */
}

/* ESP-AT 字符串参数里的 " , \ 需要反斜杠转义 */
static size_t append_escaped(char *dst, size_t pos, size_t cap, const char *src)
{
    for (; *src != '\0'; ++src) {
        if (*src == '"' || *src == ',' || *src == '\\') {
            if (pos + 1 < cap) {
                dst[pos++] = '\\';
            }
        }
        if (pos + 1 < cap) {
            dst[pos++] = *src;
        }
    }
    dst[pos] = '\0';
    return pos;
}

static size_t append_raw(char *dst, size_t pos, size_t cap, const char *src)
{
    for (; *src != '\0' && pos + 1 < cap; ++src) {
        dst[pos++] = *src;
    }
    dst[pos] = '\0';
    return pos;
}

static void finish(mc_sntp_t *s, mc_sntp_status_t status, mc_sntp_error_t error)
{
    s->status = status;
    s->error = error;
    s->waiting_retry = false;
}

static void send_step(mc_sntp_t *s, uint32_t now_ms)
{
    char cmd[160];
    size_t n = 0;
    uint32_t timeout = CMD_TIMEOUT_MS;

    switch (s->step) {
    case STEP_AT:
        n = append_raw(cmd, n, sizeof cmd, "AT");
        timeout = AT_TIMEOUT_MS;
        break;
    case STEP_ECHO_OFF:
        n = append_raw(cmd, n, sizeof cmd, "ATE0");
        break;
    case STEP_MODE:
        n = append_raw(cmd, n, sizeof cmd, "AT+CWMODE=1");
        break;
    case STEP_JOIN:
        n = append_raw(cmd, n, sizeof cmd, "AT+CWJAP=\"");
        n = append_escaped(cmd, n, sizeof cmd, s->cfg.ssid);
        n = append_raw(cmd, n, sizeof cmd, "\",\"");
        n = append_escaped(cmd, n, sizeof cmd, s->cfg.password);
        n = append_raw(cmd, n, sizeof cmd, "\"");
        timeout = JOIN_TIMEOUT_MS;
        break;
    case STEP_CFG: {
        char head[32];
        snprintf(head, sizeof head, "AT+CIPSNTPCFG=1,%d,\"", (int)s->cfg.tz_hours);
        n = append_raw(cmd, n, sizeof cmd, head);
        n = append_escaped(cmd, n, sizeof cmd, s->cfg.server);
        n = append_raw(cmd, n, sizeof cmd, "\"");
        break;
    }
    case STEP_QUERY:
    default:
        n = append_raw(cmd, n, sizeof cmd, "AT+CIPSNTPTIME?");
        s->got_time = false;
        break;
    }
    n = append_raw(cmd, n, sizeof cmd, "\r\n");

    s->waiting_retry = false;
    s->deadline_ms = now_ms + timeout;
    s->write(s->write_ctx, cmd, n);
}

static void enter_step(mc_sntp_t *s, uint8_t step, uint32_t now_ms)
{
    s->step = step;
    s->retries = 0;
    send_step(s, now_ms);
}

/* 查询没拿到有效时间：还有次数就等一会儿再问，否则判未同步 */
static void query_retry_or_fail(mc_sntp_t *s, uint32_t now_ms)
{
    s->retries++;
    if (s->retries >= QUERY_MAX_TRIES) {
        finish(s, MC_SNTP_FAILED, MC_SNTP_ERR_NOT_SYNCED);
        return;
    }
    s->waiting_retry = true;
    s->deadline_ms = now_ms + QUERY_RETRY_DELAY_MS;
}

static void on_ok(mc_sntp_t *s, uint32_t now_ms)
{
    if (s->step == STEP_QUERY) {
        if (s->got_time) {
            finish(s, MC_SNTP_DONE, MC_SNTP_ERR_NONE);
        } else {
            query_retry_or_fail(s, now_ms);
        }
        return;
    }
    enter_step(s, (uint8_t)(s->step + 1), now_ms);
}

static void on_error(mc_sntp_t *s, uint32_t now_ms)
{
    if (s->step == STEP_QUERY) {
        query_retry_or_fail(s, now_ms);
    } else if (s->step == STEP_JOIN) {
        finish(s, MC_SNTP_FAILED, MC_SNTP_ERR_WIFI);
    } else {
        finish(s, MC_SNTP_FAILED, MC_SNTP_ERR_CMD);
    }
}

static void on_line(mc_sntp_t *s, const char *line, uint32_t now_ms)
{
    if (s->status != MC_SNTP_BUSY || s->waiting_retry) {
        return;
    }
    if (strcmp(line, "OK") == 0) {
        on_ok(s, now_ms);
    } else if (strcmp(line, "ERROR") == 0 || strcmp(line, "FAIL") == 0) {
        on_error(s, now_ms);
    } else if (s->step == STEP_QUERY && strncmp(line, TIME_PREFIX, sizeof TIME_PREFIX - 1) == 0) {
        mc_datetime_t dt;
        if (mc_sntp_parse_time(line, &dt) && dt.date.year >= MC_SNTP_MIN_VALID_YEAR) {
            s->result = dt;
            s->got_time = true;
        }
    }
    /* 其余（回显、WIFI CONNECTED、busy p... 等）忽略 */
}

void mc_sntp_init(mc_sntp_t *s, const mc_sntp_config_t *cfg, mc_sntp_write_fn write, void *ctx)
{
    memset(s, 0, sizeof *s);
    s->cfg = *cfg;
    s->write = write;
    s->write_ctx = ctx;
    s->status = MC_SNTP_IDLE;
}

void mc_sntp_start(mc_sntp_t *s, uint32_t now_ms)
{
    if (s->status == MC_SNTP_BUSY) {
        return;
    }
    s->status = MC_SNTP_BUSY;
    s->error = MC_SNTP_ERR_NONE;
    s->line_len = 0;
    s->line_overflow = false;
    enter_step(s, STEP_AT, now_ms);
}

void mc_sntp_feed(mc_sntp_t *s, uint8_t byte, uint32_t now_ms)
{
    if (byte == '\n') {
        bool overflow = s->line_overflow;
        if (s->line_len > 0 && s->line[s->line_len - 1] == '\r') {
            s->line_len--;
        }
        s->line[s->line_len] = '\0';
        s->line_len = 0;
        s->line_overflow = false;
        if (!overflow && s->line[0] != '\0') {
            on_line(s, s->line, now_ms);
        }
        return;
    }
    if (s->line_len + 1u < sizeof s->line) {
        s->line[s->line_len++] = (char)byte;
    } else {
        s->line_overflow = true; /* 超长行整行丢弃，避免把截断内容误判成应答 */
    }
}

void mc_sntp_poll(mc_sntp_t *s, uint32_t now_ms)
{
    if (s->status != MC_SNTP_BUSY || !deadline_passed(now_ms, s->deadline_ms)) {
        return;
    }
    if (s->waiting_retry) {
        send_step(s, now_ms);
        return;
    }
    /* 以下为应答超时 */
    switch (s->step) {
    case STEP_AT:
        s->retries++;
        if (s->retries >= AT_MAX_TRIES) {
            finish(s, MC_SNTP_FAILED, MC_SNTP_ERR_NO_MODULE);
        } else {
            send_step(s, now_ms);
        }
        break;
    case STEP_JOIN:
        finish(s, MC_SNTP_FAILED, MC_SNTP_ERR_WIFI);
        break;
    case STEP_QUERY:
        query_retry_or_fail(s, now_ms);
        break;
    default:
        finish(s, MC_SNTP_FAILED, MC_SNTP_ERR_CMD);
        break;
    }
}

mc_sntp_status_t mc_sntp_status(const mc_sntp_t *s)
{
    return s->status;
}

mc_sntp_error_t mc_sntp_error(const mc_sntp_t *s)
{
    return s->error;
}

bool mc_sntp_result(const mc_sntp_t *s, mc_datetime_t *out)
{
    if (s->status != MC_SNTP_DONE) {
        return false;
    }
    *out = s->result;
    return true;
}

static const char *skip_spaces(const char *p)
{
    while (*p == ' ') {
        ++p;
    }
    return p;
}

/* 读 1..max_digits 位十进制数；失败返回 NULL */
static const char *parse_uint(const char *p, int max_digits, int *out)
{
    int value = 0;
    int digits = 0;
    while (*p >= '0' && *p <= '9' && digits < max_digits) {
        value = value * 10 + (*p - '0');
        ++p;
        ++digits;
    }
    if (digits == 0) {
        return NULL;
    }
    *out = value;
    return p;
}

bool mc_sntp_parse_time(const char *line, mc_datetime_t *out)
{
    static const char k_months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const size_t prefix_len = sizeof TIME_PREFIX - 1;

    if (strncmp(line, TIME_PREFIX, prefix_len) != 0) {
        return false;
    }
    const char *p = skip_spaces(line + prefix_len);

    /* 星期：三个字母，跳过不用，星期由日期自己算 */
    if (strlen(p) < 3) {
        return false;
    }
    p = skip_spaces(p + 3);

    if (strlen(p) < 3) {
        return false;
    }
    int month = 0;
    for (int i = 0; i < 12; ++i) {
        if (strncmp(p, k_months + i * 3, 3) == 0) {
            month = i + 1;
            break;
        }
    }
    if (month == 0) {
        return false;
    }
    p = skip_spaces(p + 3);

    int day, hour, minute, second, year;
    if ((p = parse_uint(p, 2, &day)) == NULL) {
        return false;
    }
    p = skip_spaces(p);
    if ((p = parse_uint(p, 2, &hour)) == NULL || *p++ != ':') {
        return false;
    }
    if ((p = parse_uint(p, 2, &minute)) == NULL || *p++ != ':') {
        return false;
    }
    if ((p = parse_uint(p, 2, &second)) == NULL) {
        return false;
    }
    p = skip_spaces(p);
    if ((p = parse_uint(p, 4, &year)) == NULL) {
        return false;
    }

    mc_datetime_t dt;
    dt.date.year = (int16_t)year;
    dt.date.month = (uint8_t)month;
    dt.date.day = (uint8_t)day;
    dt.hour = (uint8_t)hour;
    dt.minute = (uint8_t)minute;
    dt.second = (uint8_t)second;
    if (!mc_datetime_valid(&dt)) {
        return false;
    }
    *out = dt;
    return true;
}
