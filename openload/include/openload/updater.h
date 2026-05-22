/*
 * OpenLoad - 升级编排
 *
 * 把 "选 receiver → 下载到 staging → 校验 → 拷贝到 target → 校验" 这一串
 * 流程封装为单个调用, 命令层调用即可。
 */
#pragma once

#include <stdint.h>
#include "openload/config.h"   /* 拉入 OL_UPGRADE_* 常量 */

/**
 * @brief 一次完整升级.
 * @param  receiver_name  "xmodem" / "ymodem" / "http" 等
 * @param  staging_part   暂存分区名 (例如 "download"). 单 bank 模式传 NULL.
 * @param  target_part    目标 App 分区名 (例如 "app")
 * @param  url_or_null    HTTP 等需要 URL 的协议传, 否则 NULL
 * @return OL_OK 或负数错误码.
 */
int ol_updater_run(const char *receiver_name,
                   const char *staging_part,
                   const char *target_part,
                   const char *url_or_null);

/**
 * @brief 仅做 staging → target 拷贝 + 校验 (假设 staging 已存在有效固件).
 *        用于场景: 用户已通过其它工具把固件写到 W25Q64, 然后命令行 install.
 */
int ol_updater_install(const char *staging_part, const char *target_part);

/**
 * @brief 把当前 target 备份到 backup 分区 (供回滚使用).
 */
int ol_updater_backup(const char *target_part, const char *backup_part);

/**
 * @brief 从 backup 分区恢复到 target.
 */
int ol_updater_rollback(const char *backup_part, const char *target_part);
