# STM32 AES固件加密和升级完整指南

## 概述

本项目已集成mbedTLS AES-128-CBC加密算法，可以安全地加密固件并通过bootloader进行升级。本指南将详细介绍如何加密固件和使用bootloader升级加密固件。

## 系统架构

- **加密算法**: AES-128-CBC (符合工业标准)
- **密钥派生**: 基于用户密码和STM32唯一ID
- **填充方式**: PKCS7填充
- **传输协议**: XMODEM
- **支持方式**: 串口传输、外部Flash存储

## 准备工作

### 1. 安装Python环境

确保系统安装了Python 3.6+：

```bash
python --version
```

### 2. 安装加密库

```bash
pip install pycryptodome
```

### 3. 编译固件

```bash
cd D:\STM32\OpenLoad\example\gcc
cmake --build build/Debug
```

编译完成后会生成 `build/Debug/OpenLoad.elf` 文件，需要转换为bin文件：

```bash
arm-none-eabi-objcopy -O binary build/Debug/OpenLoad.elf OpenLoad.bin
```

## 固件加密流程

### 方法1：使用批处理脚本（推荐）

```batch
cd tools
encrypt_firmware.bat OpenLoad.bin yangcan
```

### 方法2：使用Python脚本

```bash
cd tools
python firmware_encryptor.py OpenLoad.bin OpenLoad_encrypted.bin yangcan
```

### 加密过程说明

1. **密钥派生**: 使用密码"yangcan"和STM32唯一ID生成AES-128密钥
2. **生成随机IV**: 每次加密使用不同的初始化向量
3. **PKCS7填充**: 将固件填充到16字节的倍数
4. **AES-CBC加密**: 使用标准AES-128-CBC算法加密
5. **生成头部**: 创建包含元数据的固件头部
6. **CRC校验**: 计算原始固件和加密数据的CRC32校验值

### 加密文件结构

```
[AES固件头部 - 80字节]
├── 魔数 (4字节): "AES1" (0x41455331)
├── 版本 (4字节): 1
├── 原始大小 (4字节)
├── 加密大小 (4字节)  
├── 原始CRC32 (4字节)
├── 加密CRC32 (4字节)
├── 保留字段 (8字节)
├── IV初始化向量 (16字节)
├── 密钥哈希 (16字节)
└── 保留字段 (8字节)

[AES加密数据]
└── 使用AES-128-CBC加密的固件数据
```

## Bootloader升级流程

### 1. 进入Bootloader命令模式

- **方式1**: 上电后3秒内按下PA0按键
- **方式2**: 上电后3秒内发送任意串口字符

### 2. 使用XMODEM升级到内部Flash

```
OpenLoad > u
Select update method:
1 = XMODEM (from UART1)
2 = OTA (via WiFi)

Enter choice: 1

Select target location:
1 = Internal Flash (Normal firmware)
2 = External Flash (Backup firmware) 
3 = Internal Flash (Encrypted XOR/AES)
4 = External Flash (Encrypted XOR/AES)

Enter choice: 3

WARNING! Update internal flash with encrypted firmware? (y/n): y

Select encryption algorithm:
1. XOR encryption
2. AES-128-CBC encryption

Enter choice: 2

Using AES-128-CBC encryption
Start XMODEM transfer (encrypted firmware)
Waiting for XMODEM transfer...

注意：密码已硬编码为"yangcan"，无需输入
```

### 3. 传输加密固件

使用支持XMODEM的串口工具（如Tera Term、SecureCRT等）发送加密的bin文件。

### 4. 升级过程

bootloader会自动：
1. 接收加密固件数据
2. 验证AES固件头部
3. **自动使用硬编码密钥"yangcan"进行解密（无需用户输入）**
4. 验证解密后的固件完整性
5. 擦除内部Flash应用区域
6. 写入解密后的固件
7. 验证写入的固件

### 5. 使用外部Flash升级

```
OpenLoad > u
...
Enter choice: 4  # External Flash (Encrypted XOR/AES)
...
Enter choice: 2  # AES-128-CBC encryption

# 固件会先存储到外部Flash
# 然后使用以下命令恢复到内部Flash：
OpenLoad > xr 0
```

## 高级功能

### 验证加密固件

使用解密工具验证加密固件的完整性：

```bash
python firmware_decryptor.py OpenLoad_encrypted.bin OpenLoad_decrypted.bin yangcan
```

### 查看系统信息

```
OpenLoad > i
===== System Information =====
Bootloader Version: 1.0
Build Date: Mar  8 2024
Build Time: 10:30:00

CPU: STM32F103ZET6 @ 72MHz
FLASH: 512KB, RAM: 64KB
Unique ID: 12345678-9ABCDEF0-11223344

Current Firmware:
- Size: 45678 bytes
- CRC32: 0x12345678
- Entry: 0x08010000
- Valid: Yes
```

### WiFi OTA升级

1. 配置WiFi连接：
```
OpenLoad > cw
Enter SSID: MyWiFi
Password: MyPassword123
```

2. 配置OTA服务器：
```
OpenLoad > co
Enter server IP: 192.168.1.100
Enter port: 8080
Enter firmware path: /firmware/OpenLoad_encrypted.bin
```

3. 执行OTA升级：
```
OpenLoad > u
Enter choice: 2  # OTA via WiFi
```

## 安全特性

### 1. 密钥安全
- 使用STM32唯一ID作为盐值
- 多轮密钥强化算法
- 内存中的密钥会在使用后清零

### 2. 传输安全
- AES-128-CBC提供强加密保护
- PKCS7填充防止填充攻击
- CRC32校验确保数据完整性

### 3. 存储安全
- 加密固件可以安全存储
- 支持外部Flash备份
- 防止固件逆向工程

## 故障排除

### 1. 加密工具错误

**错误**: `ModuleNotFoundError: No module named 'Crypto'`
**解决**: 安装pycryptodome库
```bash
pip install pycryptodome
```

**错误**: `文件太小，不是有效的加密固件`
**解决**: 检查加密过程是否正确完成

### 2. Bootloader升级错误

**错误**: `AES init failed!`
**解决**: 
- 确保固件使用正确的密码"yangcan"进行加密
- 确保STM32 unique ID正确读取

**错误**: `AES decryption failed!`
**解决**:
- 确认使用密码"yangcan"加密固件
- 检查固件文件是否损坏
- 验证传输过程无错误

**错误**: `Firmware verification failed!`
**解决**:
- 重新使用密码"yangcan"加密固件
- 检查原始固件是否有效
- 确保加密工具和bootloader使用相同的算法

**错误**: `Invalid AES header magic`
**解决**:
- 确保使用正确的加密工具生成固件
- 验证固件头部结构完整性

### 3. XMODEM传输错误

**错误**: 传输超时或失败
**解决**:
- 检查串口连接
- 确认波特率设置正确
- 使用可靠的串口工具
- 检查流控制设置

## 修改加密密码

如果需要使用自定义密码，需要修改bootloader源码：

1. 在 `Core/Src/firmware_aes.c` 中找到并修改：
```c
static const char* aes_default_password = "yangcan";  // 改为你的密码
```

2. 重新编译bootloader固件并烧录到STM32

3. 使用加密工具时指定相同的密码：
```bash
python firmware_encryptor.py OpenLoad.bin OpenLoad_encrypted.bin 你的密码
```

**注意**: bootloader和加密工具必须使用完全相同的密码。

## 性能参数

- **加密速度**: ~100KB/s (取决于固件大小)
- **解密速度**: ~50KB/s (受Flash写入限制)
- **RAM使用**: 约4KB临时缓冲区
- **支持最大固件**: 448KB (STM32F103ZET6应用区域)

## 文件清单

```
tools/
├── firmware_encryptor.py     # AES固件加密工具
├── firmware_decryptor.py     # AES固件解密验证工具
├── encrypt_firmware.bat      # Windows批处理脚本
└── AES_FIRMWARE_GUIDE.md     # 本使用指南
```

## 版本信息

- **项目版本**: v2.0
- **mbedTLS集成**: 完成
- **支持算法**: AES-128-CBC
- **最后更新**: 2024年9月2日