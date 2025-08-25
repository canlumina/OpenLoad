# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

这是一个基于 STM32F103ZET6 的嵌入式项目，使用 GCC ARM 工具链和 CMake 构建系统。项目实现了串口 DMA + 空闲中断 + 环形缓冲区的高效串口通信方案。

## 构建命令

### 配置和构建项目
```bash
# 配置 Debug 版本
cmake --preset=Debug

# 构建 Debug 版本
cmake --build build/Debug

# 配置 Release 版本
cmake --preset=Release

# 构建 Release 版本  
cmake --build build/Release
```

### 使用 Ninja
项目默认使用 Ninja 作为构建生成器，确保系统中已安装 Ninja 和 ARM GCC 工具链。

## 硬件架构

- **主控芯片**: STM32F103ZET6 (Cortex-M3, 72MHz)
- **主要外设**:
  - USART1: PA9(TX), PA10(RX) - 配置了 DMA 收发
  - SPI2: PB13(SCK), PB14(MISO), PB15(MOSI)
  - DMA1_Channel4: USART1_TX
  - DMA1_Channel5: USART1_RX

## 软件架构

### 核心模块

1. **UART 设备驱动** (`Core/Src/dev_usart.c`)
   - 实现了基于 DMA + FIFO 的高效串口通信
   - 支持空闲中断触发的接收处理
   - 使用环形缓冲区管理收发数据

2. **环形缓冲区** (`Core/Src/ringbuff.c`)
   - 线程安全的 FIFO 缓冲区实现
   - 支持加锁/解锁机制
   - 提供读写、大小查询等接口

3. **HAL 层**
   - 基于 STM32 HAL 库
   - 自动生成的初始化代码在 `cmake/stm32cubemx/` 目录

### 数据流

```
应用层 ↔ UART API (uart_read/uart_write) ↔ FIFO缓冲区 ↔ DMA ↔ 硬件UART
```

- 发送：数据写入发送FIFO → 轮询发送函数从FIFO读取 → DMA传输
- 接收：DMA接收 → 空闲/完成中断 → 数据写入接收FIFO → 应用层读取

### 内存布局

- UART1 DMA接收缓冲区：固定分配在 `0x20001000` 地址
- FIFO缓冲区大小：
  - UART1 TX FIFO: 512 bytes
  - UART1 RX FIFO: 1024 bytes  
  - DMA RX Buffer: 1024 bytes
  - DMA TX Buffer: 128 bytes

## 开发注意事项

1. **DMA 接收处理**
   - 使用空闲中断 + DMA完成中断 + DMA半完成中断的组合方式
   - 通过 `uart_poll_dma_tx()` 函数轮询发送数据

2. **中断处理函数**
   - `uart_dmarx_idle_isr()`: 空闲中断处理
   - `uart_dmarx_done_isr()`: DMA接收完成中断
   - `uart_dmatx_done_isr()`: DMA发送完成中断

3. **API 使用**
   ```c
   uart_device_init(DEV_UART1);           // 初始化UART设备
   uart_write(DEV_UART1, data, len);      // 写入数据到发送FIFO
   uart_read(DEV_UART1, buffer, size);    // 从接收FIFO读取数据
   uart_poll_dma_tx(DEV_UART1);          // 轮询发送数据
   ```

## 文件组织

- `Core/Inc/`: 头文件目录
- `Core/Src/`: 源文件目录（用户代码）
- `Drivers/`: STM32 HAL库和CMSIS库
- `cmake/`: CMake配置文件
- `STM32F103XX_FLASH.ld`: 链接脚本
- `OpenLoad.ioc`: STM32CubeMX配置文件

## 工具链要求

- ARM GCC 工具链 (arm-none-eabi-gcc)
- CMake 3.22+
- Ninja 构建工具
- STM32CubeMX (用于硬件配置)