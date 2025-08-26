# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述
这是一个综合性的Bootloader项目，实现以下核心功能：
- 上电3秒按键检测，决定进入App还是Bootloader模式
- 串口IAP固件升级功能
- W25Q64外部Flash存储管理
- ESP8266 OTA无线升级功能
- 命令行交互界面

## 项目架构
- **目标平台**: STM32F103ZET6 (512KB Flash, 64KB SRAM, 72MHz)
- **外部存储**: W25Q64 (8MB SPI Flash)
- **网络模块**: ESP8266 WiFi模块 (AT指令控制)
- **通信接口**: UART1(调试)、UART2(ESP8266)、SPI1(W25Q64)
- **开发环境**: Keil MDK-ARM + 标准外设库

## 关键组件
1. **启动管理**: 按键检测和应用程序跳转逻辑
2. **命令系统**: 串口命令解析和执行框架
3. **Flash驱动**: 内部和外部Flash擦写管理
4. **通信协议**: IAP数据传输协议实现  
5. **OTA管理**: ESP8266驱动和固件下载逻辑
6. **安全机制**: 固件校验和签名验证

## 内存分区方案
### STM32F103ZET6 内部Flash (512KB)
- **Bootloader区**: 0x08000000-0x08007FFF (32KB)
- **应用程序区**: 0x08008000-0x0807EFFF (472KB)
- **配置参数区**: 0x0807F000-0x0807FFFF (4KB)

### W25Q64 外部Flash (8MB)
- **固件下载区**: 0x000000-0x1FFFFF (2MB)
- **备份固件区**: 0x200000-0x3FFFFF (2MB)  
- **用户数据区**: 0x400000-0x7FFFFF (4MB)

## 开发指南
- 优先实现基础启动逻辑和串口通信
- 模块化设计，便于单独测试各个功能
- 重视异常处理和断电保护机制
- 注意32KB Bootloader区域的代码大小控制
- 合理使用STM32F103的144引脚资源和RCC时钟配置
- 详细的开发计划参考 `bootloader_development_plan.md`

## 常用命令
```bash
# Keil 编译项目
# 在Keil中打开项目文件，使用Ctrl+F7编译

# 下载到目标板 (使用ST-Link 或 J-Link)
# 在Keil中使用F8下载，或使用命令行工具
STM32_Programmer_CLI -c port=SWD -d bootloader.hex -v

# 串口调试连接 (115200bps, 8N1)
# Windows: 使用PuTTY或串口调试助手
```