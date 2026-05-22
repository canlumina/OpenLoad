/*
 * STM32F1 Port - 公共声明
 *
 * 用户工程在 board_init.c 里调用以下函数, 完成时钟与外设初始化,
 * 并把全部 ops 注册到 OpenLoad 框架。
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 一键初始化全部硬件 + 注册全部 ops 给框架.
 *        必须在 ol_boot_init() 之前调用.
 */
void port_stm32f1_init(void);

/**
 * @brief IRQ Hook: 由 stm32f1xx_it.c 中的 USART1_IRQHandler 调用,
 *        框架内部据此处理空闲中断, 将 DMA 接收的数据搬入 ringbuf。
 */
void port_uart1_idle_isr(void);

#ifdef __cplusplus
}
#endif
