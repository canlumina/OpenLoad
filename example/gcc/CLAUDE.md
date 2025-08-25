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
  - USART2: PA2(TX), PA3(RX) - 配置了 DMA 收发
  - SPI2: PB13(SCK), PB14(MISO), PB15(MOSI)
  - DMA1_Channel4: USART1_TX
  - DMA1_Channel5: USART1_RX
  - DMA1_Channel6: USART2_RX
  - DMA1_Channel7: USART2_TX

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

- 发送：数据写入发送FIFO → 轮询发送函数从FIFO读取 → HAL_UART_Transmit_DMA传输
- 接收：HAL_UARTEx_ReceiveToIdle_DMA接收 → HAL_UARTEx_RxEventCallback回调 → 数据写入接收FIFO → 应用层读取

### 内存布局

- UART1 DMA接收缓冲区：固定分配在 `0x20001000` 地址
- UART2 DMA接收缓冲区：固定分配在 `0x20002000` 地址
- FIFO缓冲区大小：
  - UART1 TX FIFO: 2048 bytes
  - UART1 RX FIFO: 2048 bytes  
  - UART1 DMA RX Buffer: 1024 bytes
  - UART1 DMA TX Buffer: 512 bytes
  - UART2 TX FIFO: 2048 bytes
  - UART2 RX FIFO: 2048 bytes
  - UART2 DMA RX Buffer: 1024 bytes
  - UART2 DMA TX Buffer: 512 bytes

## 开发注意事项

1. **DMA 接收处理**
   - 使用 `HAL_UARTEx_ReceiveToIdle_DMA()` 统一处理空闲中断、DMA半满和DMA满中断
   - 通过 `uart_poll_dma_tx()` 函数轮询发送数据

2. **HAL库回调函数**
   - `HAL_UARTEx_RxEventCallback()`: 统一处理所有接收事件（空闲/DMA半满/DMA满）
   - `HAL_UART_TxCpltCallback()`: DMA发送完成回调

3. **API 使用**
   ```c
   // 初始化UART设备
   uart_device_init(DEV_UART1);           // 初始化UART1
   uart_device_init(DEV_UART2);           // 初始化UART2
   
   // UART1操作
   uart_write(DEV_UART1, data, len);      // 写入数据到UART1发送FIFO
   uart_read(DEV_UART1, buffer, size);    // 从UART1接收FIFO读取数据
   uart_poll_dma_tx(DEV_UART1);          // 轮询UART1发送数据
   
   // UART2操作
   uart_write(DEV_UART2, data, len);      // 写入数据到UART2发送FIFO
   uart_read(DEV_UART2, buffer, size);    // 从UART2接收FIFO读取数据
   uart_poll_dma_tx(DEV_UART2);          // 轮询UART2发送数据
   ```

4. **HAL库串口驱动改进**
   - 使用 `HAL_UARTEx_ReceiveToIdle_DMA()` 进行接收，自动处理空闲中断、DMA半满和DMA满中断
   - 使用 `HAL_UART_Transmit_DMA()` 进行发送
   - 通过 `HAL_UARTEx_RxEventCallback()` 回调统一处理接收事件（支持UART1和UART2）
   - 通过 `HAL_UART_TxCpltCallback()` 回调处理发送完成（支持UART1和UART2）
   - 两个串口使用相同的HAL API架构，回调函数根据UART实例自动识别对应串口

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
- 现在同样的方式添加串口2的驱动，跟串口1相同