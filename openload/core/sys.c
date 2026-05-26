/*
 * OpenLoad - sys ops 封装
 *
 * 把用户注册的 ops 函数包装为框架内统一调用点;
 * 缺失可选项时提供默认行为 (例如基于 tick_ms 的 delay_ms 退化实现).
 */
#include "openload/ops/sys_ops.h"
#include "openload/errno.h"
#include <stddef.h>

static const ol_sys_ops_t *s_ops;

void ol_sys_register(const ol_sys_ops_t *ops)
{
    s_ops = ops;
}

uint32_t ol_tick_ms(void)
{
    /* tick_ms 是必填项, 但若未注册则返回 0, 让上层超时立即结束 */
    return s_ops && s_ops->tick_ms ? s_ops->tick_ms() : 0;
}

void ol_delay_ms(uint32_t ms)
{
    if (!s_ops) {
        return;
    }
    if (s_ops->delay_ms) {
        s_ops->delay_ms(ms);
        return;
    }
    /* 退化: 基于 tick_ms 轮询. 适合 ms 不大、不进低功耗的场景. */
    if (!s_ops->tick_ms) {
        return;
    }
    uint32_t start = s_ops->tick_ms();
    while ((s_ops->tick_ms() - start) < ms) {
        /* spin */
    }
}

void ol_reboot(void)
{
    if (s_ops && s_ops->reboot) {
        s_ops->reboot();
    }
    /* 永不返回; 若 reboot 未实现则死循环 */
    for (;;) { }
}

void ol_disable_irq(void)
{
    if (s_ops && s_ops->disable_irq) {
        s_ops->disable_irq();
    }
}

void ol_jump(uint32_t app_addr)
{
    if (s_ops && s_ops->jump) {
        s_ops->jump(app_addr);
    }
}

int ol_magic_read(uint32_t *out)
{
    if (!s_ops || !s_ops->magic_read) {
        return OL_E_NOT_SUPPORTED;
    }
    return s_ops->magic_read(out);
}

int ol_magic_write(uint32_t value)
{
    if (!s_ops || !s_ops->magic_write) {
        return OL_E_NOT_SUPPORTED;
    }
    return s_ops->magic_write(value);
}

int ol_rdp_get(uint8_t *level_out)
{
    if (!level_out) {
        return OL_E_INVAL;
    }
    if (!s_ops || !s_ops->rdp_get_level) {
        return OL_E_NOT_SUPPORTED;
    }
    return s_ops->rdp_get_level(level_out);
}

int ol_rdp_lock(void)
{
    if (!s_ops || !s_ops->rdp_lock) {
        return OL_E_NOT_SUPPORTED;
    }
    return s_ops->rdp_lock();
}
