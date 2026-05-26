/*
 * 用户工程的额外 CLI 命令 (M2 调试期)
 *
 * 这些命令是在 builtins 之外通过 OL_CMD_REGISTER 注册的, 用户可参考
 * 本文件给自己工程加私有命令.
 *
 * M2-12a:  uart2  — UART2 (ESP8266 链路) 联调
 *   uart2 echo                  字节回环, 收到啥 echo 回 UART2 + 打 console
 *   uart2 send <bytes...>       把后续参数按 ASCII 拼接发往 UART2
 *   uart2 baud <rate>           改 UART2 波特率
 *   uart2 dump                  把 UART2 ringbuf 当前缓存打出来
 *
 * M2-12b:  at / esp  — ESP8266 AT 引擎封装
 *   at <cmd...>                 透传一条 AT 命令, 等 OK/ERROR/timeout 后打响应
 *   esp ping                    探活 (内部 AT)
 *
 * M2-12c:  wifi / tcp  — WiFi + TCP 调试
 *   wifi join <ssid> <pass>     CWMODE=1 + CWJAP, 成功后自动保存到 wifi_cfg
 *   wifi load                   从 wifi_cfg 读凭据并 join
 *   wifi clear                  擦除 wifi_cfg
 *   wifi status                 CWJAP? + CIFSR
 *   tcp test <host> <port>      开 TCP + 发 HTTP GET / + 收响应打到 console
 */
#include "openload/cli.h"
#include "openload/logger.h"
#include "openload/errno.h"
#include "openload/ops/io_ops.h"
#include "openload/ops/sys_ops.h"
#include "openload/config.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#if OPENLOAD_ENABLE_ESP8266

#include "port_uart2.h"
#include "port_esp8266.h"
#include "wifi_cfg.h"

static int cmd_uart2_echo_loop(void)
{
    ol_io_dev_t *con = ol_io_dev_find("console");
    if (!con) { return OL_E_NOT_FOUND; }
    ol_print("uart2 echo: 任意 console 字符退出\r\n");

    uint8_t  buf[64];
    uint32_t start = ol_tick_ms();
    while (1) {
        int n = port_uart2_read(buf, sizeof(buf));
        if (n > 0) {
            (void)port_uart2_write(buf, (uint32_t)n);     /* echo 回 UART2 */
            /* 同步打到 console — 16 字节一行 hex */
            for (int i = 0; i < n; ++i) {
                ol_printf("%02X ", buf[i]);
                if (((i + 1) & 0x0F) == 0) { ol_print("\r\n"); }
            }
            ol_print("\r\n");
            start = ol_tick_ms();
        }
        /* 任意 console 输入即退出 */
        int a = con->ops->available ? con->ops->available(con) : 0;
        if (a > 0) {
            uint8_t junk;
            con->ops->read(con, &junk, 1);
            ol_print("exit echo\r\n");
            return OL_OK;
        }
        /* 30s 无输入也退出, 防止误进入卡死 */
        if ((ol_tick_ms() - start) > 30000U) {
            ol_print("uart2 echo idle 30s, exit\r\n");
            return OL_OK;
        }
    }
}

static int cmd_uart2(int argc, char **argv)
{
    if (argc < 2) {
        ol_print("usage:\r\n"
                 "  uart2 echo\r\n"
                 "  uart2 send <bytes...>\r\n"
                 "  uart2 baud <rate>\r\n"
                 "  uart2 dump\r\n");
        return OL_E_INVAL;
    }
    if (strcmp(argv[1], "echo") == 0) {
        return cmd_uart2_echo_loop();
    }
    if (strcmp(argv[1], "send") == 0) {
        if (argc < 3) { ol_print("missing payload\r\n"); return OL_E_INVAL; }
        for (int i = 2; i < argc; ++i) {
            (void)port_uart2_write((const uint8_t *)argv[i],
                                   (uint32_t)strlen(argv[i]));
            if (i + 1 < argc) {
                (void)port_uart2_write((const uint8_t *)" ", 1);
            }
        }
        (void)port_uart2_write((const uint8_t *)"\r\n", 2);
        ol_print("sent\r\n");
        return OL_OK;
    }
    if (strcmp(argv[1], "baud") == 0) {
        if (argc < 3) { ol_print("missing rate\r\n"); return OL_E_INVAL; }
        uint32_t baud = 0;
        for (const char *p = argv[2]; *p; ++p) {
            if (*p < '0' || *p > '9') { return OL_E_INVAL; }
            baud = baud * 10 + (uint32_t)(*p - '0');
        }
        int rc = port_uart2_setbaud(baud);
        ol_printf("setbaud %lu: %s\r\n", baud,
                  (rc == OL_OK) ? "ok" : "fail");
        return rc;
    }
    if (strcmp(argv[1], "dump") == 0) {
        uint8_t buf[128];
        int n = port_uart2_read(buf, sizeof(buf));
        ol_printf("dump %d bytes:\r\n", n);
        for (int i = 0; i < n; ++i) {
            ol_printf("%02X ", buf[i]);
            if (((i + 1) & 0x0F) == 0) { ol_print("\r\n"); }
        }
        if (n & 0x0F) { ol_print("\r\n"); }
        return OL_OK;
    }
    ol_printf("unknown sub-command: %s\r\n", argv[1]);
    return OL_E_INVAL;
}
OL_CMD_REGISTER("uart2", "UART2 联调 (echo/send/baud/dump)", cmd_uart2);

/* ============================================================
 * M2-12b: ESP8266 AT 透传
 * ============================================================ */
static int cmd_at(int argc, char **argv)
{
    if (argc < 2) {
        ol_print("usage: at <cmd...>\r\n"
                 "  e.g.: at AT\r\n"
                 "        at AT+GMR\r\n"
                 "        at AT+CWMODE=1\r\n");
        return OL_E_INVAL;
    }

    /* 把多 argv 拼成单行 AT 命令, 字段间补空格 (绝大多数 AT 不含空格,
     * 偶尔出现的 AT+CIPSTART="TCP","host",80 内部用引号包裹, 不影响) */
    char cmd[160];
    uint32_t off = 0;
    for (int i = 1; i < argc; ++i) {
        uint32_t l = (uint32_t)strlen(argv[i]);
        if (off + l + 2 >= sizeof(cmd)) { break; }
        memcpy(cmd + off, argv[i], l);
        off += l;
        if (i + 1 < argc) { cmd[off++] = ' '; }
    }
    cmd[off] = 0;

    char resp[512];
    int rc = port_esp_at_send(cmd, 3000, resp, sizeof(resp));
    ol_print("--- response ---\r\n");
    ol_print(resp);
    if (resp[0] && resp[strlen(resp) - 1] != '\n') { ol_print("\r\n"); }
    ol_print("--- end ---\r\n");
    ol_printf("at: %s\r\n", ol_strerror(rc));
    return rc;
}
OL_CMD_REGISTER("at", "Send raw AT to ESP8266 (e.g. at AT+GMR)", cmd_at);

static int cmd_esp(int argc, char **argv)
{
    if (argc < 2) {
        ol_print("usage: esp <ping>\r\n");
        return OL_E_INVAL;
    }
    if (strcmp(argv[1], "ping") == 0) {
        int rc = port_esp_at_ping();
        ol_printf("esp ping: %s\r\n", ol_strerror(rc));
        return rc;
    }
    ol_printf("unknown sub-command: %s\r\n", argv[1]);
    return OL_E_INVAL;
}
OL_CMD_REGISTER("esp", "ESP8266 helpers (ping/...)", cmd_esp);

/* ============================================================
 * M2-12c: WiFi 连接 + TCP smoke
 * ============================================================ */
static int parse_u32(const char *s, uint32_t *out)
{
    if (!s || !*s) { return OL_E_INVAL; }
    uint32_t v = 0;
    for (; *s; ++s) {
        if (*s < '0' || *s > '9') { return OL_E_INVAL; }
        v = v * 10 + (uint32_t)(*s - '0');
    }
    *out = v;
    return OL_OK;
}

static int cmd_wifi(int argc, char **argv)
{
    if (argc < 2) {
        ol_print("usage:\r\n"
                 "  wifi join <ssid> <pass>   join + 自动保存\r\n"
                 "  wifi load                 从 wifi_cfg 读 + join\r\n"
                 "  wifi clear                清空 wifi_cfg\r\n"
                 "  wifi status               当前 AP/IP 信息\r\n");
        return OL_E_INVAL;
    }
    if (strcmp(argv[1], "join") == 0) {
        if (argc < 4) {
            ol_print("usage: wifi join <ssid> <pass>\r\n");
            return OL_E_INVAL;
        }
        ol_printf("joining %s ...\r\n", argv[2]);
        int rc = port_esp_wifi_join(argv[2], argv[3]);
        if (rc != OL_OK) {
            ol_printf("wifi join: %s\r\n", ol_strerror(rc));
            return rc;
        }
        /* 自动持久化, 失败不影响连接已成功 */
        int srv = wifi_cfg_save(argv[2], argv[3]);
        ol_printf("wifi join: ok (save: %s)\r\n", ol_strerror(srv));
        return OL_OK;
    }
    if (strcmp(argv[1], "load") == 0) {
        char ssid[64], pass[64];
        int rc = wifi_cfg_load(ssid, sizeof(ssid), pass, sizeof(pass));
        if (rc != OL_OK) {
            ol_printf("wifi load: %s\r\n", ol_strerror(rc));
            return rc;
        }
        ol_printf("loaded ssid=%s, joining...\r\n", ssid);
        rc = port_esp_wifi_join(ssid, pass);
        ol_printf("wifi load: %s\r\n", ol_strerror(rc));
        return rc;
    }
    if (strcmp(argv[1], "clear") == 0) {
        int rc = wifi_cfg_clear();
        ol_printf("wifi clear: %s\r\n", ol_strerror(rc));
        return rc;
    }
    if (strcmp(argv[1], "status") == 0) {
        char info[512];
        int rc = port_esp_wifi_status(info, sizeof(info));
        ol_print("--- wifi status ---\r\n");
        ol_print(info);
        if (info[0] && info[strlen(info) - 1] != '\n') { ol_print("\r\n"); }
        ol_print("--- end ---\r\n");
        ol_printf("wifi: %s\r\n", ol_strerror(rc));
        return rc;
    }
    ol_printf("unknown: %s\r\n", argv[1]);
    return OL_E_INVAL;
}
OL_CMD_REGISTER("wifi", "WiFi (join/load/clear/status)", cmd_wifi);

static int cmd_tcp(int argc, char **argv)
{
    if (argc < 4 || strcmp(argv[1], "test") != 0) {
        ol_print("usage: tcp test <host> <port>\r\n"
                 "  e.g.: tcp test 192.168.1.100 8000\r\n");
        return OL_E_INVAL;
    }
    const char *host = argv[2];
    uint32_t port = 0;
    int rc = parse_u32(argv[3], &port);
    if (rc != OL_OK || port == 0 || port > 65535) {
        ol_print("invalid port\r\n");
        return OL_E_INVAL;
    }

    ol_printf("tcp connecting %s:%u ...\r\n", host, (unsigned)port);
    rc = port_esp_tcp_open(host, (uint16_t)port);
    if (rc != OL_OK) {
        ol_printf("tcp open: %s\r\n", ol_strerror(rc));
        return rc;
    }

    /* 简单 HTTP GET. Connection: close 让 server 发完就断, 我们靠 +IPD 段 +
     * 总超时退出循环. */
    char req[200];
    int reqlen = snprintf(req, sizeof(req),
                          "GET / HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n"
                          "User-Agent: OpenLoad-Smoke/1\r\n\r\n",
                          host);
    if (reqlen <= 0 || (size_t)reqlen >= sizeof(req)) {
        port_esp_tcp_close();
        return OL_E_INVAL;
    }
    rc = port_esp_tcp_send((const uint8_t *)req, (uint32_t)reqlen);
    if (rc != OL_OK) {
        ol_printf("tcp send: %s\r\n", ol_strerror(rc));
        port_esp_tcp_close();
        return rc;
    }

    /* 循环收 +IPD 段, 直到 3s 无新数据或拿到 16 段 */
    ol_print("--- response ---\r\n");
    ol_io_dev_t *con = ol_io_dev_find("console");
    uint8_t rxbuf[256];
    int total_bytes = 0;
    for (int seg = 0; seg < 16; ++seg) {
        int got = port_esp_tcp_recv(rxbuf, sizeof(rxbuf), 3000);
        if (got <= 0) { break; }
        total_bytes += got;
        if (con) { (void)con->ops->write(con, rxbuf, (uint32_t)got); }
    }
    ol_print("\r\n--- end ---\r\n");
    ol_printf("tcp test: got %d bytes\r\n", total_bytes);

    (void)port_esp_tcp_close();
    return (total_bytes > 0) ? OL_OK : OL_E_TIMEOUT;
}
OL_CMD_REGISTER("tcp", "TCP smoke (test <host> <port>)", cmd_tcp);

#endif  /* OPENLOAD_ENABLE_ESP8266 */

/* ================================================================
 *  M6-1: STM32 RDP (Read-out Protection) 软件控制
 *
 *  rdp           — 显示当前 RDP level (0/1/2) 与含义
 *  rdp status    — 同上
 *  rdp lock      — 触发 L0→L1, 带 10s 'y' 确认 + 清晰警告
 *
 *  L2 不在 CLI 暴露 — 永久不可逆, 走 ST_LINK 外部工具烧 option byte.
 *  解锁出测试板: STM32_Programmer_CLI -c port=SWD -ob RDP=0xAA
 *  (触发 mass erase, 整片 flash 被擦, 之后重烧 bootloader.)
 * ================================================================ */
#if OPENLOAD_ENABLE_RDP

static const char *rdp_level_desc(uint8_t lvl)
{
    switch (lvl) {
        case OL_RDP_LEVEL_NONE:      return "unlocked, debug access full";
        case OL_RDP_LEVEL_READ_PROT: return "read-protected, SWD locked from flash";
        case OL_RDP_LEVEL_FULL:      return "FULL, permanent, debug forever closed";
        default:                     return "unknown";
    }
}

static int cmd_rdp(int argc, char **argv)
{
    uint8_t lvl;
    int rc;

    if (argc < 2 || strcmp(argv[1], "status") == 0) {
        rc = ol_rdp_get(&lvl);
        if (rc == OL_E_NOT_SUPPORTED) {
            ol_print("rdp: not supported on this port\r\n");
            return rc;
        }
        if (rc != OL_OK) {
            ol_printf("rdp: get level failed (%d)\r\n", rc);
            return rc;
        }
        ol_printf("RDP level : %u  (%s)\r\n", lvl, rdp_level_desc(lvl));
        return OL_OK;
    }

    if (strcmp(argv[1], "lock") == 0) {
        rc = ol_rdp_get(&lvl);
        if (rc == OL_E_NOT_SUPPORTED) {
            ol_print("rdp lock: not supported on this port\r\n");
            return rc;
        }
        if (rc != OL_OK) {
            ol_printf("rdp lock: cannot read current level (%d)\r\n", rc);
            return rc;
        }
        if (lvl != OL_RDP_LEVEL_NONE) {
            ol_printf("rdp lock: current is L%u, only L0 can be locked\r\n", lvl);
            return OL_E_INVAL;
        }

        ol_print("\r\n");
        ol_print("================ WARNING =================\r\n");
        ol_print("RDP L0 -> L1 makes SWD/JTAG unable to read flash.\r\n");
        ol_print("Recovery wipes the entire flash (mass erase).\r\n");
        ol_print("After lock, this dev board can NOT be flashed normally\r\n");
        ol_print("until you run:\r\n");
        ol_print("  STM32_Programmer_CLI -c port=SWD -ob RDP=0xAA\r\n");
        ol_print("==========================================\r\n");
        ol_print("Press 'y' within 10s to confirm, anything else cancels.\r\n");
        ol_print("> ");

        ol_io_dev_t *con = ol_io_dev_find("console");
        if (!con) { return OL_E_NOT_FOUND; }

        uint32_t start = ol_tick_ms();
        uint8_t  ch    = 0;
        while ((ol_tick_ms() - start) < 10000U) {
            int a = con->ops->available ? con->ops->available(con) : 0;
            if (a > 0) {
                con->ops->read(con, &ch, 1);
                break;
            }
        }
        ol_print("\r\n");

        if (ch != 'y' && ch != 'Y') {
            ol_print("rdp lock: cancelled\r\n");
            return OL_OK;
        }

        ol_print("rdp lock: programming RDP=L1 (device will reset)...\r\n");
        rc = ol_rdp_lock();
        /* OB_Launch 一般立即触发复位, 不会返回. 防御性兜底: */
        if (rc == OL_OK) {
            ol_print("rdp lock: OB programmed but no auto-reset; rebooting\r\n");
            ol_reboot();
        }
        ol_printf("rdp lock: failed (%d)\r\n", rc);
        return rc;
    }

    ol_print("usage: rdp [status|lock]\r\n");
    return OL_E_INVAL;
}
OL_CMD_REGISTER("rdp", "RDP status / lock L0->L1 (irreversible)", cmd_rdp);

#endif  /* OPENLOAD_ENABLE_RDP */
