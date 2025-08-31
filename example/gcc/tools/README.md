# STM32 加密固件更新工具使用文档

## 概述

本工具集为STM32F103ZET6提供完整的加密固件更新解决方案，支持XOR和AES-128-CBC两种加密算法，具备设备绑定、内存优化和流式处理等特性。

## 系统架构

### STM32端功能
- **双加密算法支持**: XOR轻量级加密 + AES-128-CBC强加密
- **内存优化**: 流式处理，仅使用4KB工作RAM
- **设备绑定**: 基于STM32唯一ID的密钥派生
- **自动检测**: 自动识别加密算法类型
- **完整性验证**: CRC32校验确保固件完整性
- **实时反馈**: 进度显示和详细状态信息

### PC端工具
- **多种加密工具**: 支持不同安全级别的加密需求
- **设备绑定加密**: 固件与特定设备绑定
- **测试验证工具**: 密钥派生算法验证
- **灵活配置**: 支持自定义密码和设备ID

## 工具说明

### 1. XOR加密工具 (`firmware_encrypt.py`)
**用途**: 轻量级固件加密，适用于基础保护需求

**使用方法**:
```bash
python firmware_encrypt.py <input.bin> <output.bin> <password> [unique_id]
```

**参数说明**:
- `input.bin`: 原始固件文件
- `output.bin`: 加密后的固件文件
- `password`: 加密密码（建议使用"yangcan"与STM32端一致）
- `unique_id`: STM32唯一ID，格式为"0xAABBCCDD,0xEEFF0011,0x22334455"（可选）

**示例**:
```bash
# 使用默认唯一ID
python firmware_encrypt.py app.bin app_xor.bin yangcan

# 使用指定设备唯一ID
python firmware_encrypt.py app.bin app_xor.bin yangcan 0x05D8FF35,0x3132564E,0x51125725
```

**特点**:
- 快速加密/解密
- 位置相关的密钥流
- 低内存占用
- 基础安全保护

### 2. 真实AES加密工具 (`firmware_aes_encrypt.py`)
**用途**: 标准AES-128-CBC加密，提供工业级安全保护

**依赖**: 需要安装pycryptodome
```bash
pip install pycryptodome
```

**使用方法**:
```bash
python firmware_aes_encrypt.py <input.bin> <output.bin> <password> [unique_id]
```

**示例**:
```bash
python firmware_aes_encrypt.py app.bin app_aes.bin yangcan 0x05D8FF35,0x3132564E,0x51125725
```

**特点**:
- 标准AES-128-CBC算法
- 随机IV生成
- PKCS7填充
- 高强度安全保护
- 设备绑定加密

### 3. 伪AES-CBC工具 (`firmware_pseudo_aes_cbc_encrypt.py`)
**用途**: 简化的AES-CBC实现，用于测试和验证

**使用方法**:
```bash
python firmware_pseudo_aes_cbc_encrypt.py <input.bin> <output.bin>
```

**示例**:
```bash
python firmware_pseudo_aes_cbc_encrypt.py app.bin app_pseudo_aes.bin
```

**特点**:
- XOR+CBC模式结构
- 与STM32端简化实现匹配
- 用于算法验证和测试
- 无需外部依赖

### 4. 密钥派生测试工具 (`test_key_derivation.py`)
**用途**: 验证PC端和STM32端密钥派生算法的一致性

**使用方法**:
```bash
python test_key_derivation.py
```

**功能**:
- 显示密钥派生过程
- 验证算法正确性
- 调试密钥相关问题

## STM32 Bootloader使用

### 1. 获取设备唯一ID
在Bootloader中输入`i`命令查看系统信息：
```
> i
=== System Info ===
MCU: STM32F103ZET6
Flash: 512KB
Boot: 0x08000000 (64KB)
App: 0x08010000 (448KB)
Unique ID: 05D8FF35,3132564E,51125725
App Status: INVALID
```

记录显示的Unique ID用于加密。

### 2. 固件更新流程
1. **进入更新模式**: 输入`u`命令
2. **选择更新方式**: 选择`5`（XMODEM加密固件到内部Flash）
3. **选择加密算法**:
   - `1`: XOR encryption（轻量级）
   - `2`: AES-128-CBC encryption（推荐）
4. **确认更新**: 输入`y`确认
5. **XMODEM传输**: 使用串口工具发送加密固件
6. **自动处理**: 系统自动解密、验证和写入

### 3. 更新选项说明
```
Firmware update method:
1 = XMODEM to Internal Flash                    # 普通固件更新
2 = XMODEM to External Flash                    # 固件备份到外部Flash
3 = HTTP OTA to Internal Flash                  # HTTP在线更新
4 = HTTP OTA to External Flash                  # HTTP备份更新
5 = XMODEM Encrypted to Internal Flash (XOR/AES)     # 加密固件更新 ★
6 = XMODEM Encrypted to External Flash (XOR/AES)     # 加密固件备份
7 = HTTP OTA Encrypted to Internal Flash (XOR/AES)   # 在线加密更新
8 = HTTP OTA Encrypted to External Flash (XOR/AES)   # 在线加密备份
```

## 完整使用流程示例

### 场景：为特定STM32设备创建AES加密固件

1. **获取设备信息**:
   ```
   # Bootloader中执行
   > i
   Unique ID: 05D8FF35,3132564E,51125725
   ```

2. **加密固件**:
   ```bash
   # 使用真实AES加密
   python tools/firmware_aes_encrypt.py firmware.bin firmware_aes.bin yangcan 0x05D8FF35,0x3132564E,0x51125725
   ```

3. **更新固件**:
   ```
   # Bootloader操作
   > u
   Select (1-8): 5
   WARNING! Update internal flash with encrypted firmware? (y/n): y
   Select encryption algorithm:
   1. XOR encryption
   2. AES-128-CBC encryption
   Choice (1-2): 2
   Using AES-128-CBC encryption
   ```

4. **传输固件**: 使用XMODEM协议传输`firmware_aes.bin`

5. **验证结果**:
   ```
   Memory-optimized decryption completed: 14240 bytes
   AES firmware verification successful!
   ```

## 安全特性

### 设备绑定
- **唯一密钥**: 每个设备使用不同的加密密钥
- **防复制**: 加密固件无法在其他设备上运行
- **密钥派生**: 基于密码+设备ID生成最终密钥

### 加密强度对比
| 算法 | 安全级别 | 加密速度 | 解密速度 | 内存占用 | 适用场景 |
|------|----------|----------|----------|----------|----------|
| XOR | 基础 | 很快 | 很快 | 极低 | 快速开发、基础保护 |
| 伪AES-CBC | 中等 | 快 | 快 | 低 | 算法验证、测试 |
| AES-128-CBC | 高 | 中等 | 中等 | 中等 | 生产环境、商业产品 |

### 完整性保护
- **CRC32校验**: 确保固件传输和存储完整性
- **头部验证**: 魔数和版本号验证
- **大小检查**: 防止缓冲区溢出
- **填充验证**: PKCS7填充格式检查

## 内存使用情况

### STM32F103ZET6 (64KB RAM)
```
程序本身:        16,536字节 (25.23%)
工作缓冲区:       4,096字节 (6.25%)
系统预留:        8,192字节 (12.50%)
可用空间:       35,536字节 (56.02%)
```

### 内存优化特性
- **流式处理**: 分块处理大固件，避免内存不足
- **双缓冲区**: 2KB读取缓冲区 + 2KB解密缓冲区
- **动态管理**: 根据处理阶段调整内存使用
- **错误恢复**: 内存不足时的优雅降级

## 故障排除

### 常见问题

1. **"AES firmware verification failed!"**
   - 检查密码是否正确（应为"yangcan"）
   - 确认设备唯一ID是否匹配
   - 验证加密固件文件完整性

2. **"Failed to initialize streaming AES!"**
   - 检查设备唯一ID读取
   - 确认密钥派生算法一致性
   - 重启设备后重试

3. **"Read encrypted chunk failed!"**
   - 检查外部Flash连接
   - 确认XMODEM传输完整
   - 检查存储分区状态

4. **内存相关错误**
   - 减少工作缓冲区大小
   - 检查其他内存使用
   - 考虑使用XOR加密减少内存占用

### 调试技巧

1. **密钥派生验证**:
   ```bash
   python tools/test_key_derivation.py
   ```

2. **固件头部检查**:
   - 使用十六进制编辑器查看加密固件
   - 验证魔数：XOR(0x58524331) 或 AES(0x41455331)

3. **逐步测试**:
   - 先使用伪AES测试流程
   - 确认流程正常后使用真实AES
   - 对比不同设备的唯一ID

## 开发说明

### 添加新的加密算法

1. **创建PC端工具**: 参考现有工具结构
2. **定义固件头部**: 设计新的魔数和头部格式  
3. **实现STM32解密**: 在`firmware_xxx.c`中实现
4. **集成到bootloader**: 在`bootloader_cmd.c`中添加检测和处理
5. **更新文档**: 补充使用说明

### 性能优化建议

1. **内存优化**: 减少缓冲区大小，增加处理次数
2. **速度优化**: 使用硬件加速器（如果可用）
3. **存储优化**: 压缩固件减少传输时间
4. **并行处理**: 边解密边写入Flash

## 版本历史

- **v1.0**: 基础XOR加密实现
- **v1.1**: 添加AES-128-CBC支持
- **v1.2**: 内存优化和流式处理
- **v1.3**: 设备绑定和完整性验证
- **v1.4**: 多工具支持和文档完善

## 技术支持

如遇问题请检查：
1. 工具版本和依赖
2. STM32唯一ID获取
3. 加密固件格式
4. 内存使用情况
5. 串口通信稳定性

更多技术细节请参考源码注释和`AES_ENCRYPTION_USAGE.md`文档。