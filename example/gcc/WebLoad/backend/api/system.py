#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import platform
import psutil
from datetime import datetime
from flask import jsonify

from . import api_v1
from ..core.serial_manager import serial_manager
from ..core.firmware_manager import FirmwareManager
from flask import current_app

def get_firmware_manager():
    if not hasattr(current_app, 'firmware_manager'):
        current_app.firmware_manager = FirmwareManager(current_app.config['UPLOAD_FOLDER'])
    return current_app.firmware_manager

@api_v1.route('/system/info', methods=['GET'])
def get_system_info():
    """获取系统信息"""
    try:
        # 系统基本信息
        system_info = {
            'platform': platform.system(),
            'platform_release': platform.release(),
            'platform_version': platform.version(),
            'architecture': platform.machine(),
            'processor': platform.processor(),
            'python_version': platform.python_version(),
            'hostname': platform.node()
        }
        
        # 系统资源信息
        memory = psutil.virtual_memory()
        disk = psutil.disk_usage('/')
        
        resource_info = {
            'cpu_count': psutil.cpu_count(),
            'cpu_percent': psutil.cpu_percent(interval=1),
            'memory_total': memory.total,
            'memory_available': memory.available,
            'memory_percent': memory.percent,
            'disk_total': disk.total,
            'disk_used': disk.used,
            'disk_free': disk.free,
            'disk_percent': disk.percent
        }
        
        # 应用信息
        app_info = {
            'name': 'OpenLoad WebManager',
            'version': '2.0.0',
            'start_time': datetime.now().isoformat(),
            'python_executable': sys.executable
        }
        
        return jsonify({
            'success': True,
            'data': {
                'system': system_info,
                'resources': resource_info,
                'application': app_info
            }
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@api_v1.route('/system/health', methods=['GET'])
def get_health_status():
    """获取系统健康状态"""
    try:
        # 检查各个组件状态
        serial_status = {
            'total_devices': len(serial_manager.devices),
            'connected_devices': len(serial_manager.get_connected_devices()),
            'available_ports': len(serial_manager.get_available_ports())
        }
        
        firmware_status = {
            'total_firmwares': len(get_firmware_manager().list_firmwares()),
            'encrypted_firmwares': len(get_firmware_manager().list_firmwares(encrypted_only=True)),
            'storage_info': get_firmware_manager().get_storage_info()
        }
        
        # 系统资源检查
        memory = psutil.virtual_memory()
        disk = psutil.disk_usage('/')
        
        warnings = []
        if memory.percent > 90:
            warnings.append('内存使用率过高')
        if disk.percent > 90:
            warnings.append('磁盘空间不足')
        
        health_status = {
            'status': 'healthy' if not warnings else 'warning',
            'warnings': warnings,
            'last_check': datetime.now().isoformat()
        }
        
        return jsonify({
            'success': True,
            'data': {
                'health': health_status,
                'serial': serial_status,
                'firmware': firmware_status
            }
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@api_v1.route('/system/config', methods=['GET'])
def get_system_config():
    """获取系统配置"""
    try:
        config = {
            'upload_directory': str(get_firmware_manager().upload_dir),
            'max_firmware_size': 16 * 1024 * 1024,  # 16MB
            'supported_file_types': ['.bin'],
            'default_baudrate': 115200,
            'supported_baudrates': [9600, 19200, 38400, 57600, 115200, 230400, 460800],
            'api_version': 'v1',
            'features': {
                'serial_communication': True,
                'firmware_management': True,
                'encryption_support': True,
                'ota_updates': True,
                'xmodem_protocol': True
            }
        }
        
        return jsonify({
            'success': True,
            'data': config
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@api_v1.route('/system/logs', methods=['GET'])
def get_system_logs():
    """获取系统日志 (简化版)"""
    try:
        # 这里可以实现真实的日志读取
        # 暂时返回模拟数据
        logs = [
            {
                'timestamp': datetime.now().isoformat(),
                'level': 'INFO',
                'component': 'system',
                'message': '系统启动完成'
            },
            {
                'timestamp': datetime.now().isoformat(),
                'level': 'INFO',
                'component': 'serial_manager',
                'message': '串口管理器初始化完成'
            },
            {
                'timestamp': datetime.now().isoformat(),
                'level': 'INFO',
                'component': 'firmware_manager',
                'message': '固件管理器初始化完成'
            }
        ]
        
        return jsonify({
            'success': True,
            'data': {
                'logs': logs,
                'count': len(logs)
            }
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@api_v1.route('/system/stats', methods=['GET'])
def get_system_stats():
    """获取系统统计信息"""
    try:
        # 设备统计
        total_devices = len(serial_manager.devices)
        connected_devices = len(serial_manager.get_connected_devices())
        
        # 固件统计
        all_firmwares = get_firmware_manager().list_firmwares()
        total_firmwares = len(all_firmwares)
        encrypted_firmwares = len([fw for fw in all_firmwares if fw.is_encrypted])
        
        # 存储统计
        storage_info = get_firmware_manager().get_storage_info()
        
        stats = {
            'devices': {
                'total': total_devices,
                'connected': connected_devices,
                'connection_rate': (connected_devices / total_devices * 100) if total_devices > 0 else 0
            },
            'firmwares': {
                'total': total_firmwares,
                'encrypted': encrypted_firmwares,
                'encryption_rate': (encrypted_firmwares / total_firmwares * 100) if total_firmwares > 0 else 0,
                'total_size': storage_info.get('total_firmware_size', 0)
            },
            'storage': storage_info,
            'last_updated': datetime.now().isoformat()
        }
        
        return jsonify({
            'success': True,
            'data': stats
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@api_v1.route('/system/cleanup', methods=['POST'])
def cleanup_system():
    """系统清理"""
    try:
        # 断开所有串口连接
        serial_manager.disconnect_all()
        
        # 清理临时文件 (如果有的话)
        cleanup_count = 0
        
        return jsonify({
            'success': True,
            'message': '系统清理完成',
            'data': {
                'disconnected_devices': len(serial_manager.devices),
                'cleaned_files': cleanup_count
            }
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500