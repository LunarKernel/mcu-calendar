/* 逐日输出 1900-01-01…2100-12-31 的农历换算结果，供 verify_lunar.py 与独立实现比对。
 * 行格式：YYYY-MM-DD 农历年 月 日 闰(0/1) */
#include <stdio.h>

#include "mc/mc_lunar.h"

int main(void)
{
    int32_t start = mc_days_from_civil(1900, 1, 1);
    int32_t end = mc_days_from_civil(2100, 12, 31);
    for (int32_t z = start; z <= end; ++z) {
        mc_date_t d = mc_civil_from_days(z);
        mc_lunar_t l;
        if (!mc_lunar_from_solar(d, &l)) {
            printf("%04d-%02d-%02d FAIL\n", d.year, d.month, d.day);
            continue;
        }
        printf("%04d-%02d-%02d %d %d %d %d\n", d.year, d.month, d.day, l.year, l.month, l.day,
               l.is_leap ? 1 : 0);
    }
    return 0;
}
