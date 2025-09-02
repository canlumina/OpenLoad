#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os

class Config:
    """应用配置类"""
    
    # Flask基础配置
    SECRET_KEY = os.environ.get('SECRET_KEY') or 'openload_webmanager_2024'
    DEBUG = os.environ.get('FLASK_DEBUG', 'False').lower() == 'true'
    
    # 服务器配置
    HOST = os.environ.get('FLASK_HOST', '0.0.0.0')
    PORT = int(os.environ.get('FLASK_PORT', 5000))
    
    # 上传配置
    UPLOAD_FOLDER = os.path.join(os.path.dirname(__file__), 'uploads')
    MAX_CONTENT_LENGTH = 16 * 1024 * 1024  # 16MB
    ALLOWED_EXTENSIONS = {'bin'}
    
    # 串口配置
    DEFAULT_BAUDRATE = 115200
    SERIAL_TIMEOUT = 1.0
    
    # Bootloader配置
    BOOTLOADER_COMMANDS = {
        'help': 'h',
        'info': 'i', 
        'update': 'u',
        'erase': 'e',
        'reset': 'r',
        'jump': 'j',
        'wifi': 'w',
        'backup': 'xb',
        'restore': 'xr'
    }
    
    # 加密选项
    ENCRYPTION_TYPES = {
        'none': '无加密',
        'xor': 'XOR加密',
        'aes': 'AES-128-CBC'
    }
    
    # 升级方式
    UPDATE_METHODS = {
        'xmodem': 'XMODEM (串口)',
        'ota': 'OTA (WiFi)'
    }
    
    # 目标位置
    TARGET_LOCATIONS = {
        'internal': '内部Flash',
        'external': '外部Flash'
    }
    
    @staticmethod
    def init_app(app):
        """初始化应用配置"""
        # 确保上传目录存在
        os.makedirs(Config.UPLOAD_FOLDER, exist_ok=True)
        
        # 设置Flask配置
        app.config.from_object(Config)
        
        return app

class DevelopmentConfig(Config):
    """开发环境配置"""
    DEBUG = True

class ProductionConfig(Config):
    """生产环境配置"""
    DEBUG = False

# 配置字典
config = {
    'development': DevelopmentConfig,
    'production': ProductionConfig,
    'default': DevelopmentConfig
}