/*
 * STM32F407VGT6 OpenLoad bootloader - main 入口 (DevEBox MCUDev 板)
 *
 * 流程:
 *   HAL_Init → SystemClock_Config (168MHz) → CubeMX 外设 → port → ol_boot_run
 */
#include "main.h"
#include "stm32f4xx_hal.h"
#include "dma.h"
#include "gpio.h"
#include "usart.h"

#include "openload/openload.h"
#include "port_stm32f4.h"

static void SystemClock_Config(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    /* CubeMX 自动生成的外设初始化 (顺序按依赖) */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();

    /* port 层: 注册 sys/io/flash ops + 启动 UART DMA + 按键 GPIO + W25Q16 探活 */
    port_stm32f4_init();

    /* 框架初始化 + 主循环. 永不返回. */
    ol_boot_init();
    ol_boot_run();

    /* never reach */
    while (1) { }
}

/* HSE 8MHz → PLL_M=8 → 1MHz → PLL_N=336 → VCO=336MHz → PLL_P=2 → SYSCLK=168MHz.
 * APB1 = 168/4 = 42MHz, APB2 = 168/2 = 84MHz. Flash latency = 5 WS (3.3V, 168MHz).
 * PLL_Q = 7 → 48MHz USB (即使现在不用 USB 也按标准配, 后续 USB DFU 直接挂). */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* PWR 时钟必须先打开, VOS scale 1 才能撑 168MHz */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType   = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState         = RCC_HSE_ON;
    osc.PLL.PLLState     = RCC_PLL_ON;
    osc.PLL.PLLSource    = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM         = 8;
    osc.PLL.PLLN         = 336;
    osc.PLL.PLLP         = RCC_PLLP_DIV2;
    osc.PLL.PLLQ         = 7;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) { Error_Handler(); }

    clk.ClockType        = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                           RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource     = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider    = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider   = RCC_HCLK_DIV4;     /* 42 MHz */
    clk.APB2CLKDivider   = RCC_HCLK_DIV2;     /* 84 MHz */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) { Error_Handler(); }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { (void)file; (void)line; }
#endif
