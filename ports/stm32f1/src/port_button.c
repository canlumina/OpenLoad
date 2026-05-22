/*
 * STM32F1 Port - 按键 (PA0)
 *
 * 实现框架的弱符号 ol_port_button_pressed(), boot 决策时被轮询.
 * 板上按键按下 = 低电平有效 (用户按住进入 CLI)。
 */
#include "stm32f1xx_hal.h"
#include <stdint.h>

#define BUTTON_GPIO_PORT    GPIOA
#define BUTTON_GPIO_PIN     GPIO_PIN_0

int ol_port_button_pressed(void)
{
    return HAL_GPIO_ReadPin(BUTTON_GPIO_PORT, BUTTON_GPIO_PIN) == GPIO_PIN_RESET;
}

void port_button_init(void)
{
    GPIO_InitTypeDef gi = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    gi.Pin  = BUTTON_GPIO_PIN;
    gi.Mode = GPIO_MODE_INPUT;
    gi.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(BUTTON_GPIO_PORT, &gi);
}
