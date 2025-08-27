# STM32F103ZET6 Bootloader 使用说明

## 功能概述

本 Bootloader 提供了完整的固件管理功能，支持通过串口进行固件更新、下载等操作。

## 内存分区

STM32F103ZET6 拥有 512KB 内部 Flash，分区如下：

- **Bootloader 区域**: 0x08000000 - 0x0800FFFF (64KB)
- **应用程序区域**: 0x08010000 - 0x0807FFFF (448KB)

## 进入 Bootloader 模式

系统启动后有 **3秒** 的时间窗口，在此期间可通过以下方式进入 Bootloader：

1. **串口输入**: 在串口终端按任意键
2. **按键输入**: 按下 PA0 按键（低电平有效）

如果 3 秒内没有任何输入，系统将自动跳转到应用程序。

## 命令列表

### 基础命令
| 命令 | 缩写 | 功能描述 |
|------|------|----------|
| `help` | `h` | 显示命令帮助信息 |
| `update` | `u` | 通过串口更新固件（XMODEM-1K协议） |
| `info` | `i` | 显示系统信息 |
| `erase` | `e` | 擦除应用程序区域 |
| `reset` | `r` | 复位系统 |
| `jump` | `j` | 跳转到应用程序 |

### 外部Flash命令
| 命令 | 缩写 | 功能描述 |
|------|------|----------|
| `xinfo` | `xi` | 显示外部Flash信息 |
| `xbackup` | `xb` | 备份固件到外部Flash |
| `xrestore` | `xr` | 从外部Flash恢复固件 |
| `xlist` | `xl` | 列出外部Flash备份 |

### ESP8266 WiFi模块命令
| 命令 | 缩写 | 功能描述 |
|------|------|----------|
| `espinit` | `ei` | 初始化ESP8266模块 |
| `esptest` | `et` | 测试ESP8266通信 |
| `espwifi` | `ew` | 连接WiFi网络（交互式配置） |
| `espinfo` | `ef` | 显示ESP8266状态和连接信息 |

### 命令输入方式

1. **完整命令**: `help`, `info`, `reset` 等
2. **单字符缩写**: `h`, `i`, `r` 等  
3. **部分匹配**: `hel`, `inf`, `res` 等（至少2个字符，且唯一匹配）
4. **大小写不敏感**: `HELP`, `Help`, `help` 都可以

## 使用示例

### 1. 进入 Bootloader

```
========================================
   STM32 Bootloader v1.0.0
========================================
Press any key or button within 3 seconds to enter bootloader...

UART input detected! Entering bootloader mode...
*******************************************
*       STM32F103 Bootloader v1.0.0      *
*       (c) 2025 - OpenLoad Project      *
*******************************************

Type 'help' or 'h' for command list
BOOT> 
```

### 2. 查看系统信息

可以使用多种方式输入命令：
```
BOOT> info       # 完整命令
BOOT> i          # 缩写
BOOT> inf        # 部分匹配
BOOT> INFO       # 大写也可以
========== System Information ==========
MCU Type           : STM32F103ZET6
Flash Total Size   : 512 KB
----------------------------------------
Bootloader Version : v1.0.0
Bootloader Size    : 64 KB
Bootloader Address : 0x08000000 - 0x0800FFFF
----------------------------------------
Application Address: 0x08010000 - 0x0807FFFF
Application Size   : 448 KB
Application Status : VALID
Application CRC32  : 0xA1B2C3D4 (first 64KB)
========================================
```

### 3. 查看帮助

```
BOOT> h

==================== HELP ====================
Available commands:

  h / help         - Show command help
  u / update       - Update firmware via XMODEM  
  i / info         - Show system information
  e / erase        - Erase application area
  r / reset        - Reset system
  j / jump         - Jump to application
  xi / xinfo       - Show external flash info
  xb / xbackup     - Backup to external flash
  xr / xrestore    - Restore from external flash
  xl / xlist       - List external flash backups
  ei / espinit     - Initialize ESP8266 module
  et / esptest     - Test ESP8266 communication
  ew / espwifi     - Connect to WiFi network
  ef / espinfo     - Show ESP8266 information

Examples:
  h          - Show this help
  u          - Update firmware (internal/external)
  i          - Show system info
  ei         - Initialize ESP8266
  ew         - Connect to WiFi
  xb         - Backup current firmware to slot 1-3
  xr         - Restore firmware from slot 1-3
  xl         - List all backup slots status
===============================================
```

### 4. 更新固件

#### 4.1 选择更新方式

```
BOOT> u
Firmware update method:
1 = XMODEM to Internal Flash
2 = XMODEM to External Flash  
3 = OTA to Internal Flash
4 = OTA to External Flash
Select (1-4): 
```

#### 4.2 XMODEM更新（选项1-2）

```
BOOT> u
Firmware update method:
1 = XMODEM to Internal Flash
2 = XMODEM to External Flash
3 = OTA to Internal Flash
4 = OTA to External Flash
Select (1-4): 2

Slot (1-3): 1
Start XMODEM transfer
Ready to receive firmware via XMODEM-1K...
Please start XMODEM transfer now

XMODEM: Ready to receive, using XMODEM-1K mode
CCCCCCC
[... 传输进度 ...]
####....####....####
XMODEM: Transfer complete, received 245760 bytes
Successfully received 245760 bytes to external flash slot 1
```

#### 4.3 OTA WiFi更新（选项3-4）

```
BOOT> u
Firmware update method:
1 = XMODEM to Internal Flash
2 = XMODEM to External Flash
3 = OTA to Internal Flash
4 = OTA to External Flash
Select (1-4): 4

Slot (1-3): 1
Enter firmware URL: http://192.168.1.100:8080/firmware.bin
Starting OTA update to external flash slot 1...
Connected to: http://192.168.1.100:8080/firmware.bin
HTTP 200 OK, Content-Length: 245760 bytes
Erasing external flash partition...
Downloading firmware...
Download: [########...........] 35% (87KB/245KB)
OTA update completed!
Total downloaded: 245760 bytes to slot 1
Firmware validation: PASSED
Use 'xr 1' to restore this firmware.
```

#### XMODEM 传输方法

**SecureCRT （推荐）:**
1. 点击菜单: Transfer -> Send File
2. Protocol: 选择 Xmodem-1K
3. 选择固件文件 (.bin)
4. 点击 Send

**Linux/Mac 终端:**
```bash
# 安装 lrzsz
sudo apt-get install lrzsz  # Ubuntu/Debian
brew install lrzsz          # Mac

# 使用 sx 命令发送文件
sx -k firmware.bin          # XMODEM-1K （推荐）
sx firmware.bin             # XMODEM-128
```

**Xshell:**
1. 文件 -> 传输 -> 发送文件
2. 协议选择 Xmodem-1K
3. 选择固件文件

**minicom:**
- Ctrl+A -> S -> 选择 xmodem-1k -> 选择文件

**注意:** XMODEM-1K 比 XMODEM-128 快8倍，推荐使用XMODEM-1K

### OTA WiFi更新详细说明

#### 前置条件
1. ESP8266模块已正确连接到UART2
2. ESP8266已连接到WiFi网络（使用`espwifi`命令）
3. 固件服务器可通过HTTP访问

#### 使用步骤
1. **连接WiFi**:
   ```
   BOOT> ew
   === WiFi Connection ===
   Enter WiFi SSID: MyWiFi
   Enter Password: ********
   Connecting to "MyWiFi"...
   WiFi connected successfully!
   IP Address: 192.168.1.100
   ```

2. **启动固件服务器**:
   ```bash
   # Python简单HTTP服务器（在固件文件目录下）
   python -m http.server 8080
   
   # 或使用nginx、Apache等Web服务器
   # 确保固件文件可通过HTTP访问
   ```

3. **执行OTA更新**:
   ```
   BOOT> u
   Select (1-4): 3  # 或 4 用于外部Flash
   Enter firmware URL: http://192.168.1.100:8080/firmware.bin
   ```

#### 支持的URL格式
- `http://192.168.1.100:8080/firmware.bin`
- `http://example.com/firmware.bin`
- `http://my-server.local:8080/app.bin`

#### OTA更新特点
- **实时进度显示**: 支持Content-Length和分块传输
- **自动验证**: 下载完成后自动验证固件有效性
- **断点续传**: 如果连接中断会自动重试
- **Flash保护**: 下载前会先验证连接和文件大小

### 5. 擦除应用程序

```
BOOT> e          # 使用缩写
WARNING: This will erase the application area!
Type 'YES' to confirm: YES
Erasing application area...
Application area erased successfully!
```

## 串口配置

- **波特率**: 115200
- **数据位**: 8
- **停止位**: 1
- **校验**: 无
- **流控**: 无

## LED 指示（如果配置）

- **快闪**: 正在等待进入 Bootloader（3秒倒计时）
- **慢闪**: 在 Bootloader 命令模式
- **常亮**: 正在执行命令（更新/备份/恢复）
- **熄灭**: 运行应用程序

## 注意事项

1. **断电保护**: 在执行更新、擦除操作时，请勿断电
2. **CRC 校验**: Bootloader 会自动计算并显示固件的 CRC32 校验值
3. **应用程序要求**: 应用程序必须编译到 0x08010000 地址

## 应用程序编译配置

在应用程序的链接脚本中，需要修改 Flash 起始地址：

```ld
MEMORY
{
  FLASH (rx) : ORIGIN = 0x08010000, LENGTH = 448K
  RAM (xrw)  : ORIGIN = 0x20000000, LENGTH = 64K
}
```

## 故障排除

| 问题 | 可能原因 | 解决方法 |
|------|----------|----------|
| 无法进入 Bootloader | 时间窗口太短 | 上电后立即按键或发送字符 |
| 无法跳转到应用程序 | 应用程序无效 | 检查应用程序编译地址是否为 0x08010000 |
| 更新失败 | Flash 写保护 | 检查 Flash 写保护状态 |

## Flash 分区说明

```
+------------------+ 0x08000000 (Flash 起始)
|                  |
|   Bootloader     |
|     (64KB)       |
|                  |
+------------------+ 0x08010000 (App 起始)
|                  |
|                  |
|   Application    |
|     (448KB)      |
|                  |
|                  |
+------------------+ 0x08080000 (Flash 结束)
```

## ESP8266 WiFi模块

### 硬件连接

ESP8266模块通过串口2（UART2）与STM32通信：

| ESP8266 | STM32F103ZET6 | 说明 |
|---------|---------------|------|
| VCC     | 3.3V         | 电源正极 |
| GND     | GND          | 电源负极 |
| TX      | PA3 (UART2_RX) | ESP8266发送数据到STM32 |
| RX      | PA2 (UART2_TX) | ESP8266接收STM32数据 |
| RST     | PE9          | 复位引脚（低电平复位） |
| EN/CH_PD| 3.3V         | 使能引脚（高电平使能） |

### ESP8266命令使用示例

#### 1. 初始化ESP8266模块

```
BOOT> ei
=== ESP8266 Initialization ===
ESP8266 initialized successfully!
```

#### 2. 测试通信

```
BOOT> et
=== ESP8266 Communication Test ===
ESP8266 communication test passed!
```

#### 3. 连接WiFi网络

```
BOOT> ew
=== WiFi Connection ===
Enter WiFi SSID: MyWiFi
Enter Password: ********
Connecting to "MyWiFi"...
WiFi connected successfully!
IP Address: 192.168.1.100
```

#### 4. 查看ESP8266信息

```
BOOT> ef
=== ESP8266 Information ===
Getting version info...
Version:
AT version:1.7.4.0(May 11 2020 19:13:04)
SDK version:3.0.4(9532ceb)
compile time:May 27 2020 10:12:17
Bin version(Wroom 02):1.7.4

Connection Status: Connected
IP Address: 192.168.1.100
Detailed Status: Connected with IP
```

### WiFi配置注意事项

1. **SSID输入**: 支持中文SSID，但建议使用英文
2. **密码输入**: 输入时显示为 `*` 号，支持退格删除
3. **连接超时**: WiFi连接超时时间为10秒
4. **断开重连**: 如需连接其他WiFi，先使用 `espwifi` 命令重新配置

### 故障排除

| 问题 | 可能原因 | 解决方法 |
|------|----------|----------|
| 初始化失败 | 模块未上电或接线错误 | 检查3.3V电源和串口接线 |
| 通信超时 | 波特率不匹配 | ESP8266默认115200，检查配置 |
| WiFi连接失败 | SSID或密码错误 | 重新输入正确的WiFi信息 |
| 无法获取IP | 路由器DHCP问题 | 检查路由器DHCP服务状态 |

## 串口配置

### UART1（调试串口）
- **波特率**: 115200
- **数据位**: 8
- **停止位**: 1
- **校验**: 无
- **流控**: 无
- **用途**: Bootloader命令交互

### UART2（ESP8266通信）
- **波特率**: 115200
- **数据位**: 8
- **停止位**: 1
- **校验**: 无
- **流控**: 无
- **用途**: ESP8266模块通信

## 后续开发计划

- [x] 实现 XMODEM 协议支持
- [x] 支持外部 Flash 备份
- [x] ESP8266 WiFi模块驱动
- [x] 通过WiFi进行OTA更新
- [ ] HTTPS支持（SSL/TLS）
- [ ] 添加固件加密功能
- [ ] 支持固件签名验证
- [ ] 支持 USB DFU 模式
- [ ] 添加固件版本管理
- [ ] Web界面固件管理
- [ ] 固件差分更新