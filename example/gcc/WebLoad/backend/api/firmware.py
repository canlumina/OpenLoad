#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import json
from flask import request, jsonify, send_file
from werkzeug.utils import secure_filename
from datetime import datetime

from . import api_v1
from ..core.firmware_manager import FirmwareManager
from flask import current_app

# 获取固件管理器实例的辅助函数
def get_firmware_manager():
    if not hasattr(current_app, 'firmware_manager'):
        current_app.firmware_manager = FirmwareManager(current_app.config['UPLOAD_FOLDER'])
    return current_app.firmware_manager
from ..core.crypto import EncryptionType

@api_v1.route('/firmwares', methods=['GET'])
def get_firmwares():
    """获取固件列表"""
    try:
        # 获取查询参数
        target_device = request.args.get('target_device')
        encrypted_only = request.args.get('encrypted_only')
        
        if encrypted_only is not None:
            encrypted_only = encrypted_only.lower() == 'true'
        
        firmwares = get_firmware_manager().list_firmwares(
            target_device=target_device,
            encrypted_only=encrypted_only
        )
        
        # 转换为字典列表并标记最新版本
        firmware_list = []
        latest_firmware = get_firmware_manager().get_latest_version_firmware(target_device)
        
        for fw in firmwares:
            fw_dict = fw.to_dict()
            # 标记是否为最新版本
            fw_dict['is_latest'] = (latest_firmware and fw.id == latest_firmware.id)
            firmware_list.append(fw_dict)
        
        return jsonify({
            'success': True,
            'data': {
                'firmwares': firmware_list,
                'count': len(firmware_list),
                'latest_firmware_id': latest_firmware.id if latest_firmware else None
            }
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@api_v1.route('/firmwares', methods=['POST'])
def upload_firmware():
    """上传固件"""
    try:
        # 检查文件
        if 'firmware' not in request.files:
            return jsonify({
                'success': False,
                'error': '未选择固件文件'
            }), 400
        
        file = request.files['firmware']
        if file.filename == '':
            return jsonify({
                'success': False,
                'error': '文件名为空'
            }), 400
        
        # 检查文件扩展名
        if not file.filename.lower().endswith('.bin'):
            return jsonify({
                'success': False,
                'error': '只支持.bin格式的固件文件'
            }), 400
        
        # 获取额外参数
        version = request.form.get('version', '').strip()
        target_device = request.form.get('target_device', 'STM32F103ZET6')
        
        # 验证版本号格式（如果用户提供了版本号）
        if version and version != 'auto':
            import re
            # 标准版本号格式：v2.1.5.2024 或 2.1.5.2024
            version_pattern = r'^v?\d+\.\d+\.\d+\.\d+$'
            if not re.match(version_pattern, version):
                return jsonify({
                    'success': False,
                    'error': '版本号格式不正确。请使用标准格式：v主版本.次版本.修订版.构建版 (例如: v2.1.5.2024)'
                }), 400
            
            # 确保版本号以v开头
            if not version.startswith('v'):
                version = 'v' + version
        
        # 保存文件
        filename = secure_filename(file.filename)
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        safe_filename = f"{timestamp}_{filename}"
        
        upload_path = get_firmware_manager().upload_dir / safe_filename
        file.save(str(upload_path))
        
        # 添加到固件管理器
        firmware = get_firmware_manager().add_firmware(
            file_path=str(upload_path),
            original_filename=filename,
            version=version,
            target_device=target_device,
            metadata={
                'upload_source': 'web_api',
                'upload_timestamp': datetime.now().isoformat()
            }
        )
        
        return jsonify({
            'success': True,
            'message': '固件上传成功',
            'data': firmware.to_dict()
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@api_v1.route('/firmwares/<firmware_id>', methods=['GET'])
def get_firmware(firmware_id):
    """获取固件详情"""
    try:
        firmware = get_firmware_manager().get_firmware(firmware_id)
        if not firmware:
            return jsonify({
                'success': False,
                'error': '固件不存在'
            }), 404
        
        return jsonify({
            'success': True,
            'data': firmware.to_dict()
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@api_v1.route('/firmwares/<firmware_id>', methods=['DELETE'])
def delete_firmware(firmware_id):
    """删除固件"""
    try:
        if get_firmware_manager().remove_firmware(firmware_id):
            return jsonify({
                'success': True,
                'message': '固件删除成功'
            })
        else:
            return jsonify({
                'success': False,
                'error': '固件不存在或删除失败'
            }), 404
            
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@api_v1.route('/firmwares/<firmware_id>/download', methods=['GET'])
def download_firmware(firmware_id):
    """下载固件"""
    try:
        firmware = get_firmware_manager().get_firmware(firmware_id)
        if not firmware:
            return jsonify({
                'success': False,
                'error': '固件不存在'
            }), 404
        
        file_path = get_firmware_manager().get_firmware_path(firmware_id)
        if not file_path or not file_path.exists():
            return jsonify({
                'success': False,
                'error': '固件文件不存在'
            }), 404
        
        return send_file(
            str(file_path),
            as_attachment=True,
            download_name=firmware.original_filename
        )
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@api_v1.route('/firmwares/<firmware_id>/encrypt', methods=['POST'])
def encrypt_firmware(firmware_id):
    """加密固件"""
    try:
        data = request.get_json()
        if not data:
            return jsonify({
                'success': False,
                'error': '请求数据为空'
            }), 400
        
        algorithm_str = data.get('algorithm', 'none')
        password = data.get('password')
        key_hex = data.get('key')  # 十六进制密钥
        
        # 验证加密算法
        try:
            algorithm = EncryptionType(algorithm_str)
        except ValueError:
            return jsonify({
                'success': False,
                'error': f'不支持的加密算法: {algorithm_str}'
            }), 400
        
        # 处理密钥
        key_bytes = None
        if key_hex:
            try:
                key_bytes = bytes.fromhex(key_hex)
            except ValueError:
                return jsonify({
                    'success': False,
                    'error': '密钥格式错误，请提供十六进制格式的密钥'
                }), 400
        
        # 执行加密
        if get_firmware_manager().encrypt_firmware(firmware_id, algorithm, password, key_bytes):
            firmware = get_firmware_manager().get_firmware(firmware_id)
            return jsonify({
                'success': True,
                'message': '固件加密成功',
                'data': firmware.to_dict() if firmware else None
            })
        else:
            return jsonify({
                'success': False,
                'error': '固件加密失败'
            }), 500
            
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@api_v1.route('/firmwares/<firmware_id>/deploy', methods=['POST'])
def deploy_firmware(firmware_id):
    """部署固件到设备"""
    try:
        data = request.get_json()
        if not data:
            return jsonify({
                'success': False,
                'error': '请求数据为空'
            }), 400
        
        device_id = data.get('device_id')
        method = data.get('method', 'xmodem')  # xmodem or ota
        target = data.get('target', 'internal')  # internal or external
        
        if not device_id:
            return jsonify({
                'success': False,
                'error': '设备ID不能为空'
            }), 400
        
        # 检查固件是否存在
        firmware = get_firmware_manager().get_firmware(firmware_id)
        if not firmware:
            return jsonify({
                'success': False,
                'error': '固件不存在'
            }), 404
        
        # 这里应该实现实际的固件部署逻辑
        # 暂时返回成功状态，后续可以集成XMODEM/OTA传输
        
        return jsonify({
            'success': True,
            'message': '固件部署任务已启动',
            'data': {
                'firmware_id': firmware_id,
                'device_id': device_id,
                'method': method,
                'target': target,
                'status': 'deploying'
            }
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@api_v1.route('/firmwares/<firmware_id>/deploy/status', methods=['GET'])
def get_deploy_status(firmware_id):
    """获取部署状态"""
    try:
        # 暂时返回模拟数据，后续可以实现真实的部署状态追踪
        return jsonify({
            'success': True,
            'data': {
                'firmware_id': firmware_id,
                'status': 'idle',  # idle, deploying, completed, failed
                'progress': 0,
                'message': '等待部署',
                'start_time': None,
                'end_time': None
            }
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@api_v1.route('/firmwares/storage', methods=['GET'])
def get_storage_info():
    """获取存储信息"""
    try:
        storage_info = get_firmware_manager().get_storage_info()
        
        return jsonify({
            'success': True,
            'data': storage_info
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

# 批量操作API
@api_v1.route('/firmwares/batch/delete', methods=['POST'])
def batch_delete_firmwares():
    """批量删除固件"""
    try:
        data = request.get_json()
        if not data:
            return jsonify({
                'success': False,
                'error': '请求数据为空'
            }), 400
        
        firmware_ids = data.get('firmware_ids', [])
        if not firmware_ids:
            return jsonify({
                'success': False,
                'error': '固件ID列表不能为空'
            }), 400
        
        success_count = 0
        failed_ids = []
        
        for firmware_id in firmware_ids:
            if get_firmware_manager().remove_firmware(firmware_id):
                success_count += 1
            else:
                failed_ids.append(firmware_id)
        
        return jsonify({
            'success': True,
            'message': f'批量删除完成，成功: {success_count}, 失败: {len(failed_ids)}',
            'data': {
                'success_count': success_count,
                'failed_count': len(failed_ids),
                'failed_ids': failed_ids
            }
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500