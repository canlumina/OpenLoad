# STM32F103ZET6 Bootloader 使用说明

## 功能概述

本 Bootloader 提供了完整的固件管理功能，支持：

### 📦 固件更新方式
- **XMODEM协议**: 通过串口上传固件（普通/加密）
- **HTTP OTA**: 通过WiFi网络下载固件（普通/加密）
- **外部Flash**: 支持固件备份和恢复

### 🔐 加密固件支持  
- **XOR加密**: 轻量级快速加密，适用于基础保护
- **AES-128-CBC**: 工业级安全加密，适用于商业产品
- **设备绑定**: 基于STM32唯一ID的设备专用加密
- **内存优化**: 流式解密，仅使用4KB工作RAM
- **完整性验证**: CRC32校验确保固件完整性

### 🌐 网络功能
- **ESP8266 WiFi**: 支持WiFi网络连接
- **配置管理**: WiFi和OTA参数持久化存储
- **实时进度**: OTA下载进度显示

### 🔧 管理功能
- **多分区支持**: 3个外部Flash备份分区
- **智能检测**: 自动识别加密算法类型
- **用户友好**: 交互式命令行界面
- **故障诊断**: 详细的错误信息和调试功能

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

### WiFi网络命令
| 命令 | 缩写 | 功能描述 |
|------|------|----------|
| `wifi` | `w` | 连接WiFi网络 |
| `wstatus` | `ws` | 显示WiFi连接状态 |
| `wdebug` | `wd` | WiFi调试信息 |

### 配置管理命令
| 命令 | 缩写 | 功能描述 |
|------|------|----------|
| `cfgshow` | `cs` | 显示当前配置 |
| `cfgwifi` | `cw` | 配置WiFi设置 |
| `cfgota` | `co` | 配置OTA设置 |
| `cfgsave` | `cS` | 保存配置 |
| `cfgreset` | `cR` | 重置为默认配置 |

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
3 = HTTP OTA to Internal Flash
4 = HTTP OTA to External Flash
5 = XMODEM Encrypted to Internal Flash (XOR/AES)
6 = XMODEM Encrypted to External Flash (XOR/AES)  
7 = HTTP OTA Encrypted to Internal Flash (XOR/AES)
8 = HTTP OTA Encrypted to External Flash (XOR/AES)
Select (1-8): 
```

#### 4.2 普通XMODEM更新（选项1-2）

```
BOOT> u
Firmware update method:
1 = XMODEM to Internal Flash
2 = XMODEM to External Flash
3 = HTTP OTA to Internal Flash
4 = HTTP OTA to External Flash
5 = XMODEM Encrypted to Internal Flash (XOR/AES)
6 = XMODEM Encrypted to External Flash (XOR/AES)  
7 = HTTP OTA Encrypted to Internal Flash (XOR/AES)
8 = HTTP OTA Encrypted to External Flash (XOR/AES)
Select (1-8): 2

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

#### 4.3 加密XMODEM更新（选项5-6）

```
BOOT> u
Select (1-8): 5
WARNING! Update internal flash with encrypted firmware? (y/n): y

Select encryption algorithm:
1. XOR encryption
2. AES-128-CBC encryption
Choice (1-2): 2
Using AES-128-CBC encryption
Erasing app area...
Start XMODEM transfer (encrypted firmware)
Ready to receive encrypted firmware via XMODEM-1K...

[... 传输进度 ...]
Transfer complete: 15360 bytes
AES encrypted firmware detected, decrypting...
Starting streaming AES-CBC decryption...
Progress: 15360/15360 (100%)
Memory-optimized decryption completed: 14240 bytes
AES firmware verification successful!
```

**选项6 - 外部Flash加密固件**:
```
BOOT> u  
Select (1-8): 6
External flash encrypted firmware options:
1. Upload encrypted firmware to external flash
2. Decrypt external flash firmware to internal flash
Choice (1-2): 1

Slot (1-3): 1
Select encryption algorithm:
1. XOR encryption
2. AES-128-CBC encryption  
Choice (1-2): 2
Using AES-128-CBC encryption
Start XMODEM transfer (encrypted firmware to external flash slot 1)
Success: 15360 bytes
```

解密外部Flash固件到内部Flash：
```
BOOT> u
Select (1-8): 6
Choice (1-2): 2
WARNING! This will overwrite internal flash! (y/n): y
Select source slot (1-3): 1
AES encrypted firmware detected
AES decryption completed: 14240 bytes
External firmware decryption successful!
You can now use 'j' to jump to the new firmware.
```

#### 4.4 普通OTA WiFi更新（选项3-4）

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

#### 4.5 加密OTA WiFi更新（选项7-8）

**选项7 - 加密OTA到内部Flash**:
```
BOOT> u
Select (1-8): 7
WARNING! Update internal flash via OTA (encrypted)? (y/n): y

Select encryption algorithm:
1. XOR encryption
2. AES-128-CBC encryption
Choice (1-2): 2
Using AES-128-CBC encryption
HTTP OTA encrypted firmware - Phase 1 Implementation:
1. Download encrypted firmware to temporary partition
2. Decrypt and install automatically

URL: http://192.168.1.100:8080/firmware_aes.bin
Starting encrypted OTA download...
Download: [########...........] 100% (15KB/15KB)
OTA download completed! Starting decryption...
AES encrypted firmware detected
Starting streaming AES-CBC decryption...
Memory-optimized decryption completed: 14240 bytes
AES firmware verification successful!
Encrypted OTA update successful!
```

**选项8 - 加密OTA到外部Flash**:
```
BOOT> u
Select (1-8): 8
Slot (1-3): 2

Select encryption algorithm:
1. XOR encryption  
2. AES-128-CBC encryption
Choice (1-2): 2
Using AES-128-CBC encryption
URL: http://192.168.1.100:8080/firmware_aes.bin
Starting encrypted OTA download...
Download: [########...........] 100% (15KB/15KB)
Encrypted OTA download to external flash completed!
Use option 6-2 to decrypt and install later.
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

### 固件加密工具使用

#### 前置条件
1. **获取设备唯一ID**: 在Bootloader中使用 `i` 命令查看
   ```
   BOOT> i
   === System Info ===
   MCU: STM32F103ZET6
   ...
   Unique ID: 05D8FF35,3132564E,51125725
   ```
   
2. **安装Python依赖** (仅AES加密需要):
   ```bash
   pip install pycryptodome
   ```

#### 加密工具位置
加密工具位于 `tools/` 目录下：
- `firmware_encrypt.py` - XOR加密工具
- `firmware_aes_encrypt.py` - AES-128-CBC加密工具
- `firmware_pseudo_aes_cbc_encrypt.py` - 伪AES-CBC测试工具
- `test_key_derivation.py` - 密钥派生测试工具

#### XOR加密固件
```bash
# 基础用法
python tools/firmware_encrypt.py app.bin app_xor.bin yangcan

# 指定设备唯一ID（推荐）
python tools/firmware_encrypt.py app.bin app_xor.bin yangcan 0x05D8FF35,0x3132564E,0x51125725
```

#### AES-128-CBC加密固件  
```bash
# 必须指定设备唯一ID
python tools/firmware_aes_encrypt.py app.bin app_aes.bin yangcan 0x05D8FF35,0x3132564E,0x51125725
```

#### 加密算法对比
| 算法 | 安全级别 | 加密速度 | 解密速度 | 内存占用 | 适用场景 |
|------|----------|----------|----------|----------|----------|
| XOR | 基础 | 很快 | 很快 | 极低 | 快速开发、基础保护 |
| AES-128-CBC | 高 | 中等 | 中等 | 中等 | 生产环境、商业产品 |

#### 密钥派生验证
```bash
# 验证PC端和STM32端密钥派生算法一致性
python tools/test_key_derivation.py
```

#### 安全特性
- **设备绑定**: 每个设备使用不同的加密密钥
- **防复制**: 加密固件无法在其他设备上运行  
- **完整性验证**: CRC32校验确保固件完整性
- **内存优化**: 流式处理，仅使用4KB工作RAM

### OTA WiFi更新详细说明

#### 前置条件
1. ESP8266模块已正确连接到UART2
2. ESP8266已连接到WiFi网络（使用`espwifi`命令）
3. 固件服务器可通过HTTP访问

#### 使用步骤
1. **连接WiFi**:
   ```
   BOOT> w
   WiFi connection options:
   1. Use hardcoded WiFi (YANG/yang123456789)  
   2. Enter WiFi credentials manually
   3. Use saved configuration
   Choice (1-3): 2
   
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
   Select (1-8): 3  # 普通固件到内部Flash
   # 或选择其他选项：
   # 4 = 普通固件到外部Flash  
   # 7 = 加密固件到内部Flash
   # 8 = 加密固件到外部Flash
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

### WiFi配置和使用

#### 1. 连接WiFi网络

```
BOOT> w
WiFi connection options:
1. Use hardcoded WiFi (YANG/yang123456789)
2. Enter WiFi credentials manually  
3. Use saved configuration
Choice (1-3): 2

Enter WiFi SSID: MyWiFi
Enter Password: ********
Connecting to "MyWiFi"...
WiFi connected successfully!
IP Address: 192.168.1.100
```

#### 2. 查看WiFi状态

```
BOOT> ws
WiFi Status: Connected with IP (192.168.1.100)
```

#### 3. 查看WiFi调试信息

```
BOOT> wd
=== WiFi Debug Information ===
ESP8266 Status: Initialized
AT Command Response: OK
Current WiFi Mode: Station
Connection Status: Connected  
IP Address: 192.168.1.100
```

### 配置管理

#### 配置WiFi参数
```
BOOT> cw  
Enter WiFi SSID: MyNetwork
Enter WiFi Password: ********
WiFi configuration saved.
```

#### 配置OTA服务器
```
BOOT> co
Enter OTA server host: 192.168.1.100
Enter OTA server port: 8080  
Enter OTA firmware path: /firmware.bin
OTA configuration saved.
```

#### 保存配置到Flash
```
BOOT> cS
Configuration saved to flash.
```

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

## 故障排除

### 加密固件相关
| 问题 | 可能原因 | 解决方法 |
|------|----------|----------|
| AES firmware verification failed | 密码错误或设备ID不匹配 | 确认使用"yangcan"密码和正确的设备唯一ID |
| Failed to initialize streaming AES | 密钥派生失败 | 检查设备唯一ID读取，重启设备后重试 |
| Read encrypted chunk failed | 外部Flash读取错误 | 检查外部Flash连接，确认XMODEM传输完整 |
| Decryption failed | 加密算法不匹配 | 确认使用对应的加密工具生成固件 |

### 常见问题
| 问题 | 可能原因 | 解决方法 |
|------|----------|----------|
| 无法进入 Bootloader | 时间窗口太短 | 上电后立即按键或发送字符 |
| 无法跳转到应用程序 | 应用程序无效 | 检查应用程序编译地址是否为 0x08010000 |
| 更新失败 | Flash 写保护 | 检查 Flash 写保护状态 |
| WiFi连接失败 | SSID或密码错误 | 重新输入正确的WiFi信息 |
| OTA下载失败 | 网络连接问题 | 检查WiFi连接状态和固件服务器可达性 |

## 后续开发计划

- [x] 实现 XMODEM 协议支持
- [x] 支持外部 Flash 备份
- [x] ESP8266 WiFi模块驱动
- [x] 通过WiFi进行OTA更新
- [x] 添加固件加密功能（XOR + AES-128-CBC）
- [x] 设备绑定加密（基于STM32唯一ID）
- [x] 内存优化流式解密
- [x] 加密固件XMODEM更新
- [x] 加密固件OTA更新
- [ ] HTTPS支持（SSL/TLS）
- [ ] 支持固件签名验证
- [ ] 支持 USB DFU 模式
- [x] 添加固件版本管理
- [x] Web界面固件管理
- [ ] 固件差分更新