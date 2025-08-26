# 阶段3-1：启动逻辑实现

## 概述
实现STM32F103ZET6 Bootloader的核心启动逻辑，包括3秒按键检测、应用程序有效性检查和程序跳转功能。

## 启动流程状态机

### 状态定义
```c
typedef enum {
    BOOT_STATE_INIT = 0,        // 初始化状态
    BOOT_STATE_KEY_DETECT,      // 按键检测状态
    BOOT_STATE_APP_CHECK,       // 应用程序检查状态
    BOOT_STATE_APP_JUMP,        // 应用程序跳转状态
    BOOT_STATE_BOOTLOADER,      // Bootloader模式状态
    BOOT_STATE_ERROR            // 错误状态
} Boot_State_t;

typedef struct {
    Boot_State_t state;
    uint32_t timeout;
    uint32_t tick_start;
    uint8_t error_code;
} Boot_Manager_t;
```

### 启动管理器实现 (boot_manager.c)
```c
/**
  * @file    boot_manager.c
  * @brief   启动管理器实现
  */

#include "boot_manager.h"
#include "gpio_driver.h"
#include "uart_driver.h"
#include "flash_manager.h"
#include "app_jump.h"

static Boot_Manager_t g_boot_manager;

/**
  * @brief  启动管理器初始化
  * @param  None
  * @retval None
  */
void Boot_Manager_Init(void)
{
    /* 初始化启动管理器 */
    g_boot_manager.state = BOOT_STATE_INIT;
    g_boot_manager.timeout = 0;
    g_boot_manager.tick_start = 0;
    g_boot_manager.error_code = 0;
    
    /* 初始化硬件 */
    GPIO_Driver_Init();
    UART1_Init(115200);
    
    /* 显示启动信息 */
    printf("\r\n");
    printf("========================================\r\n");
    printf("STM32F103ZET6 Bootloader V1.0.0\r\n");
    printf("Build: %s %s\r\n", __DATE__, __TIME__);
    printf("========================================\r\n");
    
    /* 点亮电源LED */
    GPIO_SetLED(LED_POWER, LED_ON);
    
    /* 进入按键检测状态 */
    Boot_Manager_SetState(BOOT_STATE_KEY_DETECT);
}

/**
  * @brief  启动管理器主循环
  * @param  None
  * @retval None
  */
void Boot_Manager_Process(void)
{
    switch (g_boot_manager.state)
    {
        case BOOT_STATE_INIT:
            /* 初始化完成，不应该到达这里 */
            break;
            
        case BOOT_STATE_KEY_DETECT:
            Boot_Process_KeyDetect();
            break;
            
        case BOOT_STATE_APP_CHECK:
            Boot_Process_AppCheck();
            break;
            
        case BOOT_STATE_APP_JUMP:
            Boot_Process_AppJump();
            break;
            
        case BOOT_STATE_BOOTLOADER:
            Boot_Process_Bootloader();
            break;
            
        case BOOT_STATE_ERROR:
            Boot_Process_Error();
            break;
            
        default:
            Boot_Manager_SetState(BOOT_STATE_ERROR);
            break;
    }
}

/**
  * @brief  设置启动状态
  * @param  state: 新状态
  * @retval None
  */
void Boot_Manager_SetState(Boot_State_t state)
{
    g_boot_manager.state = state;
    g_boot_manager.tick_start = HAL_GetTick();
    
    switch (state)
    {
        case BOOT_STATE_KEY_DETECT:
            g_boot_manager.timeout = 3000; // 3秒超时
            printf("Boot: Waiting for key press (3s)...\r\n");
            GPIO_SetLED(LED_STATUS, LED_ON);
            break;
            
        case BOOT_STATE_APP_CHECK:
            printf("Boot: Checking application...\r\n");
            break;
            
        case BOOT_STATE_APP_JUMP:
            printf("Boot: Jumping to application...\r\n");
            GPIO_SetLED(LED_STATUS, LED_OFF);
            break;
            
        case BOOT_STATE_BOOTLOADER:
            printf("Boot: Entering bootloader mode...\r\n");
            GPIO_BlinkLED(LED_STATUS, 3, 200);
            break;
            
        case BOOT_STATE_ERROR:
            printf("Boot: Error occurred!\r\n");
            GPIO_SetLED(LED_ERROR, LED_ON);
            break;
            
        default:
            break;
    }
}

/**
  * @brief  按键检测处理
  * @param  None
  * @retval None
  */
static void Boot_Process_KeyDetect(void)
{
    uint32_t elapsed = HAL_GetTick() - g_boot_manager.tick_start;
    
    /* 检查任意按键是否按下 */
    if (GPIO_ReadKey(KEY_BOOT) == KEY_PRESSED || 
        GPIO_ReadKey(KEY1) == KEY_PRESSED || 
        GPIO_ReadKey(KEY2) == KEY_PRESSED)
    {
        printf("Boot: Key pressed! Entering bootloader...\r\n");
        Boot_Manager_SetState(BOOT_STATE_BOOTLOADER);
        return;
    }
    
    /* 显示倒计时 */
    static uint32_t last_second = 0;
    uint32_t current_second = elapsed / 1000;
    if (current_second != last_second && current_second <= 3)
    {
        printf("Boot: %d...\r\n", 3 - current_second);
        last_second = current_second;
        
        /* LED闪烁指示倒计时 */
        GPIO_BlinkLED(LED_STATUS, 1, 100);
    }
    
    /* 超时检查 */
    if (elapsed >= g_boot_manager.timeout)
    {
        printf("Boot: Timeout! Checking application...\r\n");
        Boot_Manager_SetState(BOOT_STATE_APP_CHECK);
    }
}

/**
  * @brief  应用程序检查处理
  * @param  None
  * @retval None
  */
static void Boot_Process_AppCheck(void)
{
    /* 检查应用程序有效性 */
    App_Check_Result_t result = App_Check_Validity();
    
    switch (result)
    {
        case APP_VALID:
            printf("Boot: Application is valid\r\n");
            Boot_Manager_SetState(BOOT_STATE_APP_JUMP);
            break;
            
        case APP_INVALID_STACK:
            printf("Boot: Invalid stack pointer\r\n");
            Boot_Manager_SetState(BOOT_STATE_BOOTLOADER);
            break;
            
        case APP_INVALID_CRC:
            printf("Boot: Invalid CRC checksum\r\n");
            Boot_Manager_SetState(BOOT_STATE_BOOTLOADER);
            break;
            
        case APP_NOT_FOUND:
            printf("Boot: Application not found\r\n");
            Boot_Manager_SetState(BOOT_STATE_BOOTLOADER);
            break;
            
        default:
            Boot_Manager_SetState(BOOT_STATE_ERROR);
            break;
    }
}

/**
  * @brief  应用程序跳转处理
  * @param  None
  * @retval None
  */
static void Boot_Process_AppJump(void)
{
    printf("Boot: Starting application...\r\n");
    Delay_ms(100);
    
    /* 关闭所有LED */
    GPIO_SetLED(LED_POWER, LED_OFF);
    GPIO_SetLED(LED_STATUS, LED_OFF);
    GPIO_SetLED(LED_ERROR, LED_OFF);
    
    /* 跳转到应用程序 */
    App_Jump_To_Application();
    
    /* 如果跳转失败，进入Bootloader模式 */
    printf("Boot: Jump failed! Entering bootloader...\r\n");
    Boot_Manager_SetState(BOOT_STATE_BOOTLOADER);
}

/**
  * @brief  Bootloader模式处理
  * @param  None
  * @retval None
  */
static void Boot_Process_Bootloader(void)
{
    /* 设置状态LED为慢闪 */
    static uint32_t last_blink = 0;
    if (HAL_GetTick() - last_blink > 500)
    {
        GPIO_SetLED(LED_STATUS, 
            (GPIO_ReadOutputDataBit(GPIOE, GPIO_Pin_5) == SET) ? LED_OFF : LED_ON);
        last_blink = HAL_GetTick();
    }
    
    /* Bootloader主循环将在command_system.c中实现 */
    // Command_System_Process();
}

/**
  * @brief  错误处理
  * @param  None
  * @retval None
  */
static void Boot_Process_Error(void)
{
    /* 错误LED快闪 */
    GPIO_BlinkLED(LED_ERROR, 1, 100);
    
    printf("Boot: System error, restarting...\r\n");
    Delay_ms(5000);
    
    /* 软件复位 */
    NVIC_SystemReset();
}

/**
  * @brief  获取当前启动状态
  * @param  None
  * @retval 当前状态
  */
Boot_State_t Boot_Manager_GetState(void)
{
    return g_boot_manager.state;
}
```

## 应用程序检查模块

### 应用程序检查实现 (app_check.c)
```c
/**
  * @file    app_check.c  
  * @brief   应用程序有效性检查
  */

#include "app_check.h"
#include "flash_manager.h"
#include <string.h>

#define APP_START_ADDRESS       0x08008000
#define APP_VECTOR_TABLE_SIZE   256
#define SRAM_BASE              0x20000000
#define SRAM_SIZE              0x10000

/**
  * @brief  检查应用程序有效性
  * @param  None
  * @retval 检查结果
  */
App_Check_Result_t App_Check_Validity(void)
{
    uint32_t app_stack_ptr;
    uint32_t app_reset_handler;
    
    /* 读取应用程序栈指针 */
    app_stack_ptr = *(__IO uint32_t*)APP_START_ADDRESS;
    
    /* 检查栈指针是否在SRAM范围内 */
    if ((app_stack_ptr < SRAM_BASE) || 
        (app_stack_ptr > (SRAM_BASE + SRAM_SIZE)))
    {
        printf("App Check: Invalid stack pointer 0x%08X\r\n", app_stack_ptr);
        return APP_INVALID_STACK;
    }
    
    /* 读取应用程序复位向量 */
    app_reset_handler = *(__IO uint32_t*)(APP_START_ADDRESS + 4);
    
    /* 检查复位向量是否在应用程序区域内 */
    if ((app_reset_handler < APP_START_ADDRESS) || 
        (app_reset_handler > 0x0807EFFF))
    {
        printf("App Check: Invalid reset handler 0x%08X\r\n", app_reset_handler);
        return APP_INVALID_STACK;
    }
    
    /* 检查是否为空Flash (全为0xFF) */
    uint32_t* app_data = (uint32_t*)APP_START_ADDRESS;
    uint8_t all_ff = 1;
    
    for (int i = 0; i < 64; i++) // 检查前256字节
    {
        if (app_data[i] != 0xFFFFFFFF)
        {
            all_ff = 0;
            break;
        }
    }
    
    if (all_ff)
    {
        printf("App Check: No application found\r\n");
        return APP_NOT_FOUND;
    }
    
    /* CRC校验 (可选) */
    if (App_Check_CRC() != APP_CRC_OK)
    {
        printf("App Check: CRC verification failed\r\n");
        return APP_INVALID_CRC;
    }
    
    printf("App Check: Application is valid\r\n");
    printf("  - Stack Pointer: 0x%08X\r\n", app_stack_ptr);
    printf("  - Reset Handler: 0x%08X\r\n", app_reset_handler);
    
    return APP_VALID;
}

/**
  * @brief  CRC校验 (简单实现)
  * @param  None
  * @retval CRC检查结果
  */
static App_CRC_Result_t App_Check_CRC(void)
{
    /* 简单的校验和计算 */
    uint32_t* app_data = (uint32_t*)APP_START_ADDRESS;
    uint32_t checksum = 0;
    uint32_t size = 1024; // 检查前4KB
    
    for (uint32_t i = 0; i < size/4; i++)
    {
        checksum += app_data[i];
    }
    
    /* 这里可以与存储的CRC值比较 */
    /* 暂时总是返回OK */
    return APP_CRC_OK;
}

/**
  * @brief  获取应用程序信息
  * @param  info: 信息结构体指针
  * @retval None
  */
void App_Get_Info(App_Info_t* info)
{
    if (info == NULL) return;
    
    info->start_address = APP_START_ADDRESS;
    info->stack_pointer = *(__IO uint32_t*)APP_START_ADDRESS;
    info->reset_handler = *(__IO uint32_t*)(APP_START_ADDRESS + 4);
    info->size = 0x76000; // 472KB
    info->valid = (App_Check_Validity() == APP_VALID);
}
```

## 应用程序跳转模块

### 程序跳转实现 (app_jump.c)
```c
/**
  * @file    app_jump.c
  * @brief   应用程序跳转功能
  */

#include "app_jump.h"
#include "stm32f10x.h"

#define APP_START_ADDRESS   0x08008000

typedef void (*pFunction)(void);

/**
  * @brief  跳转到应用程序
  * @param  None
  * @retval None
  */
void App_Jump_To_Application(void)
{
    uint32_t jump_address;
    pFunction jump_to_application;
    
    /* 检查应用程序有效性 */
    if (App_Check_Validity() != APP_VALID)
    {
        return;
    }
    
    /* 关闭所有中断 */
    __disable_irq();
    
    /* 关闭SysTick */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
    
    /* 关闭所有外设时钟 */
    App_Deinit_Peripherals();
    
    /* 获取应用程序跳转地址 */
    jump_address = *(__IO uint32_t*)(APP_START_ADDRESS + 4);
    jump_to_application = (pFunction)jump_address;
    
    /* 重新设置主栈指针 */
    __set_MSP(*(__IO uint32_t*)APP_START_ADDRESS);
    
    /* 设置向量表偏移 */
    SCB->VTOR = APP_START_ADDRESS;
    
    /* 跳转到应用程序 */
    jump_to_application();
}

/**
  * @brief  关闭外设初始化
  * @param  None
  * @retval None
  */
static void App_Deinit_Peripherals(void)
{
    /* 关闭UART */
    USART_DeInit(USART1);
    USART_DeInit(USART2);
    
    /* 关闭SPI */
    SPI_DeInit(SPI1);
    
    /* 复位GPIO */
    GPIO_DeInit(GPIOA);
    GPIO_DeInit(GPIOB);
    GPIO_DeInit(GPIOE);
    
    /* 关闭外设时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | 
                          RCC_APB2Periph_SPI1 | 
                          RCC_APB2Periph_GPIOA |
                          RCC_APB2Periph_GPIOB |
                          RCC_APB2Periph_GPIOE, DISABLE);
    
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, DISABLE);
    
    /* 清除所有中断挂起位 */
    for (int i = 0; i < 8; i++)
    {
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }
}

/**
  * @brief  复位系统 (软件复位)
  * @param  None
  * @retval None
  */
void App_System_Reset(void)
{
    printf("System: Software reset...\r\n");
    Delay_ms(100);
    
    NVIC_SystemReset();
}
```

## 头文件定义

### boot_manager.h
```c
#ifndef __BOOT_MANAGER_H
#define __BOOT_MANAGER_H

#include "stm32f10x.h"
#include "system_clock.h"

/* 启动状态定义 */
typedef enum {
    BOOT_STATE_INIT = 0,
    BOOT_STATE_KEY_DETECT,
    BOOT_STATE_APP_CHECK,
    BOOT_STATE_APP_JUMP,
    BOOT_STATE_BOOTLOADER,
    BOOT_STATE_ERROR
} Boot_State_t;

/* 函数声明 */
void Boot_Manager_Init(void);
void Boot_Manager_Process(void);
void Boot_Manager_SetState(Boot_State_t state);
Boot_State_t Boot_Manager_GetState(void);

/* 私有函数声明 */
static void Boot_Process_KeyDetect(void);
static void Boot_Process_AppCheck(void);
static void Boot_Process_AppJump(void);
static void Boot_Process_Bootloader(void);
static void Boot_Process_Error(void);

#endif /* __BOOT_MANAGER_H */
```

### app_check.h
```c
#ifndef __APP_CHECK_H
#define __APP_CHECK_H

#include "stm32f10x.h"

/* 应用程序检查结果 */
typedef enum {
    APP_VALID = 0,
    APP_INVALID_STACK,
    APP_INVALID_CRC,
    APP_NOT_FOUND
} App_Check_Result_t;

/* CRC检查结果 */
typedef enum {
    APP_CRC_OK = 0,
    APP_CRC_ERROR
} App_CRC_Result_t;

/* 应用程序信息结构 */
typedef struct {
    uint32_t start_address;
    uint32_t stack_pointer;
    uint32_t reset_handler;
    uint32_t size;
    uint8_t valid;
} App_Info_t;

/* 函数声明 */
App_Check_Result_t App_Check_Validity(void);
void App_Get_Info(App_Info_t* info);
static App_CRC_Result_t App_Check_CRC(void);

#endif /* __APP_CHECK_H */
```

### app_jump.h
```c
#ifndef __APP_JUMP_H
#define __APP_JUMP_H

#include "stm32f10x.h"
#include "app_check.h"

/* 函数声明 */
void App_Jump_To_Application(void);
void App_System_Reset(void);
static void App_Deinit_Peripherals(void);

#endif /* __APP_JUMP_H */
```

## 测试验证

### 测试用例
1. **按键检测测试**：
   - 上电后立即按键 -> 进入Bootloader
   - 上电后2秒按键 -> 进入Bootloader  
   - 上电后不按键 -> 3秒后检查应用程序

2. **应用程序检查测试**：
   - 空Flash -> 进入Bootloader
   - 无效栈指针 -> 进入Bootloader
   - 有效应用程序 -> 跳转到应用

3. **程序跳转测试**：
   - 成功跳转 -> 应用程序正常运行
   - 跳转失败 -> 返回Bootloader

### 调试方法
```c
// 在main.c中添加测试代码
int main(void)
{
    SystemInit();
    SystemClock_Config();
    
    Boot_Manager_Init();
    
    while (1)
    {
        Boot_Manager_Process();
        Delay_ms(10);
    }
}
```

## 验证标准
1. 按键检测响应时间 < 100ms
2. 3秒倒计时精度 ±50ms
3. 应用程序检查时间 < 500ms
4. 程序跳转时间 < 50ms
5. 错误状态能正确处理和恢复

## 下一步行动
启动逻辑实现完成后，继续开发命令系统。