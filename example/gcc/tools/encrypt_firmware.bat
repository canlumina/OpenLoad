@echo off
REM STM32 固件AES加密批处理脚本
REM 用法: encrypt_firmware.bat <固件文件> [密码]

setlocal enabledelayedexpansion

if "%~1"=="" (
    echo 用法: encrypt_firmware.bat ^<固件文件^> [密码]
    echo 示例: encrypt_firmware.bat OpenLoad.bin yangcan
    pause
    exit /b 1
)

set INPUT_FILE=%~1
set PASSWORD=%~2
if "%PASSWORD%"=="" set PASSWORD=yangcan

REM 生成输出文件名
set OUTPUT_FILE=%~dpn1_encrypted%~x1

echo ============================================================
echo STM32 固件AES加密工具
echo ============================================================
echo 输入文件: %INPUT_FILE%
echo 输出文件: %OUTPUT_FILE%
echo 加密密码: %PASSWORD%
echo ============================================================

REM 检查Python是否安装
python --version >nul 2>&1
if errorlevel 1 (
    echo 错误: 未找到Python，请先安装Python 3.x
    echo 下载地址: https://www.python.org/downloads/
    pause
    exit /b 1
)

REM 检查是否安装了pycryptodome
python -c "import Crypto.Cipher.AES" >nul 2>&1
if errorlevel 1 (
    echo 正在安装pycryptodome库...
    pip install pycryptodome
    if errorlevel 1 (
        echo 错误: 无法安装pycryptodome库
        echo 请手动执行: pip install pycryptodome
        pause
        exit /b 1
    )
)

REM 执行加密（优先使用真实UID加密工具）
if exist "%~dp0encrypt_with_real_uid.py" (
    echo 使用真实STM32 Unique ID进行加密...
    python "%~dp0encrypt_with_real_uid.py" "%INPUT_FILE%" "%OUTPUT_FILE%" "%PASSWORD%"
) else (
    echo 使用默认UID进行加密...
    python "%~dp0firmware_encryptor.py" "%INPUT_FILE%" "%OUTPUT_FILE%" "%PASSWORD%"
)

if errorlevel 1 (
    echo 加密失败!
    pause
    exit /b 1
)

echo.
echo 加密完成! 加密文件: %OUTPUT_FILE%
echo.
echo ============================================================
echo 使用bootloader升级AES加密固件的步骤:
echo ============================================================
echo 1. 连接STM32，进入bootloader命令模式
echo 2. 输入命令 'u' 或 'update'
echo 3. 选择 '1 = XMODEM (from UART1)'
echo 4. 选择 '3 = Internal Flash (Encrypted XOR/AES)'
echo 5. 选择 '2. AES-128-CBC encryption'
echo 6. bootloader将自动使用硬编码密码: %PASSWORD%
echo 7. 使用串口工具发送加密文件: %OUTPUT_FILE%
echo ============================================================

pause