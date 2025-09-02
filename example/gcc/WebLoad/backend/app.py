#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import logging
from flask import Flask, render_template, jsonify
from flask_cors import CORS

# 设置日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# 导入核心模块
from .core.serial_manager import serial_manager
from .core.firmware_manager import FirmwareManager

# Flask 应用配置
app = Flask(__name__, static_folder='../static', template_folder='../templates')
app.config['SECRET_KEY'] = 'openload_webmanager_2024'
app.config['UPLOAD_FOLDER'] = os.path.join(os.path.dirname(__file__), '..', 'uploads')
app.config['MAX_CONTENT_LENGTH'] = 16 * 1024 * 1024  # 16MB max file size

# 启用CORS
CORS(app)

# 确保上传目录存在
os.makedirs(app.config['UPLOAD_FOLDER'], exist_ok=True)

# 初始化固件管理器
firmware_manager = FirmwareManager(app.config['UPLOAD_FOLDER'])

# 注册API蓝图
from .api import api_v1
app.register_blueprint(api_v1)

@app.route('/')
def index():
    """主页 - 新的固件管理界面"""
    return render_template('index_new.html')

@app.route('/legacy')
def legacy():
    """旧版界面 - 包含串口功能"""
    return render_template('index.html')

# 为了兼容现有前端，保留一些旧的API路由
@app.route('/api/serial/ports')
def get_serial_ports_legacy():
    """获取可用串口列表 (兼容性API)"""
    ports = serial_manager.get_available_ports()
    return jsonify({'ports': [{'name': p['name'], 'description': p['description'], 'hwid': p['hwid']} for p in ports]})

# 应用清理函数
@app.teardown_appcontext
def cleanup(error):
    """应用上下文清理"""
    if error:
        logger.error(f"应用错误: {error}")

# 注册清理函数
import atexit
atexit.register(lambda: serial_manager.cleanup())

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)