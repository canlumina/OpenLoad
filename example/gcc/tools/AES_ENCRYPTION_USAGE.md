# AES固件加密使用说明

## 概述

本项目现在支持两种固件加密方式：
1. **XOR加密** - 轻量级加密，用于基础保护
2. **AES-128-CBC加密** - 强加密，用于高安全性需求

## PC端加密工具

### XOR加密工具
```bash
python tools/firmware_encrypt.py input.bin output.bin yangcan [unique_id1,unique_id2,unique_id3]
```

### AES-128加密工具
```bash
python tools/firmware_aes_encrypt.py input.bin output.bin yangcan [unique_id1,unique_id2,unique_id3]
```

**参数说明：**
- `input.bin`: 原始固件文件
- `output.bin`: 加密后的固件文件
- `yangcan`: 加密密码（与STM32端硬编码密码一致）
- `unique_id`: STM32唯一ID（可选，不指定则使用示例ID）

## STM32端支持

### Bootloader命令

使用`u`命令进入固件更新菜单，选择加密选项：

- **选项5**: XMODEM加密固件到内部Flash (支持XOR/AES)
- **选项6**: XMODEM加密固件到外部Flash (支持XOR/AES)
- **选项7**: HTTP OTA加密固件到内部Flash (规划中)
- **选项8**: HTTP OTA加密固件到外部Flash (规划中)

### 加密算法选择

选择加密固件更新选项后，系统会提示选择加密算法：
1. XOR encryption - 轻量级快速加密
2. AES-128-CBC encryption - 强加密（推荐）

### 密钥管理

- **密码**: 硬编码为 "yangcan"
- **设备绑定**: 使用STM32唯一ID作为盐值，确保固件与设备绑定
- **密钥派生**: 通过密码 + 唯一ID 生成最终加密密钥

## 使用流程

### 1. 获取STM32唯一ID

在Bootloader中使用`i`命令查看设备信息：
```
=== System Info ===
MCU: STM32F103ZET6
Flash: 512KB
Boot: 0x08000000 (64KB)
App: 0x08010000 (448KB)
Unique ID: FFFFFFFF,FFFFFFFF,FFFFFFFF
App Status: INVALID
```

记录显示的Unique ID。

### 2. 加密固件

使用PC端工具加密固件：
```bash
# AES加密（推荐）
python tools/firmware_aes_encrypt.py app.bin app_encrypted_aes.bin yangcan FFFFFFFF,FFFFFFFF,FFFFFFFF

# XOR加密（轻量级）
python tools/firmware_encrypt.py app.bin app_encrypted_xor.bin yangcan FFFFFFFF,FFFFFFFF,FFFFFFFF
```

### 3. 更新固件

1. 连接串口到STM32 Bootloader
2. 输入`u`进入固件更新菜单
3. 选择`5`（XMODEM加密固件到内部Flash）
4. 选择加密算法：`2`（AES-128-CBC）
5. 确认更新：`y`
6. 使用XMODEM协议发送加密固件文件

### 4. 验证更新

更新完成后：
- 系统会自动解密并验证固件
- 使用`i`命令确认App Status为VALID
- 使用`j`命令跳转到应用程序

## 安全特性

### AES-128-CBC加密
- **算法**: 标准AES-128-CBC模式
- **填充**: PKCS7填充
- **初始化向量**: 随机生成的16字节IV
- **密钥派生**: 基于用户密码和设备唯一ID的密钥派生算法
- **完整性检查**: CRC32校验确保固件完整性

### 设备绑定
- 每个设备使用唯一的加密密钥（基于STM32唯一ID）
- 加密固件只能在对应设备上运行
- 防止固件在不同设备间复制使用

## 错误处理

### 常见错误
1. **Crypto init failed**: 加密模块初始化失败
2. **AES decryption failed**: AES解密失败，可能是密钥不匹配
3. **Firmware verification failed**: 固件校验失败，文件可能损坏

### 排查步骤
1. 确认使用的密码正确（yangcan）
2. 确认STM32唯一ID正确
3. 确认加密固件文件完整
4. 检查XMODEM传输是否成功

## 性能对比

| 加密方式 | 加密速度 | 解密速度 | Flash占用 | RAM占用 | 安全级别 |
|---------|----------|----------|-----------|---------|----------|
| XOR     | 很快     | 很快     | 小        | 小      | 基础     |
| AES-128 | 中等     | 中等     | 中等      | 中等    | 高       |

## 注意事项

1. **密码管理**: 当前密码硬编码为"yangcan"，生产环境应使用安全的密码管理方案
2. **唯一ID**: 确保使用真实的STM32唯一ID，避免使用示例ID
3. **备份**: 更新前建议备份当前固件
4. **测试**: 在生产环境前充分测试加密固件功能
5. **兼容性**: 加密固件只能通过加密更新方式安装，普通固件更新无法处理加密固件
