/* 普中 Z300（V2.0）板级引脚。出处：普中资料包里的 Z300-V2.0 原理图（2020/9/22），
 * 逐条在 CPU 引脚旁的网络标号上直读；实板丝印 P4、DS0/DS1、BEEP、J6、P3、BAT1 与图一致。
 * 原理图本身不进本仓库（厂家资料）。图上没写、要上板才知道的，在对应行注明。 */
#ifndef BOARD_H
#define BOARD_H

#include "stm32f1xx_hal.h"

/* LED：3.3 V → 1 kΩ → LED → 引脚，低电平亮 */
#define BOARD_LED0_PORT GPIOB /* DS0 */
#define BOARD_LED0_PIN GPIO_PIN_5
#define BOARD_LED1_PORT GPIOE /* DS1 */
#define BOARD_LED1_PIN GPIO_PIN_5

/* 蜂鸣器：1 kΩ 驱动 S8050，高电平响。有源（10/29 上板：持续高电平出「嘀」声，不必 PWM） */
#define BOARD_BEEP_PORT GPIOB
#define BOARD_BEEP_PIN GPIO_PIN_8

/* 按键：KEY_UP 按下接 3.3 V（高有效，图上无外部下拉）；KEY0、KEY1 按下接地（低有效） */
#define BOARD_KEY_UP_PORT GPIOA
#define BOARD_KEY_UP_PIN GPIO_PIN_0
#define BOARD_KEY0_PORT GPIOE
#define BOARD_KEY0_PIN GPIO_PIN_4
#define BOARD_KEY1_PORT GPIOE
#define BOARD_KEY1_PIN GPIO_PIN_3

/* 调试串口：USART1 PA9/PA10 经 P4 跳线到 CH340C（USB2） */
#define BOARD_DBG_USART USART1
/* ESP8266 座 P3：USART3 PB10 → ESP URXD，PB11 ← ESP UTXD */
#define BOARD_ESP_USART USART3

/* 屏 R61509V：FSMC Bank1 NE4（PG12），RS 接 A10（PG0），16 位。
 * 16 位宽时 FSMC 地址线 A[n] 对应 HADDR[n+1]，所以 RS＝1 的地址是 bank 基址加 1 << 11。 */
#define BOARD_LCD_CMD_ADDR 0x6C000000u
#define BOARD_LCD_DATA_ADDR (0x6C000000u | (1u << 11))
/* 背光 LCD_BL 直接接屏座，高电平亮（10/29 上板实测） */
#define BOARD_LCD_BL_PORT GPIOB
#define BOARD_LCD_BL_PIN GPIO_PIN_0

/* DHT11 座 J6 的 DQ，板上已有 10 kΩ 上拉 */
#define BOARD_DHT11_PORT GPIOG
#define BOARD_DHT11_PIN GPIO_PIN_11

#endif
