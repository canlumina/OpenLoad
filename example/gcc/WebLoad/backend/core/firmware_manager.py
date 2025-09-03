#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import json
import time
from datetime import datetime
from typing import List, Dict, Optional, Any
from dataclasses import dataclass, asdict
from pathlib import Path
import logging

from .crypto import crypto_manager, EncryptionType

logger = logging.getLogger(__name__)

@dataclass
class FirmwareInfo:
    """固件信息数据类"""
    id: str
    filename: str
    original_filename: str
    version: str
    size: int
    checksum: str
    upload_time: str
    target_device: str
    
    # 加密相关
    is_encrypted: bool = False
    encryption_algorithm: str = "none"
    encryption_metadata: Dict[str, Any] = None
    
    # 元数据
    metadata: Dict[str, Any] = None
    
    def __post_init__(self):
        if self.encryption_metadata is None:
            self.encryption_metadata = {}
        if self.metadata is None:
            self.metadata = {}
    
    def to_dict(self) -> Dict[str, Any]:
        """转换为字典"""
        return asdict(self)
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'FirmwareInfo':
        """从字典创建实例"""
        return cls(**data)

class FirmwareManager:
    """固件管理器"""
    
    def __init__(self, upload_dir: str):
        self.upload_dir = Path(upload_dir)
        self.upload_dir.mkdir(exist_ok=True)
        
        # 元数据文件
        self.metadata_file = self.upload_dir / 'metadata.json'
        self.firmwares: Dict[str, FirmwareInfo] = {}
        
        self.logger = logging.getLogger(self.__class__.__name__)
        
        # 加载现有固件信息
        self._load_metadata()
    
    def _load_metadata(self):
        """加载固件元数据"""
        try:
            if self.metadata_file.exists():
                with open(self.metadata_file, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                    for fw_id, fw_data in data.items():
                        try:
                            self.firmwares[fw_id] = FirmwareInfo.from_dict(fw_data)
                        except Exception as e:
                            logger.warning(f"加载固件 {fw_id} 元数据失败: {e}")
                            
                logger.info(f"加载了 {len(self.firmwares)} 个固件的元数据")
            else:
                # 扫描现有固件文件
                self._scan_existing_files()
                
        except Exception as e:
            logger.error(f"加载固件元数据失败: {e}")
            self.firmwares = {}
    
    def _save_metadata(self):
        """保存固件元数据"""
        try:
            data = {fw_id: fw.to_dict() for fw_id, fw in self.firmwares.items()}
            with open(self.metadata_file, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=2, ensure_ascii=False)
        except Exception as e:
            logger.error(f"保存固件元数据失败: {e}")
    
    def _scan_existing_files(self):
        """扫描现有固件文件并生成元数据"""
        for file_path in self.upload_dir.glob('*.bin'):
            if file_path.name != 'metadata.json':
                try:
                    firmware = self._create_firmware_from_file(file_path)
                    self.firmwares[firmware.id] = firmware
                except Exception as e:
                    logger.warning(f"扫描文件 {file_path.name} 失败: {e}")
        
        if self.firmwares:
            self._save_metadata()
    
    def _create_firmware_from_file(self, file_path: Path) -> FirmwareInfo:
        """从文件创建固件信息"""
        stat = file_path.stat()
        
        # 读取文件内容计算校验和
        with open(file_path, 'rb') as f:
            content = f.read()
            checksum = crypto_manager.calculate_checksum(content)
        
        # 生成唯一ID
        firmware_id = f"fw_{int(time.time())}_{file_path.stem}"
        
        # 尝试从文件名提取版本信息
        version = self._extract_version_from_filename(file_path.name)
        
        firmware = FirmwareInfo(
            id=firmware_id,
            filename=file_path.name,
            original_filename=file_path.name,
            version=version,
            size=stat.st_size,
            checksum=checksum,
            upload_time=datetime.fromtimestamp(stat.st_ctime).isoformat(),
            target_device="STM32F103ZET6",  # 默认目标设备
            metadata={
                'scanned': True,
                'scan_time': datetime.now().isoformat()
            }
        )
        
        return firmware
    
    def _extract_version_from_filename(self, filename: str) -> str:
        """从文件名提取版本号"""
        import re
        
        # 标准版本号模式: v2.1.5.2024 (Major.Minor.Patch.Build)
        patterns = [
            r'v?(\d+\.\d+\.\d+\.\d+)',  # v2.1.5.2024
            r'v?(\d+\.\d+\.\d+)',       # v1.2.3
            r'v?(\d+\.\d+)',            # v1.2
            r'v?(\d+)',                 # v1
        ]
        
        for pattern in patterns:
            match = re.search(pattern, filename, re.IGNORECASE)
            if match:
                version = match.group(1)
                # 标准化为4段版本号
                return self._normalize_version(version)
        
        # 如果没有找到版本号，尝试从时间戳生成
        timestamp_match = re.search(r'(\d{8})', filename)  # 20240902
        if timestamp_match:
            timestamp = timestamp_match.group(1)
            return f"1.0.0.{timestamp}"
        
        return "1.0.0.0"
    
    def _normalize_version(self, version: str) -> str:
        """标准化版本号为4段格式"""
        parts = version.split('.')
        
        # 补齐到4段
        while len(parts) < 4:
            if len(parts) == 3:
                # 如果是3段，最后一段作为build号
                parts.append(str(int(time.time()) % 10000))
            else:
                parts.append('0')
        
        # 只取前4段
        parts = parts[:4]
        
        return '.'.join(parts)
    
    def _compare_versions(self, version1: str, version2: str) -> int:
        """
        比较版本号
        返回值: 1 if version1 > version2, -1 if version1 < version2, 0 if equal
        """
        def version_tuple(version):
            try:
                # 移除v前缀
                clean_version = version.lstrip('v')
                return tuple(int(x) for x in clean_version.split('.'))
            except:
                return (0, 0, 0, 0)
        
        v1_tuple = version_tuple(version1)
        v2_tuple = version_tuple(version2)
        
        if v1_tuple > v2_tuple:
            return 1
        elif v1_tuple < v2_tuple:
            return -1
        else:
            return 0
    
    def get_latest_version_firmware(self, target_device: str = None) -> Optional[FirmwareInfo]:
        """获取最新版本的固件"""
        firmwares = self.list_firmwares(target_device=target_device)
        if not firmwares:
            return None
        
        latest_firmware = firmwares[0]
        for firmware in firmwares[1:]:
            if self._compare_versions(firmware.version, latest_firmware.version) > 0:
                latest_firmware = firmware
        
        return latest_firmware
    
    def add_firmware(self, 
                    file_path: str,
                    original_filename: str,
                    version: str = None,
                    target_device: str = "STM32F103ZET6",
                    metadata: Dict[str, Any] = None) -> FirmwareInfo:
        """添加固件"""
        try:
            file_path = Path(file_path)
            if not file_path.exists():
                raise FileNotFoundError(f"固件文件不存在: {file_path}")
            
            # 读取文件内容
            with open(file_path, 'rb') as f:
                content = f.read()
            
            # 计算校验和
            checksum = crypto_manager.calculate_checksum(content)
            
            # 生成唯一ID
            firmware_id = f"fw_{int(time.time())}_{file_path.stem}"
            
            # 如果未指定版本，尝试从文件名提取
            if not version:
                version = self._extract_version_from_filename(original_filename)
            
            firmware = FirmwareInfo(
                id=firmware_id,
                filename=file_path.name,
                original_filename=original_filename,
                version=version,
                size=file_path.stat().st_size,
                checksum=checksum,
                upload_time=datetime.now().isoformat(),
                target_device=target_device,
                metadata=metadata or {}
            )
            
            # 保存固件信息
            self.firmwares[firmware_id] = firmware
            self._save_metadata()
            
            logger.info(f"添加固件成功: {firmware.original_filename} -> {firmware_id}")
            return firmware
            
        except Exception as e:
            logger.error(f"添加固件失败: {e}")
            raise
    
    def remove_firmware(self, firmware_id: str) -> bool:
        """删除固件"""
        try:
            if firmware_id not in self.firmwares:
                return False
            
            firmware = self.firmwares[firmware_id]
            file_path = self.upload_dir / firmware.filename
            
            # 删除文件
            if file_path.exists():
                file_path.unlink()
            
            # 删除元数据
            del self.firmwares[firmware_id]
            self._save_metadata()
            
            logger.info(f"删除固件成功: {firmware_id}")
            return True
            
        except Exception as e:
            logger.error(f"删除固件失败: {e}")
            return False
    
    def get_firmware(self, firmware_id: str) -> Optional[FirmwareInfo]:
        """获取固件信息"""
        return self.firmwares.get(firmware_id)
    
    def get_firmware_by_filename(self, filename: str) -> Optional[FirmwareInfo]:
        """根据文件名获取固件信息"""
        for firmware in self.firmwares.values():
            if firmware.filename == filename or firmware.original_filename == filename:
                return firmware
        return None
    
    def list_firmwares(self, 
                      target_device: str = None,
                      encrypted_only: bool = None) -> List[FirmwareInfo]:
        """获取固件列表"""
        firmwares = list(self.firmwares.values())
        
        # 过滤条件
        if target_device:
            firmwares = [fw for fw in firmwares if fw.target_device == target_device]
        
        if encrypted_only is not None:
            firmwares = [fw for fw in firmwares if fw.is_encrypted == encrypted_only]
        
        # 按版本号排序 (最新版本在前)，版本号相同则按上传时间排序
        def version_sort_key(firmware):
            try:
                # 解析版本号为元组用于比较
                version = firmware.version.lstrip('v')
                parts = [int(x) for x in version.split('.')]
                # 确保是4段版本号，不足的补0
                while len(parts) < 4:
                    parts.append(0)
                return (tuple(parts[:4]), firmware.upload_time)
            except:
                # 版本号解析失败时，使用上传时间排序
                return ((0, 0, 0, 0), firmware.upload_time)
        
        firmwares.sort(key=version_sort_key, reverse=True)
        
        return firmwares
    
    def get_firmware_path(self, firmware_id: str) -> Optional[Path]:
        """获取固件文件路径"""
        firmware = self.get_firmware(firmware_id)
        if firmware:
            return self.upload_dir / firmware.filename
        return None
    
    def read_firmware_content(self, firmware_id: str) -> Optional[bytes]:
        """读取固件内容"""
        file_path = self.get_firmware_path(firmware_id)
        if file_path and file_path.exists():
            try:
                with open(file_path, 'rb') as f:
                    return f.read()
            except Exception as e:
                logger.error(f"读取固件内容失败: {e}")
        return None
    
    def encrypt_firmware(self, 
                        firmware_id: str,
                        algorithm: EncryptionType,
                        password: str = None,
                        key: bytes = None) -> bool:
        """加密固件"""
        try:
            firmware = self.get_firmware(firmware_id)
            if not firmware:
                raise ValueError(f"固件不存在: {firmware_id}")
            
            # 读取原始固件内容
            content = self.read_firmware_content(firmware_id)
            if not content:
                raise ValueError("读取固件内容失败")
            
            # 如果已经加密，先解密
            if firmware.is_encrypted:
                logger.warning(f"固件 {firmware_id} 已加密，跳过")
                return True
            
            # 加密固件，传入版本信息用于AES固件头
            firmware_version = firmware.version
            encrypted_content, encryption_metadata = crypto_manager.encrypt_firmware(
                content, algorithm, key, password, firmware_version
            )
            
            # 写入加密后的固件
            file_path = self.get_firmware_path(firmware_id)
            with open(file_path, 'wb') as f:
                f.write(encrypted_content)
            
            # 更新固件信息
            firmware.is_encrypted = True
            firmware.encryption_algorithm = algorithm.value
            
            # 合并加密元数据，确保不覆盖密码
            if firmware.encryption_metadata is None:
                firmware.encryption_metadata = {}
            firmware.encryption_metadata.update(encryption_metadata)
            
            # 将密码保存到加密元数据中（便于OTA下载时获取）
            if password:
                firmware.encryption_metadata['password'] = password
                logger.info(f"Password saved to metadata: {password}")
            elif key:
                # 当使用手动密钥时，将密钥的十六进制表示作为密码保存
                # 这样单片机端可以使用这个"密码"重新生成相同的密钥
                key_as_password = key.hex()
                firmware.encryption_metadata['password'] = key_as_password
                logger.info(f"Manual key saved as password: {key_as_password}")
            
            firmware.size = len(encrypted_content)
            firmware.checksum = crypto_manager.calculate_checksum(encrypted_content)
            
            self._save_metadata()
            
            logger.info(f"固件加密成功: {firmware_id} ({algorithm.value})")
            return True
            
        except Exception as e:
            logger.error(f"固件加密失败: {e}")
            return False
    
    def get_storage_info(self) -> Dict[str, Any]:
        """获取存储信息"""
        try:
            total_size = sum(fw.size for fw in self.firmwares.values())
            
            storage_info = {
                'firmware_count': len(self.firmwares),
                'total_firmware_size': total_size,
                'upload_directory': str(self.upload_dir)
            }
            
            # 获取磁盘使用情况 (跨平台兼容)
            try:
                import shutil
                total_space, used_space, free_space = shutil.disk_usage(str(self.upload_dir))
                storage_info.update({
                    'disk_free_space': free_space,
                    'disk_total_space': total_space,
                    'disk_used_space': used_space,
                    'disk_usage_percent': (used_space / total_space * 100) if total_space > 0 else 0
                })
            except Exception as disk_error:
                logger.warning(f"无法获取磁盘使用信息: {disk_error}")
            
            return storage_info
            
        except Exception as e:
            logger.error(f"获取存储信息失败: {e}")
            return {
                'firmware_count': len(self.firmwares),
                'total_firmware_size': sum(fw.size for fw in self.firmwares.values()),
                'upload_directory': str(self.upload_dir)
            }