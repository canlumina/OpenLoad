#!/bin/bash

# OpenLoad Web管理器 Linux/macOS启动脚本

echo "================================================"
echo "OpenLoad Web管理器启动脚本"  
echo "================================================"

# 检查Python是否安装
if ! command -v python3 &> /dev/null; then
    echo "错误：未找到Python3，请先安装Python 3.7+"
    exit 1
fi

# 检查是否已创建虚拟环境
if [ ! -d "venv" ]; then
    echo "首次运行，正在创建虚拟环境..."
    python3 -m venv venv
    if [ $? -ne 0 ]; then
        echo "错误：创建虚拟环境失败"
        exit 1
    fi
fi

# 激活虚拟环境
echo "激活虚拟环境..."
source venv/bin/activate

# 升级pip
echo "升级pip..."
pip install --upgrade pip

# 安装依赖
echo "检查依赖包..."
pip install -r requirements.txt

# 创建uploads目录
if [ ! -d "uploads" ]; then
    mkdir -p uploads
fi

# 设置权限
chmod +x run.py

# 启动应用
echo "启动OpenLoad Web管理器..."
python3 run.py --host 0.0.0.0 --port 5000