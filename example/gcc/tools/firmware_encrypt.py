#!/usr/bin/env python3
"""
STM32 固件加密工具
支持XOR加密+CRC32校验
作者: Claude
"""

import sys
import os
import struct
import hashlib
from binascii import crc32

# 常量定义
FIRMWARE_CRYPTO_MAGIC = 0x43525950  # "CRYP"
FIRMWARE_CRYPTO_VERSION = 1
CRYPTO_KEY_SIZE = 32

# STM32型号的唯一ID地址
STM32_UNIQUE_ID_ADDRESSES = {
    'STM32F103': 0x1FFFF7E8,
    'STM32F407': 0x1FFF7A10,
    'STM32F429': 0x1FFF7A10,
    'STM32L476': 0x1FFF7590,
    'STM32H743': 0x1FF1E800,
}

# 默认使用STM32F103的示例ID，用户需要提供真实ID
DEFAULT_UNIQUE_ID = [0x12345678, 0x9ABCDEF0, 0x13579BDF]

def print_usage():
    """打印使用说明"""
    print("STM32固件加密工具")
    print("用法: python firmware_encrypt.py <input.bin> <output.bin> [key] [unique_id]")
    print("")
    print("参数:")
    print("  input.bin   - 输入的原始固件文件")
    print("  output.bin  - 输出的加密固件文件")
    print("  key         - 加密密钥(可选，默认使用内置密钥)")
    print("  unique_id   - STM32唯一ID，格式：0x12345678,0x9ABCDEF0,0x13579BDF")
    print("")
    print("获取STM32唯一ID:")
    print("  在bootloader中输入 'i' 命令查看设备的Unique ID")
    print("")
    print("示例:")
    print("  python firmware_encrypt.py app.bin app_encrypted.bin")
    print("  python firmware_encrypt.py app.bin app_encrypted.bin MySecretKey123")
    print("  python firmware_encrypt.py app.bin app_encrypted.bin MySecretKey123 0x12345678,0x9ABCDEF0,0x13579BDF")

def expand_key(key_bytes, unique_id):
    """密钥扩展，模仿STM32上的算法"""
    expanded_key = bytearray(CRYPTO_KEY_SIZE)
    
    # 复制密钥，如果密钥太短则重复填充
    for i in range(CRYPTO_KEY_SIZE):
        expanded_key[i] = key_bytes[i % len(key_bytes)]
    
    # 与唯一ID异或（模拟STM32上的操作）
    for i in range(3):  # STM32F103有3个32位唯一ID
        id_word = unique_id[i]
        for j in range(4):
            if i*4+j < CRYPTO_KEY_SIZE:
                expanded_key[i*4+j] ^= (id_word >> (j*8)) & 0xFF
    
    return expanded_key

def xor_encrypt_decrypt(data, key, offset):
    """XOR加密/解密"""
    result = bytearray(data)
    
    for i in range(len(data)):
        # 使用偏移量和位置计算密钥索引
        key_index = (offset + i) % len(key)
        key_byte = key[key_index]
        
        # 增加基于位置的变化
        key_byte ^= ((offset + i) & 0xFF)
        key_byte ^= (((offset + i) >> 8) & 0xFF)
        
        result[i] ^= key_byte
    
    return result

def calculate_crc32(data):
    """计算CRC32校验值"""
    return crc32(data) & 0xFFFFFFFF

def generate_key_hash(key_bytes):
    """生成密钥哈希（简单版本）"""
    hash_result = bytearray(16)
    
    # 简单的哈希算法：使用XOR和移位
    for i in range(len(key_bytes)):
        byte = key_bytes[i]
        pos = i % 16
        
        hash_result[pos] ^= byte
        hash_result[(pos + 1) % 16] ^= ((byte >> 1) | (byte << 7)) & 0xFF
        hash_result[(pos + 8) % 16] ^= (byte ^ 0xAA) & 0xFF
    
    # 额外的混淆
    for round_num in range(3):
        for i in range(16):
            hash_result[i] ^= hash_result[(i + 1) % 16]
            hash_result[i] = ((hash_result[i] << 1) | (hash_result[i] >> 7)) & 0xFF
    
    return hash_result

def encrypt_firmware(input_file, output_file, key_str, unique_id=None):
    """加密固件文件"""
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
        
        key_bytes = key_str.encode('utf-8')
        expanded_key = expand_key(key_bytes, unique_id)
        print(f"使用密钥: {key_str}")
        print(f"扩展密钥: {expanded_key.hex()}")
        
        # 生成密钥哈希
        key_hash = generate_key_hash(expanded_key)
        print(f"密钥哈希: {key_hash.hex()}")
        
        # 加密固件数据
        print("正在加密固件...")
        encrypted_data = xor_encrypt_decrypt(firmware_data, expanded_key, 0)
        encrypted_crc32 = calculate_crc32(encrypted_data)
        print(f"加密数据CRC32: 0x{encrypted_crc32:08X}")
        
        # 创建固件头部
        header = struct.pack('<IIIIII16s12s',
            FIRMWARE_CRYPTO_MAGIC,       # magic
            FIRMWARE_CRYPTO_VERSION,     # version
            len(firmware_data),          # firmware_size
            len(encrypted_data),         # encrypted_size
            original_crc32,              # crc32
            encrypted_crc32,             # encrypted_crc32
            bytes(key_hash),             # key_hash[16]
            b'\x00' * 12                 # reserved[12]
        )
        
        # 写入加密固件文件
        with open(output_file, 'wb') as f:
            f.write(header)
            f.write(encrypted_data)
        
        total_size = len(header) + len(encrypted_data)
        print(f"加密完成!")
        print(f"头部大小: {len(header)} 字节")
        print(f"加密数据大小: {len(encrypted_data)} 字节") 
        print(f"总文件大小: {total_size} 字节")
        print(f"输出文件: {output_file}")
        
        return True
        
    except Exception as e:
        print(f"加密过程中出错: {e}")
        return False

def decrypt_firmware(input_file, output_file, key_str, unique_id=None):
    """解密固件文件（用于验证）"""
    try:
        # 读取加密固件
        with open(input_file, 'rb') as f:
            file_data = f.read()
        
        if len(file_data) < 64:  # 头部大小检查
            print(f"错误: 文件 {input_file} 太小，不是有效的加密固件")
            return False
        
        # 解析头部
        header_data = file_data[:64]
        header = struct.unpack('<IIIIII16s12s', header_data)
        
        magic, version, firmware_size, encrypted_size, orig_crc32, enc_crc32, key_hash, reserved = header
        
        # 验证头部
        if magic != FIRMWARE_CRYPTO_MAGIC:
            print(f"错误: 无效的魔数 0x{magic:08X}")
            return False
        
        if version != FIRMWARE_CRYPTO_VERSION:
            print(f"错误: 不支持的版本 {version}")
            return False
        
        print(f"固件信息:")
        print(f"  原始大小: {firmware_size} 字节")
        print(f"  加密大小: {encrypted_size} 字节")
        print(f"  原始CRC32: 0x{orig_crc32:08X}")
        print(f"  加密CRC32: 0x{enc_crc32:08X}")
        
        # 密钥处理
        if unique_id is None:
            unique_id = DEFAULT_UNIQUE_ID
            
        key_bytes = key_str.encode('utf-8')
        expanded_key = expand_key(key_bytes, unique_id)
        calculated_hash = generate_key_hash(expanded_key)
        
        # 验证密钥
        if bytes(calculated_hash) != key_hash:
            print(f"错误: 密钥不匹配!")
            print(f"期望哈希: {key_hash.hex()}")
            print(f"计算哈希: {bytes(calculated_hash).hex()}")
            return False
        
        print(f"密钥验证通过")
        
        # 提取加密数据
        encrypted_data = file_data[64:64+encrypted_size]
        if len(encrypted_data) != encrypted_size:
            print(f"错误: 加密数据大小不匹配")
            return False
        
        # 验证加密数据CRC32
        calc_enc_crc32 = calculate_crc32(encrypted_data)
        if calc_enc_crc32 != enc_crc32:
            print(f"错误: 加密数据CRC32校验失败")
            print(f"期望: 0x{enc_crc32:08X}, 计算: 0x{calc_enc_crc32:08X}")
            return False
        
        # 解密数据
        print("正在解密...")
        decrypted_data = xor_encrypt_decrypt(encrypted_data, expanded_key, 0)
        
        # 验证解密数据CRC32
        calc_orig_crc32 = calculate_crc32(decrypted_data[:firmware_size])
        if calc_orig_crc32 != orig_crc32:
            print(f"错误: 解密数据CRC32校验失败")
            print(f"期望: 0x{orig_crc32:08X}, 计算: 0x{calc_orig_crc32:08X}")
            return False
        
        # 写入解密固件
        with open(output_file, 'wb') as f:
            f.write(decrypted_data[:firmware_size])
        
        print(f"解密完成!")
        print(f"输出文件: {output_file}")
        print(f"解密文件大小: {firmware_size} 字节")
        
        return True
        
    except Exception as e:
        print(f"解密过程中出错: {e}")
        return False

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

def main():
    if len(sys.argv) < 3:
        print_usage()
        return 1
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    key_str = sys.argv[3] if len(sys.argv) > 3 else "yangcan"
    
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
    is_decrypt = input_file.endswith('.enc') or input_file.endswith('.encrypted')
    
    print("=" * 50)
    if is_decrypt:
        print("STM32 固件解密工具")
        print(f"输入: {input_file}")
        print(f"输出: {output_file}")
        print(f"密钥: {key_str}")
        print(f"唯一ID: {[hex(x) for x in unique_id]}")
        print("=" * 50)
        
        success = decrypt_firmware(input_file, output_file, key_str, unique_id)
    else:
        print("STM32 固件加密工具")
        print(f"输入: {input_file}")
        print(f"输出: {output_file}")
        print(f"密钥: {key_str}")
        print(f"唯一ID: {[hex(x) for x in unique_id]}")
        print("=" * 50)
        
        success = encrypt_firmware(input_file, output_file, key_str, unique_id)
    
    return 0 if success else 1

if __name__ == "__main__":
    exit(main())