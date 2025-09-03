import urllib.request
import urllib.error
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
        req = urllib.request.Request(flask_url)
        req.add_header('Range', 'bytes=0-1023')
        
        with urllib.request.urlopen(req, timeout=10) as response:
            data = response.read()
            status_code = response.getcode()
            content_range = response.headers.get('Content-Range', 'N/A')
            content_length = response.headers.get('Content-Length', 'N/A')
            
            print(f"第1个分块 (0-1023):")
            print(f"  状态码: {status_code}")
            print(f"  实际数据长度: {len(data)} 字节")
            print(f"  Content-Range: {content_range}")
            print(f"  Content-Length: {content_length}")
            
            # 保存文件
            with open('flask_chunk1.bin', 'wb') as f:
                f.write(data)
            print(f"  已保存到: flask_chunk1.bin")
        
    except Exception as e:
        print(f"第1个分块测试失败: {e}")
    
    print()
    
    # 测试第2个分块
    try:
        req = urllib.request.Request(flask_url)
        req.add_header('Range', 'bytes=1024-2047')
        
        with urllib.request.urlopen(req, timeout=10) as response:
            data = response.read()
            status_code = response.getcode()
            content_range = response.headers.get('Content-Range', 'N/A')
            
            print(f"第2个分块 (1024-2047):")
            print(f"  状态码: {status_code}")
            print(f"  实际数据长度: {len(data)} 字节")
            print(f"  Content-Range: {content_range}")
            
            with open('flask_chunk2.bin', 'wb') as f:
                f.write(data)
            print(f"  已保存到: flask_chunk2.bin")
        
    except Exception as e:
        print(f"第2个分块测试失败: {e}")
    
    print()
    
    # 测试完整文件
    try:
        req = urllib.request.Request(flask_url)
        
        with urllib.request.urlopen(req, timeout=30) as response:
            data = response.read()
            status_code = response.getcode()
            content_length = response.headers.get('Content-Length', 'N/A')
            
            print(f"完整文件:")
            print(f"  状态码: {status_code}")
            print(f"  实际数据长度: {len(data)} 字节")
            print(f"  Content-Length: {content_length}")
            
            with open('flask_full.bin', 'wb') as f:
                f.write(data)
            print(f"  已保存到: flask_full.bin")
        
    except Exception as e:
        print(f"完整文件测试失败: {e}")
    
    print()
    print("=" * 50)
    print("对比测试参考服务器...")
    print("=" * 50)
    
    # 测试参考服务器
    try:
        req = urllib.request.Request(ref_url)
        req.add_header('Range', 'bytes=0-1023')
        
        with urllib.request.urlopen(req, timeout=10) as response:
            data = response.read()
            status_code = response.getcode()
            content_range = response.headers.get('Content-Range', 'N/A')
            
            print(f"参考服务器第1个分块 (0-1023):")
            print(f"  状态码: {status_code}")
            print(f"  实际数据长度: {len(data)} 字节")
            print(f"  Content-Range: {content_range}")
            
            with open('ref_chunk1.bin', 'wb') as f:
                f.write(data)
            print(f"  已保存到: ref_chunk1.bin")
        
    except Exception as e:
        print(f"参考服务器测试失败: {e}")
    
    print()
    print("=" * 50)
    print("总结:")
    print("=" * 50)
    print("关键指标:")
    print("- Flask服务器分块状态码应该是 206")
    print("- Flask服务器每个分块应该是 1024 字节")
    print("- 参考服务器分块应该也是 1024 字节")
    print()
    print("如果Flask服务器的分块只有几百字节而不是1024字节,")
    print("那么问题在Flask服务器的Range处理上。")
    print("如果Flask服务器返回1024字节,")
    print("那么问题在ESP8266的TCP接收处理上。")
    print("=" * 50)

if __name__ == "__main__":
    test_range_requests()
    input("按回车键退出...")