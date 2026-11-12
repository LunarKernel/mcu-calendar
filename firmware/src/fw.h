/* main.c 提供给固件其他文件的少数几样东西 */
#ifndef FW_H
#define FW_H

#include <stdint.h>

/* 蜂鸣器响 count 声，每声 on_ms、间隔 off_ms。不阻塞：由主循环按期限开关。重复调用以最后一次为准 */
void fw_beep_pattern(uint8_t count, uint32_t on_ms, uint32_t off_ms);
/* 响一声 ms 毫秒 */
void fw_beep(uint32_t ms);

#endif
