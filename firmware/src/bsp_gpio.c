#include "board.h"
#include "bsp.h"

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} pin_t;

static const pin_t k_leds[2] = {
    {BOARD_LED0_PORT, BOARD_LED0_PIN},
    {BOARD_LED1_PORT, BOARD_LED1_PIN},
};

static const pin_t k_keys[BSP_KEY_COUNT] = {
    [BSP_KEY_UP] = {BOARD_KEY_UP_PORT, BOARD_KEY_UP_PIN},
    [BSP_KEY0] = {BOARD_KEY0_PORT, BOARD_KEY0_PIN},
    [BSP_KEY1] = {BOARD_KEY1_PORT, BOARD_KEY1_PIN},
};

static void init_pin(const pin_t *p, uint32_t mode, uint32_t pull)
{
    GPIO_InitTypeDef cfg = {0};
    cfg.Pin = p->pin;
    cfg.Mode = mode;
    cfg.Pull = pull;
    cfg.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(p->port, &cfg);
}

void bsp_gpio_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /* 先写电平再切成输出，上电那一下 LED 不闪、蜂鸣器不响 */
    for (uint8_t i = 0; i < 2u; ++i) {
        HAL_GPIO_WritePin(k_leds[i].port, k_leds[i].pin, GPIO_PIN_SET);
        init_pin(&k_leds[i], GPIO_MODE_OUTPUT_PP, GPIO_NOPULL);
    }
    const pin_t beep = {BOARD_BEEP_PORT, BOARD_BEEP_PIN};
    HAL_GPIO_WritePin(beep.port, beep.pin, GPIO_PIN_RESET);
    init_pin(&beep, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL);

    /* KEY_UP 高有效且板上没有下拉，悬空会乱跳，必须开内部下拉 */
    init_pin(&k_keys[BSP_KEY_UP], GPIO_MODE_INPUT, GPIO_PULLDOWN);
    init_pin(&k_keys[BSP_KEY0], GPIO_MODE_INPUT, GPIO_PULLUP);
    init_pin(&k_keys[BSP_KEY1], GPIO_MODE_INPUT, GPIO_PULLUP);
}

void bsp_led_set(uint8_t idx, bool on)
{
    if (idx < 2u) {
        HAL_GPIO_WritePin(k_leds[idx].port, k_leds[idx].pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }
}

void bsp_led_toggle(uint8_t idx)
{
    if (idx < 2u) {
        HAL_GPIO_TogglePin(k_leds[idx].port, k_leds[idx].pin);
    }
}

void bsp_beep_set(bool on)
{
    HAL_GPIO_WritePin(BOARD_BEEP_PORT, BOARD_BEEP_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool bsp_key_pressed(bsp_key_t key)
{
    if ((unsigned)key >= (unsigned)BSP_KEY_COUNT) {
        return false;
    }
    const bool high = HAL_GPIO_ReadPin(k_keys[key].port, k_keys[key].pin) == GPIO_PIN_SET;
    return key == BSP_KEY_UP ? high : !high;
}
