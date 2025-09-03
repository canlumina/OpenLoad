#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), 'backend'))

from backend.core.firmware_manager import FirmwareManager
from backend.core.crypto import crypto_manager, EncryptionType

def test_crc32_fix():
    """测试修复后的CRC32算法"""
    
    # 初始化固件管理器
    uploads_dir = os.path.join(os.path.dirname(__file__), 'uploads')
    fm = FirmwareManager(uploads_dir)
    
    # 源固件路径
    source_firmware = r"D:\STM32\OpenLoad\example\gcc\tools\RTC2.bin"
    
    if not os.path.exists(source_firmware):
        print(f"错误：源固件文件不存在: {source_firmware}")
        return False
    
    # 复制固件到uploads目录
    target_firmware = os.path.join(uploads_dir, "RTC2_crc32_test.bin")
    import shutil
    shutil.copy2(source_firmware, target_firmware)
    
    try:
        # 添加固件
        firmware = fm.add_firmware(
            file_path=target_firmware,
            original_filename="RTC2_crc32_test.bin",
            version="v2.21.1.2025",
            target_device="STM32F103ZET6",
            metadata={
                'source': 'crc32_test',
                'test_time': '2025-09-03T17:49:00'
            }
        )
        
        print(f"固件添加成功: {firmware.id}")
        print(f"原始大小: {firmware.size} bytes")
        print(f"原始CRC32: {firmware.checksum}")
        
        # 使用随机生成的密钥加密固件
        password = "test_password_for_crc32_fix_123456"
        success = fm.encrypt_firmware(
            firmware_id=firmware.id,
            algorithm=EncryptionType.AES_128_CBC,
            password=password
        )
        
        if success:
            # 重新获取更新后的固件信息
            encrypted_firmware = fm.get_firmware(firmware.id)
            print(f"\n加密成功!")
            print(f"加密大小: {encrypted_firmware.size} bytes")
            print(f"密码: {encrypted_firmware.encryption_metadata.get('password', 'N/A')}")
            print(f"版本: {encrypted_firmware.version}")
            
            # 读取加密后的数据验证
            encrypted_content = fm.read_firmware_content(firmware.id)
            if encrypted_content:
                print(f"加密数据前16字节: {encrypted_content[:16].hex().upper()}")
                
                # 验证是否为AES加密格式
                import struct
                if len(encrypted_content) >= 4:
                    magic = struct.unpack('<I', encrypted_content[:4])[0]
                    if magic == 0x41455331:  # "AES1"
                        print("OK - AES固件头验证成功")
                        # 读取CRC32值
                        if len(encrypted_content) >= 20:
                            firmware_crc32 = struct.unpack('<I', encrypted_content[16:20])[0]
                            print(f"固件头中的CRC32: 0x{firmware_crc32:08X}")
                    else:
                        print(f"ERROR - 意外的魔数: 0x{magic:08X}")
            
            return True
        else:
            print("ERROR - 固件加密失败")
            return False
            
    except Exception as e:
        print(f"错误: {e}")
        import traceback
        traceback.print_exc()
        return False

if __name__ == "__main__":
    test_crc32_fix()