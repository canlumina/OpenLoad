/*
 * STM32F4 Port - 按键 (PA0, DevEBox MCUDev K1)
 *
 * 实现框架的弱符号 ol_port_button_pressed(), boot 决策时被轮询.
 * 板上 K1 按下 = 高电平 (按下接 3V3; 释放靠内部下拉拉低).
 * 跟 F103 那块板 active-low 接法相反.
 */
#include "stm32f4xx_hal.h"
#include <stdint.h>

#define BUTTON_GPIO_PORT    GPIOA
#define BUTTON_GPIO_PIN     GPIO_PIN_0

int ol_port_button_pressed(void)
{
    return HAL_GPIO_ReadPin(BUTTON_GPIO_PORT, BUTTON_GPIO_PIN) == GPIO_PIN_SET;
}

void port_button_init(void)
{
    GPIO_InitTypeDef gi = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    gi.Pin  = BUTTON_GPIO_PIN;
    gi.Mode = GPIO_MODE_INPUT;
    gi.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(BUTTON_GPIO_PORT, &gi);
}
