/*
 * STM32F103ZET6 OpenLoad bootloader - main 入口
 *
 * 流程:
 *   HAL_Init → SystemClock_Config → CubeMX 外设初始化 → port → ol_boot_run
 */
#include "main.h"
#include "stm32f1xx_hal.h"
#include "dma.h"
#include "gpio.h"
#include "usart.h"

#include "openload/openload.h"
#include "port_stm32f1.h"

static void SystemClock_Config(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    /* CubeMX 自动生成的外设初始化 (顺序按依赖) */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();

    /* port 层: 注册 sys/io/flash ops + 启动 UART DMA + 按键 GPIO + W25Q64 探活 */
    port_stm32f1_init();

    /* 框架初始化 + 主循环. 永不返回. */
    ol_boot_init();
    ol_boot_run();

    /* never reach */
    while (1) { }
}

/* HSE 8MHz → PLLx9 → SYSCLK 72MHz; APB1 36MHz, APB2 72MHz. */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType   = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState         = RCC_HSE_ON;
    osc.HSEPredivValue   = RCC_HSE_PREDIV_DIV1;
    osc.HSIState         = RCC_HSI_ON;
    osc.PLL.PLLState     = RCC_PLL_ON;
    osc.PLL.PLLSource    = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL       = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) { Error_Handler(); }

    clk.ClockType        = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                           RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource     = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider    = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider   = RCC_HCLK_DIV2;
    clk.APB2CLKDivider   = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) { Error_Handler(); }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { (void)file; (void)line; }
#endif
