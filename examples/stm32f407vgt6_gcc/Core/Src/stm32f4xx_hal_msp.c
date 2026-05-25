/**
  * @file    stm32f4xx_hal_msp.c
  * @brief   全局 MSP. F4 不需要 F1 那种 AFIO REMAP (SWJ_NOJTAG), SWD 默认启用.
  */
#include "main.h"

void HAL_MspInit(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    /* SWD 在 F4 默认启用, 不需要额外配置 (跟 F1 不同). */
}
