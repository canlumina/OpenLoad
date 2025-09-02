#!/bin/bash
# STM32 固件AES加密脚本 (Linux/macOS版本)
# 用法: ./encrypt_firmware.sh <固件文件> [密码]

if [ "$#" -lt 1 ]; then
    echo "用法: ./encrypt_firmware.sh <固件文件> [密码]"
    echo "示例: ./encrypt_firmware.sh OpenLoad.bin yangcan"
    exit 1
fi

INPUT_FILE="$1"
PASSWORD="${2:-yangcan}"

# 生成输出文件名
BASENAME=$(basename "$INPUT_FILE" .bin)
OUTPUT_FILE="${BASENAME}_encrypted.bin"

echo "============================================================"
echo "STM32 固件AES加密工具"
echo "============================================================"
echo "输入文件: $INPUT_FILE"
echo "输出文件: $OUTPUT_FILE"  
echo "加密密码: $PASSWORD"
echo "============================================================"

# 检查Python是否安装
if ! command -v python3 &> /dev/null; then
    echo "错误: 未找到Python 3，请先安装Python 3.x"
    exit 1
fi

# 检查是否安装了pycryptodome
python3 -c "import Crypto.Cipher.AES" &> /dev/null
if [ $? -ne 0 ]; then
    echo "正在安装pycryptodome库..."
    pip3 install pycryptodome
    if [ $? -ne 0 ]; then
        echo "错误: 无法安装pycryptodome库"
        echo "请手动执行: pip3 install pycryptodome"
        exit 1
    fi
fi

# 执行加密（优先使用真实UID加密工具）
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -f "$SCRIPT_DIR/encrypt_with_real_uid.py" ]; then
    echo "使用真实STM32 Unique ID进行加密..."
    python3 "$SCRIPT_DIR/encrypt_with_real_uid.py" "$INPUT_FILE" "$OUTPUT_FILE" "$PASSWORD"
else
    echo "使用默认UID进行加密..."
    python3 "$SCRIPT_DIR/firmware_encryptor.py" "$INPUT_FILE" "$OUTPUT_FILE" "$PASSWORD"
fi

if [ $? -ne 0 ]; then
    echo "加密失败!"
    exit 1
fi

echo ""
echo "加密完成! 加密文件: $OUTPUT_FILE"
echo ""
echo "============================================================"
echo "使用bootloader升级AES加密固件的步骤:"
echo "============================================================"
echo "1. 连接STM32，进入bootloader命令模式"
echo "2. 输入命令 'u' 或 'update'"
echo "3. 选择 '1 = XMODEM (from UART1)'"
echo "4. 选择 '3 = Internal Flash (Encrypted XOR/AES)'"
echo "5. 选择 '2. AES-128-CBC encryption'"
echo "6. bootloader将自动使用硬编码密码: $PASSWORD"
echo "7. 使用串口工具发送加密文件: $OUTPUT_FILE"
echo "============================================================"