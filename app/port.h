/* 硬件边界：core/ 与 app/ 只通过这里碰硬件。
 * 这一组函数由驱动侧（B 同学：DS3231、蜂鸣器、ESP8266 串口）实现，
 * host 测试里由 tests/fake_port.c 实现。开发板型号确定前，上层代码不依赖任何 HAL 头文件。 */
#ifndef PORT_H
#define PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mc/mc_date.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 单调毫秒时基，SysTick 1 ms 累加即可（HAL_GetTick 满足） */
uint32_t port_millis(void);

/* RTC（DS3231）读写本地时间；I2C 失败返回 false */
bool port_rtc_get(mc_datetime_t *out);
bool port_rtc_set(const mc_datetime_t *dt);

/* ESP8266 串口。写可以阻塞发完（单条指令 < 160 字节）；
 * 读必须非阻塞：RX 中断或 DMA 把字节放进环形缓冲，这里取一个，没有则返回 -1。 */
void port_esp_write(const char *data, size_t len);
int port_esp_read_byte(void);

/* 倒计时通道到点通知（响蜂鸣器、刷新界面等），在主循环上下文调用 */
void port_on_countdown_fired(uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif
