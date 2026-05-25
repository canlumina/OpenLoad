/*
 * GPIO 全局配置 — DevEBox MCUDev F407VGT6
 *
 * 用到的 GPIO 引脚:
 *   PA0  K1 按键 (input + pulldown, 按下=高电平; 在 port_button.c 配置)
 *   PA1  LED D2 (active low)
 *   PA9/10  USART1 console (在 usart.c 的 MspInit 配置)
 *   PA2/3   USART2 ESP8266 (在 usart.c 的 MspInit 配置, 条件编译)
 *   PA15 + PB3/4/5  SPI1 W25Q16 (在 port_flash_w25q16.c 配置)
 *
 * 本文件只管 LED. 按键由 port_button.c, USART 由 usart.c, SPI 由 W25Q16 driver.
 */
#include "gpio.h"

void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* LED PA1, 默认熄灭 (高电平 = active low) */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);

    GPIO_InitStruct.Pin   = GPIO_PIN_1;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}
