# OpenLoad 需求规格说明书

> **版本**: v0.1 (重构方案初稿)
> **日期**: 2026-05-22
> **状态**: 待评审

---

## 1. 项目定位

OpenLoad 是一个**面向资源受限 MCU 的、可裁剪、可移植的开源 bootloader 框架**。

它**不是**某一颗芯片的 bootloader 实现，而是一套**接口规范 + 协议引擎 + 升级状态机**，由用户提供底层驱动接入。

### 对标项目

| 项目 | 定位 | 借鉴点 |
|------|------|--------|
| MCUboot | 工业级安全 bootloader | 分区抽象 (`flash_area`)、签名机制、swap 算法 |
| Zephyr Bootloader | RTOS 内置 bootloader | Kconfig 配置体系 |
| RT-Thread BootLoader | 国产 RTOS 配套 | menuconfig 体验、AT 协议接入 |
| FreeRTOS | 可裁剪 RTOS | 头文件配置 (`FreeRTOSConfig.h`) |

OpenLoad 的差异化：**更轻量**（核心 < 20KB）、**更易上手**（中文文档+模板工程）、**更友好的 OTA 协议矩阵**（XMODEM/YMODEM/HTTP/MQTT/DFU 全栈可选）。

---

## 2. 设计哲学

**P1. 零强制依赖**
框架本身只依赖 C99 标准库（去掉 `malloc`/`stdio`），不强依赖任何 HAL、RTOS、网络栈、加密库。

**P2. 接口先行**
所有平台相关操作通过 `*_ops_t` 结构体接口暴露，用户实现接口即可接入。

**P3. 编译期裁剪**
未启用的功能模块**完全不参与编译**，通过 `openload_config.h` 控制。bootloader 体积只为实际启用的功能付费。

**P4. 单一职责**
一个模块只做一件事。固件加密 = 一个模块；固件签名 = 另一个模块；XMODEM 协议 = 另一个模块。模块间通过接口通信，不互相 include 实现头。

**P5. 渐进式安全**
最小化也能跑（仅 CRC32）；需要时可叠加 AES → SHA-256 → ECDSA 签名 → 防回滚 → 安全启动。

**P6. 故障可观测**
所有错误必须有错误码 + 日志条目，关键状态可持久化到外部 Flash 供事后排查。

---

## 3. 范围

### 3.1 在范围内

- ✅ 提供 bootloader 核心状态机
- ✅ 定义 Porting API 标准接口
- ✅ 提供主流升级协议实现（XMODEM/YMODEM/HTTP OTA）
- ✅ 提供分区管理与多存储介质抽象
- ✅ 提供固件元数据格式与校验/解密/签名验证
- ✅ 提供命令行交互框架
- ✅ 提供配置系统（头文件 `#define` 驱动）
- ✅ 提供 STM32F103ZET6 + W25Q64 + ESP8266 的**参考实现**作为示例
- ✅ 提供 CMake + Keil MDK 双构建工程模板

### 3.2 不在范围内（v1）

- ❌ App 端的运行时 OTA 库（OpenLoad 只管 bootloader 侧；App 侧只需写一个 trigger 标志位）
- ❌ 主流芯片厂的官方 HAL 适配（由参考实现引导用户自行接入）
- ❌ 图形化 PC 烧录工具（v1 用现有的 Tera Term/Xshell 配合 XMODEM；v2 再考虑）
- ❌ 文件系统挂载（FatFs/LittleFS 等由用户在外部叠加，OpenLoad 不集成）
- ❌ 安全启动 / RDP 等芯片级安全机制（属于芯片配置，文档指引而不代码实现）

---

## 4. 功能需求

### F1. 启动决策

| ID | 需求 | 说明 |
|----|------|------|
| F1.1 | 上电后等待用户触发 | 等待时间 `OPENLOAD_BOOT_DELAY_MS` 可配，默认 3000ms |
| F1.2 | 多种触发源 | 按键 / UART 字符 / 强制标志位 / App 软触发 任一可进入 |
| F1.3 | 触发源可配置 | 通过 `OPENLOAD_ENTRY_TRIGGER_*` 宏选择启用 |
| F1.4 | App 完整性校验 | 跳转前校验 App 区固件头与 CRC（启用 SHA/签名时一并验证）|
| F1.5 | 校验失败处理策略 | 进入 CLI / 自动回滚 / 死等 三种模式可配 |
| F1.6 | 跳转前清理 | 关闭已启用外设、复位向量表、关中断、设栈指针 |

### F2. 升级通道（Receiver 子系统）

每个 Receiver 是一个独立模块，用户在配置文件中按需启用。

| ID | 协议 | v1 状态 | 备注 |
|----|------|---------|------|
| F2.1 | XMODEM (128 / CRC) | ✅ 必做 | 兼容性最好的兜底通道 |
| F2.2 | XMODEM-1K | ✅ 必做 | 1024 字节包，提速 |
| F2.3 | YMODEM | ✅ 必做 | 带文件名/大小信息，单文件 |
| F2.4 | YMODEM-G | ⏸ 可选 | 无握手提速，对线质量敏感 |
| F2.5 | HTTP(S) OTA Client | ✅ 必做 | 通过 IO 抽象，可接 ESP/WiFi/以太网 |
| F2.6 | MQTT OTA | ⏸ v2 | 留接口，v1 不实现 |
| F2.7 | USB DFU | ⏸ v2 | 留接口，v1 不实现 |
| F2.8 | CAN ISP | ⏸ v3 | 留接口 |
| F2.9 | 自定义协议挂载 | ✅ 必做 | 用户可实现 `ol_receiver_t` 注册自己的协议 |

所有 Receiver 通过统一的 `ol_io_t` 读写数据，**不关心**底层是 UART / USB CDC / TCP socket。

### F3. 存储管理

| ID | 需求 | 说明 |
|----|------|------|
| F3.1 | Flash 设备抽象 | `ol_flash_dev_t` 提供 read/write/erase 接口 |
| F3.2 | 多设备共存 | 同时管理内部 Flash + 多块外部 Flash（SPI/QSPI/SD）|
| F3.3 | 分区表静态声明 | 用宏 `OL_PARTITION_TABLE()` 在配置文件中声明 |
| F3.4 | 分区操作 API | `ol_part_read/write/erase/verify` |
| F3.5 | 跨页跨扇区自动处理 | 写入跨越扇区边界时自动擦除目标扇区 |
| F3.6 | 写入对齐处理 | 用户提供 `write_granularity`，框架内部做缓冲合并 |
| F3.7 | 分区访问权限 | `R/W/X` 标志，App 区只可读写不可执行（由 CLI 校验）|

### F4. 固件元数据

| ID | 需求 | 说明 |
|----|------|------|
| F4.1 | 标准固件头 | 64 字节固定结构，含 magic/version/size/checksum |
| F4.2 | 头部位置 | 固件文件最前 64 字节（与裸 bin 拼接） |
| F4.3 | 必填字段 | magic / format_version / firmware_size / crc32 / build_timestamp |
| F4.4 | 可选字段 | sha256 / signature / aes_iv / firmware_version / board_id |
| F4.5 | 版本号语义 | semver `major.minor.patch.build` 各 8bit, 共 uint32_t |
| F4.6 | 防回滚 | 当启用版本检查时，新固件版本必须 ≥ 当前版本（>= 还是 > 可配）|
| F4.7 | 头部 CRC | 头部自身末 4 字节为头部 CRC32，防止头部损坏误判 |

### F5. 安全

| ID | 需求 | v1 默认 | 说明 |
|----|------|---------|------|
| F5.1 | CRC32 完整性校验 | ✅ 强制启用 | 框架内置查表实现 (~1KB ROM) |
| F5.2 | AES-128-CTR 解密 | ⏸ 默认关闭 | 启用时链接 tiny-AES (~2KB) |
| F5.3 | AES-256-GCM | ⏸ 可选 | 兼具加密 + 认证 |
| F5.4 | SHA-256 摘要 | ⏸ 默认关闭 | 启用时链接精简 sha256 (~1KB) |
| F5.5 | Ed25519 签名验证 | ⏸ v2 | 引入 micro-ecc 或自实现 (~7KB) |
| F5.6 | ECDSA P-256 签名 | ⏸ v2 | 同上 |
| F5.7 | 防回滚检查 | ⏸ 默认关闭 | 与 F4.6 配套 |
| F5.8 | CLI 密码保护 | ⏸ 默认关闭 | 防止物理接入修改固件 |

### F6. 升级策略

通过 `OPENLOAD_UPGRADE_STRATEGY` 配置项三选一：

| ID | 策略 | 适用场景 |
|----|------|----------|
| F6.1 | `SINGLE_BANK_OVERWRITE` | 接收后直接擦写 App 区，断电窗口=擦写时长 |
| F6.2 | `STAGING_OVERWRITE`（v1 默认） | 接收到外部 Staging 区，验证通过再拷贝到 App 区 |
| F6.3 | `STAGING_WITH_BACKUP` | 拷贝前先备份现 App，失败可回滚 |
| F6.4 | `DUAL_BANK_SWAP` | A/B 双 slot 切换；需向量表重定位，App 体积减半 (v2 实现) |

### F7. 命令行 (CLI)

| ID | 需求 | 说明 |
|----|------|------|
| F7.1 | 简易行编辑 | 退格 / 回车，无需历史 |
| F7.2 | 命令注册机制 | 用宏 `OL_CMD_REGISTER(name, handler)` 静态注册，链接段聚合 |
| F7.3 | 帮助系统 | `help` 或 `?` 列出所有命令 |
| F7.4 | 内置命令 | `help / info / version / reset / jump / part / update / erase` |
| F7.5 | 子命令风格 | `update xmodem internal` / `update http external download` |
| F7.6 | 可选密码保护 | 启用 `OPENLOAD_CLI_PASSWORD` 时进入需口令 |

### F8. 日志

| ID | 需求 | 说明 |
|----|------|------|
| F8.1 | 多级别 | NONE / ERROR / WARN / INFO / DEBUG，编译期裁剪 |
| F8.2 | 输出接口可替换 | 默认走 `ol_io_t`，可重定向到 RTT/SWO |
| F8.3 | 持久化日志（可选） | 启用时写入分区 `oplog`，循环覆盖 |
| F8.4 | 颜色支持 | 启用 `OPENLOAD_ENABLE_LOG_COLOR` 时输出 ANSI 颜色码 |

### F9. 配置系统

| ID | 需求 | 说明 |
|----|------|------|
| F9.1 | 配置文件 | 用户在工程内提供 `openload_config.h`，覆盖 `openload_config_default.h` |
| F9.2 | 配置项规范 | 所有配置项以 `OPENLOAD_` 前缀，编译期可检测冲突 |
| F9.3 | 默认值合理 | 不配置任何项也能跑（仅启用 XMODEM + CRC32 + UART） |
| F9.4 | 配置预检 | 通过 `static_assert` 检查必要项与互斥项 |
| F9.5 | 未来 Kconfig 支持 | v1 文件结构对 Kconfig 友好，v2 可加 menuconfig.py |

### F10. Porting API（用户必须实现的接口）

| 接口 | 是否必须 | 说明 |
|------|----------|------|
| `ol_sys_get_tick_ms()` | ✅ 必须 | 返回毫秒级 tick |
| `ol_sys_reboot()` | ✅ 必须 | 系统复位 |
| `ol_sys_jump(addr)` | ⏸ 可选 | 跳转 App；ARM Cortex-M 我们提供默认实现 |
| `ol_flash_dev_t (internal)` | ✅ 必须 | 至少注册一个内部 Flash 设备 |
| `ol_io_t (console)` | ✅ 必须 | 至少注册一个 CLI/日志 IO（通常 UART） |
| `ol_flash_dev_t (external)` | ⏸ 启用外部存储时 | SPI Flash / SD 等 |
| `ol_io_t (network)` | ⏸ 启用 HTTP OTA 时 | TCP socket / ESP AT 桥接 |

---

## 5. 非功能需求

### NF1. 资源预算（编译目标 ARM Cortex-M3 -Os）

| 配置 | ROM 目标 | RAM 目标 |
|------|----------|----------|
| 最小（仅 XMODEM + CRC32 + CLI） | ≤ 18 KB | ≤ 3 KB |
| 默认（+ YMODEM + AES-128） | ≤ 24 KB | ≤ 4 KB |
| 全功能（+ HTTP OTA + SHA-256 + 签名） | ≤ 40 KB | ≤ 6 KB |

（不含 HAL/用户驱动；BSP 部分单独预算）

### NF2. 可移植性

- 核心代码 100% C99，无 GNU 扩展、无内联汇编（跳转 App 的少量汇编封装在 ports 层）
- 提供至少 1 个完整参考实现（STM32F103ZET6）
- Porting 工作量 ≤ 200 行 C 代码（不含芯片厂 HAL 本身）

### NF3. 可裁剪性

- 任意单一模块禁用不影响其他模块编译
- 提供完整功能矩阵的 CI 编译验证（至少 5 种配置组合）

### NF4. 可测试性

- 核心模块 可在 PC 端 host 编译并跑单元测试
- 协议层与传输层解耦，可用 socketpair / pipe 模拟 UART

### NF5. 可维护性

- 头文件层次：核心头只允许 include 标准库与同层接口头，禁止 include port 层与 driver 层
- 命名规范统一：模块前缀 `ol_<module>_`，宏 `OL_*`，配置宏 `OPENLOAD_*`
- 每个公开接口 1 行 doxygen 注释（参数、返回值、错误码）

### NF6. 性能

| 操作 | 目标 |
|------|------|
| 启动延时（不含 HAL_Init） | ≤ 10 ms |
| XMODEM-1K @ 115200 实测 | ≥ 10 KB/s |
| HTTP OTA @ ESP8266 (WiFi 良好) | ≥ 30 KB/s |
| 外部 → 内部 Flash 拷贝（含验证） | ≥ 50 KB/s |
| 启动 App 校验（448KB CRC32） | ≤ 30 ms |

---

## 6. 默认参考实现（Reference Port）

随框架交付的 `ports/stm32f1/` 包含：

- STM32F103ZET6 时钟/GPIO/中断初始化
- UART1 DMA + 空闲中断驱动（实现 `ol_io_t` 控制台）
- UART2 DMA 驱动（用于 ESP8266）
- SPI2 + W25Q64 驱动（实现 `ol_flash_dev_t` 外部 Flash）
- 按键检测（PA0）
- ESP8266 AT 桥接（实现 HTTP OTA 用的 `ol_io_t` 网络通道）
- ARM Cortex-M3 跳转汇编（向量表重定位 + MSP 切换）

构建工程：CMake (GCC) + Keil MDK 各一份。

---

## 7. 第一版交付范围（v1 Scope）

按优先级排序，**M1 必做**、M2 推荐、M3 可推迟：

### M1 — 核心可用 (MVP)

1. Porting API 完整定义（flash/io/sys）
2. 配置系统（头文件 + 默认值）
3. 分区管理 + 内部 Flash + W25Q64 驱动
4. CLI 框架（行编辑 + 命令注册）
5. XMODEM / XMODEM-1K Receiver
6. 固件头格式 + CRC32 校验
7. Staging 升级策略（外部 → 内部）
8. STM32F103ZET6 参考实现
9. 文档：README + 入门指南 + Porting 指南

### M2 — 联网升级

10. YMODEM Receiver
11. HTTP OTA Receiver
12. ESP8266 AT 桥接参考实现
13. 持久化日志

### M3 — 加固

14. AES-128-CTR 解密
15. SHA-256 摘要
16. 防回滚检查
17. CLI 密码保护
18. STAGING_WITH_BACKUP 策略
19. CI 编译矩阵

### M4+ — 后续（不在 v1）

- Ed25519 签名 / ECDSA
- USB DFU
- MQTT OTA
- A/B Dual Bank
- menuconfig.py（Kconfig 接入）
- 其它芯片参考实现（STM32F4 / GD32 / N32）

---

## 8. 验收标准

- **AC1**：在 STM32F103ZET6 + W25Q64 板上完成一次"按键进入 → XMODEM 上传 → 校验 → 跳转 App"全流程
- **AC2**：在同一板上完成一次"配置 WiFi → HTTP OTA 下载 → 校验 → 跳转 App"全流程
- **AC3**：将框架移植到一颗未参考过的 STM32F4 板上（用户场景），移植工作 ≤ 1 天 / 200 行代码
- **AC4**：最小配置编译出的 bootloader.bin ≤ 18 KB
- **AC5**：完整功能配置编译出的 bootloader.bin ≤ 40 KB
- **AC6**：从已有 v0 项目（当前混乱版本）平滑迁移文档存在，老用户可参考

---

## 9. 待确认的开放问题

> 这些问题在 v1 开始动手前需要拍板。

| ID | 问题 | 候选方案 | 我的倾向 |
|----|------|----------|----------|
| Q1 | 老代码（example/gcc）保留还是清空? | A. 整个清空重写 / B. 改名 `example/legacy` 保留作参考 | B —— 至少保留到 M2 完成 |
| Q2 | Keil 工程是否同步维护? | A. 仅 CMake 主，Keil 用户自己生成 / B. 两套并维护 | B —— 但 Keil 工程在 M1 收尾时一次性补 |
| Q3 | mbedtls 处置 | A. 立即删除 / B. 留到 M3 加密落地后再删 | A —— 不删它会被无意 include |
| Q4 | 固件签名工具 | A. 自己写 Python 脚本 / B. 复用 imgtool (MCUboot) | A —— 自己写更简单，30 行 Python |
| Q5 | 配置文件位置 | A. 工程根目录 / B. 用户工程 include 路径任意位置 | B —— 不强制路径 |
| Q6 | 错误码命名空间 | A. 全局 `OL_E_*` / B. 模块前缀 `OL_E_PART_*`/`OL_E_XMODEM_*` | B —— 易于定位错误来源 |
| Q7 | bootloader 与 App 通信机制 | A. RAM 共享段 / B. RTC 备份寄存器 / C. 内部 Flash 特定页 | A + C 组合（A 易失，C 持久）|

---

## 10. 词汇表

| 术语 | 释义 |
|------|------|
| **Port** | 平台适配层。每个芯片/板子有一份。 |
| **BSP** | Board Support Package。Port 的同义词。 |
| **Receiver** | 升级数据接收器（XMODEM/HTTP 等协议引擎） |
| **Staging Area** | 升级暂存区，固件先下载到这里再做完整验证 |
| **App** / **Application** | 主应用程序，由 bootloader 跳转执行 |
| **Slot** | 一个固件可执行/存放的位置（Primary Slot, Secondary Slot 等） |
| **IAP** | In-Application Programming，串口本地升级 |
| **OTA** | Over-The-Air，无线/远程升级 |
| **ops** | operations 结构体，定义一组函数指针接口 |
