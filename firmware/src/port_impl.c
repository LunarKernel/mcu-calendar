/* app/port.h 在 Z300 上的实现。RTC 改用片内 RTC（原理图上 VBAT 接 BAT1，装电池即可断电走时，
 * 不必外接 DS3231）；port.h 的语义不变，仍是「本地时间读写」。 */
#include "port.h"

#include "bsp.h"
#include "fw.h"
#include "stm32f1xx_hal.h"

#define COUNTDOWN_BEEP_MS 600u

uint32_t port_millis(void)
{
    return HAL_GetTick();
}

bool port_rtc_get(mc_datetime_t *out)
{
    uint32_t s;
    if (!bsp_rtc_read(&s)) {
        return false;
    }
    mc_local_from_unix(s, MC_TZ_BEIJING_S, out);
    return true;
}

bool port_rtc_set(const mc_datetime_t *dt)
{
    uint32_t s;
    return mc_unix_from_local(dt, MC_TZ_BEIJING_S, &s) && bsp_rtc_write(s);
}

void port_esp_write(const char *data, size_t len)
{
    bsp_esp_write(data, len);
}

int port_esp_read_byte(void)
{
    return bsp_esp_read_byte();
}

void port_on_countdown_fired(uint8_t channel)
{
    (void)channel;
    fw_beep(COUNTDOWN_BEEP_MS);
}
