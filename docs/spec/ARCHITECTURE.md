# OpenLoad 架构设计文档

> **版本**: v0.1 (重构方案初稿)
> **日期**: 2026-05-22
> **状态**: 待评审
> **配套文档**: [REQUIREMENTS.md](./REQUIREMENTS.md)

---

## 1. 设计原则

| 原则 | 落实方式 |
|------|---------|
| **接口隔离** | 平台相关操作只通过 `ops` 结构体暴露，核心层不持有任何 HAL 头文件 |
| **编译期裁剪** | 配置宏 `OPENLOAD_ENABLE_*` 控制 CMake/Makefile 是否将该模块的 .c 加入构建 |
| **静态分配** | 不使用 `malloc`；所有缓冲区在编译期由配置宏确定大小 |
| **段属性聚合** | 命令注册、receiver 注册用链接段实现自动发现（无需 main 手工 init） |
| **错误码统一** | 所有公开 API 返回 `int`，0 = 成功，负数 = 模块化错误码 |
| **零深拷贝** | 流式接收：数据到达即写 Flash，不缓存整个固件 |

---

## 2. 总体架构

```
                ┌─────────────────────────────────────────────┐
                │           用户工程 (App Project)             │
                │  main.c · openload_config.h · board_init.c  │
                └────────────┬────────────────────┬───────────┘
                             │                    │ implements
                             ▼                    ▼
┌──────────────────────────────────────────────────────────────────┐
│                       OpenLoad Core                              │
│ ┌──────────────────────────────────────────────────────────────┐ │
│ │                  Boot Manager (状态机)                        │ │
│ │  init → entry decision → cli/update → verify → jump          │ │
│ └──────────────────────────────────────────────────────────────┘ │
│ ┌────────────┬──────────────┬────────────┬──────────┬─────────┐ │
│ │   CLI      │  Receivers   │  Crypto    │  Logger  │ Updater │ │
│ │  cmd reg   │  xmodem      │  crc32     │  multi   │ staging │ │
│ │  parser    │  ymodem      │  aes       │  level   │ install │ │
│ │  history   │  http_ota    │  sha256    │  color   │ verify  │ │
│ │            │  user_proto  │  ed25519   │  persist │ rollback│ │
│ └────────────┴──────┬───────┴────────────┴──────────┴────┬────┘ │
│ ┌──────────────────▼────────────────────────────────────▼─────┐ │
│ │     Partition Manager  (统一分区抽象, 支持多设备)            │ │
│ └─────────────────────────────┬─────────────────────────────────┘ │
│ ┌─────────────────────────────▼─────────────────────────────────┐ │
│ │     Porting API  (ol_flash_ops_t · ol_io_ops_t · ol_sys_ops) │ │
│ └─────────────────────────────┬─────────────────────────────────┘ │
└────────────────────────────────┼─────────────────────────────────┘
                                 ▼
                ┌──────────────────────────────────┐
                │       Port Layer (BSP)           │
                │  HAL 调用 / 寄存器操作 / 中断    │
                │  (ports/stm32f1/ 等)             │
                └──────────────────────────────────┘
```

**核心规则**：
- **上层只 include 下层接口头**，反向 include 是编译错误
- **同层模块通过 Partition Manager 协作**，不互相直接 include
- **Port Layer 只实现接口，不调用核心 API**

---

## 3. 目录结构

```
OpenLoad/
├── openload/                       # 框架本体 (核心代码)
│   ├── include/openload/
│   │   ├── openload.h              # 总入口头
│   │   ├── config_default.h        # 默认配置项
│   │   ├── errno.h                 # 错误码定义
│   │   ├── boot.h                  # Boot Manager
│   │   ├── cli.h                   # 命令行接口
│   │   ├── partition.h             # 分区管理
│   │   ├── receiver.h              # Receiver 基类
│   │   ├── image.h                 # 固件头格式
│   │   ├── logger.h                # 日志
│   │   ├── ops/
│   │   │   ├── flash_ops.h         # Flash 接口
│   │   │   ├── io_ops.h            # IO 接口
│   │   │   └── sys_ops.h           # 系统接口
│   │   └── proto/
│   │       ├── xmodem.h
│   │       ├── ymodem.h
│   │       └── http_ota.h
│   ├── core/
│   │   ├── boot.c
│   │   ├── cli.c
│   │   ├── partition.c
│   │   ├── image.c
│   │   ├── logger.c
│   │   └── ringbuf.c               # 通用工具
│   ├── proto/
│   │   ├── xmodem.c
│   │   ├── ymodem.c
│   │   └── http_ota.c
│   ├── crypto/
│   │   ├── crc32.c
│   │   ├── aes_ctr.c
│   │   ├── sha256.c
│   │   └── ed25519.c
│   ├── commands/                   # 内置命令实现
│   │   ├── cmd_help.c
│   │   ├── cmd_info.c
│   │   ├── cmd_update.c
│   │   ├── cmd_part.c
│   │   ├── cmd_erase.c
│   │   ├── cmd_jump.c
│   │   └── cmd_reset.c
│   └── CMakeLists.txt
│
├── ports/                          # 参考实现
│   └── stm32f1/
│       ├── include/
│       │   └── port_stm32f1.h
│       ├── src/
│       │   ├── port_sys.c          # 系统时钟 / tick / 跳转 / 复位
│       │   ├── port_flash_int.c    # 内部 Flash 实现 ol_flash_ops_t
│       │   ├── port_flash_w25q64.c # 外部 Flash 实现 ol_flash_ops_t
│       │   ├── port_io_uart.c      # UART DMA 实现 ol_io_ops_t
│       │   ├── port_io_esp_http.c  # 通过 ESP8266 AT 桥接为 ol_io_ops_t (网络通道)
│       │   ├── port_button.c
│       │   ├── esp8266_at.c        # AT 协议底层 (内部用)
│       │   └── ringbuffer.c
│       └── CMakeLists.txt
│
├── examples/                       # 示例工程
│   ├── stm32f103zet6_gcc/          # CMake + GCC 模板
│   │   ├── CMakeLists.txt
│   │   ├── openload_config.h       # 用户配置
│   │   ├── main.c
│   │   ├── board.c                 # 引脚/外设初始化
│   │   ├── partitions.def          # 分区表声明 (用 X-macro)
│   │   ├── linker.ld
│   │   └── startup_stm32f103xe.s
│   └── stm32f103zet6_keil/         # Keil MDK 工程
│
├── tools/                          # PC 端工具
│   ├── image_tool.py               # 给 bin 加 header / 签名 / 加密
│   └── menuconfig.py               # (v2) Kconfig 风格图形配置
│
├── tests/                          # 单元测试 (host 编译)
│   ├── test_crc32.c
│   ├── test_xmodem.c
│   ├── test_image.c
│   └── CMakeLists.txt
│
├── docs/
│   ├── REQUIREMENTS.md
│   ├── ARCHITECTURE.md             # 本文件
│   ├── PORTING_GUIDE.md
│   ├── PROTOCOL_SPEC.md
│   └── MIGRATION_FROM_V0.md
│
└── legacy/                         # 旧代码归档 (可选保留, M2 后删除)
    └── example/                    # 当前的 example/ 移到这里
```

---

## 4. Porting API（核心接口签名）

### 4.1 Flash 接口

```c
// openload/include/openload/ops/flash_ops.h

#include <stdint.h>
#include <stdbool.h>

typedef struct ol_flash_dev ol_flash_dev_t;

typedef struct {
    /** 同步读 */
    int (*read)(ol_flash_dev_t *dev, uint32_t offset,
                void *buf, uint32_t len);

    /** 同步写。len 必须按 write_granularity 对齐 */
    int (*write)(ol_flash_dev_t *dev, uint32_t offset,
                 const void *buf, uint32_t len);

    /** 按 sector 擦除。offset + len 必须按 sector_size 对齐 */
    int (*erase)(ol_flash_dev_t *dev, uint32_t offset, uint32_t len);

    /** 可选: 解除写保护 */
    int (*unlock)(ol_flash_dev_t *dev);

    /** 可选: 加写保护 */
    int (*lock)(ol_flash_dev_t *dev);
} ol_flash_ops_t;

struct ol_flash_dev {
    const char         *name;             /* 设备名, 如 "internal" */
    uint32_t            base;             /* 设备起始绝对地址 (XIP 设备用) */
    uint32_t            size;             /* 设备总大小 (bytes) */
    uint32_t            sector_size;      /* 最小擦除单元 */
    uint32_t            write_granularity;/* 最小写入粒度 (1/2/4/8/256 ...) */
    bool                xip;              /* 是否可直接 CPU 访问 (内部 Flash=true) */
    const ol_flash_ops_t *ops;
    void               *priv;             /* 驱动私有数据 */
};

/* 用户用此宏注册设备 (放到链接段供 partition manager 枚举) */
#define OL_FLASH_DEV_REGISTER(name, dev_ptr) \
    static const ol_flash_dev_t * const __ol_flash_##name \
        __attribute__((used, section(".ol_flash_devs"))) = (dev_ptr)

/* 查找已注册设备 */
ol_flash_dev_t *ol_flash_dev_find(const char *name);
```

### 4.2 IO 接口（UART / USB CDC / TCP 通用）

```c
// openload/include/openload/ops/io_ops.h

typedef struct ol_io_dev ol_io_dev_t;

typedef struct {
    /** 非阻塞读: 返回实际读到的字节数, 0=无数据, <0=错误 */
    int (*read)(ol_io_dev_t *dev, uint8_t *buf, uint32_t len);

    /** 阻塞写: 全部写完返回 len, <0=错误 */
    int (*write)(ol_io_dev_t *dev, const uint8_t *buf, uint32_t len);

    /** 可选: 立即可读字节数查询 */
    int (*available)(ol_io_dev_t *dev);

    /** 可选: 刷新发送缓冲 */
    int (*flush)(ol_io_dev_t *dev);
} ol_io_ops_t;

struct ol_io_dev {
    const char       *name;
    const ol_io_ops_t *ops;
    void             *priv;
};

#define OL_IO_DEV_REGISTER(name, dev_ptr) \
    static const ol_io_dev_t * const __ol_io_##name \
        __attribute__((used, section(".ol_io_devs"))) = (dev_ptr)

ol_io_dev_t *ol_io_dev_find(const char *name);

/* 带超时阻塞读 (核心层提供基于 read+tick 的封装) */
int ol_io_read_timeout(ol_io_dev_t *dev, uint8_t *buf,
                       uint32_t len, uint32_t timeout_ms);
```

### 4.3 系统接口

```c
// openload/include/openload/ops/sys_ops.h

typedef struct {
    uint32_t (*tick_ms)(void);          /* 必须 */
    void     (*delay_ms)(uint32_t ms);  /* 可选, 默认基于 tick_ms 轮询 */
    void     (*reboot)(void);           /* 必须 */
    void     (*disable_irq)(void);      /* 必须 (跳转前用) */

    /** 跳转到指定地址执行 (Cortex-M 默认实现可用) */
    void     (*jump)(uint32_t app_addr);

    /** 持久化标志位读写 (用于 App ↔ Bootloader 通信) */
    int      (*magic_read)(uint32_t *out);
    int      (*magic_write)(uint32_t value);
} ol_sys_ops_t;

/* 用户在入口处一次性注册 */
void ol_sys_register(const ol_sys_ops_t *ops);

/* 核心层调用 */
uint32_t ol_tick_ms(void);
void     ol_delay_ms(uint32_t ms);
void     ol_reboot(void);
```

---

## 5. 分区管理

### 5.1 分区描述

```c
// openload/include/openload/partition.h

#define OL_PART_FLAG_READABLE   (1u << 0)
#define OL_PART_FLAG_WRITABLE   (1u << 1)
#define OL_PART_FLAG_EXECUTABLE (1u << 2)  /* App 区 */
#define OL_PART_FLAG_ENCRYPTED  (1u << 3)
#define OL_PART_FLAG_SIGNED     (1u << 4)

typedef struct {
    const char           *name;
    const char           *device_name;  /* 引用的 flash_dev 名字 */
    uint32_t              offset;       /* 设备内偏移 */
    uint32_t              size;
    uint32_t              flags;
} ol_partition_t;

/* 查询 */
const ol_partition_t *ol_part_find(const char *name);

/* 操作 (内部会查找 device 并调 ops) */
int ol_part_read(const ol_partition_t *p, uint32_t off,
                 void *buf, uint32_t len);
int ol_part_write(const ol_partition_t *p, uint32_t off,
                  const void *buf, uint32_t len);
int ol_part_erase(const ol_partition_t *p, uint32_t off, uint32_t len);
int ol_part_erase_all(const ol_partition_t *p);
int ol_part_verify_crc32(const ol_partition_t *p, uint32_t off,
                         uint32_t len, uint32_t expected);
```

### 5.2 分区表声明（用户在 `partitions.def`）

```c
/* X-macro 风格, 编译期生成分区表 */
/* device,    name,       offset,    size,      flags */
OL_PART(internal, "boot",       0x00000000, 0x10000, OL_PART_FLAG_READABLE)
OL_PART(internal, "app",        0x00010000, 0x70000, OL_PART_FLAG_READABLE | \
                                                     OL_PART_FLAG_WRITABLE | \
                                                     OL_PART_FLAG_EXECUTABLE)
OL_PART(w25q64,   "download",   0x00000000, 0x200000, OL_PART_FLAG_READABLE | \
                                                      OL_PART_FLAG_WRITABLE)
OL_PART(w25q64,   "backup",     0x00200000, 0x70000,  OL_PART_FLAG_READABLE | \
                                                      OL_PART_FLAG_WRITABLE)
OL_PART(w25q64,   "config",     0x003D0000, 0x10000,  OL_PART_FLAG_READABLE | \
                                                      OL_PART_FLAG_WRITABLE)
OL_PART(w25q64,   "oplog",      0x00350000, 0x80000,  OL_PART_FLAG_READABLE | \
                                                      OL_PART_FLAG_WRITABLE)
```

核心层一次展开此宏生成 `g_partition_table[]` 数组。

---

## 6. 固件头格式 (Image Header)

```c
// openload/include/openload/image.h

#define OL_IMAGE_MAGIC      0x4F4C4F41   /* 'AOLO' little-endian = "OLOA" */
#define OL_IMAGE_HDR_SIZE   64
#define OL_IMAGE_FMT_VER    1

typedef struct {
    uint32_t magic;             /* OL_IMAGE_MAGIC */
    uint8_t  hdr_version;       /* 头格式版本 = OL_IMAGE_FMT_VER */
    uint8_t  flags;             /* bit0=encrypted, bit1=signed */
    uint16_t board_id;          /* 用户定义, 跨型号刷固件保护 */

    uint32_t firmware_size;     /* 原始 (解密后) 固件大小 */
    uint32_t firmware_crc32;    /* 原始固件 CRC32 */
    uint32_t firmware_version;  /* major:8 minor:8 patch:8 build:8 */
    uint32_t build_timestamp;   /* Unix epoch */

    uint8_t  firmware_sha256[16]; /* 截断的 SHA256 前 16 字节 (可选, 未启用为 0) */
    uint8_t  aes_iv[16];          /* AES-CTR IV (可选, 未启用为 0) */

    uint8_t  signature[/* TBD */]; /* 预留 v2 用于 Ed25519 */

    uint32_t hdr_crc32;         /* 头部本身 CRC32, 最后 4 字节 */
} __attribute__((packed)) ol_image_header_t;

_Static_assert(sizeof(ol_image_header_t) == OL_IMAGE_HDR_SIZE,
               "image header size mismatch");
```

**布局规则**：
```
[ header 64B ][ firmware payload (size 字节) ]
```
固件 payload 紧接 header，**总大小** = 64 + `firmware_size`。

---

## 7. 模块详细设计

### 7.1 Boot Manager（启动状态机）

```c
typedef enum {
    OL_BOOT_INIT,
    OL_BOOT_DECIDE,
    OL_BOOT_CLI,
    OL_BOOT_UPDATE,
    OL_BOOT_VERIFY,
    OL_BOOT_INSTALL,
    OL_BOOT_JUMP,
    OL_BOOT_RECOVERY,
    OL_BOOT_ERROR,
} ol_boot_state_t;

int  ol_boot_init(void);             /* 注册回调, 准备 console */
void ol_boot_run(void) __attribute__((noreturn));  /* 主循环 */
```

`ol_boot_run` 内部跑状态机，永不返回（要么跳 App，要么死循环 CLI）。

**决策逻辑**：
```
DECIDE 状态:
  1. 检查 magic_read() → 若 App 设置了 "go_bootloader" 标志, 进入 CLI
  2. 否则等待 OPENLOAD_BOOT_DELAY_MS:
     - 期间任一启用的触发源 (按键 / UART 字符) 满足 → 进入 CLI
  3. 超时未触发 → 校验 App
     - 校验通过 → JUMP
     - 校验失败 → RECOVERY (策略可配: CLI / 回滚 / 死等)
```

### 7.2 Receiver 抽象

```c
// openload/include/openload/receiver.h

typedef struct ol_receiver ol_receiver_t;

typedef struct {
    /** 接收启动: 协议握手, 拿到固件元数据 */
    int (*begin)(ol_receiver_t *r,
                 ol_io_dev_t  *io,
                 const ol_partition_t *dst);

    /** 主循环单次推进, 返回 0=继续 / 1=完成 / <0=错误 */
    int (*poll)(ol_receiver_t *r);

    /** 收尾: 错误或正常都要调 */
    int (*end)(ol_receiver_t *r);

    /** 进度查询 */
    uint32_t (*progress)(ol_receiver_t *r);  /* 返回 0..100 */
} ol_receiver_ops_t;

struct ol_receiver {
    const char            *name;       /* "xmodem" / "ymodem" / "http" */
    const ol_receiver_ops_t *ops;
    void                  *priv;       /* 协议私有状态 */
};

/* 各协议自行声明实例 */
extern ol_receiver_t ol_xmodem_receiver;
extern ol_receiver_t ol_ymodem_receiver;
extern ol_receiver_t ol_http_receiver;
```

**调用模式**（在命令 `update xmodem download` 中）：
```c
ol_receiver_t *r = &ol_xmodem_receiver;
ol_io_dev_t *io = ol_io_dev_find("console");
const ol_partition_t *part = ol_part_find("download");

if (r->ops->begin(r, io, part) < 0) goto err;
while (1) {
    int rc = r->ops->poll(r);
    if (rc == 1) break;        /* 完成 */
    if (rc < 0) goto err;
    /* 可在此打印进度 */
}
r->ops->end(r);
```

### 7.3 CLI 与命令注册

```c
// openload/include/openload/cli.h

typedef int (*ol_cmd_handler_t)(int argc, char **argv);

typedef struct {
    const char        *name;
    const char        *help;
    ol_cmd_handler_t   handler;
} ol_cmd_t;

#define OL_CMD_REGISTER(_name, _help, _handler) \
    static const ol_cmd_t __ol_cmd_##_handler \
        __attribute__((used, section(".ol_cmds"))) = { \
            .name = _name, .help = _help, .handler = _handler }

/* CLI 主循环 */
void ol_cli_run(ol_io_dev_t *io);

/* 输出 */
int ol_cli_printf(const char *fmt, ...);
```

**示例** (命令实现)：
```c
// openload/commands/cmd_jump.c

static int handle_jump(int argc, char **argv) {
    const ol_partition_t *app = ol_part_find("app");
    if (!app) return -OL_E_PART_NOT_FOUND;

    if (ol_image_verify(app) < 0) {
        ol_cli_printf("App invalid, refuse to jump\r\n");
        return -OL_E_IMAGE_INVALID;
    }
    ol_boot_jump_to(app);  /* never return */
    return 0;
}
OL_CMD_REGISTER("jump", "Verify and jump to app", handle_jump);
```

链接脚本需添加：
```ld
.ol_cmds : {
    PROVIDE(__ol_cmds_start = .);
    KEEP(*(.ol_cmds))
    PROVIDE(__ol_cmds_end = .);
} > FLASH
```

### 7.4 Crypto 子系统

每种算法是独立的 .c 文件，只在配置启用时纳入构建。

```c
// openload/include/openload/crypto.h

/* CRC32 (始终启用) */
uint32_t ol_crc32(uint32_t init, const void *data, uint32_t len);
uint32_t ol_crc32_partition(const ol_partition_t *p,
                            uint32_t off, uint32_t len);

#if OPENLOAD_ENABLE_AES
typedef struct ol_aes_ctx { ... } ol_aes_ctx_t;
int  ol_aes_ctr_init(ol_aes_ctx_t *ctx, const uint8_t *key,
                     uint32_t keylen, const uint8_t iv[16]);
void ol_aes_ctr_xcrypt(ol_aes_ctx_t *ctx, const uint8_t *in,
                       uint8_t *out, uint32_t len);
#endif

#if OPENLOAD_ENABLE_SHA256
typedef struct ol_sha256_ctx { ... } ol_sha256_ctx_t;
void ol_sha256_init(ol_sha256_ctx_t *ctx);
void ol_sha256_update(ol_sha256_ctx_t *ctx, const uint8_t *data, uint32_t len);
void ol_sha256_final(ol_sha256_ctx_t *ctx, uint8_t out[32]);
#endif

#if OPENLOAD_ENABLE_ED25519
int  ol_ed25519_verify(const uint8_t pubkey[32],
                       const uint8_t signature[64],
                       const uint8_t *msg, uint32_t msglen);
#endif
```

### 7.5 Updater（升级编排）

```c
// openload/include/openload/updater.h

typedef enum {
    OL_UPGRADE_SINGLE_BANK,
    OL_UPGRADE_STAGING,         /* v1 默认 */
    OL_UPGRADE_STAGING_BACKUP,
    OL_UPGRADE_DUAL_BANK,       /* v2 */
} ol_upgrade_strategy_t;

/* 完整升级流程封装 */
int ol_updater_run(const char *receiver_name,    /* "xmodem" / "http" */
                   const char *staging_part,     /* "download" */
                   const char *target_part,      /* "app" */
                   const char *url_or_null);     /* HTTP 时用 */
```

内部流程见 §8。

### 7.6 Logger

```c
typedef enum {
    OL_LOG_NONE = 0,
    OL_LOG_ERR  = 1,
    OL_LOG_WRN  = 2,
    OL_LOG_INF  = 3,
    OL_LOG_DBG  = 4,
} ol_log_level_t;

void ol_log(ol_log_level_t lvl, const char *fmt, ...);

#define OL_LOGE(...) ol_log(OL_LOG_ERR, __VA_ARGS__)
#define OL_LOGW(...) ol_log(OL_LOG_WRN, __VA_ARGS__)
#define OL_LOGI(...) ol_log(OL_LOG_INF, __VA_ARGS__)
#define OL_LOGD(...) ol_log(OL_LOG_DBG, __VA_ARGS__)
```

编译期裁剪：
```c
#if OPENLOAD_LOG_LEVEL < 4
#undef OL_LOGD
#define OL_LOGD(...) ((void)0)
#endif
/* 类似处理其他级别 */
```

---

## 8. 关键流程时序

### 8.1 启动流程

```
[Reset]
   │
   ▼
HAL_Init / SystemClock_Config        ← 用户 board_init.c
   │
   ▼
board_register_ops()                 ← 注册 sys/flash/io ops
   │
   ▼
ol_boot_init()                       ← 框架初始化
   │
   ▼
ol_boot_run()  ───▶  状态机循环
       │
       ├─ DECIDE
       │    ├─ App 设置了 magic? → CLI
       │    ├─ 等待 3s, 按键/UART? → CLI
       │    └─ 超时 → VERIFY
       │
       ├─ VERIFY
       │    ├─ 头部 CRC OK + payload CRC OK? → JUMP
       │    └─ 失败 → RECOVERY
       │
       ├─ CLI
       │    └─ ol_cli_run(console_io)  /* 用户命令进入 UPDATE */
       │
       ├─ UPDATE
       │    └─ ol_updater_run(...)
       │
       ├─ JUMP
       │    └─ ol_sys_jump(app_addr)   /* never return */
       │
       └─ RECOVERY  (策略可配)
            ├─ go to CLI / 自动从 backup 回滚 / 死等
```

### 8.2 XMODEM 升级时序

```
PC (sender)              MCU (receiver)
   │                          │
   │ ◀─── 'C' (轮询)──────────│  begin: 发 'C' 等待 SOH/STX
   │                          │
   │── SOH/STX + seq + data + CRC ─▶│
   │                          │  校验 CRC → 写入 staging partition
   │                          │  (流式写, 每 page 触发 flash 写)
   │ ◀──── ACK ───────────────│
   │                          │
   │   ... 重复多包 ...       │
   │                          │
   │── EOT ──────────────────▶│
   │ ◀──── NAK ───────────────│
   │── EOT ──────────────────▶│
   │ ◀──── ACK ───────────────│
   │                          │  end: 收尾, 返回 1
   │                          │
                              │  → 进入 VERIFY
                              │     ol_image_verify(staging)
                              │  → 进入 INSTALL
                              │     copy staging → app
                              │     ol_image_verify(app)
                              │  → JUMP
```

### 8.3 HTTP OTA 时序

```
CLI cmd: update http <url> --target download

   ol_updater_run("http", "download", "app", url)
        │
        ▼
   ol_http_receiver.begin()
        ├─ 解析 URL (host/port/path)
        ├─ 通过 ol_io_dev_find("net") 拿到网络通道
        │   (net 通道由 port_io_esp_http.c 实现, 内部走 AT)
        ├─ 发 HTTP GET 请求头
        └─ 解析响应头, 拿 Content-Length 与可选自定义头
                │
                ▼
   loop:  ol_http_receiver.poll()
        ├─ 从 net 读 chunk
        ├─ 流式写入 staging partition
        ├─ 累计 SHA / CRC (启用时)
        └─ 返回进度, 1=done
                │
                ▼
   ol_http_receiver.end()  → 关闭 socket

        │
        ▼
   后续: VERIFY → INSTALL → JUMP (与 XMODEM 路径合并)
```

### 8.4 Staging Install 流程

```
staging partition         app partition
  ┌──────────┐               ┌──────────┐
  │ header   │               │  (erased)│
  ├──────────┤               ├──────────┤
  │ payload  │   ─copy─▶     │ payload  │
  │          │               │ (raw,    │
  │          │               │  no hdr) │
  └──────────┘               └──────────┘

steps:
  1. verify_staging(): 读 staging[0..64], 校验 header CRC
                       计算 staging[64..64+size] CRC, 比对 header.crc32
                       若加密则解密后再算 CRC
  2. erase_app_partition()
  3. copy_loop:
       for each 4KB chunk:
         read staging[64+i..]
         (decrypt if needed)
         write app[i..]
  4. verify_app(): 重算 app CRC 比对 header.crc32
  5. write magic to ol_sys_ops.magic_write("APP_OK")
  6. reboot or jump
```

**断电窗口**：步骤 2~4 之间断电会导致 App 残缺。下次启动时 VERIFY 会失败 → 进入 RECOVERY。
若启用 `STAGING_WITH_BACKUP`：步骤 1 之后先把当前 App copy 到 backup 分区，步骤 4 失败时从 backup 回滚。

---

## 9. 配置项清单（`openload_config_default.h`）

```c
/* ============================================================
 *                   OpenLoad Configuration
 * ============================================================ */

/* --- Core --- */
#define OPENLOAD_BOOT_DELAY_MS              3000
#define OPENLOAD_ENTRY_TRIGGER_BUTTON       1
#define OPENLOAD_ENTRY_TRIGGER_UART         1
#define OPENLOAD_ENTRY_TRIGGER_MAGIC        1

/* --- Receivers --- */
#define OPENLOAD_ENABLE_XMODEM              1
#define OPENLOAD_ENABLE_XMODEM_1K           1
#define OPENLOAD_ENABLE_YMODEM              0  /* M2 */
#define OPENLOAD_ENABLE_HTTP_OTA            0  /* M2 */
#define OPENLOAD_ENABLE_USB_DFU             0  /* M4 */

/* --- Crypto --- */
#define OPENLOAD_ENABLE_CRC32               1  /* forced */
#define OPENLOAD_ENABLE_AES_128_CTR         0  /* M3 */
#define OPENLOAD_ENABLE_SHA256              0  /* M3 */
#define OPENLOAD_ENABLE_ED25519             0  /* M4 */

/* --- Image / Update --- */
#define OPENLOAD_IMAGE_FORMAT_VERSION       1
#define OPENLOAD_UPGRADE_STRATEGY           OL_UPGRADE_STAGING
#define OPENLOAD_ANTI_ROLLBACK              0
#define OPENLOAD_BOARD_ID                   0x0001

/* --- CLI --- */
#define OPENLOAD_ENABLE_CLI                 1
#define OPENLOAD_CLI_LINE_MAX               128
#define OPENLOAD_CLI_PROMPT                 "OpenLoad> "
#define OPENLOAD_CLI_PASSWORD               NULL  /* "mysecret" 启用 */
#define OPENLOAD_CLI_HISTORY                0

/* --- Logger --- */
#define OPENLOAD_LOG_LEVEL                  OL_LOG_INF
#define OPENLOAD_LOG_COLOR                  1
#define OPENLOAD_LOG_PERSISTENT             0

/* --- Buffers --- */
#define OPENLOAD_COPY_CHUNK_SIZE            4096
#define OPENLOAD_HTTP_RX_BUF_SIZE           1500
```

---

## 10. 错误码

```c
// openload/include/openload/errno.h

#define OL_OK                       0

/* Common (1..15) */
#define OL_E_INVAL                  -1
#define OL_E_TIMEOUT                -2
#define OL_E_NOMEM                  -3
#define OL_E_NOT_FOUND              -4
#define OL_E_BUSY                   -5
#define OL_E_IO                     -6

/* Partition (16..31) */
#define OL_E_PART_NOT_FOUND         -16
#define OL_E_PART_OUT_OF_RANGE      -17
#define OL_E_PART_WRITE_DENIED      -18
#define OL_E_PART_ALIGN             -19

/* Image (32..47) */
#define OL_E_IMAGE_MAGIC            -32
#define OL_E_IMAGE_HDR_CRC          -33
#define OL_E_IMAGE_PAYLOAD_CRC      -34
#define OL_E_IMAGE_SIZE             -35
#define OL_E_IMAGE_BOARD            -36
#define OL_E_IMAGE_VERSION          -37  /* anti-rollback */
#define OL_E_IMAGE_SIGNATURE        -38

/* Receiver (48..63) */
#define OL_E_RX_CANCELED            -48
#define OL_E_RX_PROTOCOL            -49
#define OL_E_RX_CRC                 -50

/* Crypto (64..79) */
#define OL_E_CRYPTO_KEY             -64
#define OL_E_CRYPTO_DECRYPT         -65
```

---

## 11. 内存与资源预算（细化）

**ROM** (Thumb-2, -Os, GCC 13)：

| 模块 | 预估字节 | 备注 |
|------|----------|------|
| boot.c | 1500 | 状态机 |
| partition.c | 1200 | 分区操作 |
| cli.c | 2500 | 解析 + 命令调度 |
| logger.c | 1000 | printf 精简版 |
| image.c | 800 | 头校验 |
| crc32.c | 1100 | 含查表 (1KB table) |
| xmodem.c | 1800 | CRC16 + 状态机 |
| ymodem.c | 800 (增量) | 复用 xmodem |
| http_ota.c | 2500 | HTTP/1.1 解析 |
| aes_ctr.c (tiny-AES) | 2000 | |
| sha256.c | 1800 | |
| ed25519.c (μNaCl) | 7000 | |
| 内置 6 个命令 | 2000 | |
| **核心合计** (M1) | **~12 KB** | |
| **+M2** (YMODEM+HTTP) | **~16 KB** | |
| **+M3** (AES+SHA) | **~20 KB** | |
| **+M4** (Ed25519) | **~27 KB** | |
| Port (STM32F1 参考) | ~10 KB | 含 HAL 子集 |

**RAM**:

| 用途 | 字节 |
|------|------|
| CLI line buffer | 128 |
| Copy chunk buffer | 4096 |
| UART RX FIFO (port 内) | 1024 |
| Receiver private (xmodem) | ~1200 (含 1K data buf) |
| HTTP RX buffer (启用时) | 1500 |
| Logger temp | 256 |
| **峰值合计** | **~8 KB** |

---

## 12. 旧代码迁移对照表

| 旧文件 | 处置 | 新位置 |
|--------|------|--------|
| `Core/Src/main.c` | 改写 | `examples/stm32f103zet6_gcc/main.c` (大幅精简) |
| `Core/Src/bootloader_cmd.c` | 拆分重写 | `openload/core/boot.c` + `openload/core/cli.c` + `openload/commands/*` |
| `Core/Src/xmodem.c` | 重写 | `openload/proto/xmodem.c` (用 io_ops 解耦) |
| `Core/Src/dev_usart.c` | 重写 | `ports/stm32f1/src/port_io_uart.c` (实现 ol_io_ops_t) |
| `Core/Src/ringbuff.c` | 保留 | `openload/core/ringbuf.c` (作为工具) 或 `ports/stm32f1/src/ringbuffer.c` |
| `Core/Src/w25q64.c` | 重写 | `ports/stm32f1/src/port_flash_w25q64.c` (实现 ol_flash_ops_t) |
| `Core/Src/spi.c`/`dma.c`/`gpio.c`/`usart.c` | 保留 | `ports/stm32f1/src/` (HAL 初始化) |
| `Core/Src/config.c` | 删除 | 改用 `openload_config.h` 编译期配置 |
| `Core/Src/firmware_crypto.c` | 删除 | 由 `openload/crypto/crc32.c` + `aes_ctr.c` 替代 |
| `Core/Src/firmware_aes.c` | 删除 | 同上 |
| `Core/Src/streaming_aes.c` | 删除 | AES-CTR 天然流式，无需独立模块 |
| `Core/Src/encrypted_firmware.c` | 删除 | 融入 `openload/core/updater.c` |
| `Core/Src/firmware_version.c` | 删除 | 融入 `openload/core/image.c` |
| `Core/Src/at_client.c` | 重写 | `ports/stm32f1/src/esp8266_at.c` (port 内部) |
| `Core/Src/esp8266_wifi.c` | 重写 | 融入 `ports/stm32f1/src/port_io_esp_http.c` |
| `Core/Src/http_client.c` | 重写 | `openload/proto/http_ota.c` (HTTP 层与 port 解耦) |
| `Core/Src/http_ota.c` | 重写 | 同上 |
| `Core/Src/network_mgr.c` | 删除 | 框架不需要"网络管理器"，HTTP receiver 直接走 io_ops |
| `Core/Inc/stm32f1xx_*.h` | 保留 | `ports/stm32f1/include/` (HAL conf) |
| `Core/mbedtls/` | **整个删除** | 自实现轻量 crypto |
| `middleware/mbedtls/` | **整个删除** | 同上 |
| `docs/bootloader_development_plan.md` | 归档 | `legacy/docs/` 或直接删除 |
| `docs/MBEDTLS_INTEGRATION.md` | **直接删除** | 不再使用 mbedtls |

---

## 13. 迭代路线图

```
M1 (核心 MVP)           M2 (联网升级)         M3 (加固)            M4+ (扩展)
─────────────────────   ─────────────────     ─────────────────    ─────────────
✦ Porting API           ✦ YMODEM             ✦ AES-128-CTR        ✦ Ed25519
✦ Partition mgr         ✦ HTTP OTA           ✦ SHA-256            ✦ ECDSA P-256
✦ Boot SM               ✦ ESP8266 port       ✦ 防回滚              ✦ USB DFU
✦ CLI + 6 cmds          ✦ 持久化日志          ✦ CLI 密码           ✦ MQTT OTA
✦ XMODEM/-1K            ✦ image_tool.py      ✦ STAGING_WITH_      ✦ Dual Bank Swap
✦ CRC32                 ✦ MIGRATION 文档      ✦   BACKUP           ✦ menuconfig.py
✦ Staging upgrade                            ✦ CI 编译矩阵         ✦ STM32F4 port
✦ STM32F103 port
✦ 入门文档

   ~2 周                  ~1.5 周               ~1.5 周              按需
```

---

## 14. 第三方依赖（可选 / 内嵌）

| 库 | 用途 | 体积 | 启用时机 |
|----|------|------|----------|
| 自实现 CRC32 | 完整性 | 1KB | 始终 |
| [tiny-AES-c](https://github.com/kokke/tiny-AES-c) | AES-128/256 | ~2KB | M3 |
| 自实现 SHA-256 | 摘要 | ~1.8KB | M3 |
| [μNaCl](https://munacl.cryptojedi.org/) 或 [micro-ecc](https://github.com/kmackay/micro-ecc) | Ed25519/ECDSA | ~7KB | M4 |

**全部以源码形式纳入 `openload/crypto/`**，不引入二进制依赖、不需要 git submodule。

---

## 15. 文档计划

- ✅ `REQUIREMENTS.md` — 需求规格 (本次)
- ✅ `ARCHITECTURE.md` — 架构设计 (本次)
- ⏸ `PORTING_GUIDE.md` — 写一份新 port 的完整步骤 (M1 完成时写)
- ⏸ `PROTOCOL_SPEC.md` — XMODEM/YMODEM/HTTP OTA 实现细节 (M2 完成时写)
- ⏸ `MIGRATION_FROM_V0.md` — 老版本迁移说明 (M3 完成时写)
- ⏸ `README.md` — 项目门面 (M3 完成时改写)
