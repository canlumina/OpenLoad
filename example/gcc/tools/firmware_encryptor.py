#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
STM32 固件AES加密工具
用于将普通固件文件加密成可被bootloader识别的AES加密固件

使用方法:
python firmware_encryptor.py input.bin output_encrypted.bin [password]
"""

import os
import sys
import struct
import hashlib
import binascii
from Crypto.Cipher import AES
from Crypto.Random import get_random_bytes
from Crypto.Util.Padding import pad

def calculate_crc32(data):
    """计算CRC32校验值 - 使用STM32兼容的算法（不取反）"""
    # 标准CRC32算法，与STM32固件中使用的算法一致
    crc32_table = [
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
        0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
        0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
        0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
        0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
        0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
        0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
        0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
        0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
        0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
        0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
        0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
        0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
        0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
        0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
        0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
        0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
        0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
        0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
        0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
        0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
        0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
        0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
        0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
        0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
        0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
        0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
    ]
    
    crc = 0xFFFFFFFF
    for byte in data:
        crc = crc32_table[(crc ^ byte) & 0xFF] ^ (crc >> 8)
    
    # STM32的CRC32实现通常不取反，但为了保持一致性，这里也不取反
    return crc & 0xFFFFFFFF

def derive_aes_key(password, salt):
    """
    派生AES密钥 - 模拟STM32中的密钥派生过程
    这里使用Python实现相同的逻辑
    """
    # 创建32字节的临时密钥缓冲区
    temp_key = bytearray(32)
    
    # 复制密码（最多24字节）
    password_bytes = password.encode('utf-8')
    pwd_len = min(len(password_bytes), 24)
    temp_key[:pwd_len] = password_bytes[:pwd_len]
    
    # 混合盐值（STM32 unique ID模拟）
    for i in range(2):  # 只使用前2个ID值
        salt_word = salt[i]
        temp_key[24 + i*4] = (salt_word >> 0) & 0xFF
        temp_key[24 + i*4 + 1] = (salt_word >> 8) & 0xFF
        temp_key[24 + i*4 + 2] = (salt_word >> 16) & 0xFF
        temp_key[24 + i*4 + 3] = (salt_word >> 24) & 0xFF
    
    # 简化的密钥强化 - 多轮哈希
    key = bytearray(16)
    for round_num in range(10):
        for i in range(16):
            key[i] = temp_key[i] ^ temp_key[i+16] ^ (round_num & 0xFF)
            # 增加一些随机性，避免全部相同的值
            key[i] ^= ((i + round_num) & 0xFF)
        
        # 更新temp_key用于下一轮
        temp_key[:16] = key
        temp_key[16:32] = key
    
    return bytes(key)

def encrypt_firmware_aes(firmware_data, password, salt):
    """
    使用AES-128-CBC加密固件
    """
    print(f"原始固件大小: {len(firmware_data)} 字节")
    
    # 派生AES密钥
    aes_key = derive_aes_key(password, salt)
    print(f"AES密钥: {binascii.hexlify(aes_key).decode()}")
    
    # 生成随机IV
    iv = get_random_bytes(16)
    print(f"IV: {binascii.hexlify(iv).decode()}")
    
    # 使用PKCS7填充
    padded_data = pad(firmware_data, AES.block_size)
    print(f"填充后大小: {len(padded_data)} 字节")
    
    # AES-128-CBC加密
    cipher = AES.new(aes_key, AES.MODE_CBC, iv)
    encrypted_data = cipher.encrypt(padded_data)
    print(f"加密后大小: {len(encrypted_data)} 字节")
    
    # 计算校验值
    firmware_crc32 = calculate_crc32(firmware_data)
    encrypted_crc32 = calculate_crc32(encrypted_data)
    
    # 生成密钥哈希（简化版本）
    key_hash = hashlib.md5(aes_key).digest()
    
    print(f"原始固件CRC32: 0x{firmware_crc32:08x}")
    print(f"加密数据CRC32: 0x{encrypted_crc32:08x}")
    
    return {
        'encrypted_data': encrypted_data,
        'iv': iv,
        'firmware_size': len(firmware_data),
        'encrypted_size': len(encrypted_data),
        'firmware_crc32': firmware_crc32,
        'encrypted_crc32': encrypted_crc32,
        'key_hash': key_hash
    }

def create_aes_firmware_header(encrypt_result):
    """
    创建AES加密固件头部
    对应STM32中的 firmware_aes_header_t 结构
    """
    # 定义魔数和版本
    FIRMWARE_AES_MAGIC = 0x41455331  # "AES1"
    FIRMWARE_AES_VERSION = 1
    
    # 按照STM32结构体定义：80字节
    # typedef struct {
    #     uint32_t magic;              // 4字节
    #     uint32_t version;            // 4字节  
    #     uint32_t firmware_size;      // 4字节
    #     uint32_t encrypted_size;     // 4字节
    #     uint32_t crc32;              // 4字节
    #     uint32_t encrypted_crc32;    // 4字节
    #     uint8_t  iv[AES_IV_SIZE];    // 16字节
    #     uint8_t  key_hash[16];       // 16字节
    #     uint8_t  reserved[8];        // 8字节
    # } __attribute__((packed)) firmware_aes_header_t;
    # 总共：6*4 + 16 + 16 + 8 = 24 + 16 + 16 + 8 = 64字节
    
    # 打包头部结构 - 总共64字节
    header = struct.pack('<IIIIII16s16s8s',
        FIRMWARE_AES_MAGIC,                    # magic (4)
        FIRMWARE_AES_VERSION,                  # version (4) 
        encrypt_result['firmware_size'],       # firmware_size (4)
        encrypt_result['encrypted_size'],      # encrypted_size (4)
        encrypt_result['firmware_crc32'],      # crc32 (4)
        encrypt_result['encrypted_crc32'],     # encrypted_crc32 (4)
        encrypt_result['iv'],                  # iv[16] (16)
        encrypt_result['key_hash'],            # key_hash[16] (16)
        b'\x00' * 8                           # reserved[8] (8)
    )
    
    print(f"头部结构大小: {len(header)} 字节")
    return header

def encrypt_firmware_file(input_file, output_file, password="yangcan", stm32_unique_id=None):
    """
    加密固件文件
    """
    if not os.path.exists(input_file):
        print(f"错误: 输入文件 {input_file} 不存在")
        return False
    
    # 默认的STM32 unique ID (可以替换为实际的设备ID)
    if stm32_unique_id is None:
        stm32_unique_id = [0x12345678, 0x9ABCDEF0, 0x11223344]
    
    try:
        # 读取原始固件
        print(f"读取固件文件: {input_file}")
        with open(input_file, 'rb') as f:
            firmware_data = f.read()
        
        if len(firmware_data) == 0:
            print("错误: 固件文件为空")
            return False
        
        print(f"固件文件大小: {len(firmware_data)} 字节")
        
        # 加密固件
        print(f"使用密码 '{password}' 加密固件...")
        encrypt_result = encrypt_firmware_aes(firmware_data, password, stm32_unique_id)
        
        # 创建头部
        header = create_aes_firmware_header(encrypt_result)
        
        # 写入加密固件文件
        print(f"写入加密固件: {output_file}")
        with open(output_file, 'wb') as f:
            f.write(header)
            f.write(encrypt_result['encrypted_data'])
        
        print(f"加密完成!")
        print(f"总文件大小: {len(header) + len(encrypt_result['encrypted_data'])} 字节")
        print(f"头部大小: {len(header)} 字节")
        print(f"加密数据大小: {len(encrypt_result['encrypted_data'])} 字节")
        
        return True
        
    except Exception as e:
        print(f"加密过程出错: {str(e)}")
        return False

def verify_encrypted_firmware(encrypted_file, password="yangcan", stm32_unique_id=None):
    """
    验证加密固件文件的完整性
    """
    if stm32_unique_id is None:
        stm32_unique_id = [0x12345678, 0x9ABCDEF0, 0x11223344]
    
    try:
        with open(encrypted_file, 'rb') as f:
            # 读取头部 - 实际大小是64字节
            header_data = f.read(64)  # firmware_aes_header_t 实际大小
            if len(header_data) < 64:
                print("错误: 文件太小，不是有效的加密固件")
                return False
            
            # 解析头部 - 对应新的结构
            header = struct.unpack('<IIIIII16s16s8s', header_data)
            magic = header[0]
            version = header[1]
            firmware_size = header[2]
            encrypted_size = header[3]
            firmware_crc32 = header[4]
            encrypted_crc32 = header[5]
            iv = header[6]
            key_hash = header[7]
            
            # 验证魔数
            if magic != 0x41455331:
                print(f"错误: 无效的魔数 0x{magic:08x}")
                return False
            
            print(f"AES加密固件验证:")
            print(f"  魔数: 0x{magic:08x} (AES1)")
            print(f"  版本: {version}")
            print(f"  原始大小: {firmware_size} 字节")
            print(f"  加密大小: {encrypted_size} 字节")
            print(f"  原始CRC32: 0x{firmware_crc32:08x}")
            print(f"  加密CRC32: 0x{encrypted_crc32:08x}")
            print(f"  IV: {binascii.hexlify(iv).decode()}")
            
            # 读取加密数据
            encrypted_data = f.read(encrypted_size)
            if len(encrypted_data) != encrypted_size:
                print("错误: 加密数据大小不匹配")
                return False
            
            # 验证加密数据CRC32
            actual_encrypted_crc32 = calculate_crc32(encrypted_data)
            if actual_encrypted_crc32 != encrypted_crc32:
                print(f"错误: 加密数据CRC32校验失败")
                print(f"  期望: 0x{encrypted_crc32:08x}")
                print(f"  实际: 0x{actual_encrypted_crc32:08x}")
                return False
            
            print("加密固件验证通过!")
            return True
            
    except Exception as e:
        print(f"验证过程出错: {str(e)}")
        return False

def main():
    if len(sys.argv) < 3:
        print("用法: python firmware_encryptor.py <input.bin> <output_encrypted.bin> [password]")
        print("示例: python firmware_encryptor.py app.bin app_encrypted.bin yangcan")
        return 1
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    password = sys.argv[3] if len(sys.argv) > 3 else "yangcan"
    
    print("=" * 60)
    print("STM32 固件AES加密工具")
    print("=" * 60)
    
    # 加密固件
    success = encrypt_firmware_file(input_file, output_file, password)
    
    if success:
        print("\n" + "=" * 60)
        print("验证加密固件...")
        print("=" * 60)
        verify_encrypted_firmware(output_file, password)
        
        print("\n" + "=" * 60)
        print("使用说明:")
        print("1. 将加密后的固件文件通过XMODEM传输到STM32")
        print("2. 在bootloader中选择选项 '3' (Internal Flash - Encrypted AES)")
        print("3. 选择加密算法 '2' (AES-128-CBC encryption)")
        print("4. bootloader将自动使用硬编码密码解密并安装固件")
        print(f"5. 注意：密码已硬编码为'{password}'，无需手动输入")
        print("=" * 60)
        
        return 0
    else:
        return 1

if __name__ == "__main__":
    exit(main())