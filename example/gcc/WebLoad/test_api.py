#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import requests
import json
import time

# 服务器配置
BASE_URL = 'http://localhost:5000'
API_BASE = f'{BASE_URL}/api/v1'

def test_api_endpoint(method, endpoint, data=None, files=None):
    """测试API端点"""
    url = f"{API_BASE}{endpoint}"
    
    try:
        print(f"\n🔍 测试: {method} {endpoint}")
        
        if method.upper() == 'GET':
            response = requests.get(url)
        elif method.upper() == 'POST':
            if files:
                response = requests.post(url, data=data, files=files)
            else:
                response = requests.post(url, json=data)
        elif method.upper() == 'DELETE':
            response = requests.delete(url, json=data)
        else:
            print(f"❌ 不支持的HTTP方法: {method}")
            return False
        
        print(f"📊 状态码: {response.status_code}")
        
        if response.headers.get('content-type', '').startswith('application/json'):
            result = response.json()
            print(f"📄 响应: {json.dumps(result, indent=2, ensure_ascii=False)}")
        else:
            print(f"📄 响应: {response.text[:200]}...")
        
        return response.status_code < 400
        
    except Exception as e:
        print(f"❌ 请求失败: {e}")
        return False

def test_system_apis():
    """测试系统API"""
    print("=" * 60)
    print("🔧 测试系统API")
    print("=" * 60)
    
    # 系统信息
    test_api_endpoint('GET', '/system/info')
    
    # 健康状态
    test_api_endpoint('GET', '/system/health')
    
    # 系统配置
    test_api_endpoint('GET', '/system/config')
    
    # 系统统计
    test_api_endpoint('GET', '/system/stats')

def test_device_apis():
    """测试设备API"""
    print("=" * 60)
    print("🔌 测试设备API")
    print("=" * 60)
    
    # 获取可用端口
    test_api_endpoint('GET', '/devices/ports')
    
    # 获取设备列表
    test_api_endpoint('GET', '/devices')

def test_crypto_apis():
    """测试加密API"""
    print("=" * 60)
    print("🔐 测试加密API")
    print("=" * 60)
    
    # 获取支持的算法
    test_api_endpoint('GET', '/crypto/algorithms')
    
    # 生成密钥
    test_api_endpoint('POST', '/crypto/key/generate', {
        'algorithm': 'aes-128-cbc',
        'password': 'test123'
    })
    
    # 测试加密
    test_api_endpoint('POST', '/crypto/encrypt', {
        'data': '48656c6c6f20576f726c64',  # "Hello World" in hex
        'algorithm': 'aes-128-cbc',
        'password': 'test123'
    })

def test_firmware_apis():
    """测试固件API"""
    print("=" * 60)
    print("📦 测试固件API")
    print("=" * 60)
    
    # 获取固件列表
    test_api_endpoint('GET', '/firmwares')
    
    # 获取存储信息
    test_api_endpoint('GET', '/firmwares/storage')

def test_legacy_apis():
    """测试兼容性API"""
    print("=" * 60)
    print("🔄 测试兼容性API")
    print("=" * 60)
    
    # 旧的串口API
    try:
        response = requests.get(f'{BASE_URL}/api/serial/ports')
        print(f"🔍 旧API测试: /api/serial/ports")
        print(f"📊 状态码: {response.status_code}")
        if response.status_code == 200:
            print(f"📄 响应: {json.dumps(response.json(), indent=2, ensure_ascii=False)}")
        print("✅ 兼容性API正常")
    except Exception as e:
        print(f"❌ 兼容性API测试失败: {e}")

def main():
    print("🧪 OpenLoad WebManager API 测试")
    print(f"🌐 服务器地址: {BASE_URL}")
    print("=" * 60)
    
    # 检查服务器是否在运行
    try:
        response = requests.get(BASE_URL, timeout=5)
        if response.status_code == 200:
            print("✅ 服务器正在运行")
        else:
            print(f"⚠️ 服务器响应异常: {response.status_code}")
    except Exception as e:
        print(f"❌ 无法连接到服务器: {e}")
        print("请先启动服务器: python run_new.py")
        return
    
    # 运行测试
    test_system_apis()
    test_device_apis()
    test_crypto_apis()
    test_firmware_apis()
    test_legacy_apis()
    
    print("\n" + "=" * 60)
    print("🎉 API测试完成")
    print("=" * 60)

if __name__ == '__main__':
    main()