/* 日期时间与文本互转，固定格式「YYYY-MM-DD HH:MM:SS」（19 个字符）。
 * 用途：串口校时指令（PC 端 `date "+T%Y-%m-%d %H:%M:%S"` 直接发过来）、串口打印、屏上显示。
 * 不用 sscanf/snprintf：目标上那一套要拉进 newlib 的格式化代码，几 KB 换这么点事不值。 */
#ifndef MC_TEXT_H
#define MC_TEXT_H

#include <stdbool.h>
#include <stddef.h>

#include "mc/mc_date.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MC_DATETIME_TEXT_LEN 19u

/* 长度必须恰为 19、分隔符逐位对上、日期时间合法才返回 true；失败时 *out 不动 */
bool mc_datetime_parse(const char *s, size_t len, mc_datetime_t *out);
/* 写 19 个字符加结尾 '\0'。年份不在 0..9999 时年份位写 "----"（不用问号：「??-」是三字符组） */
void mc_datetime_format(const mc_datetime_t *dt, char out[MC_DATETIME_TEXT_LEN + 1u]);

#ifdef __cplusplus
}
#endif

#endif
