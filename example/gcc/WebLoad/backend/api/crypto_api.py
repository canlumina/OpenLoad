#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
from flask import request, jsonify

from . import api_v1
from ..core.crypto import crypto_manager, EncryptionType

@api_v1.route('/crypto/algorithms', methods=['GET'])
def get_supported_algorithms():
    """获取支持的加密算法"""
    try:
        algorithms = crypto_manager.get_supported_algorithms()
        
        return jsonify({
            'success': True,
            'data': {
                'algorithms': algorithms,
                'count': len(algorithms)
            }
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@api_v1.route('/crypto/key/generate', methods=['POST'])
def generate_key():
    """生成加密密钥"""
    try:
        data = request.get_json()
        if not data:
            return jsonify({
                'success': False,
                'error': '请求数据为空'
            }), 400
        
        algorithm_str = data.get('algorithm', 'aes-128-cbc')
        password = data.get('password')
        
        # 验证加密算法
        try:
            algorithm = EncryptionType(algorithm_str)
        except ValueError:
            return jsonify({
                'success': False,
                'error': f'不支持的加密算法: {algorithm_str}'
            }), 400
        
        # 生成密钥
        key = crypto_manager.generate_key(algorithm, password)
        
        return jsonify({
            'success': True,
            'message': '密钥生成成功',
            'data': {
                'algorithm': algorithm_str,
                'key': key.hex() if key else '',
                'key_length': len(key),
                'password_based': bool(password)
            }
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@api_v1.route('/crypto/encrypt', methods=['POST'])
def encrypt_data():
    """加密数据"""
    try:
        # 检查是否有文件上传
        if 'data' in request.files:
            # 文件上传方式
            file = request.files['data']
            if file.filename == '':
                return jsonify({
                    'success': False,
                    'error': '文件名为空'
                }), 400
            
            data_bytes = file.read()
            algorithm_str = request.form.get('algorithm', 'aes-128-cbc')
            password = request.form.get('password')
            key_hex = request.form.get('key')
        else:
            # JSON数据方式
            json_data = request.get_json()
            if not json_data:
                return jsonify({
                    'success': False,
                    'error': '请求数据为空'
                }), 400
            
            # 处理十六进制数据
            data_hex = json_data.get('data')
            if not data_hex:
                return jsonify({
                    'success': False,
                    'error': '数据不能为空'
                }), 400
            
            try:
                data_bytes = bytes.fromhex(data_hex)
            except ValueError:
                return jsonify({
                    'success': False,
                    'error': '数据格式错误，请提供十六进制格式的数据'
                }), 400
            
            algorithm_str = json_data.get('algorithm', 'aes-128-cbc')
            password = json_data.get('password')
            key_hex = json_data.get('key')
        
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
        try:
            encrypted_data, metadata = crypto_manager.encrypt_firmware(
                data_bytes, algorithm, key_bytes, password
            )
        except Exception as encrypt_error:
            import traceback
            return jsonify({
                'success': False,
                'error': f'加密失败: {str(encrypt_error)}',
                'traceback': traceback.format_exc()
            }), 500
        
        return jsonify({
            'success': True,
            'message': '数据加密成功',
            'data': {
                'original_size': len(data_bytes),
                'encrypted_size': len(encrypted_data),
                'encrypted_data': encrypted_data.hex(),
                'algorithm': algorithm_str,
                'metadata': metadata
            }
        })
        
    except Exception as e:
        import traceback
        return jsonify({
            'success': False,
            'error': str(e),
            'traceback': traceback.format_exc()
        }), 500

@api_v1.route('/crypto/decrypt', methods=['POST'])
def decrypt_data():
    """解密数据"""
    try:
        # 检查是否有文件上传
        if 'data' in request.files:
            # 文件上传方式
            file = request.files['data']
            if file.filename == '':
                return jsonify({
                    'success': False,
                    'error': '文件名为空'
                }), 400
            
            encrypted_bytes = file.read()
            algorithm_str = request.form.get('algorithm', 'aes-128-cbc')
            key_hex = request.form.get('key', '')
            metadata_str = request.form.get('metadata', '{}')
            
            try:
                metadata = json.loads(metadata_str) if metadata_str else {}
            except:
                metadata = {}
        else:
            # JSON数据方式
            json_data = request.get_json()
            if not json_data:
                return jsonify({
                    'success': False,
                    'error': '请求数据为空'
                }), 400
            
            # 处理加密数据
            encrypted_hex = json_data.get('encrypted_data')
            if not encrypted_hex:
                return jsonify({
                    'success': False,
                    'error': '加密数据不能为空'
                }), 400
            
            try:
                encrypted_bytes = bytes.fromhex(encrypted_hex)
            except ValueError:
                return jsonify({
                    'success': False,
                    'error': '加密数据格式错误，请提供十六进制格式的数据'
                }), 400
            
            algorithm_str = json_data.get('algorithm', 'aes-128-cbc')
            key_hex = json_data.get('key', '')
            metadata = json_data.get('metadata', {})
        
        # 验证加密算法
        try:
            algorithm = EncryptionType(algorithm_str)
        except ValueError:
            return jsonify({
                'success': False,
                'error': f'不支持的加密算法: {algorithm_str}'
            }), 400
        
        # 处理密钥
        if not key_hex:
            return jsonify({
                'success': False,
                'error': '解密密钥不能为空'
            }), 400
        
        try:
            key_bytes = bytes.fromhex(key_hex)
        except ValueError:
            return jsonify({
                'success': False,
                'error': '密钥格式错误，请提供十六进制格式的密钥'
            }), 400
        
        # 执行解密
        decrypted_data = crypto_manager.decrypt_firmware(
            encrypted_bytes, algorithm, key_bytes, metadata
        )
        
        return jsonify({
            'success': True,
            'message': '数据解密成功',
            'data': {
                'encrypted_size': len(encrypted_bytes),
                'decrypted_size': len(decrypted_data),
                'decrypted_data': decrypted_data.hex(),
                'algorithm': algorithm_str
            }
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@api_v1.route('/crypto/checksum', methods=['POST'])
def calculate_checksum():
    """计算数据校验和"""
    try:
        # 检查是否有文件上传
        if 'data' in request.files:
            file = request.files['data']
            if file.filename == '':
                return jsonify({
                    'success': False,
                    'error': '文件名为空'
                }), 400
            
            data_bytes = file.read()
            algorithm = request.form.get('algorithm', 'md5')
        else:
            # JSON数据方式
            json_data = request.get_json()
            if not json_data:
                return jsonify({
                    'success': False,
                    'error': '请求数据为空'
                }), 400
            
            data_hex = json_data.get('data')
            if not data_hex:
                return jsonify({
                    'success': False,
                    'error': '数据不能为空'
                }), 400
            
            try:
                data_bytes = bytes.fromhex(data_hex)
            except ValueError:
                return jsonify({
                    'success': False,
                    'error': '数据格式错误，请提供十六进制格式的数据'
                }), 400
            
            algorithm = json_data.get('algorithm', 'md5')
        
        # 计算校验和
        checksum = crypto_manager.calculate_checksum(data_bytes, algorithm)
        
        return jsonify({
            'success': True,
            'message': '校验和计算成功',
            'data': {
                'algorithm': algorithm,
                'checksum': checksum,
                'data_size': len(data_bytes)
            }
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@api_v1.route('/crypto/verify', methods=['POST'])
def verify_checksum():
    """验证数据校验和"""
    try:
        # 检查是否有文件上传
        if 'data' in request.files:
            file = request.files['data']
            if file.filename == '':
                return jsonify({
                    'success': False,
                    'error': '文件名为空'
                }), 400
            
            data_bytes = file.read()
            expected_checksum = request.form.get('checksum', '')
            algorithm = request.form.get('algorithm', 'md5')
        else:
            # JSON数据方式
            json_data = request.get_json()
            if not json_data:
                return jsonify({
                    'success': False,
                    'error': '请求数据为空'
                }), 400
            
            data_hex = json_data.get('data')
            if not data_hex:
                return jsonify({
                    'success': False,
                    'error': '数据不能为空'
                }), 400
            
            try:
                data_bytes = bytes.fromhex(data_hex)
            except ValueError:
                return jsonify({
                    'success': False,
                    'error': '数据格式错误，请提供十六进制格式的数据'
                }), 400
            
            expected_checksum = json_data.get('checksum', '')
            algorithm = json_data.get('algorithm', 'md5')
        
        if not expected_checksum:
            return jsonify({
                'success': False,
                'error': '期望的校验和不能为空'
            }), 400
        
        # 验证校验和
        is_valid = crypto_manager.verify_checksum(data_bytes, expected_checksum, algorithm)
        actual_checksum = crypto_manager.calculate_checksum(data_bytes, algorithm)
        
        return jsonify({
            'success': True,
            'message': '校验和验证完成',
            'data': {
                'algorithm': algorithm,
                'expected_checksum': expected_checksum,
                'actual_checksum': actual_checksum,
                'is_valid': is_valid,
                'data_size': len(data_bytes)
            }
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500