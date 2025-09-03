import requests
import sys

def test_range_requests():
    print("=" * 50)
    print("           Range请求测试")
    print("=" * 50)
    
    flask_url = "http://192.168.0.100:5000/api/v1/firmwares/latest?download=true"
    ref_url = "http://120.27.208.180:80/RTC2.bin"
    
    print("测试 Flask 服务器...")
    print()
    
    # 测试第1个分块
    try:
        headers = {'Range': 'bytes=0-1023'}
        response = requests.get(flask_url, headers=headers, timeout=10)
        print(f"第1个分块 (0-1023):")
        print(f"  状态码: {response.status_code}")
        print(f"  实际数据长度: {len(response.content)} 字节")
        print(f"  Content-Range: {response.headers.get('Content-Range', 'N/A')}")
        print(f"  Content-Length: {response.headers.get('Content-Length', 'N/A')}")
        
        # 保存文件以便检查
        with open('flask_chunk1.bin', 'wb') as f:
            f.write(response.content)
        print(f"  已保存到: flask_chunk1.bin")
        
    except Exception as e:
        print(f"第1个分块测试失败: {e}")
    
    print()
    
    # 测试第2个分块
    try:
        headers = {'Range': 'bytes=1024-2047'}
        response = requests.get(flask_url, headers=headers, timeout=10)
        print(f"第2个分块 (1024-2047):")
        print(f"  状态码: {response.status_code}")
        print(f"  实际数据长度: {len(response.content)} 字节")
        print(f"  Content-Range: {response.headers.get('Content-Range', 'N/A')}")
        
        with open('flask_chunk2.bin', 'wb') as f:
            f.write(response.content)
        print(f"  已保存到: flask_chunk2.bin")
        
    except Exception as e:
        print(f"第2个分块测试失败: {e}")
    
    print()
    
    # 测试完整文件
    try:
        response = requests.get(flask_url, timeout=30)
        print(f"完整文件:")
        print(f"  状态码: {response.status_code}")
        print(f"  实际数据长度: {len(response.content)} 字节")
        print(f"  Content-Length: {response.headers.get('Content-Length', 'N/A')}")
        
        with open('flask_full.bin', 'wb') as f:
            f.write(response.content)
        print(f"  已保存到: flask_full.bin")
        
    except Exception as e:
        print(f"完整文件测试失败: {e}")
    
    print()
    print("=" * 50)
    print("对比测试参考服务器...")
    print("=" * 50)
    
    # 测试参考服务器
    try:
        headers = {'Range': 'bytes=0-1023'}
        response = requests.get(ref_url, headers=headers, timeout=10)
        print(f"参考服务器第1个分块 (0-1023):")
        print(f"  状态码: {response.status_code}")
        print(f"  实际数据长度: {len(response.content)} 字节")
        print(f"  Content-Range: {response.headers.get('Content-Range', 'N/A')}")
        
        with open('ref_chunk1.bin', 'wb') as f:
            f.write(response.content)
        print(f"  已保存到: ref_chunk1.bin")
        
    except Exception as e:
        print(f"参考服务器测试失败: {e}")
    
    print()
    print("=" * 50)
    print("总结:")
    print("=" * 50)
    print("如果Flask服务器的分块只有几百字节而不是1024字节，")
    print("那么问题在Flask服务器的Range处理上。")
    print("如果Flask服务器返回1024字节，")
    print("那么问题在ESP8266的TCP接收处理上。")
    print("=" * 50)

if __name__ == "__main__":
    test_range_requests()
    input("按回车键退出...")