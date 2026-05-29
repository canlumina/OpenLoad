# OpenLoad menuconfig 使用指南

`menuconfig` 是 OpenLoad 的图形化配置工具（基于 Linux 内核 Kconfig 格式 + Python kconfiglib 库），用于代替手动编辑 `openload_config.h`。

---

## 快速开始

### 安装依赖

```powershell
# Windows
python -m pip install -r tools\requirements.txt

# Linux / WSL
pip install -r tools/requirements.txt
```

### 基本流程

```powershell
# 1. 打开 TUI
python tools\menuconfig.py examples\stm32f407vgt6_gcc\.config

# 2. 完成配置后 F5 保存、F9 退出

# 3. 生成 C 头文件 + CMake 配置
python tools\genconfig.py examples\stm32f407vgt6_gcc\.config --out-dir build\kconfig

# 4. 编译
cd examples\stm32f407vgt6_gcc
cmake -S . -B build\Debug -G Ninja `
    -DCMAKE_BUILD_TYPE=Debug `
    -DCMAKE_TOOLCHAIN_FILE=$pwd\cmake\gcc-arm-none-eabi.cmake
cmake --build build\Debug
```

### TUI 操作键

| 按键 | 作用 |
|---|---|
| ↑ ↓ | 导航菜单项 |
| Enter | 进入子菜单 / 编辑数值或字符串字段 |
| Space | 切换 bool 开关（`[*]` = 启用，`[ ]` = 禁用） |
| Y / N | choice 互斥项中选择一个 |
| `?` | 查看当前项的详细帮助说明 |
| `Z` | 查看所有可配置符号列表 |
| `/` | 搜索配置项 |
| F5 / `O` | 保存配置到 .config |
| F9 / `Q` | 退出（未保存会提示） |
| Esc Esc | 回退到上级菜单 |

> **注意**：必须从 repo **根目录**执行 `python tools/menuconfig.py`，不要 `cd tools/` 再跑，否则脚本名会遮挡 kconfiglib 的 menuconfig 模块导致导入错误。

---

## 配置项详解

以下按 TUI 菜单顺序，逐项说明含义和适用场景。

### Identity（设备标识）

| 配置项 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `OPENLOAD_BOARD_ID` | hex | `0x0001` | **板子 ID（16-bit）**。嵌入 image header，防止 F1 镜像误刷到 F4 板。用 `image_tool.py --board-id` 打镜像时需与之匹配。同一份镜像最多支持 3 个 ID（M6-2）。 |

### Boot（启动行为）

| 配置项 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `OPENLOAD_BOOT_DELAY_MS` | int | `3000` | **启动延时（毫秒）**。上电后等待多久才跳 App。在此窗口内检测到按键 / UART 活动 / RAM magic 值则进入 CLI。范围 0~60000ms。 |
| `OPENLOAD_ENTRY_TRIGGER_BUTTON` | bool | `y` | **按键进入 CLI**。上电时按住对应 GPIO 按键即可停在 bootloader CLI。 |
| `OPENLOAD_ENTRY_TRIGGER_UART` | bool | `y` | **UART 活动进入 CLI**。上电后串口发送任意字符可进 CLI。 |
| `OPENLOAD_ENTRY_TRIGGER_MAGIC` | bool | `y` | **RAM magic 进入 CLI**。App 请求 warm reboot 进 bootloader 时，在 RAM 特定地址写 magic 值，bootloader 检测到就不跳 App 而进 CLI。 |

### Recovery（App 校验失败时的恢复策略）

choice 互斥选项，四选一：

| 配置项 | 数值 | 说明 |
|---|---|---|
| `OPENLOAD_RECOVERY_POLICY_CLI` | 1 | **CLI 模式**（默认）。校验失败停在 bootloader CLI，等待用户决定下一步。 |
| `OPENLOAD_RECOVERY_POLICY_HANG` | 2 | **Hang 死等看门狗**。不做任何处理，依赖独立看门狗（IWDG）复位重试。 |
| `OPENLOAD_RECOVERY_POLICY_ROLLBACK` | 3 | **自动回滚**。从备份分区恢复上一个版本。需要 `ENABLE_BACKUP=y` + 升级策略为 Staging+BACKUP。 |

### Protocols（接收协议）

| 配置项 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `OPENLOAD_ENABLE_XMODEM` | bool | `y` | **XMODEM（128 字节/块）**。最基础的串口升级协议，兼容几乎所有终端软件（SecureCRT / TeraTerm / PuTTY）。M1 基线。 |
| `OPENLOAD_ENABLE_XMODEM_1K` | bool | `y` | **XMODEM-1K（1024 字节/块）**。比标准 XMODEM 快 8 倍，需要对方支持。依赖于 `ENABLE_XMODEM`。 |
| `OPENLOAD_ENABLE_YMODEM` | bool | `n` | **YMODEM（批处理协议）**。支持一次发送多个文件（先发 .bin 再发校验文件等），通常关闭以节省 ROM。 |
| `OPENLOAD_ENABLE_HTTP_OTA` | bool | `n` | **HTTP OTA**。通过 ESP8266 Wi-Fi 模块从 HTTP URL 拉取镜像更新。依赖于 `ENABLE_ESP8266`。 |

### Network（网络配置）

| 配置项 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `OPENLOAD_ENABLE_ESP8266` | bool | `n` | **ESP8266 AT 桥接（UART2）**。启用 UART2 + AT 指令引擎驱动 ESP8266 模块。`HTTP_OTA` 依赖它。开启后会自动同步 CMake 侧的 port cache var，确保 `port_uart2.c` 被编入。 |
| `OPENLOAD_ESP_UART_BAUD` | int | `115200` | **ESP8266 UART2 波特率**。新固件（AT v1.x+）默认 115200；老 ESP-01 模块（AT v0.x）默认 9600。根据手上模块改。依赖于 `ENABLE_ESP8266`。 |

### Crypto（密码学配置）

| 配置项 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `OPENLOAD_ENABLE_CRC32` | bool | `y` | **CRC32 镜像校验**。M1 基线特性，始终启用。验证 image header + payload 的完整性。 |
| `OPENLOAD_ENABLE_AES_128_CTR` | bool | `n` | **AES-128-CTR 镜像解密**。用 `image_tool.py --aes-key` 加密镜像后，bootloader 需使用相同的 16 字节密钥解密。密钥由 `AES_KEY` 字段指定。 |
| `OPENLOAD_AES_KEY` | string | `"4F70656E..."` | **AES-128 密钥（32 hex chars = 16 字节）**。小写/大写 hex 字符串，不含空格和 `0x` 前缀。demo key 对应 ASCII 字符串 `"OpenLoad demoKey"`。**生产环境必须换成随机密钥。** 依赖于 `ENABLE_AES_128_CTR`。 |
| `OPENLOAD_ENABLE_SHA256` | bool | `n` | **SHA-256 镜像摘要校验**。当 image header 中 `firmware_sha256` 非零时，对明文 payload 做 SHA-256 校验。Ed25519 签名路径也依赖它（M4-2）。 |
| `OPENLOAD_ENABLE_ED25519` | bool | `n` | **Ed25519 签名校验**。当 image header 的 `flags` 带 `OL_IMG_F_SIGNED` 位时，验证 image trailer（末尾 64 字节）中的 Ed25519 签名。**这是 OpenLoad 安全栈的核心防线。** 依赖于 `ENABLE_SHA256`。 |
| `OPENLOAD_ED25519_PUBKEY` | string | `"7542CE6A..."` | **Ed25519 公钥（64 hex chars = 32 字节）**。demo 公钥对应 seed `"OpenLoad-demo-ed25519-seed-v1!!!"`。**生产环境必须换成真实 Ed25519 密钥对。** 依赖于 `ENABLE_ED25519`。 |

> **安全提示**：demo 密钥公开在 GitHub 上，仅用于开发调试。量产前必须运行 `tools/gen_demo_ed25519.py` 生成新密钥对，并把私钥妥善保管。

### Security（安全功能）

| 配置项 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `OPENLOAD_ENABLE_RDP` | bool | `n` | **RDP CLI 命令（STM32 读保护）**。启用 `rdp status` / `rdp lock` 命令。L0→L1 锁住 SWD 读 flash 路径，保护 bootloader 内的公钥 / AES 密钥不被 dump（M6-1）。F1 和 F4 均支持。**L2（永久锁定）不暴露在 CLI 中——必须用 ST-LINK 外部工具烧。** |
| `OPENLOAD_ANTI_ROLLBACK` | bool | `n` | **防回滚**。拒绝安装 `firmware_version` 低于当前运行 App 版本的镜像。`install ... force` 可绕过（用于紧急降级）。依赖于 `ENABLE_ED25519`。 |
| `OPENLOAD_ENABLE_BACKUP` | bool | `n` | **备份分区 + 自动回滚**。升级前备份当前分区内容，升级失败或 App 校验失败时自动从备份恢复。需要 `partitions.def` 中定义 `backup` 分区。 |

### Image / Update（升级策略）

**Upgrade strategy** — choice 互斥，四选一：

| 配置项 | 数值 | 说明 |
|---|---|---|
| `OPENLOAD_UPGRADE_STRATEGY_SINGLE_BANK` | 1 | **直接覆盖写 App 分区**。无 staging 阶段，最简单但无容错。 |
| `OPENLOAD_UPGRADE_STRATEGY_STAGING` | 2 | **外部 staging → 写入 App 分区**（默认）。先接收镜像到 staging 分区（通常是外部 SPI flash），校验通过后再拷贝到 App 分区。 |
| `OPENLOAD_UPGRADE_STRATEGY_STAGING_BACKUP` | 3 | **Staging + 备份**。先备份 App 到 backup 分区 → staging 接收 → 校验通过后拷贝到 App。校验失败自动从 backup 恢复。需要 `ENABLE_BACKUP=y`。 |
| `OPENLOAD_UPGRADE_STRATEGY_DUAL_BANK` | 4 | **A/B 双 Bank 切换**。F4 原生支持 Dual Bank flash。未实现，预留 M7+。 |

| 配置项 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `OPENLOAD_COPY_CHUNK_SIZE` | int | `512` | **staging → App 拷贝块大小（字节）**。RAM 缓冲区大小，用于从 staging 分区拷贝到目标分区。越大越快但占 RAM 越多。范围 64~4096。 |

### Logging（日志）

| 配置项 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `OPENLOAD_ENABLE_OPLOG` | bool | `n` | **持久化操作日志**。在外部 flash 维护一个只能追加的环形日志，记录最近的升级/校验操作。需要 `partitions.def` 中定义 `oplog` 分区（扇区对齐）。关掉省 ~2KB ROM。 |
| `OPENLOAD_LOG_LEVEL` | int | `3` | **日志级别**。0=NONE（关日志），1=ERR（仅错误），2=WRN（错误+警告），3=INF（默认，信息级），4=DBG（调试，最详细）。 |
| `OPENLOAD_LOG_COLOR` | bool | `y` | **ANSI 彩色日志**。串口终端输出带颜色的日志（对 SecureCRT / PuTTY 有效）。关掉省少许 ROM。 |

### CLI（命令行界面）

| 配置项 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `OPENLOAD_ENABLE_CLI` | bool | `y` | **启用交互式 CLI**。关闭后 bootloader 无命令行界面，仅通过 GPIO/协议自动执行升级。 |
| `OPENLOAD_CLI_LINE_MAX` | int | `128` | **CLI 行缓冲区（字节）**。单行输入的最大长度。长命令路径或参数需调大。范围 32~1024。依赖于 `ENABLE_CLI`。 |
| `OPENLOAD_CLI_PROMPT` | string | `"OpenLoad> "` | **CLI 提示符**。可自定义，例如改为板子名称 `"F407> "`。依赖于 `ENABLE_CLI`。 |
| `OPENLOAD_CLI_PASSWORD` | string | `""`（空） | **CLI 密码**。非空字符串开启密码门控，进入 CLI 前需输密码。空=无密码。**demo 阶段建议空密码**，生产环境用哈希密码。依赖于 `ENABLE_CLI`。 |
| `OPENLOAD_CLI_PASSWORD_MAX_ATTEMPTS` | int | `3` | **密码最大尝试次数**。超限后锁定。范围 1~100。依赖于 `ENABLE_CLI`。 |
| `OPENLOAD_CLI_PASSWORD_LOCKOUT_MS` | int | `30000` | **锁定持续时间（毫秒）**。密码尝试耗尽后锁多久。范围 1000~600000（1 秒~10 分钟）。依赖于 `ENABLE_CLI`。 |

---

## 按场景推荐配置

### 🟢 最小化（~21KB, M1 only）
仅基础升级功能，适合资源受限或替换方案：
```
XMODEM=y, XMODEM_1K=y
CRC32=y
CLI=y
其余全部 = n
```

### 🟡 开发调试（~46KB, 全功能）
```powershell
# 开所有协议 + 日志 + 网络
XMODEM=y, XMODEM_1K=y, YMODEM=y
CRC32=y, AES_128_CTR=y, SHA256=y, ED25519=y
OPLOG=y, LOG_LEVEL=4, LOG_COLOR=y
CLI=y
ESP8266=y, HTTP_OTA=y（如硬件支持）
```

### 🔴 量产安全（~46KB, demo key 替换后）
```
ED25519=y（关闭 AES 和 YMODEM 等不需要的特性）
RDP=y → 量产最后一步 rdp lock
ANTI_ROLLBACK=y
BACKUP=y + 升级策略 = STAGING_BACKUP
```

---

## 常见操作

### 查看当前配置

```powershell
# 直接用 genconfig 生成可读的 C header
python tools\genconfig.py examples\stm32f407vgt6_gcc\.config --out-dir build\kconfig
type build\kconfig\openload_autoconfig.h
```

### 配置 F1 板

```powershell
python tools\menuconfig.py examples\stm32f103zet6_gcc\.config
```

### 配置 CI minimal

CI 用独立配置，不改主 `.config`：

```powershell
python tools\menuconfig.py examples\stm32f407vgt6_gcc\ci_minimal\.config
```

改完后 commit `ci_minimal/.config`（该文件进 git）。

### 改完配置后忘记生成怎么办？

CMake 已在 `examples/*/CMakeLists.txt` 中集成了 genconfig 调用。
直接 `cmake --build` 会自动生成，无需手动跑 genconfig。

---

## 文件结构

```
Kconfig                          ← 描述全部选项（repo 根，进 git）
tools/
  menuconfig.py                  ← ncurses TUI wrapper
  genconfig.py                   ← .config → C header + CMake
  migrate_config.py              ← 旧 openload_config.h → .config（一次性）
examples/<board>/
  .config                        ← 用户配置（不进 git，类似 Linux 内核模式）
  ci_minimal/.config             ← CI 用最小配置（进 git）
build/<cfg>/kconfig/
  openload_autoconfig.h          ← 生成的 C #define（不进 git）
  openload_kconfig.cmake         ← 生成的 CMake set() 块（不进 git）
```

---

## 从旧版 openload_config.h 迁移

如果是从 M5 或更早版本升级，可用 migrate 工具把旧配置转到 .config：

```powershell
python tools\migrate_config.py old_openload_config.h -o examples\stm32f407vgt6_gcc\.config
```

迁移后建议 `diff` 核对 autoconfig.h 跟旧 header 的语义一致性。参见 `docs/devlog/M6.md` §6.5。
