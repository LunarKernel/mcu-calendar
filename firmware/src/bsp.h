/* 板级驱动。只给 firmware/ 内部用：app/ 不 include 这里，经 port.h（port_impl.c）间接调用。
 * 除特别注明外都在主循环上下文调用。 */
#ifndef BSP_H
#define BSP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* HSE 8 MHz × PLL 9 → SYSCLK 72 MHz，APB1 36 MHz，APB2 72 MHz；SysTick 随之重配为 1 ms。
 * 失败时留在复位后的 HSI 8 MHz，返回 false */
bool bsp_clock_init(void);

void bsp_gpio_init(void); /* LED、蜂鸣器、按键 */
void bsp_led_set(uint8_t idx, bool on); /* idx 0 = DS0，1 = DS1 */
void bsp_led_toggle(uint8_t idx);
void bsp_beep_set(bool on);

typedef enum { BSP_KEY_UP, BSP_KEY0, BSP_KEY1, BSP_KEY_COUNT } bsp_key_t;
bool bsp_key_pressed(bsp_key_t key); /* 原始电平按电路换算好，未消抖 */

/* 两路串口：调试口（USART1）与 ESP8266（USART3）。收都走 RXNE 中断进环形缓冲；
 * 发是阻塞的逐字节写，单条不长，等得起。 */
void bsp_uart_init(void);
void bsp_dbg_write(const char *data, size_t len);
void bsp_dbg_puts(const char *s);
void bsp_dbg_put_u32(uint32_t v);
void bsp_dbg_put_hex(uint32_t v, uint8_t digits);
int bsp_dbg_read_byte(void); /* 没有则 -1 */
void bsp_esp_write(const char *data, size_t len);
int bsp_esp_read_byte(void);
uint32_t bsp_esp_rx_dropped(void); /* 环形缓冲满了丢掉的字节数 */

/* 片内 RTC：LSE 32.768 kHz，计数器存 UTC 秒。
 * 备份域已在跑 RTC（VBAT 保住了）就原样接着用，否则从头配，计数器写 default_s。 */
typedef struct {
    bool ok;            /* RTC 可用 */
    bool retained;      /* 上电时 RTC 已在跑：断电期间备份域保住了 */
    bool time_set;      /* 时间被人设过（备份寄存器里有标记） */
    uint32_t bdcr_initial;
    uint32_t lse_wait_ms; /* 本次从头起振时的等待时长；retained 时为 0 */
} bsp_rtc_status_t;

void bsp_rtc_init(uint32_t default_s, bsp_rtc_status_t *st);
bool bsp_rtc_read(uint32_t *seconds);
bool bsp_rtc_write(uint32_t seconds); /* 同时置「时间已设」标记 */

/* 屏 R61509VE，横屏 400×240，RGB565。
 * bsp_lcd_init 配好 FSMC，之后 bsp_lcd_read_id 应读出 0xB509；bsp_lcd_panel_init 写初始化序列并开显示。
 * bsp_lcd_fill 坐标含两端，越界整块不画 */
#define BSP_LCD_W 400u
#define BSP_LCD_H 240u
void bsp_lcd_init(void);
uint16_t bsp_lcd_read_id(void);
void bsp_lcd_panel_init(void);
void bsp_lcd_fill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);

#endif
