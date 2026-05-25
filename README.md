# OpenLoad

> 面向资源受限 MCU 的、**可裁剪、可移植**的开源 bootloader 框架。
> 接口抽象 + 编译期裁剪, 用户提供底层驱动即可接入任意单片机。

[![status](https://img.shields.io/badge/status-M4%20done-green)](docs/spec/REQUIREMENTS.md#7-第一版交付范围v1-scope)
[![license](https://img.shields.io/badge/license-MIT-blue)](LICENSE)

---

## 这是什么

OpenLoad **不是**某颗芯片的 bootloader, 而是一套 **接口规范 + 协议引擎 + 升级状态机**:

```
┌──────────────────────────────────────────────┐
│  用户工程 (你写)                              │
│  实现 ops 接口: uart / spi flash / sys        │
├──────────────────────────────────────────────┤
│  OpenLoad Core (本框架, 纯 C, 平台无关)       │
│  ├─ Boot State Machine                       │
│  ├─ Partition Manager + Flash 抽象           │
│  ├─ Protocol Engine (XMODEM / YMODEM / HTTP) │
│  ├─ Crypto (CRC32 / AES / SHA / Ed25519)     │
│  ├─ CLI + 命令注册                            │
│  └─ Logger                                   │
├──────────────────────────────────────────────┤
│  Porting API: ol_flash_ops_t / ol_io_ops_t   │
│              / ol_sys_ops_t                  │
└──────────────────────────────────────────────┘
```

每个功能都通过 `openload_config.h` 的 `#define` 开关裁剪, **未启用的代码不进 ROM**。

## 主要特性

- 🪶 **极轻量** — 默认配置 (XMODEM + CRC32 + CLI) 约 **14 KB** Flash
- 🧩 **接口驱动** — 5 个 ops 接口, 移植 ≈ 200 行代码
- 🛠 **协议矩阵** — XMODEM / XMODEM-1K / YMODEM / HTTP OTA / 自定义可挂载
- 🔐 **渐进安全** — CRC32 → AES-128-CTR → SHA-256 → Ed25519 签名
- 📦 **流式升级** — 接收 → 外部 staging → 校验 → 内部覆盖, 全程不缓存整个固件
- 🧪 **静态注册** — `OL_CMD_REGISTER` / `OL_FLASH_DEV_REGISTER` / `OL_IO_DEV_REGISTER` 用链接段自动发现, 无需手工 init list

## 当前进度

| 里程碑 | 状态 | 内容 |
|--------|------|------|
| **M1 核心 MVP** | ✅ | Porting API · 分区管理 · CLI · XMODEM/-1K · CRC32 · Staging 升级 · STM32F103 参考实现 |
| **M2 联网升级** | ✅ | YMODEM · HTTP OTA · ESP8266 port · 持久化日志 (oplog) |
| **M3 加固** | ✅ | 防回滚 · CLI 密码 + 失败锁定 · backup/rollback · AES-128-CTR |
| **M4 真实性/抗篡改** | ✅ | SHA-256 摘要 · Ed25519 签名验证 (M4-3/M4-4 需切 F4+, 见 [M4.md](docs/devlog/M4.md)) |
| M5+ 平台扩展 | 🔲 | STM32F4 port · A/B Dual Bank · HTTPS · USB DFU · MQTT OTA · RDP/OTP key 锁定 |

详见 [REQUIREMENTS.md](docs/spec/REQUIREMENTS.md)。开发过程见 [docs/devlog/](docs/devlog/)。

## 快速开始 (STM32F103ZET6 板子)

### 前提

- ARM GCC 工具链 (`arm-none-eabi-gcc` 在 PATH)
- CMake ≥ 3.22 + Ninja
- ST-Link / J-Link

### 编译

```bash
cd examples/stm32f103zet6_gcc

cmake --preset=Release
cmake --build build/Release

# 产物: build/Release/openload_bootloader.{elf,bin,map}
```

### 烧录

```bash
STM32_Programmer_CLI -c port=SWD \
    -d build/Release/openload_bootloader.bin 0x08000000 -v
```

### 测试

串口 115200 8N1, 上电应看到:

```
[I] OpenLoad 0.1.0-m1 starting
[I] press button or send any char in 3000 ms to enter CLI
OpenLoad> help
  help       List all commands
  info       Show bootloader and app info
  reset      Reboot the SoC
  jump       Verify and jump to app partition
  part       List partitions
  erase      Erase a partition
  update     Receive firmware and install (proto staging target)
  install    Install pre-staged firmware to target
```

### 制作可升级 App

App 工程链接到 `0x08010040` (boot 64K + image header 64B), 编译完用工具加 header:

```bash
python tools/image_tool.py myapp.bin \
    --board-id 0x0103 --version 1.2.0.0 \
    -o myapp-ol.bin
```

然后在 OpenLoad CLI 里:

```
OpenLoad> update xmodem download app
```

通过终端 (Tera Term / Xshell) 的 XMODEM 发送 `myapp-ol.bin`, 接收完毕框架自动校验 + 拷贝 + 跳转。

## 移植到新单片机

5 步, 约半天:

1. 实现 `ol_sys_ops_t` (tick / reboot / jump / disable_irq)
2. 注册 `ol_flash_dev_t` (至少内部 Flash)
3. 注册 `ol_io_dev_t` (至少 console UART)
4. 调整 `linker.ld` 的 BOOT 区大小与 `.ol_*` 链接段
5. 写 `partitions.def` 定义分区表

完整指南: [docs/guide/PORTING_GUIDE.md](docs/guide/PORTING_GUIDE.md)

## 目录结构

```
OpenLoad/
├── openload/                   # 框架核心 (平台无关)
│   ├── include/openload/       # 公开头文件
│   ├── core/                   # boot / cli / partition / image / updater / logger
│   ├── proto/                  # xmodem, ymodem, http_ota
│   ├── crypto/                 # crc32, aes, sha256, ed25519
│   ├── commands/               # 内置 CLI 命令
│   └── CMakeLists.txt
│
├── ports/                      # 芯片参考实现
│   └── stm32f1/                # STM32F103ZET6 + W25Q64
│
├── examples/                   # 工程模板
│   └── stm32f103zet6_gcc/      # CMake + GCC + ST HAL
│
├── tools/
│   └── image_tool.py           # 给 bin 加 OpenLoad image header
│
├── docs/
│   ├── spec/                   # 需求规格 + 架构设计
│   │   ├── REQUIREMENTS.md
│   │   └── ARCHITECTURE.md
│   ├── guide/                  # 使用 / 移植指南
│   │   └── PORTING_GUIDE.md
│   ├── devlog/                 # 开发日志 (按里程碑)
│   │   ├── M1.md
│   │   ├── M2.md
│   │   ├── M3.md
│   │   └── M4.md
│   └── images/
│
└── legacy/                     # 旧版本代码归档
```

## 体积参考

GCC 14.x, `-Os`, STM32F103ZET6:

| 配置 | bootloader.bin |
|------|----------------|
| 仅 XMODEM + CRC32 + CLI (M1 默认) | **~14 KB** |
| + YMODEM + HTTP OTA + oplog (M2) | ~37 KB |
| + 防回滚 + backup + AES-128 (M3) | ~41 KB |
| + SHA-256 + Ed25519 (M4) | ~48 KB |

## 设计文档

- 📋 [需求规格](docs/spec/REQUIREMENTS.md) — 设计哲学 / 功能清单 / 验收标准
- 🏗 [架构设计](docs/spec/ARCHITECTURE.md) — 接口签名 / 关键流程 / 配置项 / 旧代码迁移对照
- 🔌 [移植指南](docs/guide/PORTING_GUIDE.md) — 5 步把 OpenLoad 跑到一颗新单片机
- 📒 [开发日志](docs/devlog/) — M1 / M2 / M3 / M4 落地过程, 踩坑与设计取舍

## 状态

当前是 **0.1.0-m1**, M1 已完成核心可用 (M1 范围内的 XMODEM 升级路径已端到端编译验证)。

实板烧录与端到端联调由用户在自己的硬件上完成; 欢迎反馈 issue。

## License

MIT — 见 [LICENSE](LICENSE)。
