/*
 * STM32F4 Port - 板级初始化聚合
 *
 * 调用顺序 (用户在 main 里只需要 port_stm32f4_init() 一次):
 *   1. HAL_Init / SystemClock_Config (main 里手工调)
 *   2. MX_GPIO_Init / MX_DMA_Init / MX_USART1_UART_Init (main 里手工调)
 *   3. port_sys_register
 *   4. port_button_init / port_uart1_init / port_w25q16_init
 *   5. (ESP8266) MX_USART2_UART_Init + port_uart2_init
 */
#include "port_stm32f4.h"
#include "openload/ops/sys_ops.h"
#include "openload/config.h"

#if OPENLOAD_ENABLE_ESP8266
#  include "usart.h"
#  include "port_uart2.h"
#endif

void port_sys_register(void);
int  port_uart1_init(void);
int  port_w25q16_init(void);
void port_button_init(void);

void port_stm32f4_init(void)
{
    /* sys ops 必须先注册, 因为 ol_log / ol_tick_ms 都依赖它 */
    port_sys_register();

    port_button_init();
    port_uart1_init();

    /* W25Q16 init 失败不致命: 没有外部 flash 时仍可用内部 flash 单 bank */
    (void)port_w25q16_init();

#if OPENLOAD_ENABLE_ESP8266
    /* USART2 物理层. ESP8266 协议层 (AT 探活 / WiFi join) 留给 CLI 触发. */
    MX_USART2_UART_Init();
    (void)port_uart2_init(OPENLOAD_ESP_UART_BAUD);
#endif
}
