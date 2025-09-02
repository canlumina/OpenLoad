#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import argparse
import logging

# 设置项目根路径
project_root = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(project_root, 'backend'))

def main():
    parser = argparse.ArgumentParser(description='OpenLoad WebManager Server')
    parser.add_argument('--host', default='0.0.0.0', help='服务器地址')
    parser.add_argument('--port', type=int, default=5000, help='服务器端口')
    parser.add_argument('--debug', action='store_true', help='调试模式')
    parser.add_argument('--production', action='store_true', help='生产模式')
    
    args = parser.parse_args()
    
    # 配置日志级别
    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)
    elif args.production:
        logging.getLogger().setLevel(logging.WARNING)
    else:
        logging.getLogger().setLevel(logging.INFO)
    
    # 导入应用
    from backend.app import app
    from backend.core.firmware_manager import FirmwareManager
    
    # 初始化固件管理器
    app.firmware_manager = FirmwareManager(app.config['UPLOAD_FOLDER'])
    
    # 启动服务器
    try:
        print("OpenLoad WebManager Server 启动中...")
        print(f"服务地址: http://{args.host}:{args.port}")
        print(f"上传目录: {app.config['UPLOAD_FOLDER']}")
        print(f"调试模式: {'开启' if args.debug else '关闭'}")
        print("-" * 50)
        
        app.run(
            host=args.host,
            port=args.port,
            debug=args.debug,
            threaded=True
        )
    except KeyboardInterrupt:
        print("\n服务器已停止")
    except Exception as e:
        print(f"服务器启动失败: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()