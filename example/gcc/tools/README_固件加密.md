# STM32 固件加密功能使用说明

## 功能概述

本固件加密功能基于XOR加密算法，结合CRC32校验，为STM32固件提供基本的保护。

## 加密特性

- **加密算法**: XOR + 位置相关密钥流
- **密钥管理**: 结合STM32唯一ID的密钥扩展
- **完整性校验**: CRC32校验防止篡改
- **安全等级**: 入门级（防止简单逆向和拷贝）

## 工具使用

### 1. PC端加密工具

#### Python脚本方式
```bash
# 使用默认密钥加密
python firmware_encrypt.py app.bin app_encrypted.bin

# 使用自定义密钥加密
python firmware_encrypt.py app.bin app_encrypted.bin "MySecretKey123"

# 解密验证（测试用）
python firmware_encrypt.py app_encrypted.bin app_decrypted.bin "MySecretKey123"
```

#### 批处理脚本方式（Windows）
```cmd
# 使用默认设置加密（会自动生成输出文件名）
encrypt_firmware.bat app.bin

# 指定输出文件名
encrypt_firmware.bat app.bin encrypted_app.bin

# 指定输出文件名和密钥
encrypt_firmware.bat app.bin encrypted_app.bin "MyKey123"
```

### 2. STM32端Bootloader使用

#### 命令说明
在Bootloader命令模式下，输入`u`命令，会看到新增的加密固件选项：

```
Firmware update method:
1 = XMODEM to Internal Flash
2 = XMODEM to External Flash
3 = HTTP OTA to Internal Flash
4 = HTTP OTA to External Flash
5 = XMODEM Encrypted to Internal Flash      <- 新增
6 = XMODEM Encrypted to External Flash      <- 新增
7 = HTTP OTA Encrypted to Internal Flash    <- 新增（暂未实现）
8 = HTTP OTA Encrypted to External Flash    <- 新增（暂未实现）
```

#### 使用流程

1. **准备加密固件**
   ```cmd
   encrypt_firmware.bat your_app.bin your_app_encrypted.bin
   ```

2. **进入Bootloader命令模式**
   - 复位STM32，在3秒内按按键或发送串口字符

3. **选择加密固件更新**
   ```
   > u
   Select (1-8): 5
   WARNING! Update internal flash with encrypted firmware? (y/n): y
   ```

4. **开始XMODEM传输**
   - 使用串口工具发送加密的.bin文件
   - Bootloader会自动检测、解密并验证固件

## 技术细节

### 加密固件格式

```c
固件文件结构:
+------------------+
| 加密固件头部(64B) |
+------------------+
| 加密的固件数据    |
+------------------+

头部结构:
- magic (4B):       0x43525950 ("CRYP")
- version (4B):     版本号 (1)
- firmware_size:    原始固件大小
- encrypted_size:   加密后大小
- crc32:           原始固件CRC32
- encrypted_crc32:  加密数据CRC32
- key_hash (16B):   密钥哈希值
- reserved (12B):   保留字段
```

### 密钥系统

1. **基础密钥**: 默认 `"OpenLoad_STM32_Crypto_Key_2024"`
2. **密钥扩展**: 重复填充到32字节
3. **唯一化**: 与STM32唯一ID异或
4. **位置相关**: 加密时考虑数据在固件中的位置

### 安全性说明

**优点:**
- 防止简单的固件拷贝
- 增加逆向工程难度
- 结合硬件唯一ID
- CRC32完整性校验

**局限性:**
- XOR加密相对简单
- 密钥在代码中可见（编译后）
- 不能防御高级攻击手段

## 常见问题

### Q1: 加密后固件变大了多少？
A: 增加64字节的固件头部

### Q2: 加密是否影响启动速度？
A: 第一次使用需要解密时间，后续启动使用已解密的固件

### Q3: 如何更改默认密钥？
A: 修改`firmware_crypto.c`和`firmware_encrypt.py`中的默认密钥字符串

### Q4: 能否支持不同的STM32型号？
A: 需要修改唯一ID地址（0x1FFFF7E8为STM32F103地址）

## 文件说明

- `firmware_encrypt.py` - Python加密工具
- `encrypt_firmware.bat` - Windows批处理脚本
- `firmware_crypto.h` - STM32端加密模块头文件
- `firmware_crypto.c` - STM32端加密模块实现

## 升级建议

后续可以考虑升级到AES加密：
1. 更高的安全性
2. 利用STM32硬件AES加速
3. 标准加密算法
4. 支持更复杂的密钥管理