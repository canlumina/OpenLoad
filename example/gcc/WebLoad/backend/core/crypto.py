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
    
    @staticmethod
    def generate_key(algorithm: EncryptionType, password: str = None) -> bytes:
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
                # 使用PBKDF2从密码生成密钥
                salt = b'openload_salt_16'  # 固定salt，实际应用中应该随机生成
                kdf = PBKDF2HMAC(
                    algorithm=hashes.SHA256(),
                    length=16,
                    salt=salt,
                    iterations=100000,
                )
                return kdf.derive(password.encode('utf-8'))
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
                        password: str = None) -> Tuple[bytes, Dict[str, Any]]:
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