/* 应用层骨架：把 core 的各状态机接到 port 上。全部在主循环上下文运行。
 *
 * 中断分层约定：
 *   中断里只做三件事——累加毫秒时基、把串口字节放进环形缓冲、给外部触发抓时间戳；
 *   解析、状态机、I2C、刷屏一律在 app_poll() 里做。
 *   这样走时与秒表的精度只取决于时基中断，不受 DHT11、刷屏、AT 应答等慢操作影响。 */
#ifndef APP_H
#define APP_H

#include "mc/mc_sntp.h"
#include "mc/mc_timers.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_SYNC_NONE,     /* 本次上电还没校过时 */
    APP_SYNC_RUNNING,
    APP_SYNC_OK,       /* 已写入 RTC */
    APP_SYNC_FAILED,   /* 原因见 app_sync_error() */
    APP_SYNC_RTC_FAIL, /* 拿到了时间但写 RTC 失败 */
} app_sync_state_t;

void app_init(void);
void app_poll(void); /* 主循环里反复调用，不阻塞 */

void app_request_sync(void); /* 「校时」键 */
app_sync_state_t app_sync_state(void);
mc_sntp_error_t app_sync_error(void);

/* 秒表与倒计时通道，界面与外部触发都经它操作 */
mc_timer_bank_t *app_timers(void);

#ifdef __cplusplus
}
#endif

#endif
