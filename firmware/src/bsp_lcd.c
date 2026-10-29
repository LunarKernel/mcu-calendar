/* 屏 R61509V 的底层：FSMC 与背光。初始化寄存器序列还没有出处（原理图里没有，例程 lcd.c 未得），
 * 这一版只做到能读控制器 ID，证明 FSMC 片选、RS 线、数据线都接对了。 */
#include "board.h"
#include "bsp.h"

#define LCD_CMD (*(volatile uint16_t *)BOARD_LCD_CMD_ADDR)
#define LCD_DATA (*(volatile uint16_t *)BOARD_LCD_DATA_ADDR)

/* Bank1 第 4 块：BTCR[6]/[7] 是 BCR4/BTR4，BWTR[6] 是 BWTR4 */
#define LCD_BANK_IDX 6u

void bsp_lcd_init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_FSMC_CLK_ENABLE();

    GPIO_InitTypeDef af = {0};
    af.Mode = GPIO_MODE_AF_PP;
    af.Speed = GPIO_SPEED_FREQ_HIGH;
    /* PD0/1＝D2/D3，PD4＝NOE，PD5＝NWE，PD8–10＝D13–D15，PD14/15＝D0/D1 */
    af.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
             GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOD, &af);
    /* D4–D12 */
    af.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 |
             GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOE, &af);
    /* A10（RS）、NE4 */
    af.Pin = GPIO_PIN_0 | GPIO_PIN_12;
    HAL_GPIO_Init(GPIOG, &af);

    /* 读写分开定时（EXTMOD）：读 ID 要慢，写像素要快。HCLK 72 MHz，一拍约 13.9 ns。
     * 读：地址建立 2 拍、数据保持 16 拍（约 250 ns）；写：1 拍、4 拍（约 70 ns）。
     * 数值是保守取的，没有对过 R61509V 数据手册的时序表；刷屏慢了再收紧。 */
    FSMC_Bank1->BTCR[LCD_BANK_IDX] = FSMC_BCRx_MBKEN | FSMC_BCRx_MWID_0 | FSMC_BCRx_WREN | FSMC_BCRx_EXTMOD;
    FSMC_Bank1->BTCR[LCD_BANK_IDX + 1u] = (2u << FSMC_BTRx_ADDSET_Pos) | (16u << FSMC_BTRx_DATAST_Pos);
    FSMC_Bank1E->BWTR[LCD_BANK_IDX] = (1u << FSMC_BWTRx_ADDSET_Pos) | (4u << FSMC_BWTRx_DATAST_Pos);

    /* 屏的 RST 接整板复位，MCU 复位时屏也复位了；控制器复位后要等几十毫秒才能访问。
     * 只在初始化时等这一次，不在主循环里。 */
    HAL_Delay(50);

    GPIO_InitTypeDef bl = {0};
    bl.Pin = BOARD_LCD_BL_PIN;
    bl.Mode = GPIO_MODE_OUTPUT_PP;
    bl.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_WritePin(BOARD_LCD_BL_PORT, BOARD_LCD_BL_PIN, GPIO_PIN_SET); /* 高电平亮 */
    HAL_GPIO_Init(BOARD_LCD_BL_PORT, &bl);
}

uint16_t bsp_lcd_read_id(void)
{
    LCD_CMD = 0x0000u; /* 索引 0 是器件代码寄存器 */
    return LCD_DATA;
}
