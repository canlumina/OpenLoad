# STM32F103ZET6 Bootloader with mbedTLS AES Encryption

一个功能完整的STM32F103ZET6 bootloader项目，支持串口DMA通信、外部Flash存储、WiFi连接和mbedTLS AES-128-CBC固件加密。

## 🚀 主要特性

### 核心功能
- ✅ **高效串口通信** - 基于DMA + 空闲中断 + 环形缓冲区
- ✅ **外部Flash存储** - W25Q64支持，用于固件备份
- ✅ **WiFi连接** - ESP8266支持，实现OTA升级
- ✅ **多种升级方式** - XMODEM串口传输、WiFi OTA
- ✅ **mbedTLS AES加密** - 工业级AES-128-CBC固件加密
- ✅ **流式解密** - 内存优化的分块加解密
- ✅ **完整的命令系统** - 丰富的bootloader命令

### 硬件支持
- **主控芯片**: STM32F103ZET6 (Cortex-M3, 72MHz)
- **Flash分区**: 64KB bootloader + 448KB application
- **外部存储**: W25Q64 (8MB SPI Flash)
- **WiFi模块**: ESP8266
- **通信接口**: UART1 (PA9/PA10), UART2 (PA2/PA3)

## 📁 项目结构

```
OpenLoad/
├── Core/
│   ├── Src/                    # 源代码
│   │   ├── bootloader_cmd.c    # Bootloader命令系统
│   │   ├── firmware_aes.c      # AES加密实现 (mbedTLS)
│   │   ├── dev_usart.c         # 串口DMA驱动
│   │   ├── w25q64.c           # 外部Flash驱动
│   │   ├── esp8266_wifi.c     # WiFi驱动
│   │   └── ...
│   ├── Inc/                    # 头文件
│   └── mbedtls/               # mbedTLS库
│       ├── include/           # mbedTLS头文件
│       └── library/           # AES实现
├── tools/                     # 固件加密工具
│   ├── firmware_encryptor.py  # AES固件加密工具
│   ├── firmware_decryptor.py  # AES固件解密验证
│   └── encrypt_firmware.bat   # Windows批处理
├── build/                     # 编译输出
└── docs/
    └── AES_FIRMWARE_GUIDE.md  # AES加密使用指南
```

## 🔧 编译和构建

### 前置要求
- ARM GCC 工具链
- CMake 3.22+
- Ninja 构建工具

### 编译步骤
```bash
# 配置Debug版本
cmake --preset=Debug

# 编译项目
cmake --build build/Debug

# 生成bin文件
arm-none-eabi-objcopy -O binary build/Debug/OpenLoad.elf OpenLoad.bin
```

## 🔐 固件加密使用

### 1. 加密固件
```bash
cd tools
encrypt_firmware.bat OpenLoad.bin yangcan
```

### 2. 使用bootloader升级
1. 进入bootloader命令模式（上电3秒内按PA0或发送串口字符）
2. 输入 `u` 选择更新
3. 选择 `1` (XMODEM) → `3` (内部Flash加密) → `2` (AES-128-CBC)
4. 使用XMODEM发送加密的bin文件

### 3. 详细说明
请参阅 [`AES_FIRMWARE_GUIDE.md`](AES_FIRMWARE_GUIDE.md) 获取完整的使用指南。

## 📋 Bootloader命令

| 命令 | 简写 | 描述 |
|------|------|------|
| help | h | 显示帮助信息 |
| update | u | 固件升级 (XMODEM/OTA) |
| info | i | 显示系统信息 |
| erase | e | 擦除应用区域 |
| reset | r | 系统复位 |
| jump | j | 跳转到应用程序 |
| wifi | w | 连接WiFi网络 |
| xbackup | xb | 备份到外部Flash |
| xrestore | xr | 从外部Flash恢复 |

## 🛡️ 安全特性

### AES-128-CBC加密
- **标准算法**: 使用mbedTLS提供的工业级AES实现
- **密钥安全**: 基于用户密码和STM32唯一ID派生密钥
- **传输安全**: PKCS7填充 + CRC32校验确保完整性
- **存储安全**: 支持加密固件的安全存储和传输

### 内存优化
- **流式解密**: 分块处理避免RAM不足
- **ROM S盒**: 使用ROM存储S盒表节省RAM
- **临时缓冲**: 仅使用4KB临时缓冲区

## 📊 性能指标

| 指标 | 数值 |
|------|------|
| Flash占用 | 50KB / 512KB (9.8%) |
| RAM占用 | 17KB / 64KB (26.6%) |
| 加密速度 | ~100KB/s |
| 解密速度 | ~50KB/s |
| 支持最大固件 | 448KB |

## 🔄 升级方式对比

| 方式 | 传输 | 加密支持 | 适用场景 |
|------|------|----------|----------|
| XMODEM串口 | 有线 | AES/XOR | 开发调试 |
| WiFi OTA | 无线 | AES/XOR | 远程升级 |
| 外部Flash | 本地 | AES/XOR | 备份恢复 |

## 🚀 快速开始

1. **硬件连接**
   - UART1: PA9(TX), PA10(RX) - 调试串口
   - UART2: PA2(TX), PA3(RX) - ESP8266通信
   - SPI2: PB13/14/15 - W25Q64连接

2. **编译烧录**
   ```bash
   cmake --build build/Debug
   # 烧录OpenLoad.elf到STM32
   ```

3. **测试加密**
   ```bash
   cd tools
   python firmware_encryptor.py OpenLoad.bin test_encrypted.bin
   ```

4. **升级测试**
   - 进入bootloader: 发送任意字符
   - 输入: `u` → `1` → `3` → `2`
   - XMODEM发送加密固件

## 📚 文档资源

- [AES固件加密完整指南](AES_FIRMWARE_GUIDE.md)
- [CLAUDE.md - 项目配置说明](CLAUDE.md)
- [mbedTLS集成报告](MBEDTLS_INTEGRATION.md)

## 🤝 开发贡献

欢迎提交Issue和Pull Request！

### 开发环境
- STM32CubeIDE 或 VS Code + ARM扩展
- OpenOCD 调试器支持
- 串口调试工具

## 📄 许可证

本项目基于MIT许可证开源。

---

**版本**: v2.0 (mbedTLS AES集成版)  
**最后更新**: 2024年9月2日  
**作者**: yangcan