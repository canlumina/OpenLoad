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
- **通信接口**: UART1(调试)、UART2(ESP8266)、SPI2(W25Q64)
- **开发环境**: Keil MDK-ARM + 标准外设库 或 GCC + CMake

## 关键组件
### 中间件层 (src目录)
- **SFUD**: 串行Flash通用驱动，支持多种SPI Flash芯片
- **FAL**: Flash抽象层，提供统一的分区管理接口
- **EasyFlash**: 轻量级嵌入式Flash存储库，提供环境变量管理

### BSP驱动层 (example/keil/Drivers/BSP)
- **bsp_esp8266**: ESP8266 WiFi模块驱动
- **bsp_key**: 按键检测驱动
- **bsp_led**: LED状态指示驱动  
- **bsp_spi**: SPI总线驱动(W25Q64通信)

### 系统层 (example/keil/Drivers/SYSTEM)
- **delay**: 延时函数实现
- **sys**: 系统时钟和中断管理
- **usart**: 串口通信驱动

## 内存分区方案
### STM32F103ZET6 内部Flash (512KB)
- **Bootloader区**: 0x08000000-0x0800FFFF (64KB)
- **应用程序区**: 0x08010000-0x0807EFFF (444KB)
- **配置参数区**: 0x0807F000-0x0807FFFF (4KB)

### W25Q64 外部Flash (8MB)
- **固件下载区**: 0x000000-0x1FFFFF (2MB)
- **备份固件区**: 0x200000-0x3FFFFF (2MB)  
- **用户数据区**: 0x400000-0x7FFFFF (4MB)

## 构建和运行命令

### Keil MDK-ARM构建
```bash
# 在Keil中打开项目文件
example/keil/Projects/MDK-ARM/OpenLoad.uvprojx

# 编译项目: Ctrl+F7 或 Project -> Build Target
# 下载到目标板: F8 或 Flash -> Download
```

### GCC + CMake构建 (example/gcc目录)
```bash
# 配置Debug版本
cd example/gcc
cmake --preset=Debug

# 构建Debug版本
cmake --build build/Debug

# 配置Release版本
cmake --preset=Release

# 构建Release版本
cmake --build build/Release
```

### 下载固件到STM32
```bash
# 使用ST-Link或J-Link下载
# Windows下使用STM32_Programmer_CLI:
STM32_Programmer_CLI -c port=SWD -d bootloader.hex -v

# 或使用OpenOCD:
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program bootloader.hex verify reset exit"
```

### 串口调试
```bash
# 串口参数: 115200 bps, 8N1
# Windows: 使用PuTTY、串口调试助手或Tera Term
# Linux: 使用minicom或screen
screen /dev/ttyUSB0 115200
```

## 重要API和使用方式

### Flash抽象层(FAL)操作
```c
// 初始化FAL
fal_init();

// 查找分区
const struct fal_partition *partition = fal_partition_find("app");

// 擦除分区
fal_partition_erase(partition, 0, partition->len);

// 写入数据
fal_partition_write(partition, 0, data, len);

// 读取数据
fal_partition_read(partition, 0, buffer, len);
```

### EasyFlash环境变量管理
```c
// 初始化EasyFlash
easyflash_init();

// 设置环境变量
ef_set_env("boot_times", "1");

// 获取环境变量
char *value = ef_get_env("boot_times");

// 保存环境变量到Flash
ef_save_env();
```

### UART操作 (GCC版本)
```c
// 初始化UART设备
uart_device_init(DEV_UART1);
uart_device_init(DEV_UART2);

// 写入数据到UART
uart_write(DEV_UART1, data, len);
uart_poll_dma_tx(DEV_UART1);  // 轮询发送

// 从UART读取数据
uart_read(DEV_UART1, buffer, size);
```

### UART操作 (Keil版本)
```c
// 初始化串口
u1_init(115200);  // UART1
u2_init(115200);  // UART2

// 使用printf输出(重定向到UART1)
printf("Boot times: %d\n", boot_times);

// 直接发送数据
HAL_UART_Transmit(&huart1, data, len, timeout);
```

## 启动流程
1. 系统复位后初始化硬件(时钟、GPIO、串口、SPI)
2. 初始化FAL Flash抽象层
3. 检查按键状态，决定进入Bootloader或跳转App
4. Bootloader模式下等待串口IAP或ESP8266 OTA升级
5. 接收固件到外部Flash下载区
6. 校验固件完整性(CRC32)
7. 从外部Flash复制到内部Flash应用程序区
8. 跳转到应用程序执行

## 开发注意事项
- Bootloader代码必须控制在64KB以内
- 使用STM32F103ZET6的144引脚，注意引脚映射
- W25Q64使用SPI2接口，最高支持18MHz时钟
- ESP8266使用UART2，通过AT指令控制
- 固件升级时注意看门狗和断电保护
- 合理使用DMA提高串口和SPI传输效率

## 调试技巧
- 使用LED指示当前运行状态(常亮/慢闪/快闪)
- 通过UART1输出调试信息
- 使用逻辑分析仪监控SPI通信
- 检查FAL分区表配置是否正确
- 验证外部Flash读写操作
- 监控启动时的按键检测逻辑

## 常见问题处理
1. **Bootloader无法跳转到App**: 检查向量表重定位和App起始地址
2. **外部Flash读写失败**: 验证SPI时序和CS引脚控制
3. **串口接收数据丢失**: 增大DMA缓冲区或使用环形缓冲
4. **ESP8266连接不稳定**: 检查电源供电和AT指令超时设置
5. **固件升级后无法启动**: 验证CRC校验和Flash擦写操作