#!/usr/bin/env python3
"""
伪AES-CBC固件加密工具 - 模拟STM32端的简化AES-CBC实现
使用XOR + CBC结构，与STM32端的简化版本保持一致
"""

import sys
import os
from struct import pack
from zlib import crc32
import secrets

def calculate_crc32(data):
    """计算CRC32校验值"""
    return crc32(data) & 0xFFFFFFFF

def pkcs7_pad(data, block_size=16):
    """PKCS7填充"""
    padding_len = block_size - (len(data) % block_size)
    padding = bytes([padding_len] * padding_len)
    return data + padding

def pseudo_aes_cbc_encrypt(plaintext, iv):
    """
    伪AES-CBC加密：使用XOR模拟AES块加密 + 标准CBC模式
    这与STM32端的简化实现相对应
    """
    if len(plaintext) % 16 != 0:
        raise ValueError("数据长度必须是16的倍数")
    
    encrypted = bytearray()
    prev_block = bytearray(iv)
    
    for i in range(0, len(plaintext), 16):
        # 获取当前明文块
        plaintext_block = bytearray(plaintext[i:i+16])
        
        # CBC模式：与前一个密文块（或IV）异或
        for j in range(16):
            plaintext_block[j] ^= prev_block[j]
        
        # 简化的"AES"加密：使用XOR
        encrypted_block = bytearray()
        for byte in plaintext_block:
            encrypted_block.append(byte ^ 0xAA)
        
        # 将密文块添加到结果中
        encrypted.extend(encrypted_block)
        
        # 更新前一个块为当前密文块
        prev_block = encrypted_block
    
    return bytes(encrypted)

def create_aes_header(firmware_size, encrypted_size, crc32_val, encrypted_crc32, iv):
    """创建AES固件头部"""
    # AES固件魔数
    magic = 0x41455331  # "AES1"
    version = 1
    
    # 生成假的密钥哈希
    key_hash = b'\x00' * 16
    
    # 保留字段
    reserved = b'\x00' * 8
    
    header = pack('<IIIIII16s16s8s', 
                  magic, version, firmware_size, encrypted_size,
                  crc32_val, encrypted_crc32, iv, key_hash, reserved)
    
    return header

def encrypt_firmware(input_file, output_file):
    """伪AES-CBC加密固件"""
    try:
        # 读取原始固件
        with open(input_file, 'rb') as f:
            firmware_data = f.read()
        
        if len(firmware_data) == 0:
            print(f"错误: 输入文件为空")
            return False
        
        print(f"原始固件大小: {len(firmware_data)} 字节")
        
        # 计算原始固件CRC32
        firmware_crc32 = calculate_crc32(firmware_data)
        print(f"原始固件CRC32: 0x{firmware_crc32:08X}")
        
        # PKCS7填充到16字节边界
        padded_firmware = pkcs7_pad(firmware_data, 16)
        print(f"填充后大小: {len(padded_firmware)} 字节")
        
        # 生成随机IV
        iv = secrets.token_bytes(16)
        print(f"生成IV: {iv.hex()}")
        
        # 伪AES-CBC加密
        encrypted_data = pseudo_aes_cbc_encrypt(padded_firmware, iv)
        encrypted_crc32 = calculate_crc32(encrypted_data)
        print(f"加密数据CRC32: 0x{encrypted_crc32:08X}")
        
        # 创建AES头部
        header = create_aes_header(len(firmware_data), len(encrypted_data), 
                                 firmware_crc32, encrypted_crc32, iv)
        
        # 写入输出文件
        with open(output_file, 'wb') as f:
            f.write(header)
            f.write(encrypted_data)
        
        print(f"伪AES-CBC加密完成!")
        print(f"头部大小: {len(header)} 字节")
        print(f"加密数据大小: {len(encrypted_data)} 字节")
        print(f"总文件大小: {len(header) + len(encrypted_data)} 字节")
        print(f"输出文件: {output_file}")
        
        return True
        
    except Exception as e:
        print(f"加密过程出错: {e}")
        return False

def main():
    if len(sys.argv) != 3:
        print("使用方法: python firmware_pseudo_aes_cbc_encrypt.py <input.bin> <output.bin>")
        print("示例: python firmware_pseudo_aes_cbc_encrypt.py app.bin app_pseudo_aes_cbc.bin")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    if not os.path.exists(input_file):
        print(f"错误: 输入文件 '{input_file}' 不存在")
        sys.exit(1)
    
    print("="*60)
    print("伪AES-CBC固件加密工具")
    print(f"输入: {input_file}")
    print(f"输出: {output_file}")
    print("="*60)
    
    if encrypt_firmware(input_file, output_file):
        print("加密成功!")
        sys.exit(0)
    else:
        print("加密失败!")
        sys.exit(1)

if __name__ == "__main__":
    main()