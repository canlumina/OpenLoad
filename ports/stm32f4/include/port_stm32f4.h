/*
 * STM32F4 Port - 公共声明
 *
 * 用户工程在 board_init.c 里调用以下函数, 完成时钟与外设初始化,
 * 并把全部 ops 注册到 OpenLoad 框架。
 */
#pragma once

#include "openload/config.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 一键初始化全部硬件 + 注册全部 ops 给框架.
 *        必须在 ol_boot_init() 之前调用.
 */
void port_stm32f4_init(void);

/**
 * @brief IRQ Hook: 由 stm32f4xx_it.c 中的 USART1_IRQHandler 调用,
 *        框架内部据此处理空闲中断, 将 DMA 接收的数据搬入 ringbuf。
 */
void port_uart1_idle_isr(void);

#if OPENLOAD_ENABLE_ESP8266
/** USART2 IDLE 中断钩子 (ESP8266 链路). 同 UART1, 由 it.c 调用. */
void port_uart2_idle_isr(void);
#endif

#ifdef __cplusplus
}
#endif
