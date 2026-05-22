/*
 * OpenLoad - Boot 状态机
 *
 * 流程: init → 决策 (magic / button / UART) → 入 CLI 或校验 App → 跳转 / Recovery.
 *
 * 触发检测在 wait_trigger 内做。按键检测依赖用户在 port 层实现的弱符号
 * ol_port_button_pressed() (默认返回 0, 即不按下), 这样不用按键的板子无需改动。
 */
#include "openload/boot.h"
#include "openload/cli.h"
#include "openload/image.h"
#include "openload/logger.h"
#include "openload/partition.h"
#include "openload/config.h"
#include "openload/errno.h"
#include "openload/ops/io_ops.h"
#include "openload/ops/sys_ops.h"
#include <stddef.h>
#include <string.h>

/* 用户可选实现 (port 层提供); 默认弱符号永不触发. */
__attribute__((weak)) int ol_port_button_pressed(void)
{
    return 0;
}

int ol_boot_init(void)
{
    /* 简单可用性检查; sys_ops 是最关键的, tick/reboot/jump 必须有 */
    if (ol_tick_ms() == 0 && ol_tick_ms() == 0) {
        /* 注意: 此处仅判断 sys_ops 已注册, tick=0 是合法值 */
    }
    OL_LOGI("OpenLoad %s starting", "0.1.0-m1");
    return OL_OK;
}

int ol_boot_wait_trigger(uint32_t timeout_ms)
{
#if OPENLOAD_ENTRY_TRIGGER_MAGIC
    {
        uint32_t m = 0;
        if (ol_magic_read(&m) == OL_OK && m == OL_MAGIC_ENTER_BOOT) {
            /* 清掉 magic 避免下次永远进 boot */
            ol_magic_write(OL_MAGIC_NONE);
            OL_LOGI("entry: magic from app");
            return 1;
        }
    }
#endif

    OL_LOGI("press button or send any char in %u ms to enter CLI", timeout_ms);
    ol_io_dev_t *console = ol_io_dev_find("console");
    uint32_t     start   = ol_tick_ms();
    while ((ol_tick_ms() - start) < timeout_ms) {
#if OPENLOAD_ENTRY_TRIGGER_BUTTON
        if (ol_port_button_pressed()) {
            OL_LOGI("entry: button");
            return 1;
        }
#endif
#if OPENLOAD_ENTRY_TRIGGER_UART
        if (console && console->ops && console->ops->available) {
            if (console->ops->available(console) > 0) {
                /* 丢弃该字符以免它被 CLI 当成命令首字符 */
                uint8_t junk;
                console->ops->read(console, &junk, 1);
                OL_LOGI("entry: uart");
                return 1;
            }
        }
#endif
    }
    return 0;
}

int ol_boot_jump_to(const char *app_partition_name)
{
    const ol_partition_t *p = ol_part_find(app_partition_name);
    if (!p) {
        OL_LOGE("partition %s not found", app_partition_name);
        return OL_E_PART_NOT_FOUND;
    }
    if (!(p->flags & OL_PART_FLAG_EXECUTABLE)) {
        OL_LOGE("partition %s not executable", app_partition_name);
        return OL_E_INVAL;
    }
    int rc = ol_image_verify(p);
    if (rc != OL_OK) {
        OL_LOGE("verify failed: %s", ol_strerror(rc));
        return rc;
    }
    ol_flash_dev_t *d = ol_part_get_device(p);
    if (!d || !d->xip) {
        OL_LOGE("app device not XIP");
        return OL_E_INVAL;
    }
    /* App 实际入口在 header 之后. 用户工程的 linker.ld 与 startup 必须
       把 App 编译为基地址 = partition.base + offset + OL_IMAGE_HDR_SIZE */
    uint32_t app_addr = d->base + p->offset + OL_IMAGE_HDR_SIZE;
    OL_LOGI("jump to 0x%08x", app_addr);

    /* 刷出 console, 否则会丢失最后的日志 */
    ol_io_dev_t *console = ol_io_dev_find("console");
    if (console && console->ops && console->ops->flush) {
        console->ops->flush(console);
    }
    ol_disable_irq();
    ol_jump(app_addr);
    return OL_E_IO;  /* 正常不会到这 */
}

void ol_boot_run(void)
{
    int trigger = ol_boot_wait_trigger(OPENLOAD_BOOT_DELAY_MS);

    if (!trigger) {
        int rc = ol_boot_jump_to("app");
        OL_LOGW("jump returned: %s, fall back to CLI", ol_strerror(rc));
        /* 落到 CLI 让用户修复 */
    } else {
        OL_LOGI("entering CLI");
    }

    ol_io_dev_t *console = ol_io_dev_find("console");
    if (!console) {
        OL_LOGE("no console, halt");
        for (;;) { }
    }
    ol_cli_run(console);
    /* CLI 永不返回 */
    for (;;) { }
}
