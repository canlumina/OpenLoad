/*
 * OpenLoad - 内置命令集合
 *
 * 用一个文件汇总所有 M1 默认命令; 用户可在自己的源文件中通过
 * OL_CMD_REGISTER 追加更多命令。
 *
 * 命令:
 *   help                       列出所有命令
 *   info                       打印框架版本 + 板子 ID
 *   reset                      复位 SoC
 *   jump   [partition]         跳转到 App (默认 "app")
 *   part                       列出分区
 *   erase  <partition>         擦除整个分区
 *   update <protocol> <staging> <target>
 *                              通过 protocol 接收固件到 staging, 再安装到 target
 *   install <staging> <target> 假设 staging 已含固件, 直接安装
 */
#include "openload/cli.h"
#include "openload/logger.h"
#include "openload/partition.h"
#include "openload/updater.h"
#include "openload/image.h"
#include "openload/ops/sys_ops.h"
#include "openload/openload.h"
#include "openload/errno.h"
#include "openload/config.h"
#if OPENLOAD_ENABLE_OPLOG
#  include "openload/oplog.h"
#endif
#include <string.h>

/* ---------- help ---------- */
static int cmd_help(int argc, char **argv)
{
    const ol_cmd_t *tbl;
    uint32_t n = ol_cmd_table(&tbl);

    /* help <cmd>: 按名查 long_help (没注册的回退一行简介). */
    if (argc >= 2) {
        for (uint32_t i = 0; i < n; ++i) {
            if (tbl[i].name && strcmp(tbl[i].name, argv[1]) == 0) {
                if (tbl[i].long_help) {
                    ol_print(tbl[i].long_help);
                } else {
                    ol_printf("  %-10s %s\r\n", tbl[i].name,
                              tbl[i].help ? tbl[i].help : "");
                    ol_print("(no detailed help)\r\n");
                }
                return OL_OK;
            }
        }
        ol_printf("unknown: %s\r\n", argv[1]);
        return OL_E_NOT_FOUND;
    }

    /* help (无参): 列全表. */
    for (uint32_t i = 0; i < n; ++i) {
        ol_printf("  %-10s %s\r\n", tbl[i].name, tbl[i].help ? tbl[i].help : "");
    }
    return OL_OK;
}
OL_CMD_REGISTER("help", "List all commands (try `help <cmd>` for details)", cmd_help);

/* ---------- info ---------- */
static int cmd_info(int argc, char **argv)
{
    (void)argc; (void)argv;
    ol_printf("OpenLoad %s\r\n", OPENLOAD_VERSION_STR);
    ol_printf("board_id  : 0x%04x\r\n", OPENLOAD_BOARD_ID);
    ol_printf("strategy  : %lu\r\n", (uint32_t)OPENLOAD_UPGRADE_STRATEGY);
    ol_printf("log level : %lu\r\n", (uint32_t)ol_log_get_level());

    /* 试读 App 头展示当前固件版本 */
    const ol_partition_t *app = ol_part_find("app");
    if (app) {
        ol_image_header_t hdr;
        if (ol_image_read_header(app, &hdr) == OL_OK) {
            ol_printf("app ver   : %lu.%lu.%lu.%lu (size %lu)\r\n",
                      OL_IMG_VER_MAJOR(hdr.firmware_version),
                      OL_IMG_VER_MINOR(hdr.firmware_version),
                      OL_IMG_VER_PATCH(hdr.firmware_version),
                      OL_IMG_VER_BUILD(hdr.firmware_version),
                      hdr.firmware_size);
        } else {
            ol_print("app ver   : (no valid image)\r\n");
        }
    }
    return OL_OK;
}
OL_CMD_REGISTER("info", "Show bootloader and app info", cmd_info);

/* ---------- reset ---------- */
static int cmd_reset(int argc, char **argv)
{
    (void)argc; (void)argv;
    ol_print("rebooting...\r\n");
    ol_reboot();
    return OL_OK;
}
OL_CMD_REGISTER("reset", "Reboot the SoC", cmd_reset);

/* ---------- jump ---------- */
static int cmd_jump(int argc, char **argv)
{
    const char *name = (argc > 1) ? argv[1] : "app";
    int rc = ol_boot_jump_to(name);
    ol_printf("jump failed: %s\r\n", ol_strerror(rc));
    return rc;
}
OL_CMD_REGISTER("jump", "Verify and jump to app partition", cmd_jump);

/* ---------- part ---------- */
static int cmd_part(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint32_t cnt;
    const ol_partition_t *tbl = ol_part_table(&cnt);
    ol_print("name        device   offset      size       flags\r\n");
    for (uint32_t i = 0; i < cnt; ++i) {
        ol_printf("  %-10s %-8s 0x%08lx  %-8lu  %c%c%c\r\n",
                  tbl[i].name, tbl[i].device_name,
                  tbl[i].offset, tbl[i].size,
                  (tbl[i].flags & OL_PART_FLAG_READABLE)   ? 'r' : '-',
                  (tbl[i].flags & OL_PART_FLAG_WRITABLE)   ? 'w' : '-',
                  (tbl[i].flags & OL_PART_FLAG_EXECUTABLE) ? 'x' : '-');
    }
    return OL_OK;
}
OL_CMD_REGISTER("part", "List partitions", cmd_part);

/* ---------- erase ---------- */
static int cmd_erase(int argc, char **argv)
{
    if (argc < 2) {
        ol_print("usage: erase <partition>\r\n");
        return OL_E_INVAL;
    }
    const ol_partition_t *p = ol_part_find(argv[1]);
    if (!p) {
        ol_printf("not found: %s\r\n", argv[1]);
        return OL_E_PART_NOT_FOUND;
    }
    ol_printf("erasing %s (%lu bytes)...\r\n", p->name, p->size);
    int rc = ol_part_erase_all(p);
    ol_printf("%s\r\n", ol_strerror(rc));
    return rc;
}
OL_CMD_REGISTER("erase", "Erase a partition", cmd_erase);

/* ---------- update ---------- */
static const char update_long_help[] =
    "update <protocol> <staging> <target> [url]\r\n"
    "  protocol: xmodem | xmodem-1k | ymodem | http\r\n"
    "  staging:  partition buffering incoming firmware\r\n"
    "  target:   partition firmware is installed to\r\n"
    "  url:      required for http; ignored otherwise\r\n"
    "examples:\r\n"
    "  update xmodem download app\r\n"
    "  update http   download app http://192.168.1.5/app.bin\r\n";

static int cmd_update(int argc, char **argv)
{
    if (argc < 4) {
        ol_print("usage: update <protocol> <staging> <target> [url]\r\n");
        ol_print("  e.g.: update xmodem download app\r\n");
        ol_print("        update http download app http://host[:port]/path\r\n");
        return OL_E_INVAL;
    }
    const char *url = (argc >= 5) ? argv[4] : NULL;
    int rc = ol_updater_run(argv[1], argv[2], argv[3], url);
    ol_printf("update: %s\r\n", ol_strerror(rc));
    return rc;
}
OL_CMD_REGISTER_FULL("update",
                     "Receive firmware and install (proto staging target [url])",
                     update_long_help, cmd_update);

/* ---------- install ---------- */
static int cmd_install(int argc, char **argv)
{
    if (argc < 3) {
        ol_print("usage: install <staging> <target> [force]\r\n");
        ol_print("  force: bypass anti-rollback (use with caution)\r\n");
        return OL_E_INVAL;
    }
    uint32_t flags = 0;
    if (argc >= 4 && strcmp(argv[3], "force") == 0) {
        flags |= OL_INSTALL_F_FORCE;
    }
    int rc = ol_updater_install_ex(argv[1], argv[2], flags);
    ol_printf("install: %s\r\n", ol_strerror(rc));
    return rc;
}
OL_CMD_REGISTER("install", "Install pre-staged firmware (staging target [force])", cmd_install);

#if OPENLOAD_ENABLE_BACKUP
/* ---------- backup / rollback ---------- */
static int cmd_backup(int argc, char **argv)
{
    const char *target = (argc > 1) ? argv[1] : "app";
    const char *backup = (argc > 2) ? argv[2] : "backup";
    int rc = ol_updater_backup(target, backup);
    ol_printf("backup %s -> %s: %s\r\n", target, backup, ol_strerror(rc));
    return rc;
}
OL_CMD_REGISTER("backup", "Backup current target (default: app -> backup)", cmd_backup);

static int cmd_rollback(int argc, char **argv)
{
    const char *backup = (argc > 1) ? argv[1] : "backup";
    const char *target = (argc > 2) ? argv[2] : "app";
    int rc = ol_updater_rollback(backup, target);
    ol_printf("rollback %s -> %s: %s\r\n", backup, target, ol_strerror(rc));
    return rc;
}
OL_CMD_REGISTER("rollback", "Restore target from backup (default: backup -> app)", cmd_rollback);
#endif

#if OPENLOAD_ENABLE_OPLOG
/* ---------- oplog ---------- */
static int oplog_print_cb(uint32_t seq, uint32_t ts_ms, uint8_t level,
                          const char *msg, uint8_t msg_len, void *user)
{
    (void)user;
    static const char tag[] = { '?', 'E', 'W', 'I', 'D' };
    char lvl = (level < sizeof(tag)) ? tag[level] : '?';
    /* msg 是从 flash 直读的 44 字节字段, 可能不带 null-term; 按 msg_len 打 */
    char tmp[OL_OPLOG_MSG_MAX + 1];
    uint32_t n = (msg_len > OL_OPLOG_MSG_MAX) ? OL_OPLOG_MSG_MAX : msg_len;
    if (n && msg) { memcpy(tmp, msg, n); }
    tmp[n] = 0;
    ol_printf("%6lu [%c] %10lu  %s\r\n",
              seq, lvl, ts_ms, tmp);
    return 0;
}

static const char oplog_long_help[] =
    "oplog <dump|clear|stat> [n]\r\n"
    "  dump [n]: print last n records (0 or omitted = all)\r\n"
    "  clear:    erase the entire op log partition\r\n"
    "  stat:     show ring buffer state (used/total slots, write_idx, next_seq)\r\n";

static int cmd_oplog(int argc, char **argv)
{
    if (argc < 2) {
        ol_print("usage: oplog <dump|clear|stat> [n]\r\n");
        return OL_E_INVAL;
    }
    if (strcmp(argv[1], "dump") == 0) {
        uint32_t n = 0;
        if (argc >= 3) {
            for (const char *p = argv[2]; *p; ++p) {
                if (*p < '0' || *p > '9') { return OL_E_INVAL; }
                n = n * 10 + (uint32_t)(*p - '0');
            }
        }
        int got = ol_oplog_iter(oplog_print_cb, NULL, n);
        if (got < 0) {
            ol_printf("oplog dump: %s\r\n", ol_strerror(got));
            return got;
        }
        ol_printf("(%d records)\r\n", got);
        return OL_OK;
    }
    if (strcmp(argv[1], "clear") == 0) {
        int rc = ol_oplog_clear();
        ol_printf("oplog clear: %s\r\n", ol_strerror(rc));
        return rc;
    }
    if (strcmp(argv[1], "stat") == 0) {
        ol_oplog_stat_t st;
        int rc = ol_oplog_get_stat(&st);
        if (rc != OL_OK) {
            ol_printf("oplog stat: %s\r\n", ol_strerror(rc));
            return rc;
        }
        ol_printf("ready=%u  used=%lu/%lu  write_idx=%lu  next_seq=%lu\r\n",
                  (unsigned)st.ready, st.valid_count,
                  st.total_slots, st.write_idx,
                  st.next_seq);
        return OL_OK;
    }
    ol_print("oplog: unknown subcommand\r\n");
    return OL_E_INVAL;
}
OL_CMD_REGISTER_FULL("oplog", "Persistent op log (dump|clear|stat)",
                     oplog_long_help, cmd_oplog);
#endif
