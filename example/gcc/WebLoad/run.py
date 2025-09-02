#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
OpenLoad Web管理器启动脚本

使用方法:
    python run.py                    # 默认开发模式
    python run.py --production      # 生产模式
    python run.py --host 0.0.0.0 --port 8080  # 自定义地址和端口
"""

import os
import sys
import argparse
from backend.app import app
from config import config

def create_app(config_name):
    """创建Flask应用"""
    config[config_name].init_app(app)
    return app

def main():
    """主函数"""
    parser = argparse.ArgumentParser(description='OpenLoad Web管理器')
    parser.add_argument('--host', default='0.0.0.0', help='服务器地址 (默认: 0.0.0.0)')
    parser.add_argument('--port', type=int, default=5000, help='服务器端口 (默认: 5000)')
    parser.add_argument('--production', action='store_true', help='生产模式')
    parser.add_argument('--debug', action='store_true', help='调试模式')
    
    args = parser.parse_args()
    
    # 确定配置环境
    if args.production:
        config_name = 'production'
    else:
        config_name = 'development'
    
    # 创建应用
    flask_app = create_app(config_name)
    
    # 设置调试模式
    if args.debug:
        flask_app.config['DEBUG'] = True
    
    print("=" * 50)
    print("OpenLoad Web管理器")
    print("=" * 50)
    print(f"环境: {config_name}")
    print(f"调试模式: {flask_app.config['DEBUG']}")
    print(f"服务器地址: http://{args.host}:{args.port}")
    print("=" * 50)
    print("使用说明:")
    print("1. 打开浏览器访问上述地址")
    print("2. 在串口连接页面选择STM32设备串口")  
    print("3. 连接后即可进行固件管理和升级")
    print("4. 按 Ctrl+C 退出服务器")
    print("=" * 50)
    
    try:
        # 启动Flask应用
        flask_app.run(
            host=args.host,
            port=args.port,
            debug=flask_app.config['DEBUG'],
            use_reloader=flask_app.config['DEBUG']
        )
    except KeyboardInterrupt:
        print("\n服务器已停止")
    except Exception as e:
        print(f"\n启动失败: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()