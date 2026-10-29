/* 两路串口，寄存器直写：HAL_UART 的收发接口是按块阻塞或按块回调，套不上「中断里只把字节塞进缓冲」的分层。
 * 中断入口也写在这里而不是 stm32f1xx_it.c：自检固件链接 it.c 却不链接本文件。 */
#include "board.h"
#include "bsp.h"

#define BAUD 115200u
#define RING_SIZE 256u /* 2 的幂，下标用掩码回绕 */

/* 单生产者（中断）单消费者（主循环）：head 只由中断写、tail 只由主循环写，不用关中断 */
typedef struct {
    uint8_t buf[RING_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint32_t dropped;
} ring_t;

static ring_t s_dbg_rx;
static ring_t s_esp_rx;

static void ring_push_isr(ring_t *r, uint8_t b)
{
    const uint16_t head = r->head;
    const uint16_t next = (uint16_t)((head + 1u) & (RING_SIZE - 1u));
    if (next == r->tail) {
        r->dropped++;
        return;
    }
    r->buf[head] = b;
    r->head = next;
}

static int ring_pop(ring_t *r)
{
    const uint16_t tail = r->tail;
    if (tail == r->head) {
        return -1;
    }
    const uint8_t b = r->buf[tail];
    r->tail = (uint16_t)((tail + 1u) & (RING_SIZE - 1u));
    return b;
}

static void usart_setup(USART_TypeDef *u, uint32_t pclk, IRQn_Type irq)
{
    u->CR1 = 0;
    u->CR2 = 0;
    u->CR3 = 0;
    u->BRR = (pclk + BAUD / 2u) / BAUD; /* BRR 即 16 倍分频系数，四舍五入；36 MHz 下误差 0.16% */
    u->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
    HAL_NVIC_SetPriority(irq, 1, 0); /* 比 SysTick（优先级 0）低：时基不被串口拖后 */
    HAL_NVIC_EnableIRQ(irq);
}

void bsp_uart_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_USART3_CLK_ENABLE();

    GPIO_InitTypeDef tx = {0};
    tx.Mode = GPIO_MODE_AF_PP;
    tx.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitTypeDef rx = {0};
    rx.Mode = GPIO_MODE_INPUT;
    rx.Pull = GPIO_PULLUP; /* 没插模块或跳线帽时线是悬空的，上拉防止收到一串噪声 */

    tx.Pin = GPIO_PIN_9;
    HAL_GPIO_Init(GPIOA, &tx);
    rx.Pin = GPIO_PIN_10;
    HAL_GPIO_Init(GPIOA, &rx);
    tx.Pin = GPIO_PIN_10;
    HAL_GPIO_Init(GPIOB, &tx);
    rx.Pin = GPIO_PIN_11;
    HAL_GPIO_Init(GPIOB, &rx);

    usart_setup(BOARD_DBG_USART, HAL_RCC_GetPCLK2Freq(), USART1_IRQn);
    usart_setup(BOARD_ESP_USART, HAL_RCC_GetPCLK1Freq(), USART3_IRQn);
}

static void usart_write(USART_TypeDef *u, const char *data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        while ((u->SR & USART_SR_TXE) == 0u) {
        }
        u->DR = (uint8_t)data[i];
    }
}

/* 先读 SR 再读 DR，硬件据此清掉 RXNE 与 ORE/FE/NE；只要有任一标志都必须读一次 DR，
 * 否则 ORE 一直挂着会让中断反复进来，主循环饿死 */
static void usart_isr(USART_TypeDef *u, ring_t *r)
{
    const uint32_t sr = u->SR;
    if ((sr & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE)) != 0u) {
        const uint8_t b = (uint8_t)(u->DR & 0xFFu);
        if ((sr & USART_SR_RXNE) != 0u) {
            ring_push_isr(r, b);
        }
    }
}

void USART1_IRQHandler(void);
void USART3_IRQHandler(void);

void USART1_IRQHandler(void)
{
    usart_isr(BOARD_DBG_USART, &s_dbg_rx);
}

void USART3_IRQHandler(void)
{
    usart_isr(BOARD_ESP_USART, &s_esp_rx);
}

void bsp_dbg_write(const char *data, size_t len)
{
    usart_write(BOARD_DBG_USART, data, len);
}

void bsp_dbg_puts(const char *s)
{
    size_t n = 0;
    while (s[n] != '\0') {
        ++n;
    }
    usart_write(BOARD_DBG_USART, s, n);
}

void bsp_dbg_put_u32(uint32_t v)
{
    char buf[10];
    size_t i = sizeof buf;
    do {
        buf[--i] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v != 0u);
    usart_write(BOARD_DBG_USART, buf + i, sizeof buf - i);
}

void bsp_dbg_put_hex(uint32_t v, uint8_t digits)
{
    static const char k_hex[] = "0123456789ABCDEF";
    char buf[8];
    if (digits == 0u || digits > 8u) {
        digits = 8u;
    }
    for (uint8_t i = digits; i > 0u; --i) {
        buf[i - 1u] = k_hex[v & 0xFu];
        v >>= 4;
    }
    usart_write(BOARD_DBG_USART, buf, digits);
}

int bsp_dbg_read_byte(void)
{
    return ring_pop(&s_dbg_rx);
}

void bsp_esp_write(const char *data, size_t len)
{
    usart_write(BOARD_ESP_USART, data, len);
}

int bsp_esp_read_byte(void)
{
    return ring_pop(&s_esp_rx);
}

uint32_t bsp_esp_rx_dropped(void)
{
    return s_esp_rx.dropped;
}
