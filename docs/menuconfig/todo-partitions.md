# menuconfig TODO: Flash 设备与分区布局配置

> ⏳ **待实现** — 记录一个可行的扩展方向，下次迭代时参考。

---

## 动机

当前分区信息分布在三处，改布局需同步维护：

| 位置 | 内容 |
|---|---|
| `examples/<board>/partitions.def` | 分区表（X-macro：设备、偏移、大小、flags） |
| `ports/stm32f4/src/port_spi_flash.c` | 外部 Flash 设备注册（base、size、sector_size） |
| `examples/<board>/STM32F407VGTx_FLASH.ld` | 链接脚本（boot 区偏移，暂不动） |

改 staging 大小或 app 偏移需要同时改 partitions.def 和 C 代码，缺少单一数据源，
也没有越界/未对齐校验，手误难以发现。

---

## 目标

在 Kconfig 中新增 `"Memory / Partitions"` 菜单，使用户能通过 menuconfig
配置 Flash 设备和分区布局，genconfig.py 自动生成 `autoconfig_partitions.h`。

---

## 分层设计

### Layer 1: Flash 设备参数

```
menu "Memory / Partitions"

config OPENLOAD_FLASH_INTERNAL_BASE
    hex "Internal Flash base address"
    default 0x08000000

config OPENLOAD_FLASH_INTERNAL_SIZE
    hex "Internal Flash total size"
    default 0x100000          # 1 MB (F407)

config OPENLOAD_FLASH_INTERNAL_SECTOR_SIZE
    int "Internal Flash min erase unit (bytes)"
    range 1024 262144
    default 16384             # F4 多 sector 混合, 取最小公倍数?

config OPENLOAD_ENABLE_EXTERNAL_FLASH
    bool "Enable external SPI flash"
    default y

config OPENLOAD_FLASH_EXTERNAL_MODEL
    string "External flash model"
    default "w25q16"

config OPENLOAD_FLASH_EXTERNAL_SIZE
    hex "External flash total size"
    default 0x200000          # 2 MB (w25q16)

config OPENLOAD_FLASH_EXTERNAL_SECTOR_SIZE
    int "External flash sector size (bytes)"
    default 4096              # w25qxx 统一 4KB

### 疑问: 外部 Flash 基址怎么设? RAM 映射还是 SPI 抽象, 填 0?
```

### Layer 2: 分区布局

每个分区 5 个字段，用 `depends on` 控制可见性：

```kconfig
# ── Boot 分区 ──
config OPENLOAD_PART_BOOT_DEVICE
    string "Boot: device name"
    default "internal"

config OPENLOAD_PART_BOOT_OFFSET
    hex "Boot: offset from flash base"
    default 0x00000000

config OPENLOAD_PART_BOOT_SIZE
    hex "Boot: size"
    default 0x00010000          # 64 KB


# ── App 分区 ──
config OPENLOAD_PART_APP_DEVICE
    string "App: device name"
    default "internal"

config OPENLOAD_PART_APP_OFFSET
    hex "App: offset"
    default 0x00020000

config OPENLOAD_PART_APP_SIZE
    hex "App: size"
    default 0x00080000          # 512 KB


# ── Staging / Download ──
config OPENLOAD_PART_STAGING_DEVICE
    string "Staging: device name"
    depends on OPENLOAD_ENABLE_EXTERNAL_FLASH
    default "w25q16"

config OPENLOAD_PART_STAGING_OFFSET
    hex "Staging: offset"
    depends on OPENLOAD_ENABLE_EXTERNAL_FLASH
    default 0x00000000

config OPENLOAD_PART_STAGING_SIZE
    hex "Staging: size"
    depends on OPENLOAD_ENABLE_EXTERNAL_FLASH
    default 0x00080000


# ── Backup ──
config OPENLOAD_PART_BACKUP_DEVICE
    string "Backup: device name"
    depends on OPENLOAD_ENABLE_BACKUP
    default "w25q16"

config OPENLOAD_PART_BACKUP_OFFSET
    hex "Backup: offset"
    depends on OPENLOAD_ENABLE_BACKUP
    default 0x00080000

config OPENLOAD_PART_BACKUP_SIZE
    hex "Backup: size"
    depends on OPENLOAD_ENABLE_BACKUP
    default 0x00080000


# ── WiFi Config (可选) ──
config OPENLOAD_PART_WIFI_CFG_DEVICE
    string "WiFi config: device name"
    depends on OPENLOAD_ENABLE_ESP8266
    default "w25q16"

config OPENLOAD_PART_WIFI_CFG_OFFSET
    hex "WiFi config: offset"
    depends on OPENLOAD_ENABLE_ESP8266
    default 0x00100000

config OPENLOAD_PART_WIFI_CFG_SIZE
    hex "WiFi config: size"
    depends on OPENLOAD_ENABLE_ESP8266
    default 0x00001000


# ── Oplog ──
config OPENLOAD_PART_OPLOG_DEVICE
    string "Oplog: device name"
    depends on OPENLOAD_ENABLE_OPLOG
    default "w25q16"

config OPENLOAD_PART_OPLOG_OFFSET
    hex "Oplog: offset"
    depends on OPENLOAD_ENABLE_OPLOG
    default 0x00101000

config OPENLOAD_PART_OPLOG_SIZE
    hex "Oplog: size"
    depends on OPENLOAD_ENABLE_OPLOG
    default 0x00004000

endmenu
```

---

## genconfig.py 改动

新增 `emit_partitions(kconf, out_path)` 函数，生成 `openload_partitions.h`：

```c
/* AUTO-GENERATED — 分区宏，替代 partitions.def 的 X-macro */
#pragma once

/* Flash 设备参数 */
#define OPENLOAD_FLASH_INTERNAL_BASE    0x08000000u
#define OPENLOAD_FLASH_INTERNAL_SIZE    0x00100000u

/* 分区定义 */
#define OL_PART_BOOT_DEVICE     "internal"
#define OL_PART_BOOT_OFFSET     0x00000000u
#define OL_PART_BOOT_SIZE       0x00010000u
#define OL_PART_APP_DEVICE      "internal"
#define OL_PART_APP_OFFSET      0x00020000u
#define OL_PART_APP_SIZE        0x00080000u
// ...
```

然后在 `emit_partitions()` 中做**编译期校验**：

- ✅ 分区 offset + size ≤ flash 设备总大小
- ✅ 分区 offset 和 size 按 sector_size 对齐
- ✅ 分区之间不重叠
- ❌ 链接脚本无法自动校验（需提示用户手动同步 `.ld`）

---

## 端口 C 代码改动

`ports/stm32f4/src/port_spi_flash.c` 等文件中的设备注册改为读宏：

```c
/* 改前: 硬编码 */
const ol_flash_dev_t g_w25q16_dev = {
    .name  = "w25q16",
    .size  = 2 * 1024 * 1024,   // 2 MB
    ...
};

/* 改后: 从自动配置头文件读 */
#include "openload_partitions.h"

const ol_flash_dev_t g_w25q16_dev = {
    .name  = "w25q16",
    .size  = OPENLOAD_FLASH_EXTERNAL_SIZE,
    ...
};
```

---

## 不做的事

1. **不自动生成链接脚本** — boot 分区起始地址跟 `.ld` 的 `VECT_TAB` / `FLASH (rx) : ORIGIN = ..., LENGTH = ...` 耦合，改错了直接启动不了，必须手动同步
2. **不替换 flash_ops 驱动层** — SPI 时序、W25Q 指令序列不变，只改设备参数

---

## 实现顺序建议

1. 先在 Kconfig 加 `Memory / Partitions` 菜单和一两个分区（boot + app）
2. genconfig.py 新增 `emit_partitions()`，只输出设备参数，暂不替代 partitions.def
3. 端口 C 代码改 1 个 flash_dev 注册（如 internal）验证 round-trip
4. 加 remaining 分区（staging / backup / oplog / wifi_cfg）
5. 废弃 `partitions.def` 的 X-macro，全切到自动生成头文件

---

## 预期收益

- ✅ 分区布局 → 单一数据源（Kconfig `.config`）
- ✅ 改分区大小 → 只改 menuconfig 一个地方
- ✅ 越界/重叠/未对齐 → genconfig 编译期拦截
- ✅ 多板共用配置逻辑（F1 512KB 内部 flash vs F4 1MB）→ 只需改默认值