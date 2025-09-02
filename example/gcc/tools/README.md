# STM32 固件AES加密工具

本目录包含用于STM32固件AES加密的工具集，基于mbedTLS AES-128-CBC算法。

## 工具文件

| 文件名 | 描述 | 用途 |
|--------|------|------|
| `firmware_encryptor.py` | Python AES固件加密工具 | 将普通固件加密为AES加密固件 |
| `firmware_decryptor.py` | Python AES固件解密工具 | 解密和验证AES加密固件 |
| `encrypt_with_real_uid.py` | 使用真实UID的加密工具 | 使用设备真实UID进行加密 |
| `encrypt_firmware.bat` | Windows批处理脚本 | Windows下简化的加密操作入口 |
| `encrypt_firmware.sh` | Linux/macOS Shell脚本 | Linux/macOS下简化的加密操作入口 |

## 快速开始

### 1. 加密固件

**方法1 - 使用脚本（推荐）：**

Windows:
```batch
encrypt_firmware.bat OpenLoad.bin yangcan
```

Linux/macOS:
```bash
./encrypt_firmware.sh OpenLoad.bin yangcan
```

**方法2 - 直接使用Python：**
```bash
# 使用真实设备UID（推荐）
python encrypt_with_real_uid.py OpenLoad.bin OpenLoad_encrypted.bin yangcan

# 或使用默认UID
python firmware_encryptor.py OpenLoad.bin OpenLoad_encrypted.bin yangcan
```

### 2. 验证加密固件
```bash
python firmware_decryptor.py OpenLoad_encrypted.bin OpenLoad_decrypted.bin yangcan
```

## 前置要求

- Python 3.6+
- pycryptodome库：`pip install pycryptodome`

## 密码说明

- 默认密码：`yangcan`
- 密码与bootloader中硬编码的密码必须一致
- 支持自定义密码（需要修改bootloader源码）

## 使用流程

1. **编译固件** → 生成 `.bin` 文件
2. **加密固件** → 使用本工具生成 `_encrypted.bin` 文件（优先使用真实UID）  
3. **传输固件** → 通过XMODEM或OTA发送加密固件到STM32
4. **自动解密** → bootloader自动使用mbedTLS解密并安装固件

## 使用说明

### Bootloader命令
- `u` 或 `update`: 进入固件升级菜单
- 选择`3 = Internal Flash (Encrypted XOR/AES)`
- 选择`2 = AES-128-CBC encryption`
- 使用XMODEM传输加密固件文件

### 密钥派生
工具使用STM32的Unique ID作为盐值，结合用户密码派生AES密钥：
- 真实设备UID: 从实际硬件读取（`encrypt_with_real_uid.py`）
- 默认UID: 用于测试的固定值

## 详细说明

请参阅项目根目录的 `AES_FIRMWARE_GUIDE.md` 获取完整使用指南。

---

**版本**: v2.0 (mbedTLS集成版本)  
**更新时间**: 2024年9月2日