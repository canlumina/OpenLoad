#include "bootloader_cmd.h"
#include "main.h"
#include "dev_usart.h"
#include "gpio.h"
#include "w25q64.h"
#include "xmodem.h"
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
    {"help",      "h",   "Show command list",              CMD_HELP,       cmd_help_handler},
    {"update",    "u",   "Update firmware via UART",       CMD_UPDATE,     cmd_update_handler},
    {"download",  "d",   "Download firmware to PC",        CMD_DOWNLOAD,   cmd_download_handler},
    {"info",      "i",   "Show firmware information",      CMD_INFO,       cmd_info_handler},
    {"erase",     "e",   "Erase application area",         CMD_ERASE,      cmd_erase_handler},
    {"reset",     "r",   "Reset system",                   CMD_RESET,      cmd_reset_handler},
    {"jump",      "j",   "Jump to application",            CMD_JUMP,       cmd_jump_handler},
    {"extinfo",   "xi",  "Show external flash info",       CMD_EXTINFO,    cmd_extinfo_handler},
    {"extbackup", "xb",  "Backup to external flash",       CMD_EXTBACKUP,  cmd_extbackup_handler},
    {"extrestore","xr",  "Restore from external flash",    CMD_EXTRESTORE, cmd_extrestore_handler},
    {"extlist",   "xl",  "List external flash backups",    CMD_EXTLIST,    cmd_extlist_handler},
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
    bootloader_print("Starting firmware update via XMODEM...\r\n");
    bootloader_print("Select target:\r\n");
    bootloader_print("  1. Internal Flash (Direct update)\r\n");
    bootloader_print("  2. External Flash Download area\r\n");
    bootloader_print("  3. External Flash Backup slot 1\r\n");
    bootloader_print("  4. External Flash Backup slot 2\r\n");
    bootloader_print("  5. External Flash Backup slot 3\r\n");
    bootloader_print("Select (1-5): ");
    
    char choice[10];
    uint16_t choice_index = 0;
    
    while(1)
    {
        uint8_t ch;
        if(uart_read(DEV_UART1, &ch, 1) > 0)
        {
            uart_write(DEV_UART1, &ch, 1);
            uart_poll_dma_tx(DEV_UART1);
            
            if(ch == '\r' || ch == '\n')
            {
                choice[choice_index] = '\0';
                bootloader_print("\r\n");
                break;
            }
            else if(choice_index < 9)
            {
                choice[choice_index++] = ch;
            }
        }
    }
    
    int target = atoi(choice);
    if(target < 1 || target > 5)
    {
        bootloader_print("Invalid choice!\r\n");
        return;
    }
    
    bootloader_print("\r\n");
    
    if(target == 1)
    {
        /* 直接更新到内部Flash */
        bootloader_print("WARNING: This will directly update the application!\r\n");
        bootloader_print("Type 'YES' to confirm: ");
        
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
        if(!bootloader_flash_erase(APP_START_ADDR, APP_MAX_SIZE))
        {
            bootloader_print("Failed to erase application area!\r\n");
            return;
        }
        
        bootloader_print("Ready to receive firmware via XMODEM-1K...\r\n");
        bootloader_print("Please start XMODEM transfer now\r\n");
        bootloader_print("(SecureCRT: Transfer->Send File->Xmodem-1K)\r\n");
        bootloader_print("(Linux: sx -k firmware.bin)\r\n\r\n");
        
        int result = xmodem_receive(APP_START_ADDR, false, 0, true);  // 使用XMODEM-1K
        if(result > 0)
        {
            bootloader_printf("Successfully received %d bytes\r\n", result);
            
            if(bootloader_validate_app())
            {
                bootloader_print("Firmware validation passed!\r\n");
            }
            else
            {
                bootloader_print("WARNING: Firmware validation failed!\r\n");
            }
        }
        else
        {
            bootloader_print("Firmware update failed!\r\n");
        }
    }
    else
    {
        /* 更新到外部Flash */
        uint8_t partition_id;
        
        switch(target)
        {
            case 2: partition_id = W25Q64_PARTITION_DOWNLOAD; break;
            case 3: partition_id = W25Q64_PARTITION_BACKUP1; break;
            case 4: partition_id = W25Q64_PARTITION_BACKUP2; break;
            case 5: partition_id = W25Q64_PARTITION_BACKUP3; break;
            default: return;
        }
        
        bootloader_printf("Updating to external flash partition %d...\r\n", partition_id);
        
        bootloader_print("Ready to receive firmware via XMODEM-1K...\r\n");
        bootloader_print("Please start XMODEM transfer now\r\n");
        bootloader_print("(SecureCRT: Transfer->Send File->Xmodem-1K)\r\n");
        bootloader_print("(Linux: sx -k firmware.bin)\r\n\r\n");
        
        int result = xmodem_receive(0, true, partition_id, true);  // 使用XMODEM-1K
        if(result > 0)
        {
            bootloader_printf("Successfully received %d bytes to external flash\r\n", result);
            bootloader_print("Use 'extrestore' or 'xr' to update firmware from this backup\r\n");
        }
        else
        {
            bootloader_print("Transfer failed!\r\n");
        }
    }
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

/* 外部Flash命令处理函数 */
void cmd_extinfo_handler(void)
{
    /* 初始化W25Q64 */
    w25q64_init();
    
    /* 读取芯片ID */
    uint32_t chip_id = w25q64_read_id();
    
    bootloader_print("\r\n========== External Flash Information ==========\r\n");
    bootloader_printf("Chip Type          : W25Q64\r\n");
    bootloader_printf("Chip ID            : 0x%06X\r\n", chip_id);
    bootloader_printf("Total Size         : 8MB (0x%08X bytes)\r\n", W25Q64_CHIP_SIZE);
    bootloader_printf("Block Size         : 64KB\r\n");
    bootloader_printf("Sector Size        : 4KB\r\n");
    bootloader_printf("Page Size          : 256 bytes\r\n");
    bootloader_print("------------------------------------------------\r\n");
    
    /* 打印分区信息 */
    bootloader_print("Partition Table:\r\n");
    bootloader_print("------------------------------------------------\r\n");
    bootloader_printf("%-12s %-10s %-10s %s\r\n", "Name", "Start", "Size", "Description");
    bootloader_print("------------------------------------------------\r\n");
    
    for(uint32_t i = 0; i < W25Q64_PARTITION_MAX; i++)
    {
        const w25q64_partition_t* p = w25q64_get_partition(i);
        if(p)
        {
            bootloader_printf("%-12s 0x%08X ", p->name, p->start_addr);
            
            if(p->size >= 0x100000)
            {
                bootloader_printf("%4dMB     ", p->size / 0x100000);
            }
            else
            {
                bootloader_printf("%4dKB     ", p->size / 0x400);
            }
            
            bootloader_printf("%s\r\n", p->description);
        }
    }
    
    bootloader_print("================================================\r\n");
}

void cmd_extbackup_handler(void)
{
    bootloader_print("Backup firmware to external flash...\r\n");
    
    /* 验证应用程序 */
    if(!bootloader_validate_app())
    {
        bootloader_print("No valid application to backup!\r\n");
        return;
    }
    
    /* 初始化W25Q64 */
    w25q64_init();
    
    /* 选择备份槽位 */
    bootloader_print("Select backup slot (1-3): ");
    
    char slot_input[10];
    uint16_t slot_index = 0;
    
    while(1)
    {
        uint8_t ch;
        if(uart_read(DEV_UART1, &ch, 1) > 0)
        {
            uart_write(DEV_UART1, &ch, 1);
            uart_poll_dma_tx(DEV_UART1);
            
            if(ch == '\r' || ch == '\n')
            {
                slot_input[slot_index] = '\0';
                bootloader_print("\r\n");
                break;
            }
            else if(slot_index < 9)
            {
                slot_input[slot_index++] = ch;
            }
        }
    }
    
    int slot = atoi(slot_input);
    if(slot < 1 || slot > 3)
    {
        bootloader_print("Invalid slot number!\r\n");
        return;
    }
    
    w25q64_partition_id_t partition_id = W25Q64_PARTITION_BACKUP1 + (slot - 1);
    
    /* 擦除备份分区 */
    bootloader_printf("Erasing backup slot %d...\r\n", slot);
    if(!w25q64_erase_partition(partition_id))
    {
        bootloader_print("Failed to erase backup partition!\r\n");
        return;
    }
    
    /* 备份固件 */
    bootloader_print("Backing up firmware...\r\n");
    uint8_t buffer[W25Q64_PAGE_SIZE];
    uint32_t total_size = APP_MAX_SIZE;
    uint32_t offset = 0;
    
    while(offset < total_size)
    {
        uint32_t chunk_size = (total_size - offset) > W25Q64_PAGE_SIZE ? W25Q64_PAGE_SIZE : (total_size - offset);
        
        /* 从内部Flash读取 */
        if(!bootloader_flash_read(APP_START_ADDR + offset, buffer, chunk_size))
        {
            bootloader_print("Failed to read internal flash!\r\n");
            return;
        }
        
        /* 写入外部Flash */
        if(!w25q64_write_partition(partition_id, offset, buffer, chunk_size))
        {
            bootloader_print("Failed to write external flash!\r\n");
            return;
        }
        
        offset += chunk_size;
        bootloader_printf("Progress: %d%%\r", (offset * 100) / total_size);
    }
    
    bootloader_printf("\r\nBackup to slot %d completed successfully!\r\n", slot);
    
    /* 计算并保存CRC */
    uint32_t crc = bootloader_calc_crc32(APP_START_ADDR, 0x10000);
    bootloader_printf("Firmware CRC32: 0x%08X\r\n", crc);
}

void cmd_extrestore_handler(void)
{
    bootloader_print("Restore firmware from external flash...\r\n");
    
    /* 初始化W25Q64 */
    w25q64_init();
    
    /* 选择恢复槽位 */
    bootloader_print("Select backup slot to restore (1-3): ");
    
    char slot_input[10];
    uint16_t slot_index = 0;
    
    while(1)
    {
        uint8_t ch;
        if(uart_read(DEV_UART1, &ch, 1) > 0)
        {
            uart_write(DEV_UART1, &ch, 1);
            uart_poll_dma_tx(DEV_UART1);
            
            if(ch == '\r' || ch == '\n')
            {
                slot_input[slot_index] = '\0';
                bootloader_print("\r\n");
                break;
            }
            else if(slot_index < 9)
            {
                slot_input[slot_index++] = ch;
            }
        }
    }
    
    int slot = atoi(slot_input);
    if(slot < 1 || slot > 3)
    {
        bootloader_print("Invalid slot number!\r\n");
        return;
    }
    
    w25q64_partition_id_t partition_id = W25Q64_PARTITION_BACKUP1 + (slot - 1);
    
    /* 验证备份 */
    bootloader_printf("Verifying backup slot %d...\r\n", slot);
    uint8_t test_buffer[256];
    if(!w25q64_read_partition(partition_id, 0, test_buffer, 256))
    {
        bootloader_print("Failed to read backup!\r\n");
        return;
    }
    
    /* 检查栈指针 */
    uint32_t stack_ptr = *((uint32_t*)test_buffer);
    if((stack_ptr < 0x20000000) || (stack_ptr > 0x20010000))
    {
        bootloader_printf("No valid firmware in slot %d!\r\n", slot);
        return;
    }
    
    /* 擦除内部Flash应用区 */
    bootloader_print("Erasing application area...\r\n");
    if(!bootloader_flash_erase(APP_START_ADDR, APP_MAX_SIZE))
    {
        bootloader_print("Failed to erase application area!\r\n");
        return;
    }
    
    /* 恢复固件 */
    bootloader_print("Restoring firmware...\r\n");
    uint8_t buffer[W25Q64_PAGE_SIZE];
    uint32_t total_size = APP_MAX_SIZE;
    uint32_t offset = 0;
    
    while(offset < total_size)
    {
        uint32_t chunk_size = (total_size - offset) > W25Q64_PAGE_SIZE ? W25Q64_PAGE_SIZE : (total_size - offset);
        
        /* 从外部Flash读取 */
        if(!w25q64_read_partition(partition_id, offset, buffer, chunk_size))
        {
            bootloader_print("Failed to read external flash!\r\n");
            return;
        }
        
        /* 写入内部Flash */
        if(!bootloader_flash_write(APP_START_ADDR + offset, buffer, chunk_size))
        {
            bootloader_print("Failed to write internal flash!\r\n");
            return;
        }
        
        offset += chunk_size;
        bootloader_printf("Progress: %d%%\r", (offset * 100) / total_size);
    }
    
    bootloader_printf("\r\nRestore from slot %d completed successfully!\r\n", slot);
    
    /* 验证恢复的固件 */
    if(bootloader_validate_app())
    {
        bootloader_print("Firmware restored and validated successfully!\r\n");
    }
    else
    {
        bootloader_print("WARNING: Restored firmware validation failed!\r\n");
    }
}

void cmd_extlist_handler(void)
{
    bootloader_print("\r\n========== External Flash Backup List ==========\r\n");
    
    /* 初始化W25Q64 */
    w25q64_init();
    
    /* 检查每个备份槽 */
    for(int slot = 1; slot <= 3; slot++)
    {
        w25q64_partition_id_t partition_id = W25Q64_PARTITION_BACKUP1 + (slot - 1);
        
        bootloader_printf("\nSlot %d: ", slot);
        
        /* 读取前256字节验证 */
        uint8_t test_buffer[256];
        if(!w25q64_read_partition(partition_id, 0, test_buffer, 256))
        {
            bootloader_print("Read error\r\n");
            continue;
        }
        
        /* 检查栈指针 */
        uint32_t stack_ptr = *((uint32_t*)test_buffer);
        if((stack_ptr >= 0x20000000) && (stack_ptr <= 0x20010000))
        {
            /* 有效固件 */
            bootloader_print("Valid firmware\r\n");
            
            /* 计算CRC (只计算前64KB) */
            uint8_t crc_buffer[1024];
            uint32_t crc = 0xFFFFFFFF;
            for(uint32_t i = 0; i < 0x10000; i += sizeof(crc_buffer))
            {
                w25q64_read_partition(partition_id, i, crc_buffer, sizeof(crc_buffer));
                for(uint32_t j = 0; j < sizeof(crc_buffer); j++)
                {
                    crc ^= crc_buffer[j];
                    for(uint8_t k = 0; k < 8; k++)
                    {
                        if(crc & 1)
                            crc = (crc >> 1) ^ 0xEDB88320;
                        else
                            crc = crc >> 1;
                    }
                }
            }
            crc = ~crc;
            
            bootloader_printf("  Stack Pointer: 0x%08X\r\n", stack_ptr);
            bootloader_printf("  CRC32: 0x%08X (first 64KB)\r\n", crc);
        }
        else
        {
            bootloader_print("Empty or invalid\r\n");
        }
    }
    
    bootloader_print("=================================================\r\n");
}