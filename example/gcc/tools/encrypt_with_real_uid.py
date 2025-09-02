#!/usr/bin/env python3
# 使用真实STM32 Unique ID的加密工具

import sys
from firmware_encryptor import encrypt_firmware_file

# 你的STM32的真实Unique ID: 05D8FF35-3132564E-51125725
real_unique_id = [0x05D8FF35, 0x3132564E, 0x51125725]

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("用法: python encrypt_with_real_uid.py <input.bin> <output.bin> [password]")
        sys.exit(1)
    
    input_file = sys.argv[1] 
    output_file = sys.argv[2]
    password = sys.argv[3] if len(sys.argv) > 3 else "yangcan"
    
    print("使用真实的STM32 Unique ID进行加密:")
    print(f"Unique ID: {real_unique_id[0]:08X}-{real_unique_id[1]:08X}-{real_unique_id[2]:08X}")
    
    success = encrypt_firmware_file(input_file, output_file, password, real_unique_id)
    
    if success:
        print("加密成功！请使用此文件进行XMODEM传输。")
    else:
        print("加密失败！")
        sys.exit(1)