/* 片内 RTC，寄存器直写（RM0008 第 18 章）。HAL_RTC 在 F1 上自己在备份寄存器里记日期、
 * 只给「当天时分秒」，与这里「计数器就是 UTC 秒、日期由 core/ 算」的做法相冲，不用它。
 *
 * 断电保持靠 VBAT：Z300 上 VBAT 由 3.3 V 与 BAT1 经两只 1N4148 取或供电，BAT1 不装电池时
 * 断电即丢（10/28 两轮实测 BDCR 读回全零）。备份寄存器 DR1 存「时间已设」标记，与 RTC 同在备份域，
 * 一起保住或一起丢。 */
#include "bsp.h"

#include "stm32f1xx_hal.h"

#define TIME_SET_MAGIC 0xC1A5u
#define LSE_TIMEOUT_MS 5000u /* 10/28 实测起振 0.6–2.9 s */
#define SYNC_TIMEOUT_MS 100u /* 影子寄存器同步、写操作完成都只要几个 RTCCLK 周期（每个约 30 µs） */

static bool wait_set(volatile uint32_t *reg, uint32_t mask, uint32_t timeout_ms)
{
    const uint32_t t0 = HAL_GetTick();
    while ((*reg & mask) == 0u) {
        if (HAL_GetTick() - t0 > timeout_ms) {
            return false;
        }
    }
    return true;
}

/* 改 RTC 寄存器的固定套路：等上一笔写完（RTOFF）→ 进配置模式 → 改 → 退出 → 等这一笔写完 */
static bool config_begin(void)
{
    if (!wait_set(&RTC->CRL, RTC_CRL_RTOFF, SYNC_TIMEOUT_MS)) {
        return false;
    }
    SET_BIT(RTC->CRL, RTC_CRL_CNF);
    return true;
}

static bool config_end(void)
{
    CLEAR_BIT(RTC->CRL, RTC_CRL_CNF);
    return wait_set(&RTC->CRL, RTC_CRL_RTOFF, SYNC_TIMEOUT_MS);
}

static bool write_counter(uint32_t s)
{
    if (!config_begin()) {
        return false;
    }
    RTC->CNTH = s >> 16;
    RTC->CNTL = s & 0xFFFFu;
    return config_end();
}

/* 复位或从待机醒来后，APB 侧读到的是旧的影子值，要等 RSF 置位才可信 */
static bool sync_shadow(void)
{
    CLEAR_BIT(RTC->CRL, RTC_CRL_RSF);
    return wait_set(&RTC->CRL, RTC_CRL_RSF, SYNC_TIMEOUT_MS);
}

void bsp_rtc_init(uint32_t default_s, bsp_rtc_status_t *st)
{
    st->ok = false;
    st->retained = false;
    st->time_set = false;
    st->lse_wait_ms = 0;

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_BKP_CLK_ENABLE();
    st->bdcr_initial = RCC->BDCR;
    SET_BIT(PWR->CR, PWR_CR_DBP); /* 备份域写保护一直开着：之后 port_rtc_set 还要写 */

    const uint32_t want = RCC_BDCR_RTCEN | RCC_BDCR_RTCSEL_LSE | RCC_BDCR_LSEON;
    if ((st->bdcr_initial & (RCC_BDCR_RTCEN | RCC_BDCR_RTCSEL | RCC_BDCR_LSEON)) == want) {
        st->retained = true;
        st->ok = wait_set(&RCC->BDCR, RCC_BDCR_LSERDY, LSE_TIMEOUT_MS) && sync_shadow();
        st->time_set = (BKP->DR1 & 0xFFFFu) == TIME_SET_MAGIC;
        return;
    }

    /* RTCSEL 一旦写过就只能靠备份域复位改回来；这会清掉备份寄存器，本来也没有可保的东西 */
    if ((st->bdcr_initial & RCC_BDCR_RTCSEL) != 0u) {
        SET_BIT(RCC->BDCR, RCC_BDCR_BDRST);
        CLEAR_BIT(RCC->BDCR, RCC_BDCR_BDRST);
    }
    const uint32_t t0 = HAL_GetTick();
    SET_BIT(RCC->BDCR, RCC_BDCR_LSEON);
    if (!wait_set(&RCC->BDCR, RCC_BDCR_LSERDY, LSE_TIMEOUT_MS)) {
        return;
    }
    st->lse_wait_ms = HAL_GetTick() - t0;
    MODIFY_REG(RCC->BDCR, RCC_BDCR_RTCSEL, RCC_BDCR_RTCSEL_LSE);
    SET_BIT(RCC->BDCR, RCC_BDCR_RTCEN);

    if (!sync_shadow() || !config_begin()) {
        return;
    }
    RTC->PRLH = 0;
    RTC->PRLL = 32767u; /* 32768 Hz / (32767 + 1) = 1 Hz */
    RTC->CNTH = default_s >> 16;
    RTC->CNTL = default_s & 0xFFFFu;
    if (!config_end()) {
        return;
    }
    BKP->DR1 = 0;
    st->ok = true;
}

bool bsp_rtc_read(uint32_t *seconds)
{
    /* CNTH、CNTL 分两次读，低 16 位恰好在两次之间进位会拼出错值；高位前后一致才算数 */
    for (int attempt = 0; attempt < 3; ++attempt) {
        const uint32_t h1 = RTC->CNTH & 0xFFFFu;
        const uint32_t l = RTC->CNTL & 0xFFFFu;
        const uint32_t h2 = RTC->CNTH & 0xFFFFu;
        if (h1 == h2) {
            *seconds = (h1 << 16) | l;
            return true;
        }
    }
    return false;
}

bool bsp_rtc_write(uint32_t seconds)
{
    if (!write_counter(seconds)) {
        return false;
    }
    BKP->DR1 = TIME_SET_MAGIC;
    return true;
}
