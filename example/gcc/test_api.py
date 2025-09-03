#!/usr/bin/env python3
# 测试API响应

import requests

def test_api():
    """测试固件API响应"""
    url = "http://192.168.0.100:5000/api/v1/firmwares/latest"
    
    print("测试API响应...")
    print(f"URL: {url}")
    
    try:
        response = requests.get(url, timeout=10)
        print(f"状态码: {response.status_code}")
        print("\n响应头:")
        for key, value in response.headers.items():
            if key.startswith('X-'):
                print(f"  {key}: {value}")
        
        print(f"\n响应体 (前200字符):")
        print(response.text[:200])
        
    except Exception as e:
        print(f"请求失败: {e}")

if __name__ == "__main__":
    test_api()