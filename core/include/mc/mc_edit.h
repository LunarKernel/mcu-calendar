/* 时间设置的编辑逻辑：六个字段轮流选中，逐一加减。对应选题表基本功能 2 的「时间设置」。
 *
 * 每个字段在自己的范围内回绕（59 加一回 0，1 月减一回 12 月），不向相邻字段进位：
 * 设置时人是一格一格拨的，进位反而让刚拨好的字段变掉。
 * 改了年或月之后，日超出当月天数就钳到月末（1 月 31 日拨到 2 月 → 2 月 28/29 日），
 * 所以任何时刻 dt 都是合法日期时间，可以直接写进 RTC。
 * 纯逻辑，不读时钟、不碰按键与屏。 */
#ifndef MC_EDIT_H
#define MC_EDIT_H

#include <stdint.h>

#include "mc/mc_date.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 年份范围：下限避开 1970 之前（32 位无符号时间戳表示不了），上限与万年历一致 */
#define MC_EDIT_MIN_YEAR 2000
#define MC_EDIT_MAX_YEAR 2099

typedef enum {
    MC_EDIT_YEAR,
    MC_EDIT_MONTH,
    MC_EDIT_DAY,
    MC_EDIT_HOUR,
    MC_EDIT_MINUTE,
    MC_EDIT_SECOND,
    MC_EDIT_FIELDS,
} mc_edit_field_t;

typedef struct {
    mc_datetime_t dt;
    mc_edit_field_t field; /* 当前选中的字段 */
} mc_edit_t;

/* 从 now 开始编辑，选中年。now 非法或年份越界时从 MC_EDIT_MIN_YEAR-01-01 00:00:00 起 */
void mc_edit_begin(mc_edit_t *e, const mc_datetime_t *now);
void mc_edit_next_field(mc_edit_t *e); /* 秒之后回到年 */
void mc_edit_step(mc_edit_t *e, int32_t delta); /* 选中字段加 delta（可负），范围内回绕 */

#ifdef __cplusplus
}
#endif

#endif
