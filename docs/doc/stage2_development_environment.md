# 阶段2-1：开发环境搭建

## 概述
搭建STM32F103ZET6 Bootloader项目的完整开发环境，包括编译工具链、调试环境和项目模板。

## 开发工具选择

### 主要开发环境
- **IDE**：Keil µVision5 MDK-ARM
- **编译器**：ARM Compiler v6 (ARMCLANG)
- **调试器**：ST-Link V2/V3 或 J-Link
- **烧录工具**：STM32CubeProgrammer
- **串口工具**：串口调试助手/PuTTY

### 辅助工具
- **Git**：版本控制
- **Hex2Bin**：固件格式转换
- **HxD**：十六进制编辑器
- **Wireshark**：网络调试（OTA功能）

## Keil MDK-ARM安装配置

### 安装步骤
1. 下载Keil MDK-ARM 5.37+
2. 安装Device Family Pack (DFP)
   - STM32F1xx_DFP.2.3.0.pack
3. 配置License（专业版或评估版）
4. 安装ST-Link驱动程序

### Pack安装
```bash
# 在Pack Installer中安装以下包
Keil::STM32F1xx_DFP::2.3.0
ARM::CMSIS::5.8.0
ARM::CMSIS-Driver::2.7.2
```

## 项目创建和配置

### 新建项目
1. File -> New µVision Project
2. 选择目标设备：STMicroelectronics -> STM32F103ZETx
3. 在Runtime Environment中选择：
   - CMSIS -> CORE
   - Device -> Startup

### 项目目录结构
```
Bootloader/
├── Project/                 # 项目文件
│   ├── MDK-ARM/            # Keil项目文件
│   │   ├── Bootloader.uvprojx
│   │   ├── Bootloader.uvoptx
│   │   └── startup_stm32f103xe.s
│   └── Objects/            # 编译输出
├── Source/                 # 源代码
│   ├── System/            # 系统文件
│   ├── Drivers/           # 硬件驱动
│   ├── Protocol/          # 通信协议
│   ├── Application/       # 应用逻辑
│   └── main.c
├── Include/               # 头文件
│   ├── system_config.h
│   ├── bootloader.h
│   └── drivers/
└── Doc/                   # 文档
```

### 项目配置选项

#### Target配置
```
Target Name: STM32F103ZETx_Bootloader
Device: STMicroelectronics STM32F103ZETx
```

#### Output配置
```
Name of Executable: Bootloader
☑ Create HEX File
☑ Browse Information
```

#### Listing配置  
```
☑ Assembly Listing
☑ C Listing
```

#### C/C++配置
```
Define: STM32F103xE, USE_STDPERIPH_DRIVER
Include Paths: 
  - ..\Include
  - ..\Source\System
  - ..\Source\Drivers
  - ..\Libraries\CMSIS\Include
  - ..\Libraries\STM32F10x_StdPeriph_Driver\inc
```

#### ASM配置
```
Define: STM32F103xE
```

#### Linker配置
```
Use Memory Layout from Target Dialog: ☑
☑ Use Default Compiler Version 6
```

## 内存配置文件

### 创建自定义Scatter文件
```scatter
; STM32F103ZE_Bootloader.sct
LR_IROM1 0x08000000 0x00010000  {    ; 加载域, 64KB Bootloader区域
  ER_IROM1 0x08000000 0x00010000  {  ; 执行域
   *.o (RESET, +First)
   *(InRoot$$Sections)
   .ANY (+RO)
  }
  RW_IRAM1 0x20000000 0x00010000  {  ; 64KB SRAM
   .ANY (+RW +ZI)
  }
}
```

### 链接器设置
```
Scatter File: ..\Project\MDK-ARM\STM32F103ZE_Bootloader.sct
```

## 启动文件配置

### startup_stm32f103xe.s修改
```assembly
; 修改堆栈大小
Stack_Size      EQU     0x00000800  ; 2KB stack

Heap_Size       EQU     0x00000400  ; 1KB heap

; 中断向量表
__Vectors       DCD     __initial_sp               ; Top of Stack
                DCD     Reset_Handler              ; Reset Handler
                DCD     NMI_Handler                ; NMI Handler  
                DCD     HardFault_Handler          ; Hard Fault Handler
                ; ... 保持其他向量不变
```

## 调试器配置

### ST-Link配置
```
Debug: ST-Link Debugger
Settings -> Debug:
  ☑ Use ST-LINK
  Port: SW (Serial Wire)
  Clock: 4MHz
  
Settings -> Flash Download:
  ☑ Download to Flash
  ☑ Reset and Run  
  ☑ Verify
  
Programming Algorithm:
  STM32F10x Med-density Flash
  Start: 0x08000000
  Size: 0x00008000
```

### 调试脚本
```
// Debug.ini - 调试初始化脚本
LOAD %L INCREMENTAL

// 设置断点
_break_init:
b main
b HardFault_Handler

// 初始化Flash
FLASH.INIT

g,main
```

## 编译脚本和批处理

### 自动编译脚本 (build.bat)
```batch
@echo off
echo Building STM32F103 Bootloader...

set KEIL_PATH="C:\Keil_v5\UV4\UV4.exe"
set PROJECT_PATH=".\Project\MDK-ARM\Bootloader.uvprojx"

%KEIL_PATH% -b %PROJECT_PATH% -t "STM32F103ZETx_Bootloader"

if %ERRORLEVEL%==0 (
    echo Build successful!
    echo Converting to binary...
    .\Tools\fromelf.exe --bin -o .\Objects\Bootloader.bin .\Objects\Bootloader.axf
    echo Done!
) else (
    echo Build failed!
)
pause
```

### 烧录脚本 (flash.bat)
```batch
@echo off
echo Flashing STM32F103 Bootloader...

set PROGRAMMER="C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
set HEX_FILE=".\Objects\Bootloader.hex"

%PROGRAMMER% -c port=SWD freq=4000 -d %HEX_FILE% 0x08000000 -v -rst

pause
```

## 库文件配置

### STM32F10x标准外设库
```
Libraries/
├── CMSIS/
│   ├── Include/
│   │   ├── core_cm3.h
│   │   ├── stm32f10x.h
│   │   └── system_stm32f10x.h
│   └── Source/
│       └── system_stm32f10x.c
└── STM32F10x_StdPeriph_Driver/
    ├── inc/
    │   ├── stm32f10x_rcc.h
    │   ├── stm32f10x_gpio.h
    │   ├── stm32f10x_usart.h
    │   ├── stm32f10x_spi.h
    │   └── stm32f10x_flash.h
    └── src/
        ├── stm32f10x_rcc.c
        ├── stm32f10x_gpio.c
        ├── stm32f10x_usart.c
        ├── stm32f10x_spi.c
        └── stm32f10x_flash.c
```

### 配置文件 (stm32f10x_conf.h)
```c
#ifndef __STM32F10x_CONF_H
#define __STM32F10x_CONF_H

// 包含需要的外设库
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_flash.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_iwdg.h"
#include "misc.h"

// 断言定义
#ifdef  USE_FULL_ASSERT
  #define assert_param(expr) ((expr) ? (void)0 : assert_failed((uint8_t *)__FILE__, __LINE__))
  void assert_failed(uint8_t* file, uint32_t line);
#else
  #define assert_param(expr) ((void)0)
#endif

#endif /* __STM32F10x_CONF_H */
```

## 版本控制配置

### .gitignore文件
```
# Keil MDK-ARM
*.uvguix.*
Objects/
Listings/
*.dep
*.crf
*.d
*.o
*.lst
*.map
*.htm
*.lnp
*.bak

# 临时文件
*.tmp
*.temp
*~
```

### Git初始化
```bash
git init
git add .
git commit -m "Initial commit: Development environment setup"
```

## 代码模板

### main.c模板
```c
/**
  ******************************************************************************
  * @file    main.c
  * @author  Your Name
  * @version V1.0.0
  * @date    2024-xx-xx
  * @brief   STM32F103ZET6 Bootloader Main Program
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"
#include "system_config.h"
#include "bootloader.h"

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{
    /* 系统初始化 */
    SystemInit();
    
    /* Bootloader初始化 */
    Bootloader_Init();
    
    /* 主循环 */
    while (1)
    {
        Bootloader_Process();
    }
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
    /* 硬件错误处理 */
    while (1)
    {
        // TODO: 错误指示和复位处理
    }
}
```

## 验证步骤

### 开发环境验证
1. **编译测试**：创建空项目能正常编译
2. **下载测试**：能正确下载到目标板
3. **调试测试**：断点调试功能正常
4. **串口测试**：串口输出Hello World

### 工具链验证
```c
// test_main.c - 验证代码
#include "stm32f10x.h"

int main(void)
{
    SystemInit();
    
    // LED闪烁测试
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    while(1)
    {
        GPIO_SetBits(GPIOB, GPIO_Pin_5);
        for(int i = 0; i < 1000000; i++);
        
        GPIO_ResetBits(GPIOB, GPIO_Pin_5);
        for(int i = 0; i < 1000000; i++);
    }
}
```

## 常见问题解决

### 编译错误
1. **找不到头文件**：检查Include路径配置
2. **链接错误**：检查库文件路径和Scatter文件
3. **存储器溢出**：检查代码大小是否超过32KB

### 下载问题
1. **无法连接**：检查ST-Link驱动和连接
2. **下载失败**：检查BOOT引脚和目标板电源
3. **程序无法运行**：检查启动代码和中断向量表

## 下一步行动
开发环境搭建完成后，开始底层驱动开发。