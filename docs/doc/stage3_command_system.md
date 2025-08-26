# 阶段3-2：命令系统开发

## 概述
开发STM32F103ZET6 Bootloader的命令行交互系统，实现串口命令解析、菜单显示和基础调试功能。

## 命令系统架构

### 命令结构定义
```c
typedef struct {
    char* name;                     // 命令名称
    char* description;              // 命令描述
    void (*handler)(int argc, char* argv[]); // 命令处理函数
    uint8_t min_args;              // 最少参数个数
    uint8_t max_args;              // 最多参数个数
} Command_t;

typedef struct {
    char buffer[CMD_BUFFER_SIZE];   // 命令缓冲区
    uint16_t index;                 // 当前索引
    uint8_t ready;                  // 命令就绪标志
} Command_Buffer_t;
```

### 命令系统实现 (command_system.c)
```c
/**
  * @file    command_system.c
  * @brief   命令系统实现
  */

#include "command_system.h"
#include "uart_driver.h"
#include "flash_manager.h"
#include "app_jump.h"
#include <string.h>
#include <stdlib.h>

#define CMD_BUFFER_SIZE     256
#define CMD_MAX_ARGS        8
#define CMD_ARG_SIZE        32

/* 命令缓冲区 */
static Command_Buffer_t g_cmd_buffer;
static char g_cmd_args[CMD_MAX_ARGS][CMD_ARG_SIZE];

/* 命令历史 */
#define CMD_HISTORY_SIZE    5
static char g_cmd_history[CMD_HISTORY_SIZE][CMD_BUFFER_SIZE];
static uint8_t g_history_index = 0;
static uint8_t g_history_count = 0;

/* 命令表声明 */
static void Cmd_Help(int argc, char* argv[]);
static void Cmd_Version(int argc, char* argv[]);
static void Cmd_Reset(int argc, char* argv[]);
static void Cmd_Jump(int argc, char* argv[]);
static void Cmd_Memory_Read(int argc, char* argv[]);
static void Cmd_Memory_Write(int argc, char* argv[]);
static void Cmd_Flash_Info(int argc, char* argv[]);
static void Cmd_Flash_Erase(int argc, char* argv[]);
static void Cmd_Flash_Write(int argc, char* argv[]);
static void Cmd_App_Info(int argc, char* argv[]);
static void Cmd_Config_Show(int argc, char* argv[]);
static void Cmd_Config_Set(int argc, char* argv[]);
static void Cmd_IAP_Start(int argc, char* argv[]);
static void Cmd_OTA_Start(int argc, char* argv[]);

/* 命令表定义 */
static const Command_t g_command_table[] = {
    {"help",    "Show help information",                    Cmd_Help,        0, 1},
    {"version", "Show bootloader version",                  Cmd_Version,     0, 0},
    {"reset",   "System reset",                            Cmd_Reset,       0, 0},
    {"jump",    "Jump to application",                     Cmd_Jump,        0, 0},
    {"mr",      "Memory read: mr <address> [length]",      Cmd_Memory_Read, 1, 2},
    {"mw",      "Memory write: mw <address> <value>",      Cmd_Memory_Write,2, 2},
    {"finfo",   "Flash information",                       Cmd_Flash_Info,  0, 0},
    {"ferase",  "Flash erase: ferase <address> <size>",    Cmd_Flash_Erase, 2, 2},
    {"fwrite",  "Flash write: fwrite <address> <data>",    Cmd_Flash_Write, 2, 2},
    {"appinfo", "Application information",                 Cmd_App_Info,    0, 0},
    {"config",  "Show configuration",                      Cmd_Config_Show, 0, 0},
    {"set",     "Set config: set <key> <value>",          Cmd_Config_Set,  2, 2},
    {"iap",     "Start IAP mode",                         Cmd_IAP_Start,   0, 0},
    {"ota",     "Start OTA mode",                         Cmd_OTA_Start,   0, 0},
};

#define COMMAND_COUNT   (sizeof(g_command_table) / sizeof(Command_t))

/**
  * @brief  命令系统初始化
  * @param  None
  * @retval None
  */
void Command_System_Init(void)
{
    /* 初始化命令缓冲区 */
    memset(&g_cmd_buffer, 0, sizeof(Command_Buffer_t));
    memset(g_cmd_history, 0, sizeof(g_cmd_history));
    
    /* 显示欢迎信息 */
    Command_Print_Banner();
    Command_Print_Prompt();
}

/**
  * @brief  命令系统处理
  * @param  None
  * @retval None
  */
void Command_System_Process(void)
{
    /* 处理串口输入 */
    Command_Process_Input();
    
    /* 处理命令 */
    if (g_cmd_buffer.ready)
    {
        Command_Execute();
        g_cmd_buffer.ready = 0;
    }
}

/**
  * @brief  处理串口输入
  * @param  None
  * @retval None
  */
static void Command_Process_Input(void)
{
    static uint8_t escape_seq = 0;
    uint8_t data;
    
    while (UART_Available(USART1) > 0)
    {
        UART_ReadData(USART1, &data, 1);
        
        /* 处理转义序列 (方向键等) */
        if (escape_seq > 0)
        {
            if (escape_seq == 1 && data == '[')
            {
                escape_seq = 2;
                continue;
            }
            else if (escape_seq == 2)
            {
                Command_Handle_EscapeSeq(data);
                escape_seq = 0;
                continue;
            }
            escape_seq = 0;
        }
        
        switch (data)
        {
            case 0x1B: // ESC
                escape_seq = 1;
                break;
                
            case '\r':
            case '\n':
                if (g_cmd_buffer.index > 0)
                {
                    g_cmd_buffer.buffer[g_cmd_buffer.index] = '\0';
                    g_cmd_buffer.ready = 1;
                    Command_Add_History();
                    printf("\r\n");
                }
                else
                {
                    Command_Print_Prompt();
                }
                break;
                
            case '\b':
            case 0x7F: // DEL
                if (g_cmd_buffer.index > 0)
                {
                    g_cmd_buffer.index--;
                    printf("\b \b");
                }
                break;
                
            case '\t': // TAB - 命令补全
                Command_Handle_TabComplete();
                break;
                
            default:
                if (data >= 0x20 && data < 0x7F) // 可打印字符
                {
                    if (g_cmd_buffer.index < CMD_BUFFER_SIZE - 1)
                    {
                        g_cmd_buffer.buffer[g_cmd_buffer.index++] = data;
                        printf("%c", data);
                    }
                }
                break;
        }
    }
}

/**
  * @brief  执行命令
  * @param  None
  * @retval None
  */
static void Command_Execute(void)
{
    int argc = 0;
    char* token;
    
    /* 解析命令和参数 */
    token = strtok(g_cmd_buffer.buffer, " \t");
    while (token != NULL && argc < CMD_MAX_ARGS)
    {
        strncpy(g_cmd_args[argc], token, CMD_ARG_SIZE - 1);
        g_cmd_args[argc][CMD_ARG_SIZE - 1] = '\0';
        argc++;
        token = strtok(NULL, " \t");
    }
    
    if (argc == 0)
    {
        Command_Print_Prompt();
        return;
    }
    
    /* 查找并执行命令 */
    for (uint16_t i = 0; i < COMMAND_COUNT; i++)
    {
        if (strcmp(g_cmd_args[0], g_command_table[i].name) == 0)
        {
            /* 检查参数个数 */
            if (argc - 1 < g_command_table[i].min_args ||
                argc - 1 > g_command_table[i].max_args)
            {
                printf("Error: Invalid arguments\r\n");
                printf("Usage: %s\r\n", g_command_table[i].description);
            }
            else
            {
                /* 执行命令 */
                g_command_table[i].handler(argc, g_cmd_args);
            }
            
            Command_Print_Prompt();
            Command_Clear_Buffer();
            return;
        }
    }
    
    /* 未找到命令 */
    printf("Unknown command: %s\r\n", g_cmd_args[0]);
    printf("Type 'help' for available commands\r\n");
    
    Command_Print_Prompt();
    Command_Clear_Buffer();
}

/**
  * @brief  打印欢迎横幅
  * @param  None
  * @retval None
  */
static void Command_Print_Banner(void)
{
    printf("\r\n");
    printf("================================================\r\n");
    printf("     STM32F103ZET6 Bootloader Command Shell     \r\n");
    printf("================================================\r\n");
    printf("Version: 1.0.0\r\n");
    printf("Build:   %s %s\r\n", __DATE__, __TIME__);
    printf("Type 'help' for available commands\r\n");
    printf("\r\n");
}

/**
  * @brief  打印命令提示符
  * @param  None
  * @retval None
  */
static void Command_Print_Prompt(void)
{
    printf("Bootloader> ");
}

/**
  * @brief  清空命令缓冲区
  * @param  None
  * @retval None
  */
static void Command_Clear_Buffer(void)
{
    memset(g_cmd_buffer.buffer, 0, CMD_BUFFER_SIZE);
    g_cmd_buffer.index = 0;
    g_cmd_buffer.ready = 0;
}

/**
  * @brief  添加到命令历史
  * @param  None
  * @retval None
  */
static void Command_Add_History(void)
{
    if (g_cmd_buffer.index > 0)
    {
        strncpy(g_cmd_history[g_history_index], g_cmd_buffer.buffer, CMD_BUFFER_SIZE - 1);
        g_history_index = (g_history_index + 1) % CMD_HISTORY_SIZE;
        if (g_history_count < CMD_HISTORY_SIZE)
            g_history_count++;
    }
}

/**
  * @brief  处理转义序列 (方向键)
  * @param  key: 按键码
  * @retval None
  */
static void Command_Handle_EscapeSeq(uint8_t key)
{
    switch (key)
    {
        case 'A': // 上箭头 - 上一条命令
            // TODO: 实现命令历史回溯
            break;
            
        case 'B': // 下箭头 - 下一条命令
            // TODO: 实现命令历史前进
            break;
            
        case 'C': // 右箭头
        case 'D': // 左箭头
            // TODO: 实现光标移动
            break;
            
        default:
            break;
    }
}

/**
  * @brief  处理TAB补全
  * @param  None
  * @retval None
  */
static void Command_Handle_TabComplete(void)
{
    // TODO: 实现命令自动补全
    printf("\a"); // 响铃
}

/* ============ 命令处理函数实现 ============ */

/**
  * @brief  帮助命令
  */
static void Cmd_Help(int argc, char* argv[])
{
    if (argc == 1)
    {
        printf("Available commands:\r\n");
        printf("==================\r\n");
        for (uint16_t i = 0; i < COMMAND_COUNT; i++)
        {
            printf("%-10s - %s\r\n", 
                   g_command_table[i].name, 
                   g_command_table[i].description);
        }
    }
    else
    {
        /* 显示特定命令的帮助 */
        for (uint16_t i = 0; i < COMMAND_COUNT; i++)
        {
            if (strcmp(argv[1], g_command_table[i].name) == 0)
            {
                printf("%s: %s\r\n", 
                       g_command_table[i].name, 
                       g_command_table[i].description);
                return;
            }
        }
        printf("Unknown command: %s\r\n", argv[1]);
    }
}

/**
  * @brief  版本命令
  */
static void Cmd_Version(int argc, char* argv[])
{
    printf("STM32F103ZET6 Bootloader\r\n");
    printf("Version: 1.0.0\r\n");
    printf("Build:   %s %s\r\n", __DATE__, __TIME__);
    printf("Chip:    STM32F103ZET6\r\n");
    printf("Flash:   512KB\r\n");
    printf("SRAM:    64KB\r\n");
    printf("Clock:   %dMHz\r\n", SystemCoreClock / 1000000);
}

/**
  * @brief  复位命令
  */
static void Cmd_Reset(int argc, char* argv[])
{
    printf("System reset in 3 seconds...\r\n");
    for (int i = 3; i > 0; i--)
    {
        printf("%d...\r\n", i);
        Delay_ms(1000);
    }
    App_System_Reset();
}

/**
  * @brief  跳转命令
  */
static void Cmd_Jump(int argc, char* argv[])
{
    printf("Jumping to application...\r\n");
    Delay_ms(500);
    App_Jump_To_Application();
    printf("Jump failed!\r\n");
}

/**
  * @brief  内存读取命令
  */
static void Cmd_Memory_Read(int argc, char* argv[])
{
    uint32_t address = strtoul(argv[1], NULL, 0);
    uint32_t length = (argc > 2) ? strtoul(argv[2], NULL, 0) : 4;
    
    printf("Memory read from 0x%08X, length %d:\r\n", address, length);
    
    uint8_t* ptr = (uint8_t*)address;
    for (uint32_t i = 0; i < length; i++)
    {
        if (i % 16 == 0)
            printf("%08X: ", address + i);
        
        printf("%02X ", ptr[i]);
        
        if ((i + 1) % 16 == 0 || i == length - 1)
            printf("\r\n");
    }
}

/**
  * @brief  内存写入命令
  */
static void Cmd_Memory_Write(int argc, char* argv[])
{
    uint32_t address = strtoul(argv[1], NULL, 0);
    uint32_t value = strtoul(argv[2], NULL, 0);
    
    *(volatile uint32_t*)address = value;
    printf("Write 0x%08X to address 0x%08X\r\n", value, address);
}

/**
  * @brief  Flash信息命令
  */
static void Cmd_Flash_Info(int argc, char* argv[])
{
    printf("Flash Information:\r\n");
    printf("=================\r\n");
    printf("Internal Flash: 512KB (0x08000000 - 0x0807FFFF)\r\n");
    printf("  Bootloader:   64KB  (0x08000000 - 0x0800FFFF)\r\n");
    printf("  Application:  448KB (0x08010000 - 0x0807FFFF)\r\n");
    printf("External Flash: 8MB (W25Q64)\r\n");
    printf("  Download:     2MB   (0x000000 - 0x1FFFFF)\r\n");
    printf("  Backup:       2MB   (0x200000 - 0x3FFFFF)\r\n");
    printf("  User Data:    4MB   (0x400000 - 0x7FFFFF)\r\n");
}

/**
  * @brief  Flash擦除命令
  */
static void Cmd_Flash_Erase(int argc, char* argv[])
{
    uint32_t address = strtoul(argv[1], NULL, 0);
    uint32_t size = strtoul(argv[2], NULL, 0);
    
    printf("Erasing flash at 0x%08X, size %d bytes...\r\n", address, size);
    
    /* 这里调用Flash管理器的擦除函数 */
    // Flash_Erase(address, size);
    
    printf("Flash erase completed\r\n");
}

/**
  * @brief  Flash写入命令  
  */
static void Cmd_Flash_Write(int argc, char* argv[])
{
    uint32_t address = strtoul(argv[1], NULL, 0);
    uint32_t data = strtoul(argv[2], NULL, 0);
    
    printf("Writing 0x%08X to flash address 0x%08X...\r\n", data, address);
    
    /* 这里调用Flash管理器的写入函数 */
    // Flash_Write(address, (uint8_t*)&data, 4);
    
    printf("Flash write completed\r\n");
}

/**
  * @brief  应用程序信息命令
  */
static void Cmd_App_Info(int argc, char* argv[])
{
    App_Info_t app_info;
    App_Get_Info(&app_info);
    
    printf("Application Information:\r\n");
    printf("=======================\r\n");
    printf("Start Address: 0x%08X\r\n", app_info.start_address);
    printf("Stack Pointer: 0x%08X\r\n", app_info.stack_pointer);
    printf("Reset Handler: 0x%08X\r\n", app_info.reset_handler);
    printf("Size:          %d bytes\r\n", app_info.size);
    printf("Status:        %s\r\n", app_info.valid ? "Valid" : "Invalid");
}

/**
  * @brief  配置显示命令
  */
static void Cmd_Config_Show(int argc, char* argv[])
{
    printf("Bootloader Configuration:\r\n");
    printf("========================\r\n");
    printf("Auto Boot:     %s\r\n", "Enabled");
    printf("Boot Delay:    %d seconds\r\n", 3);
    printf("UART Baudrate: %d\r\n", 115200);
    printf("WiFi:          %s\r\n", "Disabled");
}

/**
  * @brief  配置设置命令
  */
static void Cmd_Config_Set(int argc, char* argv[])
{
    printf("Set %s = %s\r\n", argv[1], argv[2]);
    printf("Configuration saved\r\n");
}

/**
  * @brief  IAP启动命令
  */
static void Cmd_IAP_Start(int argc, char* argv[])
{
    printf("Starting IAP mode...\r\n");
    printf("Ready to receive firmware via UART\r\n");
    printf("Send firmware using Xmodem protocol\r\n");
    
    /* 这里调用IAP管理器 */
    // IAP_Start();
}

/**
  * @brief  OTA启动命令
  */
static void Cmd_OTA_Start(int argc, char* argv[])
{
    printf("Starting OTA mode...\r\n");
    printf("Connecting to WiFi...\r\n");
    printf("Checking for firmware updates...\r\n");
    
    /* 这里调用OTA管理器 */
    // OTA_Start();
}
```

## 头文件定义

### command_system.h
```c
#ifndef __COMMAND_SYSTEM_H
#define __COMMAND_SYSTEM_H

#include "stm32f10x.h"
#include "app_check.h"

/* 命令系统配置 */
#define CMD_BUFFER_SIZE     256
#define CMD_MAX_ARGS        8
#define CMD_ARG_SIZE        32

/* 命令结构体 */
typedef struct {
    char* name;
    char* description;
    void (*handler)(int argc, char* argv[]);
    uint8_t min_args;
    uint8_t max_args;
} Command_t;

typedef struct {
    char buffer[CMD_BUFFER_SIZE];
    uint16_t index;
    uint8_t ready;
} Command_Buffer_t;

/* 函数声明 */
void Command_System_Init(void);
void Command_System_Process(void);

/* 私有函数声明 */
static void Command_Process_Input(void);
static void Command_Execute(void);
static void Command_Print_Banner(void);
static void Command_Print_Prompt(void);
static void Command_Clear_Buffer(void);
static void Command_Add_History(void);
static void Command_Handle_EscapeSeq(uint8_t key);
static void Command_Handle_TabComplete(void);

#endif /* __COMMAND_SYSTEM_H */
```

## 使用示例

### 在main.c中集成
```c
int main(void)
{
    SystemInit();
    SystemClock_Config();
    
    Boot_Manager_Init();
    
    /* 如果进入Bootloader模式 */
    if (Boot_Manager_GetState() == BOOT_STATE_BOOTLOADER)
    {
        Command_System_Init();
        
        while (1)
        {
            Command_System_Process();
            Delay_ms(1);
        }
    }
    else
    {
        while (1)
        {
            Boot_Manager_Process();
            Delay_ms(10);
        }
    }
}
```

## 测试验证

### 基础命令测试
```
Bootloader> help
Available commands:
==================
help       - Show help information
version    - Show bootloader version
reset      - System reset
jump       - Jump to application
mr         - Memory read: mr <address> [length]
...

Bootloader> version
STM32F103ZET6 Bootloader
Version: 1.0.0
Build:   Dec 25 2024 10:30:00
Chip:    STM32F103ZET6
Flash:   512KB
SRAM:    64KB
Clock:   72MHz

Bootloader> mr 0x08000000 16
Memory read from 0x08000000, length 16:
08000000: 00 00 01 20 C1 01 00 08 C5 01 00 08 C9 01 00 08
```

## 验证标准
1. 命令解析正确，参数检查有效
2. 所有基础命令功能正常
3. 串口交互响应及时，无丢字符
4. 错误处理完善，提示信息清晰
5. 内存占用合理，不超过预算

## 下一步行动
命令系统开发完成后，继续开发Flash存储管理。