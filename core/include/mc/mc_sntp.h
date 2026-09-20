/* ESP8266 AT 固件网络校时。对应选题表扩展功能 5。
 *
 * 非阻塞状态机：主循环里把串口收到的字节喂给 mc_sntp_feed()，并周期调用
 * mc_sntp_poll() 处理超时与重试；模块自己不碰串口，发数据走回调。
 *
 * AT 序列：AT -> ATE0 -> AT+CWMODE=1 -> AT+CWJAP -> AT+CIPSNTPCFG -> AT+CIPSNTPTIME?
 * 时区交给 ESP8266 处理，读回的已经是本地时间，可直接写 RTC。
 *
 * 失效机理备忘：CIPSNTPCFG 返回 OK 只表示配置生效，不表示已经对上时。
 * 此时查询会得到 "Thu Jan 01 00:00:00 1970"（加上时区偏移），所以年份早于
 * MC_SNTP_MIN_VALID_YEAR 的应答一律按「未同步」处理，隔一秒重问，不写 RTC。 */
#ifndef MC_SNTP_H
#define MC_SNTP_H

#include <stddef.h>

#include "mc/mc_date.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MC_SNTP_MIN_VALID_YEAR 2024
#define MC_SNTP_LINE_MAX 96

typedef enum {
    MC_SNTP_IDLE,
    MC_SNTP_BUSY,
    MC_SNTP_DONE,
    MC_SNTP_FAILED,
} mc_sntp_status_t;

typedef enum {
    MC_SNTP_ERR_NONE,
    MC_SNTP_ERR_NO_MODULE,  /* AT 无应答：接线、波特率、供电 */
    MC_SNTP_ERR_CMD,        /* 某条配置指令返回 ERROR 或超时 */
    MC_SNTP_ERR_WIFI,       /* CWJAP 失败：热点名、密码、信号 */
    MC_SNTP_ERR_NOT_SYNCED, /* 已联网但始终拿不到有效时间 */
} mc_sntp_error_t;

typedef void (*mc_sntp_write_fn)(void *ctx, const char *data, size_t len);

typedef struct {
    const char *ssid;
    const char *password;
    const char *server; /* 例如 "ntp.aliyun.com" */
    int8_t tz_hours;    /* 北京时间填 8 */
} mc_sntp_config_t;

typedef struct {
    mc_sntp_config_t cfg;
    mc_sntp_write_fn write;
    void *write_ctx;

    mc_sntp_status_t status;
    mc_sntp_error_t error;
    uint8_t step;
    uint8_t retries;
    bool waiting_retry; /* 处于两次重试之间的等待期 */
    bool got_time;      /* 本轮查询是否收到有效时间行 */
    uint32_t deadline_ms;
    mc_datetime_t result;

    char line[MC_SNTP_LINE_MAX];
    uint8_t line_len;
    bool line_overflow;
} mc_sntp_t;

/* cfg 里的字符串指针须在整个校时过程中保持有效 */
void mc_sntp_init(mc_sntp_t *s, const mc_sntp_config_t *cfg, mc_sntp_write_fn write, void *ctx);

/* 发起一次校时。BUSY 时调用无动作；DONE/FAILED 后可再次调用 */
void mc_sntp_start(mc_sntp_t *s, uint32_t now_ms);
void mc_sntp_feed(mc_sntp_t *s, uint8_t byte, uint32_t now_ms);
void mc_sntp_poll(mc_sntp_t *s, uint32_t now_ms);

mc_sntp_status_t mc_sntp_status(const mc_sntp_t *s);
mc_sntp_error_t mc_sntp_error(const mc_sntp_t *s);
/* 仅 DONE 时返回 true */
bool mc_sntp_result(const mc_sntp_t *s, mc_datetime_t *out);

/* 解析 "+CIPSNTPTIME:Sun Sep 20 12:34:56 2026"（asctime 格式，日为一位数时
 * 前面是两个空格）。只做格式与合法性检查，不判年份是否可信。 */
bool mc_sntp_parse_time(const char *line, mc_datetime_t *out);

#ifdef __cplusplus
}
#endif

#endif
