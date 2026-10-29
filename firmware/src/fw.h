/* main.c 提供给固件其他文件的少数几样东西 */
#ifndef FW_H
#define FW_H

#include <stdint.h>

/* 蜂鸣器响 ms 毫秒，不阻塞：到点由主循环关掉。重复调用以最后一次为准 */
void fw_beep(uint32_t ms);

#endif
