/* 极简绘图：只靠 bsp_lcd_fill（矩形填单色）拼出文字与七段数码。
 * 字形是本仓库自绘的 5×7 点阵（只有空格、数字、大写字母与几个标点，小写按大写画），
 * 大号数字是按比例画的七段，不依赖任何外部字库。汉字（农历月日名、星期）还没有字库，暂用数字表示。 */
#ifndef GFX_H
#define GFX_H

#include <stdint.h>

#define GFX_BLACK 0x0000u
#define GFX_WHITE 0xFFFFu
#define GFX_RED 0xF800u
#define GFX_GREEN 0x07E0u
#define GFX_CYAN 0x07FFu
#define GFX_YELLOW 0xFFE0u
#define GFX_GRAY 0x8410u

/* 每个字符占 6×8 个点（含 1 点字间距与 1 点行距），乘 scale 后为像素。返回画完后的 x */
uint16_t gfx_text(uint16_t x, uint16_t y, const char *s, uint8_t scale, uint16_t fg, uint16_t bg);
uint16_t gfx_text_width(const char *s, uint8_t scale);

/* 七段数字：w×h 的格子里画 digit（0–9；其他值只清格子），段粗 t */
void gfx_seg_digit(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t t, uint8_t digit, uint16_t fg,
                   uint16_t bg);

#endif
