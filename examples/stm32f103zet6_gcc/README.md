# OpenLoad — STM32F103ZET6 GCC 示例

参考板: 普中科技 STM32F103ZET6 + W25Q64 外部 Flash。

## 硬件接线

| 外设 | 引脚 | 说明 |
|------|------|------|
| UART1 TX | PA9  | console 输出 |
| UART1 RX | PA10 | console 输入 |
| SPI2 SCK | PB13 | W25Q64 |
| SPI2 MISO | PB14 | W25Q64 |
| SPI2 MOSI | PB15 | W25Q64 |
| SPI2 CS | PB12 | W25Q64 (软件控制) |
| 按键 | PA0  | 低电平有效 (按住上电进入 bootloader) |
| LED | PB5  | 可选 (本工程未点亮) |

## 编译

需要工具:
- ARM GCC `arm-none-eabi-gcc` (在 PATH 中)
- CMake 3.22+
- Ninja

```bash
cd examples/stm32f103zet6_gcc

cmake --preset=Debug
cmake --build build/Debug

# 产物: build/Debug/openload.elf / openload.bin / openload.map
```

如果你的 arm-gcc 不在 PATH:

```bash
cmake --preset=Debug -DOPENLOAD_GCC_PREFIX="D:/your/path/arm-none-eabi-"
```

## 烧录

用 ST-Link/J-Link 把 `openload.bin` 烧到 `0x08000000`:

```bash
# 用 STM32CubeProgrammer CLI
STM32_Programmer_CLI -c port=SWD -d build/Debug/openload.bin 0x08000000 -v
```

## 第一次启动

上电后串口 (115200 8N1) 应看到:

```
[I] OpenLoad 0.1.0-m1 starting
[I] press button or send any char in 3000 ms to enter CLI
```

按住 PA0 按键, 或在串口里随便按一个键, 进入 CLI:

```
OpenLoad>
```

可用命令 (输入 `help`):

| 命令 | 用途 |
|------|------|
| `help` | 列出所有命令 |
| `info` | 显示框架 / App 版本 |
| `part` | 列出分区 |
| `erase <part>` | 擦除分区 |
| `update xmodem download app` | XMODEM 接收到 download, 装到 app |
| `install download app` | 假设 download 已含固件, 直接装到 app |
| `jump` | 校验并跳转到 App |
| `reset` | 复位 |

## 制作可烧录的 App

App 端用普通工程编译到 0x08010040 偏移 (boot 64K + header 64B), 编译完用工具加 header:

```bash
python ../../tools/image_tool.py myapp.bin \
    --board-id 0x0103 \
    --version 1.2.0.0 \
    -o myapp-ol.bin
```

然后用 XMODEM 发送 `myapp-ol.bin`:

```
OpenLoad> update xmodem download app
[I] erase staging download
[I] (等待 XMODEM 发送...)
```

在终端里 (Tera Term 例: File → Transfer → XMODEM → Send), 选 `myapp-ol.bin`。

传输完成后框架自动:
1. 校验 staging 头部 CRC + payload CRC
2. 擦除 app 分区
3. 拷贝 staging → app
4. 重新校验 app
5. 打印 `[I] install ok` → 输入 `jump` 跳转

## 体积参考 (-Os, GCC 13)

| 配置 | bootloader.bin |
|------|----------------|
| 仅 XMODEM + CRC32 (M1 默认) | ~14 KB |
| + YMODEM + HTTP OTA (M2 目标) | ~20 KB |
| 全功能 (M3 目标) | ~28 KB |
