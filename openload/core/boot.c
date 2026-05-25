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
#if OPENLOAD_ENABLE_OPLOG
#  include "openload/oplog.h"
#endif
#if OPENLOAD_ENABLE_BACKUP
#  include "openload/updater.h"
#endif
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
#if OPENLOAD_ENABLE_OPLOG
    /* 在第一条 LOGI 之前 init oplog, 让 starting/jump 等 ERR/WRN 都能入盘.
     * init 失败 (分区未配置等) silently disable, 不影响主流程. */
    (void)ol_oplog_init();
#endif
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

    OL_LOGI("press button or send any char in %lu ms to enter CLI", timeout_ms);
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
    OL_LOGI("jump to 0x%08lx", app_addr);

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
#if OPENLOAD_ENABLE_BACKUP
    /* install 中断检测: 上次 install 中途断电会留下 INSTALLING magic.
     * 此时 app 可能写到一半, ol_image_verify 大概率也能挡, 但 CRC 偶然
     * 通过的边界下 magic 是更硬的指示器. 直接从 backup 恢复. */
    {
        uint32_t m = 0;
        if (ol_magic_read(&m) == OL_OK && m == OL_MAGIC_INSTALLING) {
            OL_LOGW("previous install was interrupted, rollback from backup");
            int rc = ol_updater_rollback("backup", "app");
            if (rc == OL_OK) {
                OL_LOGW("rollback ok");
            } else {
                OL_LOGE("rollback failed: %s", ol_strerror(rc));
            }
            (void)ol_magic_write(OL_MAGIC_NONE);
        }
    }
#endif

    int trigger = ol_boot_wait_trigger(OPENLOAD_BOOT_DELAY_MS);

    if (!trigger) {
        int rc = ol_boot_jump_to("app");
#if OPENLOAD_ENABLE_BACKUP
        /* jump 失败 → app verify fail / 分区缺失等. 给 backup 一次机会. */
        if (rc != OL_OK && rc != OL_E_PART_NOT_FOUND) {
            OL_LOGW("app jump failed (%s), try rollback from backup",
                    ol_strerror(rc));
            int rr = ol_updater_rollback("backup", "app");
            if (rr == OL_OK) {
                OL_LOGW("rollback ok, retry jump");
                rc = ol_boot_jump_to("app");
            } else {
                OL_LOGW("rollback unavailable: %s", ol_strerror(rr));
            }
        }
#endif
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
