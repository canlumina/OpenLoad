#!/usr/bin/env python3
"""
STM32 AES固件加密工具
使用AES-128-CBC加密算法
作者: Claude
"""

import sys
import os
import struct
import secrets
from binascii import crc32
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad, unpad
from Crypto.Hash import SHA256

# 常量定义
FIRMWARE_AES_MAGIC = 0x41455331  # "AES1"
FIRMWARE_AES_VERSION = 1
AES_BLOCK_SIZE = 16
AES_KEY_SIZE = 16

# STM32型号的唯一ID地址
STM32_UNIQUE_ID_ADDRESSES = {
    'STM32F103': 0x1FFFF7E8,
    'STM32F407': 0x1FFF7A10,
    'STM32F429': 0x1FFF7A10,
    'STM32L476': 0x1FFF7590,
    'STM32H743': 0x1FF1E800,
}

# 默认使用STM32F103的示例ID
DEFAULT_UNIQUE_ID = [0x12345678, 0x9ABCDEF0, 0x13579BDF]

def print_usage():
    """打印使用说明"""
    print("STM32 AES固件加密工具")
    print("用法: python firmware_aes_encrypt.py <input.bin> <output.bin> [password] [unique_id]")
    print("")
    print("参数:")
    print("  input.bin   - 输入的原始固件文件")
    print("  output.bin  - 输出的AES加密固件文件")
    print("  password    - 加密密码(可选，默认使用内置密码)")
    print("  unique_id   - STM32唯一ID，格式：0x12345678,0x9ABCDEF0,0x13579BDF")
    print("")
    print("获取STM32唯一ID:")
    print("  在bootloader中输入 'i' 命令查看设备的Unique ID")
    print("")
    print("示例:")
    print("  python firmware_aes_encrypt.py app.bin app_aes.bin")
    print("  python firmware_aes_encrypt.py app.bin app_aes.bin MyPassword123")
    print("  python firmware_aes_encrypt.py app.bin app_aes.bin MyPassword123 0x12345678,0x9ABCDEF0,0x13579BDF")

def derive_aes_key(password, unique_id):
    """从密码和唯一ID派生AES密钥"""
    # 创建盐值
    salt = b''
    for uid in unique_id:
        salt += struct.pack('<I', uid)
    
    # 使用PBKDF2派生密钥
    key_data = password.encode('utf-8') + salt
    
    # 简化的密钥派生（模拟STM32端的算法）
    temp_key = bytearray(32)
    
    # 复制密码
    pwd_bytes = password.encode('utf-8')
    pwd_len = min(len(pwd_bytes), 24)
    temp_key[:pwd_len] = pwd_bytes[:pwd_len]
    
    # 混合盐值
    for i in range(2):  # 只使用前2个ID值
        temp_key[24 + i*4] = (unique_id[i] >> 0) & 0xFF
        temp_key[24 + i*4 + 1] = (unique_id[i] >> 8) & 0xFF
        temp_key[24 + i*4 + 2] = (unique_id[i] >> 16) & 0xFF
        temp_key[24 + i*4 + 3] = (unique_id[i] >> 24) & 0xFF
    
    # 简化的哈希混合（模拟STM32算法）
    aes_sbox = [
        0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
        0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
        0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
        0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
        0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
        0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
        0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
        0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
        0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
        0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
        0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
        0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
        0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
        0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
        0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
        0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
    ]
    
    key = bytearray(16)
    # 与STM32端保持一致 - 使用10轮简化版本
    for round_num in range(10):  # 与STM32端的简化版本一致
        for i in range(16):
            key[i] = temp_key[i] ^ temp_key[i+16]
            key[i] ^= (round_num & 0xFF)
            key[i] = aes_sbox[key[i]]
        
        temp_key[:16] = key
        temp_key[16:32] = key
    
    return bytes(key)

def calculate_crc32(data):
    """计算CRC32校验值"""
    return crc32(data) & 0xFFFFFFFF

def parse_unique_id(unique_id_str):
    """解析唯一ID字符串"""
    try:
        parts = unique_id_str.split(',')
        if len(parts) != 3:
            raise ValueError("唯一ID必须包含3个32位值")
        
        unique_id = []
        for part in parts:
            part = part.strip()
            if part.startswith('0x') or part.startswith('0X'):
                value = int(part, 16)
            else:
                value = int(part, 10)
            unique_id.append(value & 0xFFFFFFFF)
        
        return unique_id
    except Exception as e:
        print(f"解析唯一ID失败: {e}")
        return None

def encrypt_firmware(input_file, output_file, password, unique_id=None):
    """AES加密固件文件"""
    try:
        # 读取原始固件
        with open(input_file, 'rb') as f:
            firmware_data = f.read()
        
        if len(firmware_data) == 0:
            print(f"错误: 固件文件 {input_file} 为空")
            return False
        
        print(f"原始固件大小: {len(firmware_data)} 字节")
        
        # 计算原始固件CRC32
        original_crc32 = calculate_crc32(firmware_data)
        print(f"原始固件CRC32: 0x{original_crc32:08X}")
        
        # 密钥处理
        if unique_id is None:
            unique_id = DEFAULT_UNIQUE_ID
        
        aes_key = derive_aes_key(password, unique_id)
        print(f"使用密码: {password}")
        print(f"唯一ID: {[hex(x) for x in unique_id]}")
        print(f"派生AES密钥: {aes_key.hex()}")
        
        # 生成随机IV
        iv = secrets.token_bytes(16)
        print(f"生成IV: {iv.hex()}")
        
        # AES-CBC加密
        cipher = AES.new(aes_key, AES.MODE_CBC, iv)
        padded_data = pad(firmware_data, AES_BLOCK_SIZE)
        encrypted_data = cipher.encrypt(padded_data)
        
        encrypted_crc32 = calculate_crc32(encrypted_data)
        print(f"加密数据CRC32: 0x{encrypted_crc32:08X}")
        
        # 生成密钥哈希
        key_hash = SHA256.new(aes_key).digest()[:16]
        
        # 创建AES固件头部
        header = struct.pack('<IIIIII16s16s8s',
            FIRMWARE_AES_MAGIC,          # magic
            FIRMWARE_AES_VERSION,        # version
            len(firmware_data),          # firmware_size
            len(encrypted_data),         # encrypted_size
            original_crc32,              # crc32
            encrypted_crc32,             # encrypted_crc32
            iv,                          # iv[16]
            key_hash,                    # key_hash[16]
            b'\x00' * 8                  # reserved[8]
        )
        
        # 写入AES加密固件文件
        with open(output_file, 'wb') as f:
            f.write(header)
            f.write(encrypted_data)
        
        total_size = len(header) + len(encrypted_data)
        print(f"AES加密完成!")
        print(f"头部大小: {len(header)} 字节")
        print(f"加密数据大小: {len(encrypted_data)} 字节")
        print(f"总文件大小: {total_size} 字节")
        print(f"输出文件: {output_file}")
        
        return True
        
    except Exception as e:
        print(f"加密过程中出错: {e}")
        return False

def decrypt_firmware(input_file, output_file, password, unique_id=None):
    """AES解密固件文件（用于验证）"""
    try:
        # 读取AES加密固件
        with open(input_file, 'rb') as f:
            file_data = f.read()
        
        if len(file_data) < 72:  # 头部大小检查
            print(f"错误: 文件 {input_file} 太小，不是有效的AES加密固件")
            return False
        
        # 解析头部
        header_data = file_data[:72]
        header = struct.unpack('<IIIIII16s16s8s', header_data)
        
        magic, version, firmware_size, encrypted_size, orig_crc32, enc_crc32, iv, key_hash, reserved = header
        
        # 验证头部
        if magic != FIRMWARE_AES_MAGIC:
            print(f"错误: 无效的魔数 0x{magic:08X}")
            return False
        
        if version != FIRMWARE_AES_VERSION:
            print(f"错误: 不支持的版本 {version}")
            return False
        
        print(f"AES加密固件信息:")
        print(f"  原始大小: {firmware_size} 字节")
        print(f"  加密大小: {encrypted_size} 字节")
        print(f"  原始CRC32: 0x{orig_crc32:08X}")
        print(f"  加密CRC32: 0x{enc_crc32:08X}")
        print(f"  IV: {iv.hex()}")
        
        # 密钥处理
        if unique_id is None:
            unique_id = DEFAULT_UNIQUE_ID
        
        aes_key = derive_aes_key(password, unique_id)
        calculated_hash = SHA256.new(aes_key).digest()[:16]
        
        # 验证密钥
        if calculated_hash != key_hash:
            print(f"错误: 密码不匹配!")
            return False
        
        print(f"密钥验证通过")
        
        # 提取加密数据
        encrypted_data = file_data[72:72+encrypted_size]
        if len(encrypted_data) != encrypted_size:
            print(f"错误: 加密数据大小不匹配")
            return False
        
        # 验证加密数据CRC32
        calc_enc_crc32 = calculate_crc32(encrypted_data)
        if calc_enc_crc32 != enc_crc32:
            print(f"错误: 加密数据CRC32校验失败")
            return False
        
        # AES-CBC解密
        print("正在解密...")
        cipher = AES.new(aes_key, AES.MODE_CBC, iv)
        decrypted_padded = cipher.decrypt(encrypted_data)
        decrypted_data = unpad(decrypted_padded, AES_BLOCK_SIZE)
        
        # 验证解密数据CRC32
        calc_orig_crc32 = calculate_crc32(decrypted_data)
        if calc_orig_crc32 != orig_crc32:
            print(f"错误: 解密数据CRC32校验失败")
            return False
        
        # 写入解密固件
        with open(output_file, 'wb') as f:
            f.write(decrypted_data)
        
        print(f"AES解密完成!")
        print(f"输出文件: {output_file}")
        print(f"解密文件大小: {len(decrypted_data)} 字节")
        
        return True
        
    except Exception as e:
        print(f"解密过程中出错: {e}")
        return False

def main():
    if len(sys.argv) < 3:
        print_usage()
        return 1
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    password = sys.argv[3] if len(sys.argv) > 3 else "yangcan"
    
    # 解析唯一ID
    unique_id = DEFAULT_UNIQUE_ID
    if len(sys.argv) > 4:
        parsed_id = parse_unique_id(sys.argv[4])
        if parsed_id is None:
            return 1
        unique_id = parsed_id
    
    # 检查输入文件是否存在
    if not os.path.exists(input_file):
        print(f"错误: 输入文件 {input_file} 不存在")
        return 1
    
    # 检查是否为解密操作
    is_decrypt = input_file.endswith('.aes') or 'aes' in input_file.lower()
    
    print("=" * 60)
    if is_decrypt:
        print("STM32 AES固件解密工具")
        print(f"输入: {input_file}")
        print(f"输出: {output_file}")
        print(f"密码: {password}")
        print(f"唯一ID: {[hex(x) for x in unique_id]}")
        print("=" * 60)
        
        success = decrypt_firmware(input_file, output_file, password, unique_id)
    else:
        print("STM32 AES固件加密工具")
        print(f"输入: {input_file}")
        print(f"输出: {output_file}")
        print(f"密码: {password}")
        print(f"唯一ID: {[hex(x) for x in unique_id]}")
        print("=" * 60)
        
        success = encrypt_firmware(input_file, output_file, password, unique_id)
    
    return 0 if success else 1

if __name__ == "__main__":
    exit(main())