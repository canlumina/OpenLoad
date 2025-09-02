#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import time
from flask import request, jsonify
from . import api_v1
from ..core.serial_manager import serial_manager

@api_v1.route('/devices/ports', methods=['GET'])
def get_serial_ports():
    """获取可用串口列表"""
    try:
        ports = serial_manager.get_available_ports()
        return jsonify({
            'success': True,
            'data': {
                'ports': ports,
                'count': len(ports)
            }
        })
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@api_v1.route('/devices', methods=['GET'])
def get_devices():
    """获取设备列表"""
    try:
        devices = []
        for port, device in serial_manager.devices.items():
            device_info = {
                'id': f"device_{port.replace('/', '_').replace('\\', '_')}",
                'port': port,
                'baudrate': device.baudrate,
                'connected': device.is_connected,
                'type': 'serial',
                'name': f"STM32 ({port})"
            }
            devices.append(device_info)
        
        return jsonify({
            'success': True,
            'data': {
                'devices': devices,
                'count': len(devices)
            }
        })
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@api_v1.route('/devices/connect', methods=['POST'])
def connect_device():
    """连接设备"""
    try:
        data = request.get_json()
        if not data:
            return jsonify({
                'success': False,
                'error': '请求数据为空'
            }), 400
        
        port = data.get('port')
        baudrate = data.get('baudrate', 115200)
        
        if not port:
            return jsonify({
                'success': False,
                'error': '端口参数不能为空'
            }), 400
        
        # 创建或获取设备
        device = serial_manager.create_device(port, baudrate)
        
        # 连接设备
        if device.connect():
            return jsonify({
                'success': True,
                'message': f'设备 {port} 连接成功',
                'data': {
                    'device_id': f"device_{port.replace('/', '_').replace('\\', '_')}",
                    'port': port,
                    'baudrate': baudrate,
                    'connected': True
                }
            })
        else:
            return jsonify({
                'success': False,
                'error': f'设备 {port} 连接失败'
            }), 400
            
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@api_v1.route('/devices/disconnect', methods=['POST'])
def disconnect_device():
    """断开设备"""
    try:
        data = request.get_json()
        if not data:
            return jsonify({
                'success': False,
                'error': '请求数据为空'
            }), 400
        
        port = data.get('port')
        if not port:
            return jsonify({
                'success': False,
                'error': '端口参数不能为空'
            }), 400
        
        device = serial_manager.get_device(port)
        if device:
            device.disconnect()
            return jsonify({
                'success': True,
                'message': f'设备 {port} 已断开'
            })
        else:
            return jsonify({
                'success': False,
                'error': f'设备 {port} 不存在'
            }), 404
            
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@api_v1.route('/devices/<path:device_id>/status', methods=['GET'])
def get_device_status(device_id):
    """获取设备状态"""
    try:
        # 从device_id提取端口
        port = device_id.replace('device_', '').replace('_', '/')
        if '\\' in device_id:
            port = device_id.replace('device_', '').replace('_', '\\')
        
        device = serial_manager.get_device(port)
        if not device:
            return jsonify({
                'success': False,
                'error': '设备不存在'
            }), 404
        
        return jsonify({
            'success': True,
            'data': {
                'device_id': device_id,
                'port': port,
                'connected': device.is_connected,
                'baudrate': device.baudrate
            }
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@api_v1.route('/devices/<path:device_id>/command', methods=['POST'])
def send_device_command(device_id):
    """发送设备命令"""
    try:
        data = request.get_json()
        if not data:
            return jsonify({
                'success': False,
                'error': '请求数据为空'
            }), 400
        
        command = data.get('command', '')
        if not command:
            return jsonify({
                'success': False,
                'error': '命令参数不能为空'
            }), 400
        
        # 从device_id提取端口
        port = device_id.replace('device_', '').replace('_', '/')
        if '\\' in device_id:
            port = device_id.replace('device_', '').replace('_', '\\')
            
        device = serial_manager.get_device(port)
        if not device:
            return jsonify({
                'success': False,
                'error': '设备不存在'
            }), 404
        
        if not device.is_connected:
            return jsonify({
                'success': False,
                'error': '设备未连接'
            }), 400
        
        # 发送命令
        if device.send_command(command):
            # 等待响应
            time.sleep(0.5)
            messages = device.get_messages()
            
            return jsonify({
                'success': True,
                'message': '命令发送成功',
                'data': {
                    'command': command,
                    'response': messages
                }
            })
        else:
            return jsonify({
                'success': False,
                'error': '命令发送失败'
            }), 400
            
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@api_v1.route('/devices/<path:device_id>/messages', methods=['GET'])
def get_device_messages(device_id):
    """获取设备消息"""
    try:
        # 从device_id提取端口
        port = device_id.replace('device_', '').replace('_', '/')
        if '\\' in device_id:
            port = device_id.replace('device_', '').replace('_', '\\')
            
        device = serial_manager.get_device(port)
        if not device:
            return jsonify({
                'success': False,
                'error': '设备不存在'
            }), 404
        
        messages = device.get_messages()
        
        return jsonify({
            'success': True,
            'data': {
                'messages': messages,
                'count': len(messages)
            }
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@api_v1.route('/devices/<path:device_id>/info', methods=['GET'])
def get_device_info(device_id):
    """获取设备信息"""
    try:
        # 从device_id提取端口
        port = device_id.replace('device_', '').replace('_', '/')
        if '\\' in device_id:
            port = device_id.replace('device_', '').replace('_', '\\')
            
        device = serial_manager.get_device(port)
        if not device:
            return jsonify({
                'success': False,
                'error': '设备不存在'
            }), 404
        
        if not device.is_connected:
            return jsonify({
                'success': False,
                'error': '设备未连接'
            }), 400
        
        # 发送信息查询命令
        device.send_command('i')
        time.sleep(1)  # 等待响应
        
        messages = device.get_messages()
        
        # 解析设备信息（这里需要根据实际bootloader响应格式调整）
        device_info = {
            'mcu': 'STM32F103ZET6',
            'bootloader_version': '2.0',
            'flash_size': '512KB',
            'bootloader_size': '64KB',
            'app_size': '448KB',
            'status': 'bootloader' if any('bootloader' in msg.lower() for msg in messages) else 'unknown',
            'raw_response': messages
        }
        
        return jsonify({
            'success': True,
            'data': device_info
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

# Bootloader特定命令
@api_v1.route('/devices/<path:device_id>/bootloader/<command>', methods=['POST'])
def bootloader_command(device_id, command):
    """执行bootloader命令"""
    try:
        # 从device_id提取端口
        port = device_id.replace('device_', '').replace('_', '/')
        if '\\' in device_id:
            port = device_id.replace('device_', '').replace('_', '\\')
            
        device = serial_manager.get_device(port)
        if not device:
            return jsonify({
                'success': False,
                'error': '设备不存在'
            }), 404
        
        if not device.is_connected:
            return jsonify({
                'success': False,
                'error': '设备未连接'
            }), 400
        
        # 命令映射
        command_map = {
            'help': 'h',
            'info': 'i',
            'erase': 'e',
            'reset': 'r',
            'jump': 'j',
            'update': 'u',
            'wifi': 'w',
            'backup': 'xb',
            'restore': 'xr'
        }
        
        actual_command = command_map.get(command, command)
        
        if device.send_command(actual_command):
            time.sleep(0.5)
            messages = device.get_messages()
            
            return jsonify({
                'success': True,
                'message': f'Bootloader命令 {command} 执行成功',
                'data': {
                    'command': command,
                    'actual_command': actual_command,
                    'response': messages
                }
            })
        else:
            return jsonify({
                'success': False,
                'error': '命令发送失败'
            }), 400
            
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500