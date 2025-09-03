#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import hashlib
from enum import Enum
from typing import Optional, Tuple, Dict, Any, List
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives import padding, hashes
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC
import logging

logger = logging.getLogger(__name__)

class EncryptionType(Enum):
    """加密类型枚举"""
    NONE = "none"
    XOR = "xor"
    AES_128_CBC = "aes-128-cbc"
    AES_256_CBC = "aes-256-cbc"

class CryptoManager:
    """加密管理器"""
    
    def __init__(self):
        self.logger = logging.getLogger(self.__class__.__name__)
    
    def generate_key(self, algorithm: EncryptionType, password: str = None) -> bytes:
        """生成加密密钥"""
        if algorithm == EncryptionType.NONE:
            return b''
        
        elif algorithm == EncryptionType.XOR:
            if password:
                return password.encode('utf-8')[:32]  # 最多32字节
            else:
                return os.urandom(16)  # 默认16字节随机密钥
        
        elif algorithm == EncryptionType.AES_128_CBC:
            if password:
                # 使用与单片机端一致的密钥派生算法
                return self._derive_aes_key_stm32_compatible(password)
            else:
                return os.urandom(16)  # AES-128需要16字节密钥
        
        elif algorithm == EncryptionType.AES_256_CBC:
            if password:
                salt = b'openload_salt_32_bytes_long!'
                kdf = PBKDF2HMAC(
                    algorithm=hashes.SHA256(),
                    length=32,
                    salt=salt,
                    iterations=100000,
                )
                return kdf.derive(password.encode('utf-8'))
            else:
                return os.urandom(32)  # AES-256需要32字节密钥
        
        else:
            raise ValueError(f"不支持的加密算法: {algorithm}")
    
    def encrypt_firmware(self, 
                        data: bytes, 
                        algorithm: EncryptionType,
                        key: bytes = None,
                        password: str = None,
                        firmware_version: str = None) -> Tuple[bytes, Dict[str, Any]]:
        """
        加密固件数据
        
        Args:
            data: 固件数据
            algorithm: 加密算法
            key: 加密密钥 (如果提供则使用此密钥)
            password: 密码 (如果未提供密钥，则从密码生成)
        
        Returns:
            加密后的数据和元数据
        """
        try:
            if algorithm == EncryptionType.NONE:
                return data, {'algorithm': algorithm.value}
            
            # 生成或使用提供的密钥
            if key is None:
                key = self.generate_key(algorithm, password)
            
            if algorithm == EncryptionType.XOR:
                return self._xor_encrypt(data, key), {
                    'algorithm': algorithm.value,
                    'key_length': len(key)
                }
            
            elif algorithm in [EncryptionType.AES_128_CBC, EncryptionType.AES_256_CBC]:
                if algorithm == EncryptionType.AES_128_CBC:
                    # 生成STM32兼容的AES固件格式
                    encrypted_data_with_header, metadata = self._aes_encrypt_stm32_format(data, key, password, firmware_version)
                    return encrypted_data_with_header, metadata
                else:
                    # AES-256保持原有格式
                    encrypted_data, iv = self._aes_encrypt(data, key)
                    return encrypted_data, {
                        'algorithm': algorithm.value,
                        'key_length': len(key),
                        'iv': iv.hex(),
                        'block_size': 16
                    }
            
            else:
                raise ValueError(f"不支持的加密算法: {algorithm}")
                
        except Exception as e:
            logger.error(f"固件加密失败: {e}")
            raise
    
    def decrypt_firmware(self, 
                        encrypted_data: bytes,
                        algorithm: EncryptionType,
                        key: bytes,
                        metadata: Dict[str, Any] = None) -> bytes:
        """
        解密固件数据
        
        Args:
            encrypted_data: 加密的数据
            algorithm: 加密算法
            key: 解密密钥
            metadata: 加密元数据
        
        Returns:
            解密后的数据
        """
        try:
            if algorithm == EncryptionType.NONE:
                return encrypted_data
            
            elif algorithm == EncryptionType.XOR:
                return self._xor_decrypt(encrypted_data, key)
            
            elif algorithm in [EncryptionType.AES_128_CBC, EncryptionType.AES_256_CBC]:
                if not metadata or 'iv' not in metadata:
                    raise ValueError("AES解密需要IV参数")
                
                iv = bytes.fromhex(metadata['iv'])
                return self._aes_decrypt(encrypted_data, key, iv)
            
            else:
                raise ValueError(f"不支持的加密算法: {algorithm}")
                
        except Exception as e:
            logger.error(f"固件解密失败: {e}")
            raise
    
    def _xor_encrypt(self, data: bytes, key: bytes) -> bytes:
        """XOR加密"""
        if not key:
            return data
        
        result = bytearray()
        key_len = len(key)
        
        for i, byte in enumerate(data):
            result.append(byte ^ key[i % key_len])
        
        return bytes(result)
    
    def _xor_decrypt(self, encrypted_data: bytes, key: bytes) -> bytes:
        """XOR解密 (与加密相同)"""
        return self._xor_encrypt(encrypted_data, key)
    
    def _aes_encrypt(self, data: bytes, key: bytes) -> Tuple[bytes, bytes]:
        """AES-CBC加密"""
        # 生成随机IV
        iv = os.urandom(16)
        
        # PKCS7填充
        padder = padding.PKCS7(128).padder()
        padded_data = padder.update(data)
        padded_data += padder.finalize()
        
        # AES-CBC加密
        cipher = Cipher(algorithms.AES(key), modes.CBC(iv))
        encryptor = cipher.encryptor()
        encrypted_data = encryptor.update(padded_data) + encryptor.finalize()
        
        return encrypted_data, iv
    
    def _aes_decrypt(self, encrypted_data: bytes, key: bytes, iv: bytes) -> bytes:
        """AES-CBC解密"""
        # AES-CBC解密
        cipher = Cipher(algorithms.AES(key), modes.CBC(iv))
        decryptor = cipher.decryptor()
        padded_data = decryptor.update(encrypted_data) + decryptor.finalize()
        
        # 去除PKCS7填充
        unpadder = padding.PKCS7(128).unpadder()
        data = unpadder.update(padded_data)
        data += unpadder.finalize()
        
        return data
    
    def _aes_encrypt_stm32_format(self, data: bytes, key: bytes, password: str, firmware_version: str = None) -> Tuple[bytes, Dict[str, Any]]:
        """
        STM32格式的AES加密 - 生成包含固件头的完整格式
        与单片机端firmware_aes_header_t结构兼容
        """
        import struct
        import hashlib
        
        # 生成随机IV
        iv = os.urandom(16)
        
        # PKCS7填充
        padder = padding.PKCS7(128).padder()
        padded_data = padder.update(data)
        padded_data += padder.finalize()
        
        # AES-CBC加密
        cipher = Cipher(algorithms.AES(key), modes.CBC(iv))
        encryptor = cipher.encryptor()
        encrypted_data = encryptor.update(padded_data) + encryptor.finalize()
        
        # 计算CRC32校验值（使用STM32兼容算法）
        firmware_crc32 = CryptoManager._calculate_crc32_stm32(data)
        encrypted_crc32 = CryptoManager._calculate_crc32_stm32(encrypted_data)
        
        # 调试信息：显示原始数据的前16字节和CRC32
        logger.info(f"Original firmware first 16 bytes: {data[:16].hex().upper()}")
        logger.info(f"Original firmware CRC32: 0x{firmware_crc32:08X}")
        
        # 生成密钥哈希
        key_hash = hashlib.md5(key).digest()
        
        # 构建固件头（64字节）
        header = struct.pack('<I', 0x41455331)  # magic = "AES1"
        header += struct.pack('<I', 0x01)       # header_version
        header += struct.pack('<I', len(data))  # firmware_size
        header += struct.pack('<I', len(encrypted_data))  # encrypted_size
        header += struct.pack('<I', firmware_crc32)  # crc32
        header += struct.pack('<I', encrypted_crc32)  # encrypted_crc32
        header += iv  # iv (16 bytes)
        header += key_hash  # key_hash (16 bytes)
        # fw_version (8 bytes) - 解析固件版本
        major, minor, patch, build = self._parse_firmware_version(firmware_version)
        header += struct.pack('<HHHH', major, minor, patch, build)
        
        # 完整的固件文件：头部 + 加密数据
        complete_firmware = header + encrypted_data
        
        return complete_firmware, {
            'algorithm': 'aes-128-cbc',
            'key_length': len(key),
            'iv': iv.hex(),
            'block_size': 16,
            'firmware_size': len(data),
            'encrypted_size': len(encrypted_data),
            'header_size': len(header)
        }
    
    @staticmethod
    def calculate_checksum(data: bytes, algorithm: str = 'md5') -> str:
        """计算数据校验和"""
        if algorithm.lower() == 'md5':
            return hashlib.md5(data).hexdigest()
        elif algorithm.lower() == 'sha1':
            return hashlib.sha1(data).hexdigest()
        elif algorithm.lower() == 'sha256':
            return hashlib.sha256(data).hexdigest()
        else:
            raise ValueError(f"不支持的校验算法: {algorithm}")
    
    @staticmethod
    def verify_checksum(data: bytes, expected_checksum: str, algorithm: str = 'md5') -> bool:
        """验证数据校验和"""
        try:
            actual_checksum = CryptoManager.calculate_checksum(data, algorithm)
            return actual_checksum.lower() == expected_checksum.lower()
        except Exception:
            return False
    
    def _derive_aes_key_stm32_compatible(self, password: str, salt: list = None) -> bytes:
        """
        STM32兼容的密钥派生算法
        与单片机端的firmware_aes_derive_key()保持一致
        """
        # 特殊处理：如果密码是32个字符的十六进制字符串，直接作为密钥使用
        if len(password) == 32:
            try:
                # 尝试将其作为十六进制解析
                key_bytes = bytes.fromhex(password)
                if len(key_bytes) == 16:  # AES-128密钥长度
                    return key_bytes
            except ValueError:
                pass  # 如果不是有效的十六进制，继续正常的密钥派生
        
        # 默认盐值（模拟STM32 unique ID）
        if salt is None:
            salt = [0x05D8FF35, 0x3132564E]  # 默认唯一ID
            
        # 创建32字节的临时密钥缓冲区
        temp_key = bytearray(32)
        
        # 复制密码（最多24字节）
        password_bytes = password.encode('utf-8')
        pwd_len = min(len(password_bytes), 24)
        temp_key[:pwd_len] = password_bytes[:pwd_len]
        
        # 混合盐值（STM32 unique ID）
        for i in range(min(2, len(salt))):
            salt_word = salt[i]
            temp_key[24 + i*4] = (salt_word >> 0) & 0xFF
            temp_key[24 + i*4 + 1] = (salt_word >> 8) & 0xFF
            temp_key[24 + i*4 + 2] = (salt_word >> 16) & 0xFF
            temp_key[24 + i*4 + 3] = (salt_word >> 24) & 0xFF
        
        # 与STM32端一致的密钥强化算法
        key = bytearray(16)
        for round_num in range(10):
            for i in range(16):
                key[i] = temp_key[i] ^ temp_key[i+16] ^ (round_num & 0xFF)
                # 增加随机性，避免全部相同的值
                key[i] ^= ((i + round_num) & 0xFF)
            
            # 更新temp_key用于下一轮
            temp_key[:16] = key
            temp_key[16:32] = key
        
        return bytes(key)
    
    def _parse_firmware_version(self, version_str: str) -> tuple:
        """
        解析固件版本字符串为(major, minor, patch, build)元组
        支持格式：v1.1.23.2025, 1.1.23.2025, v1.1.23, 1.1.23等
        """
        if not version_str:
            return (1, 0, 0, 1)  # 默认版本
        
        # 移除v前缀
        clean_version = version_str.lstrip('v')
        
        try:
            # 分割版本号
            parts = [int(x) for x in clean_version.split('.')]
            
            # 补齐到4段
            while len(parts) < 4:
                parts.append(0)
            
            # 只取前4段
            parts = parts[:4]
            
            return tuple(parts)
        except (ValueError, AttributeError):
            # 解析失败时返回默认版本
            return (1, 0, 0, 1)
    
    @staticmethod
    def _calculate_crc32_stm32(data: bytes) -> int:
        """
        计算STM32兼容的CRC32校验值
        与单片机端的CRC32算法保持一致
        """
        # STM32 CRC32查找表
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
        
        # 标准CRC32需要取反，与STM32端保持一致
        return (~crc) & 0xFFFFFFFF

    def get_supported_algorithms(self) -> List[Dict[str, Any]]:
        """获取支持的加密算法列表"""
        return [
            {
                'id': EncryptionType.NONE.value,
                'name': '无加密',
                'description': '不对固件进行加密',
                'key_required': False
            },
            {
                'id': EncryptionType.XOR.value,
                'name': 'XOR加密',
                'description': '简单的XOR异或加密',
                'key_required': True,
                'key_length': [1, 32]  # 1-32字节
            },
            {
                'id': EncryptionType.AES_128_CBC.value,
                'name': 'AES-128-CBC',
                'description': 'AES 128位密钥 CBC模式',
                'key_required': True,
                'key_length': [16]  # 16字节
            },
            {
                'id': EncryptionType.AES_256_CBC.value,
                'name': 'AES-256-CBC',
                'description': 'AES 256位密钥 CBC模式',
                'key_required': True,
                'key_length': [32]  # 32字节
            }
        ]

# 全局加密管理器实例
crypto_manager = CryptoManager()