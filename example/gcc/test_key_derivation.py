#!/usr/bin/env python3
# 测试密钥派生一致性

import sys
import os
sys.path.insert(0, 'WebLoad/backend')

from core.crypto import crypto_manager, EncryptionType

def test_key_derivation():
    """测试密钥派生"""
    password = "cancan"
    
    print("测试密钥派生一致性...")
    print(f"密码: {password}")
    
    # 使用Web端的密钥派生
    key = crypto_manager._derive_aes_key_stm32_compatible(password)
    
    print(f"生成的AES密钥: {key.hex().upper()}")
    print(f"密钥长度: {len(key)} 字节")
    
    # 测试加密
    test_data = b"Hello, STM32!"
    print(f"测试数据: {test_data}")
    
    try:
        encrypted_data, metadata = crypto_manager.encrypt_firmware(test_data, EncryptionType.AES_128_CBC, password=password)
        print(f"加密成功！")
        print(f"原始大小: {len(test_data)} 字节")
        print(f"加密后大小: {len(encrypted_data)} 字节")
        print(f"元数据: {metadata}")
        
        # 显示前32字节的十六进制
        print(f"前32字节: {encrypted_data[:32].hex().upper()}")
        
    except Exception as e:
        print(f"加密失败: {e}")

if __name__ == "__main__":
    test_key_derivation()