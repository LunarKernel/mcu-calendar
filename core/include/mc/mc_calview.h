/* 万年历查阅：年月游标与月视图。对应选题表扩展功能 2（1900–2099 可查阅）。 */
#ifndef MC_CALVIEW_H
#define MC_CALVIEW_H

#include "mc/mc_date.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MC_CALVIEW_MIN_YEAR 1900
#define MC_CALVIEW_MAX_YEAR 2099

typedef struct {
    int16_t year;
    uint8_t month;
} mc_calview_t;

typedef struct {
    uint8_t first_weekday; /* 当月 1 日的星期，0=周日 */
    uint8_t days;          /* 当月天数 */
} mc_month_info_t;

/* 越界的初值会被钳到范围内 */
void mc_calview_init(mc_calview_t *v, int32_t year, uint8_t month);

/* 翻页。到 1900-01 / 2099-12 即停，不回绕；返回游标是否移动 */
bool mc_calview_step_month(mc_calview_t *v, int32_t delta);
bool mc_calview_step_year(mc_calview_t *v, int32_t delta);

mc_month_info_t mc_calview_month_info(const mc_calview_t *v);

/* 填 6x7 月视图，行优先、每行自周日起；无日期的格子为 0 */
void mc_calview_fill_grid(const mc_calview_t *v, uint8_t grid[42]);

#ifdef __cplusplus
}
#endif

#endif
