# STM32 固件版本管理指南

本文档详细介绍如何在STM32 Bootloader项目中使用固件版本管理功能。

## 概述

固件版本管理功能提供了完整的版本跟踪和比较能力，支持：
- 固件版本信息嵌入
- 版本自动比较和验证
- 升级版本检查
- 多固件版本管理

## 版本号格式

### 版本结构
固件版本采用4段式版本号：**major.minor.patch.build**

```
v2.1.5.2024
│ │ │ └── Build号：构建编号、时间戳
│ │ └──── Patch版本：bug修复、小改动
│ └────── Minor版本：功能增加、改进
└──────── Major版本：重大功能更新、API变更
```

### 版本号规范

| 组成部分 | 范围 | 用途 | 示例 |
|----------|------|------|------|
| **Major** | 1-999 | 重大版本更新、不兼容变更 | 1→2（架构升级） |
| **Minor** | 0-999 | 新功能添加、向后兼容 | 1.0→1.1（新功能） |
| **Patch** | 0-999 | Bug修复、小改动 | 1.1.0→1.1.1（修复） |
| **Build** | 1-99999 | 构建号、时间戳 | 任意递增数字 |

## 创建带版本信息的加密固件

### 1. 基本用法

```bash
# 默认版本 (v1.0.0.1)
python firmware_encryptor.py input.bin output.bin

# 指定密码
python firmware_encryptor.py input.bin output.bin yangcan

# 指定版本信息
python firmware_encryptor.py input.bin output.bin yangcan 2.1.5.2024
```

### 2. 批处理脚本使用

**Windows:**
```batch
encrypt_firmware.bat OpenLoad.bin yangcan
```

**Linux/macOS:**
```bash
./encrypt_firmware.sh OpenLoad.bin yangcan
```

### 3. 使用真实设备UID

```bash
# 使用设备特定的UID进行加密
python encrypt_with_real_uid.py input.bin output.bin yangcan 1.2.3.456
```

## 版本信息验证

### 1. 解密并查看版本信息

```bash
python firmware_decryptor.py encrypted.bin decrypted.bin yangcan
```

输出示例：
```
AES加密固件信息:
  魔数: 0x41455331
  头部版本: 1
  固件版本: v2.1.5.2024    ← 版本信息
  原始大小: 16220 字节
  加密大小: 16224 字节
  IV: 2326aff3a5d00593ea9bc03703654aeb
```

### 2. Bootloader版本查询

连接STM32设备，在bootloader命令行中：

```
> version
=== Firmware Version Information ===

Current Running Firmware:
  Version: v2.1.5.2024
  Build Date: Dec 02 2024 16:30:15
  
Bootloader Information:
  Version: v2.0.0 (AES+Version)
  Features: XMODEM, OTA, AES-128-CBC, Version Management
```

### 3. 版本比较分析

```
> vcompare
=== Firmware Version Comparison ===

Current Firmware: v2.1.5.2024 (Dec 02 2024)

External Flash Backup Versions:
  Slot 1: v2.0.3.1800 (AES Encrypted, 16220 bytes)
    Status: Older than current
  Slot 2: v2.2.0.2100 (AES Encrypted, 18340 bytes)
    Status: Newer than current
  Slot 3: Empty

Download Partition: v2.1.8.2050 (AES Encrypted)
  Status: Newer than current - Ready to upgrade!
```

## 版本管理最佳实践

### 1. 版本递增规则

```bash
# 开发过程
v1.0.0.1    # 初始版本
v1.0.0.2    # 修复小bug
v1.0.1.3    # 修复重要bug
v1.1.0.4    # 添加新功能
v2.0.0.5    # 重大架构更新

# 发布版本
v1.0.0.100  # 正式版本
v1.0.1.101  # 补丁版本
v1.1.0.200  # 功能版本
```

### 2. 构建号管理

**时间戳方式：**
```bash
# 使用年月日时分
python firmware_encryptor.py app.bin app_enc.bin yangcan 1.2.3.$(date +%Y%m%d%H%M)
# 结果：v1.2.3.202412021630
```

**递增编号：**
```bash
# 手动维护构建号
BUILD_NUM=1234
python firmware_encryptor.py app.bin app_enc.bin yangcan 1.2.3.$BUILD_NUM
```

### 3. 版本标签规范

建议在Git中使用对应的标签：

```bash
git tag v2.1.5.2024
git push origin v2.1.5.2024
```

## 升级工作流程

### 1. 开发阶段

```bash
# 1. 编译固件
cmake --build build/Debug

# 2. 生成带版本的加密固件
python tools/firmware_encryptor.py OpenLoad.bin OpenLoad_v1.2.3.456.bin yangcan 1.2.3.456

# 3. 验证版本信息
python tools/firmware_decryptor.py OpenLoad_v1.2.3.456.bin verify.bin yangcan
```

### 2. 部署阶段

```bash
# 1. 通过XMODEM上传到设备
# 2. Bootloader自动检测版本
> vcompare  # 查看版本状态
> u         # 执行升级
```

### 3. 版本验证

升级完成后验证：
```bash
> version   # 查看当前版本
> vcompare  # 确认版本更新成功
```

## 版本信息结构

### AES固件头部结构

```c
typedef struct {
    uint32_t magic;                    // 魔数: 0x41455331 ("AES1")
    uint32_t header_version;           // 头部版本: 1
    uint32_t firmware_size;            // 原始固件大小
    uint32_t encrypted_size;           // 加密后大小
    uint32_t crc32;                    // 原始固件CRC32
    uint32_t encrypted_crc32;          // 加密数据CRC32
    uint8_t  iv[16];                   // AES初始化向量
    uint8_t  key_hash[16];             // 密钥哈希
    firmware_version_t fw_version;     // 固件版本 (8字节)
} __attribute__((packed)) firmware_aes_header_t;
```

### 固件版本结构

```c
typedef struct {
    uint16_t major;          // 主版本号
    uint16_t minor;          // 次版本号
    uint16_t patch;          // 补丁版本号
    uint16_t build;          // 构建号
} firmware_version_t;
```

## 故障排除

### 常见错误

1. **版本格式错误**
   ```
   错误: python firmware_encryptor.py app.bin out.bin yangcan 1.2
   正确: python firmware_encryptor.py app.bin out.bin yangcan 1.2.0.1
   ```

2. **版本号超出范围**
   ```
   错误: 主版本号 > 999
   解决: 使用合理的版本号范围 (0-999)
   ```

3. **版本信息读取失败**
   ```
   原因: 固件头部损坏或不是加密固件
   解决: 使用正确的加密工具重新生成
   ```

### 调试方法

1. **查看原始头部数据**
   ```bash
   hexdump -C encrypted.bin | head -5
   ```

2. **验证魔数**
   ```bash
   # 前4字节应该是 31 53 45 41 (AES1的小端序)
   ```

3. **检查结构体大小**
   ```bash
   # 头部应该是64字节
   ls -l encrypted.bin  # 应该比原文件大64字节
   ```

## 高级应用

### 1. 自动版本递增脚本

```python
#!/usr/bin/env python3
# auto_version.py - 自动版本管理脚本

import re
import sys
from datetime import datetime

def increment_version(current_version, increment_type):
    """自动递增版本号"""
    parts = current_version.split('.')
    major, minor, patch = int(parts[0]), int(parts[1]), int(parts[2])
    build = int(datetime.now().strftime('%Y%m%d%H%M'))
    
    if increment_type == 'major':
        major += 1
        minor = patch = 0
    elif increment_type == 'minor':
        minor += 1
        patch = 0
    elif increment_type == 'patch':
        patch += 1
    
    return f"{major}.{minor}.{patch}.{build}"

# 使用示例
# python auto_version.py 1.2.3 patch
# 输出: 1.2.4.202412021630
```

### 2. 版本兼容性检查

在bootloader中可以添加版本兼容性检查逻辑，防止不兼容的固件升级。

### 3. 版本回滚功能

利用外部Flash的多个备份槽位，实现固件版本的快速回滚。

---

## 总结

固件版本管理功能提供了完整的版本跟踪能力，通过规范的版本号管理和自动化工具，可以有效管理固件的开发、测试和部署流程。建议在项目中建立版本管理规范，确保固件版本的可追溯性和升级的可靠性。

**版本**: v1.0.0 - Version Management Guide  
**更新时间**: 2024年12月2日  
**兼容性**: STM32 Bootloader v2.0.0+