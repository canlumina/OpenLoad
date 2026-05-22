/*
 * STM32F1 Port - 板级初始化聚合
 *
 * 调用顺序 (用户在 main 里只需要 ol_port_stm32f1_init() 一次):
 *   1. HAL_Init() / SystemClock_Config (CubeMX 生成, 在 main 里手工调)
 *   2. MX_GPIO_Init / MX_DMA_Init / MX_USART1_UART_Init (CubeMX 生成)
 *   3. port_button_init / port_uart1_init / port_w25q64_init
 *   4. port_sys_register
 */
#include "port_stm32f1.h"
#include "openload/ops/sys_ops.h"

void port_sys_register(void);
int  port_uart1_init(void);
int  port_w25q64_init(void);
void port_button_init(void);

void port_stm32f1_init(void)
{
    /* sys ops 必须先注册, 因为 ol_log / ol_tick_ms 都依赖它 */
    port_sys_register();

    port_button_init();
    port_uart1_init();

    /* W25Q64 init 失败不致命: 没有外部 flash 时仍可用内部 flash 单 bank */
    (void)port_w25q64_init();
}
