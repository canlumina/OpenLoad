# STM32F103ZET6 Bootloader 使用说明

## 功能概述

本 Bootloader 提供了完整的固件管理功能，支持通过串口进行固件更新、下载等操作。

## 内存分区

STM32F103ZET6 拥有 512KB 内部 Flash，分区如下：

- **Bootloader 区域**: 0x08000000 - 0x0800FFFF (64KB)
- **应用程序区域**: 0x08010000 - 0x0807FFFF (448KB)

## 进入 Bootloader 模式

系统启动后有 **3秒** 的时间窗口，在此期间可通过以下方式进入 Bootloader：

1. **串口输入**: 在串口终端按任意键
2. **按键输入**: 按下 PA0 按键（低电平有效）

如果 3 秒内没有任何输入，系统将自动跳转到应用程序。

## 命令列表

| 命令 | 缩写 | 功能描述 |
|------|------|----------|
| `help` | `h` | 显示命令帮助信息 |
| `update` | `u` | 通过串口更新固件（XMODEM协议） |
| `download` | `d` | 下载当前固件到PC |
| `info` | `i` | 显示固件信息（版本、大小、CRC等） |
| `erase` | `e` | 擦除应用程序区域 |
| `reset` | `r` | 复位系统 |
| `jump` | `j` | 跳转到应用程序 |

### 命令输入方式

1. **完整命令**: `help`, `info`, `reset` 等
2. **单字符缩写**: `h`, `i`, `r` 等  
3. **部分匹配**: `hel`, `inf`, `res` 等（至少2个字符，且唯一匹配）
4. **大小写不敏感**: `HELP`, `Help`, `help` 都可以

## 使用示例

### 1. 进入 Bootloader

```
========================================
   STM32 Bootloader v1.0.0
========================================
Press any key or button within 3 seconds to enter bootloader...

UART input detected! Entering bootloader mode...
*******************************************
*       STM32F103 Bootloader v1.0.0      *
*       (c) 2025 - OpenLoad Project      *
*******************************************

Type 'help' or 'h' for command list
BOOT> 
```

### 2. 查看系统信息

可以使用多种方式输入命令：
```
BOOT> info       # 完整命令
BOOT> i          # 缩写
BOOT> inf        # 部分匹配
BOOT> INFO       # 大写也可以
========== System Information ==========
MCU Type           : STM32F103ZET6
Flash Total Size   : 512 KB
----------------------------------------
Bootloader Version : v1.0.0
Bootloader Size    : 64 KB
Bootloader Address : 0x08000000 - 0x0800FFFF
----------------------------------------
Application Address: 0x08010000 - 0x0807FFFF
Application Size   : 448 KB
Application Status : VALID
Application CRC32  : 0xA1B2C3D4 (first 64KB)
========================================
```

### 3. 查看帮助

```
BOOT> h

Available commands:
=======================================================
  Command      Short    Description
  ----------------------------------------------------
  help         h        Show command list
  update       u        Update firmware via UART
  download     d        Download firmware to PC
  info         i        Show firmware information
  erase        e        Erase application area
  reset        r        Reset system
  jump         j        Jump to application
=======================================================
Note: You can use either full command or short form
```

### 4. 擦除应用程序

```
BOOT> e          # 使用缩写
WARNING: This will erase the application area!
Type 'YES' to confirm: YES
Erasing application area...
Application area erased successfully!
```

## 串口配置

- **波特率**: 115200
- **数据位**: 8
- **停止位**: 1
- **校验**: 无
- **流控**: 无

## LED 指示（如果配置）

- **快闪**: 正在等待进入 Bootloader（3秒倒计时）
- **慢闪**: 在 Bootloader 命令模式
- **常亮**: 正在执行命令（更新/备份/恢复）
- **熄灭**: 运行应用程序

## 注意事项

1. **断电保护**: 在执行更新、擦除操作时，请勿断电
2. **CRC 校验**: Bootloader 会自动计算并显示固件的 CRC32 校验值
3. **应用程序要求**: 应用程序必须编译到 0x08010000 地址

## 应用程序编译配置

在应用程序的链接脚本中，需要修改 Flash 起始地址：

```ld
MEMORY
{
  FLASH (rx) : ORIGIN = 0x08010000, LENGTH = 448K
  RAM (xrw)  : ORIGIN = 0x20000000, LENGTH = 64K
}
```

## 故障排除

| 问题 | 可能原因 | 解决方法 |
|------|----------|----------|
| 无法进入 Bootloader | 时间窗口太短 | 上电后立即按键或发送字符 |
| 无法跳转到应用程序 | 应用程序无效 | 检查应用程序编译地址是否为 0x08010000 |
| 更新失败 | Flash 写保护 | 检查 Flash 写保护状态 |

## Flash 分区说明

```
+------------------+ 0x08000000 (Flash 起始)
|                  |
|   Bootloader     |
|     (64KB)       |
|                  |
+------------------+ 0x08010000 (App 起始)
|                  |
|                  |
|   Application    |
|     (448KB)      |
|                  |
|                  |
+------------------+ 0x08080000 (Flash 结束)
```

## 后续开发计划

- [ ] 实现 XMODEM 协议支持
- [ ] 添加固件加密功能
- [ ] 支持固件签名验证
- [ ] 支持 USB DFU 模式
- [ ] 添加固件版本管理
- [ ] 支持外部 Flash 备份