/* 屏 R61509VE（240×400，16 位 8080，经 FSMC）的底层。
 *
 * 初始化寄存器序列与横屏方向、开窗寄存器的用法，取自普中 Z300 资料包 41 号例程
 * APP/tftlcd/tftlcd.c 里 TFTLCD_R61509VE 那一段（该例程头文件默认就选这一款）；
 * 寄存器值原样照抄，不改，延时改为毫秒级（原文是空循环，72 MHz 下等于没等，宁可多等）。
 *
 * 本仓库只用横屏（400×240）。对外只给「矩形填单色」：开窗后连续写同一颜色，
 * 结果与 GRAM 的扫描方向无关，方向位万一不对也只会整体镜像，不会把字写成乱码。 */
#include "board.h"
#include "bsp.h"

#define LCD_CMD (*(volatile uint16_t *)BOARD_LCD_CMD_ADDR)
#define LCD_DATA (*(volatile uint16_t *)BOARD_LCD_DATA_ADDR)

/* Bank1 第 4 块：BTCR[6]/[7] 是 BCR4/BTR4，BWTR[6] 是 BWTR4 */
#define LCD_BANK_IDX 6u

#define DELAY 0xFFFFu /* 序列表里的伪寄存器：值是毫秒数 */

typedef struct {
    uint16_t reg;
    uint16_t val;
} reg_val_t;

static const reg_val_t k_init[] = {
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {DELAY, 5},
    {0x0400, 0x6200}, {0x0008, 0x0808},
    /* γ 校正 */
    {0x0300, 0x0C00}, {0x0301, 0x5A0B}, {0x0302, 0x0906}, {0x0303, 0x1017}, {0x0304, 0x2300},
    {0x0305, 0x1700}, {0x0306, 0x6309}, {0x0307, 0x0C09}, {0x0308, 0x010C}, {0x0309, 0x2232},
    {0x0010, 0x0016}, {0x0011, 0x0101}, {0x0012, 0x0000}, {0x0013, 0x0001},
    /* 电源 */
    {0x0100, 0x0330}, {0x0101, 0x0336}, {0x0103, 0x1000}, {0x0280, 0x6100}, {0x0102, 0xBBB4}, {DELAY, 5},
    {0x0001, 0x0100}, {0x0002, 0x0100}, {0x0003, 0x1030}, {0x0009, 0x0001}, {0x000C, 0x0000},
    {0x0090, 0x0800}, {0x000F, 0x0000},
    {0x0210, 0x0000}, {0x0211, 0x00EF}, {0x0212, 0x0000}, {0x0213, 0x018F},
    {0x0500, 0x0000}, {0x0501, 0x0000}, {0x0502, 0x005F},
    {0x0401, 0x0001}, {0x0404, 0x0000}, {DELAY, 5},
    {0x0007, 0x0100}, /* 开显示 */
    {DELAY, 5},
    /* 横屏：例程 LCD_Display_Dir(1) */
    {0x0001, 0x0000}, {0x0003, 0x1038},
};

static void write_reg(uint16_t reg, uint16_t val)
{
    LCD_CMD = reg;
    LCD_DATA = val;
}

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

    /* 读写分开定时（EXTMOD），HCLK 72 MHz 一拍约 13.9 ns。
     * 读：地址建立 2 拍、数据保持 16 拍（约 250 ns），与例程同量级。
     * 写：例程按标准库结构体写的是 ADDSET 0x15、ADDHLD 0x15、DATAST 5，标准库把三者移位相或，
     * 落到寄存器里实际是 ADDSET 5、ADDHLD 5、DATAST 5（字段只有 4 位）；这里直接写这个实际值。 */
    FSMC_Bank1->BTCR[LCD_BANK_IDX] = FSMC_BCRx_MBKEN | FSMC_BCRx_MWID_0 | FSMC_BCRx_WREN | FSMC_BCRx_EXTMOD;
    FSMC_Bank1->BTCR[LCD_BANK_IDX + 1u] = (2u << FSMC_BTRx_ADDSET_Pos) | (16u << FSMC_BTRx_DATAST_Pos);
    FSMC_Bank1E->BWTR[LCD_BANK_IDX] =
        (5u << FSMC_BWTRx_ADDSET_Pos) | (5u << FSMC_BWTRx_ADDHLD_Pos) | (5u << FSMC_BWTRx_DATAST_Pos);

    /* 屏的 RST 接整板复位，MCU 复位时屏也复位了；控制器复位后要等几十毫秒才能访问。
     * 只在初始化时等，不在主循环里。 */
    HAL_Delay(50);

    GPIO_InitTypeDef bl = {0};
    bl.Pin = BOARD_LCD_BL_PIN;
    bl.Mode = GPIO_MODE_OUTPUT_PP;
    bl.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_WritePin(BOARD_LCD_BL_PORT, BOARD_LCD_BL_PIN, GPIO_PIN_SET); /* 现用模块的背光常亮、不接此脚，置高无害，见 board.h */
    HAL_GPIO_Init(BOARD_LCD_BL_PORT, &bl);
}

uint16_t bsp_lcd_read_id(void)
{
    LCD_CMD = 0x0000u; /* 索引 0 是器件代码寄存器 */
    return LCD_DATA;
}

void bsp_lcd_panel_init(void)
{
    for (size_t i = 0; i < sizeof k_init / sizeof k_init[0]; ++i) {
        if (k_init[i].reg == DELAY) {
            HAL_Delay(k_init[i].val);
        } else {
            write_reg(k_init[i].reg, k_init[i].val);
        }
    }
}

void bsp_lcd_fill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    if (x0 > x1 || y0 > y1 || x1 >= BSP_LCD_W || y1 >= BSP_LCD_H) {
        return;
    }
    /* 横屏下屏幕 x 对应 GRAM 的垂直地址（0x0212/0x0213/0x0201），屏幕 y 对应水平地址（0x0210/0x0211/0x0200），
     * 与例程 LCD_Set_Window 横屏分支一致 */
    write_reg(0x0212, x0);
    write_reg(0x0213, x1);
    write_reg(0x0210, y0);
    write_reg(0x0211, y1);
    write_reg(0x0201, x0);
    write_reg(0x0200, y0);
    LCD_CMD = 0x0202; /* 之后的数据写进 GRAM，地址在窗口内自动递增 */
    uint32_t n = (uint32_t)(x1 - x0 + 1u) * (uint32_t)(y1 - y0 + 1u);
    while (n-- > 0u) {
        LCD_DATA = color;
    }
}
