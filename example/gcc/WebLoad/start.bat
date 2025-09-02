@echo off
REM OpenLoad Web管理器 Windows启动脚本

echo ================================================
echo OpenLoad Web管理器启动脚本
echo ================================================

REM 检查Python是否安装
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo 错误：未找到Python，请先安装Python 3.7+
    pause
    exit /b 1
)

REM 检查是否已安装依赖
if not exist "venv\" (
    echo 首次运行，正在创建虚拟环境...
    python -m venv venv
    if %errorlevel% neq 0 (
        echo 错误：创建虚拟环境失败
        pause
        exit /b 1
    )
)

REM 激活虚拟环境
echo 激活虚拟环境...
call venv\Scripts\activate.bat

REM 安装依赖
echo 检查依赖包...
pip install -r requirements.txt

REM 创建uploads目录
if not exist "uploads\" (
    mkdir uploads
)

REM 启动应用
echo 启动OpenLoad Web管理器...
python run.py --host 0.0.0.0 --port 5000

pause