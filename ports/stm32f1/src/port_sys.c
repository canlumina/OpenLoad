/*
 * STM32F1 Port - sys ops
 *
 * tick 走 SysTick (HAL_GetTick); reboot 走 NVIC_SystemReset;
 * disable_irq 走 __disable_irq; jump 实现 Cortex-M 标准跳转流程。
 *
 * magic 用 RAM 末尾 4 字节 (linker script 保留, 不被 .bss 覆盖) 实现
 * App ↔ Bootloader 通信。复位不丢; 掉电丢失 (符合"软触发"语义)。
 */
#include "openload/ops/sys_ops.h"
#include "openload/errno.h"
#include "stm32f1xx_hal.h"
#include <stdint.h>

/* 标志位放在 RAM 末尾 4 字节。链接脚本需保留 .ol_magic 段:
 *   .ol_magic (NOLOAD) : { KEEP(*(.ol_magic)) } > RAM
 * 不在 startup 中清 0, 这样 reset 能携带 App 写入的值。
 */
__attribute__((section(".ol_magic"), used))
static volatile uint32_t s_ol_magic;

static uint32_t sys_tick_ms(void)
{
    return HAL_GetTick();
}

static void sys_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

static void sys_reboot(void)
{
    NVIC_SystemReset();
    /* never return */
    for (;;) { }
}

static void sys_disable_irq(void)
{
    __disable_irq();
}

/* Cortex-M3 跳转: 取 [addr] 为 MSP, [addr+4] 为 Reset_Handler 入口. */
static void sys_jump(uint32_t app_addr)
{
    /* 1. 重定位向量表 */
    SCB->VTOR = app_addr;

    /* 2. 取 MSP (向量表第 0 个 word) 与 Reset_Handler (第 1 个 word) */
    uint32_t msp_init    = *(volatile uint32_t *)(app_addr);
    uint32_t app_reset_h = *(volatile uint32_t *)(app_addr + 4);

    /* 3. 设栈 + 跳转. 必须连续, 避免编译器在中间插入栈帧操作. */
    __asm volatile (
        "msr msp, %0       \n"
        "bx  %1            \n"
        :
        : "r" (msp_init), "r" (app_reset_h)
        : "memory"
    );

    /* never return */
    for (;;) { }
}

static int sys_magic_read(uint32_t *out)
{
    if (!out) { return OL_E_INVAL; }
    *out = s_ol_magic;
    return OL_OK;
}

static int sys_magic_write(uint32_t value)
{
    s_ol_magic = value;
    return OL_OK;
}

static const ol_sys_ops_t s_sys_ops = {
    .tick_ms     = sys_tick_ms,
    .delay_ms    = sys_delay_ms,
    .reboot      = sys_reboot,
    .disable_irq = sys_disable_irq,
    .jump        = sys_jump,
    .magic_read  = sys_magic_read,
    .magic_write = sys_magic_write,
};

void port_sys_register(void)
{
    ol_sys_register(&s_sys_ops);
}
