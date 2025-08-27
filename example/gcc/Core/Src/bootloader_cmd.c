#include "bootloader_cmd.h"
#include "main.h"
#include "dev_usart.h"
#include "gpio.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

/* 私有变量 */
static bootloader_state_t bootloader_state = BOOT_STATE_IDLE;
static char cmd_buffer[256];
static uint16_t cmd_index = 0;
static uint32_t boot_start_tick = 0;

/* 命令表 - 支持全名和缩写 */
static const bootloader_cmd_t cmd_table[] = {
    {"help",     "h",  "Show command list",           CMD_HELP,     cmd_help_handler},
    {"update",   "u",  "Update firmware via UART",    CMD_UPDATE,   cmd_update_handler},
    {"download", "d",  "Download firmware to PC",     CMD_DOWNLOAD, cmd_download_handler},
    {"info",     "i",  "Show firmware information",   CMD_INFO,     cmd_info_handler},
    {"erase",    "e",  "Erase application area",      CMD_ERASE,    cmd_erase_handler},
    {"reset",    "r",  "Reset system",                CMD_RESET,    cmd_reset_handler},
    {"jump",     "j",  "Jump to application",         CMD_JUMP,     cmd_jump_handler},
};

/* 私有函数声明 */
static void bootloader_print(const char* str);
static void bootloader_printf(const char* format, ...);
static void bootloader_process_cmd(char* cmd);
static bool bootloader_check_button(void);
static bool bootloader_check_uart_input(void);
static void bootloader_show_banner(void);

/* 初始化Bootloader */
void bootloader_init(void)
{
    boot_start_tick = HAL_GetTick();
    bootloader_state = BOOT_STATE_IDLE;
    cmd_index = 0;
    memset(cmd_buffer, 0, sizeof(cmd_buffer));
}

/* 检查是否进入Bootloader命令模式 */
bool bootloader_check_entry(uint32_t timeout_ms)
{
    uint32_t start_tick = HAL_GetTick();
    
    bootloader_print("\r\n");
    bootloader_print("========================================\r\n");
    bootloader_print("   STM32 Bootloader v1.0.0\r\n");
    bootloader_print("========================================\r\n");
    bootloader_printf("Press any key or button within %d seconds to enter bootloader...\r\n", timeout_ms/1000);
    
    /* 清空串口接收缓冲区 */
    uint8_t dummy[256];
    while(uart_read(DEV_UART1, dummy, sizeof(dummy)) > 0);
    
    /* 等待超时或用户输入 */
    while((HAL_GetTick() - start_tick) < timeout_ms)
    {
        /* 检查按键 - 假设使用PA0作为Boot按键 */
        if(bootloader_check_button())
        {
            bootloader_print("\r\nButton detected! Entering bootloader mode...\r\n");
            return true;
        }
        
        /* 检查串口输入 */
        if(bootloader_check_uart_input())
        {
            bootloader_print("\r\nUART input detected! Entering bootloader mode...\r\n");
            return true;
        }
        
        /* 显示倒计时 */
        static uint32_t last_second = 0;
        uint32_t current_second = (timeout_ms - (HAL_GetTick() - start_tick)) / 1000;
        if(current_second != last_second)
        {
            bootloader_printf("\rTime remaining: %d seconds... ", current_second);
            last_second = current_second;
        }
        
        HAL_Delay(10);
    }
    
    bootloader_print("\r\nTimeout! Jumping to application...\r\n");
    return false;
}

/* Bootloader命令模式主循环 */
void bootloader_cmd_mode(void)
{
    bootloader_state = BOOT_STATE_CMD_MODE;
    
    bootloader_show_banner();
    bootloader_print("Type 'help' or 'h' for command list\r\n");
    bootloader_print("BOOT> ");
    
    while(bootloader_state == BOOT_STATE_CMD_MODE)
    {
        uint8_t ch;
        uint16_t size = uart_read(DEV_UART1, &ch, 1);
        
        if(size > 0)
        {
            /* 回显字符 */
            uart_write(DEV_UART1, &ch, 1);
            uart_poll_dma_tx(DEV_UART1);
            
            /* 处理回车 */
            if(ch == '\r' || ch == '\n')
            {
                if(cmd_index > 0)
                {
                    cmd_buffer[cmd_index] = '\0';
                    bootloader_print("\r\n");
                    bootloader_process_cmd(cmd_buffer);
                    cmd_index = 0;
                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
                }
                else
                {
                    bootloader_print("\r\n");
                }
                
                if(bootloader_state == BOOT_STATE_CMD_MODE)
                {
                    bootloader_print("BOOT> ");
                }
            }
            /* 处理退格 */
            else if(ch == '\b' || ch == 0x7F)
            {
                if(cmd_index > 0)
                {
                    cmd_index--;
                    cmd_buffer[cmd_index] = '\0';
                    bootloader_print(" \b");
                }
            }
            /* 普通字符 */
            else if(cmd_index < (sizeof(cmd_buffer) - 1))
            {
                cmd_buffer[cmd_index++] = ch;
            }
        }
        
        /* 轮询发送 */
        uart_poll_dma_tx(DEV_UART1);
    }
}

/* 跳转到应用程序 */
void bootloader_jump_to_app(void)
{
    /* 检查应用程序是否有效 */
    if(!bootloader_validate_app())
    {
        bootloader_print("Invalid application! Cannot jump.\r\n");
        return;
    }
    
    bootloader_print("Jumping to application...\r\n");
    HAL_Delay(100);
    
    /* 关闭所有中断 */
    __disable_irq();
    
    /* 复位所有外设 */
    HAL_DeInit();
    
    /* 获取应用程序栈指针和复位向量 */
    uint32_t app_stack = *(__IO uint32_t*)APP_START_ADDR;
    uint32_t app_reset = *(__IO uint32_t*)(APP_START_ADDR + 4);
    
    /* 设置栈指针 */
    __set_MSP(app_stack);
    
    /* 设置向量表 */
    SCB->VTOR = APP_START_ADDR;
    
    /* 跳转到应用程序 */
    void (*app_reset_handler)(void) = (void (*)(void))app_reset;
    app_reset_handler();
}

/* 验证应用程序 */
bool bootloader_validate_app(void)
{
    uint32_t app_stack = *(__IO uint32_t*)APP_START_ADDR;
    
    /* 检查栈指针是否在RAM范围内 */
    if((app_stack < 0x20000000) || (app_stack > 0x20010000))
    {
        return false;
    }
    
    /* 检查复位向量是否在Flash范围内 */
    uint32_t app_reset = *(__IO uint32_t*)(APP_START_ADDR + 4);
    if((app_reset < APP_START_ADDR) || (app_reset > (APP_START_ADDR + APP_MAX_SIZE)))
    {
        return false;
    }
    
    return true;
}

/* 命令处理函数实现 */
void cmd_help_handler(void)
{
    bootloader_print("\r\nAvailable commands:\r\n");
    bootloader_print("=======================================================\r\n");
    bootloader_printf("  %-12s %-8s %s\r\n", "Command", "Short", "Description");
    bootloader_print("  ----------------------------------------------------\r\n");
    
    for(uint32_t i = 0; i < sizeof(cmd_table)/sizeof(cmd_table[0]); i++)
    {
        bootloader_printf("  %-12s %-8s %s\r\n", 
                         cmd_table[i].name, 
                         cmd_table[i].short_name,
                         cmd_table[i].description);
    }
    
    bootloader_print("=======================================================\r\n");
    bootloader_print("Note: You can use either full command or short form\r\n");
}

void cmd_update_handler(void)
{
    bootloader_print("Starting firmware update via UART...\r\n");
    bootloader_print("Please send firmware file using XMODEM protocol\r\n");
    bootloader_print("(This feature will be implemented with XMODEM protocol)\r\n");
    // TODO: 实现XMODEM协议接收固件
}

void cmd_download_handler(void)
{
    bootloader_print("Starting firmware download...\r\n");
    
    if(!bootloader_validate_app())
    {
        bootloader_print("No valid application found!\r\n");
        return;
    }
    
    bootloader_print("Ready to send firmware via XMODEM protocol\r\n");
    bootloader_print("(This feature will be implemented with XMODEM protocol)\r\n");
    // TODO: 实现XMODEM协议发送固件
}

void cmd_backup_handler(void)
{
    bootloader_print("Backup function not available in this configuration.\r\n");
    bootloader_print("Flash is divided into Bootloader (64KB) and Application (448KB) only.\r\n");
}

void cmd_restore_handler(void)
{
    bootloader_print("Restore function not available in this configuration.\r\n");
    bootloader_print("Flash is divided into Bootloader (64KB) and Application (448KB) only.\r\n");
}

void cmd_info_handler(void)
{
    bootloader_print("\r\n========== System Information ==========\r\n");
    bootloader_printf("MCU Type           : STM32F103ZET6\r\n");
    bootloader_printf("Flash Total Size   : %d KB\r\n", FLASH_TOTAL_SIZE / 1024);
    bootloader_print("----------------------------------------\r\n");
    bootloader_printf("Bootloader Version : v1.0.0\r\n");
    bootloader_printf("Bootloader Size    : %d KB\r\n", BOOTLOADER_SIZE / 1024);
    bootloader_printf("Bootloader Address : 0x%08X - 0x%08X\r\n", FLASH_BASE_ADDR, FLASH_BASE_ADDR + BOOTLOADER_SIZE - 1);
    bootloader_print("----------------------------------------\r\n");
    bootloader_printf("Application Address: 0x%08X - 0x%08X\r\n", APP_START_ADDR, APP_START_ADDR + APP_MAX_SIZE - 1);
    bootloader_printf("Application Size   : %d KB\r\n", APP_MAX_SIZE / 1024);
    
    if(bootloader_validate_app())
    {
        bootloader_print("Application Status : VALID\r\n");
        uint32_t app_crc = bootloader_calc_crc32(APP_START_ADDR, 0x10000);  // Calculate CRC for first 64KB of app
        bootloader_printf("Application CRC32  : 0x%08X (first 64KB)\r\n", app_crc);
    }
    else
    {
        bootloader_print("Application Status : INVALID or NOT FOUND\r\n");
    }
    
    bootloader_print("========================================\r\n");
}

void cmd_erase_handler(void)
{
    bootloader_print("WARNING: This will erase the application area!\r\n");
    bootloader_print("Type 'YES' to confirm: ");
    
    /* 等待用户确认 */
    char confirm[10];
    uint16_t confirm_index = 0;
    
    while(1)
    {
        uint8_t ch;
        if(uart_read(DEV_UART1, &ch, 1) > 0)
        {
            uart_write(DEV_UART1, &ch, 1);
            uart_poll_dma_tx(DEV_UART1);
            
            if(ch == '\r' || ch == '\n')
            {
                confirm[confirm_index] = '\0';
                bootloader_print("\r\n");
                break;
            }
            else if(confirm_index < 9)
            {
                confirm[confirm_index++] = ch;
            }
        }
    }
    
    if(strcmp(confirm, "YES") != 0)
    {
        bootloader_print("Operation cancelled.\r\n");
        return;
    }
    
    bootloader_print("Erasing application area...\r\n");
    if(bootloader_flash_erase(APP_START_ADDR, APP_MAX_SIZE))
    {
        bootloader_print("Application area erased successfully!\r\n");
    }
    else
    {
        bootloader_print("Failed to erase application area!\r\n");
    }
}

void cmd_reset_handler(void)
{
    bootloader_print("Resetting system...\r\n");
    HAL_Delay(100);
    HAL_NVIC_SystemReset();
}

void cmd_jump_handler(void)
{
    bootloader_state = BOOT_STATE_JUMP_APP;
    bootloader_jump_to_app();
}

/* 私有函数实现 */
static void bootloader_print(const char* str)
{
    uart_write(DEV_UART1, (uint8_t*)str, strlen(str));
    uart_poll_dma_tx(DEV_UART1);
}

static void bootloader_printf(const char* format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    bootloader_print(buffer);
}

static void bootloader_process_cmd(char* cmd)
{
    /* 去除前后空格 */
    while(*cmd == ' ') cmd++;
    char* end = cmd + strlen(cmd) - 1;
    while(end > cmd && *end == ' ') *end-- = '\0';
    
    /* 转换为小写 */
    char cmd_lower[256];
    uint32_t j = 0;
    while(cmd[j] && j < 255)
    {
        cmd_lower[j] = (cmd[j] >= 'A' && cmd[j] <= 'Z') ? cmd[j] + 32 : cmd[j];
        j++;
    }
    cmd_lower[j] = '\0';
    
    /* 查找命令 - 支持全名、缩写和部分匹配 */
    for(uint32_t i = 0; i < sizeof(cmd_table)/sizeof(cmd_table[0]); i++)
    {
        /* 完全匹配缩写 */
        if(strcmp(cmd_lower, cmd_table[i].short_name) == 0)
        {
            cmd_table[i].handler();
            return;
        }
        
        /* 完全匹配全名 */
        if(strcmp(cmd_lower, cmd_table[i].name) == 0)
        {
            cmd_table[i].handler();
            return;
        }
        
        /* 部分匹配全名（至少两个字符） */
        if(strlen(cmd_lower) >= 2 && strncmp(cmd_lower, cmd_table[i].name, strlen(cmd_lower)) == 0)
        {
            /* 检查是否唯一匹配 */
            uint32_t match_count = 0;
            uint32_t match_index = i;
            for(uint32_t k = 0; k < sizeof(cmd_table)/sizeof(cmd_table[0]); k++)
            {
                if(strncmp(cmd_lower, cmd_table[k].name, strlen(cmd_lower)) == 0)
                {
                    match_count++;
                    if(match_count > 1) break;
                }
            }
            
            if(match_count == 1)
            {
                cmd_table[match_index].handler();
                return;
            }
            else if(match_count > 1)
            {
                bootloader_printf("Ambiguous command: '%s'. Matches multiple commands.\r\n", cmd);
                bootloader_print("Type 'help' or 'h' for command list.\r\n");
                return;
            }
        }
    }
    
    bootloader_printf("Unknown command: '%s'. Type 'help' or 'h' for command list.\r\n", cmd);
}

static bool bootloader_check_button(void)
{
    /* 检查PA0按键 - 低电平有效 */
    return (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET);
}

static bool bootloader_check_uart_input(void)
{
    uint8_t dummy;
    return (uart_read(DEV_UART1, &dummy, 1) > 0);
}

static void bootloader_show_banner(void)
{
    bootloader_print("\r\n");
    bootloader_print("*******************************************\r\n");
    bootloader_print("*       STM32F103 Bootloader v1.0.0      *\r\n");
    bootloader_print("*       (c) 2025 - OpenLoad Project      *\r\n");
    bootloader_print("*******************************************\r\n");
    bootloader_print("\r\n");
}

/* Flash操作函数实现 */
uint32_t bootloader_calc_crc32(uint32_t addr, uint32_t size)
{
    uint32_t crc = 0xFFFFFFFF;
    uint8_t* data = (uint8_t*)addr;
    
    for(uint32_t i = 0; i < size; i++)
    {
        crc ^= data[i];
        for(uint8_t j = 0; j < 8; j++)
        {
            if(crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc = crc >> 1;
        }
    }
    
    return ~crc;
}

bool bootloader_flash_erase(uint32_t addr, uint32_t size)
{
    HAL_FLASH_Unlock();
    
    FLASH_EraseInitTypeDef erase_init;
    uint32_t page_error = 0;
    
    erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_init.PageAddress = addr;
    erase_init.NbPages = size / FLASH_PAGE_SIZE;
    
    if(HAL_FLASHEx_Erase(&erase_init, &page_error) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return false;
    }
    
    HAL_FLASH_Lock();
    return true;
}

bool bootloader_flash_write(uint32_t addr, const uint8_t* data, uint32_t size)
{
    HAL_FLASH_Unlock();
    
    for(uint32_t i = 0; i < size; i += 4)
    {
        uint32_t word_data = 0;
        memcpy(&word_data, &data[i], (size - i) >= 4 ? 4 : (size - i));
        
        if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word_data) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return false;
        }
    }
    
    HAL_FLASH_Lock();
    return true;
}

bool bootloader_flash_read(uint32_t addr, uint8_t* data, uint32_t size)
{
    memcpy(data, (uint8_t*)addr, size);
    return true;
}