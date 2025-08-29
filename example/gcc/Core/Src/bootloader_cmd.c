#include "bootloader_cmd.h"
#include "main.h"
#include "dev_usart.h"
#include "gpio.h"
#include "w25q64.h"
#include "xmodem.h"
#include "ota.h"
#include "wifi.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* 私有变量 */
static bootloader_state_t bootloader_state = BOOT_STATE_IDLE;
static char cmd_buffer[128];  // 减小缓冲区
static uint8_t cmd_index = 0;

/* 命令表 */
static const bootloader_cmd_t cmd_table[] = {
    {"help",    "h",  "Show command help",           CMD_HELP,       cmd_help_handler},
    {"update",  "u",  "Update firmware (XMODEM/OTA)", CMD_UPDATE,     cmd_update_handler},
    {"info",    "i",  "Show system information",    CMD_INFO,       cmd_info_handler},
    {"erase",   "e",  "Erase application area",     CMD_ERASE,      cmd_erase_handler},
    {"reset",   "r",  "Reset system",               CMD_RESET,      cmd_reset_handler},
    {"jump",    "j",  "Jump to application",        CMD_JUMP,       cmd_jump_handler},
    {"xinfo",   "xi", "Show external flash info",   CMD_EXTINFO,    cmd_extinfo_handler},
    {"xbackup", "xb", "Backup to external flash",   CMD_EXTBACKUP,  cmd_extbackup_handler},
    {"xrestore","xr", "Restore from external flash",CMD_EXTRESTORE, cmd_extrestore_handler},
    {"xlist",   "xl", "List external flash backups",CMD_EXTLIST,    cmd_extlist_handler},
};

#define CMD_TABLE_SIZE (sizeof(cmd_table)/sizeof(cmd_table[0]))

/* 私有函数声明 */
static void print_str(const char* str);
static void print_hex(uint32_t val);
static void print_dec(uint32_t val);
static void process_cmd(char* cmd);
static uint8_t read_char(void);
static void show_progress(uint32_t current, uint32_t total, const char* prefix);
static uint32_t calculate_firmware_size(w25q64_partition_id_t pid);

/* 初始化Bootloader */
void bootloader_init(void)
{
    bootloader_state = BOOT_STATE_IDLE;
    cmd_index = 0;
}

/* 检查是否进入Bootloader命令模式 */
bool bootloader_check_entry(uint32_t timeout_ms)
{
    uint32_t start_tick = HAL_GetTick();
    
    print_str("\r\n===== STM32 Bootloader v1.0 =====\r\n");
    print_str("Press any key within ");
    print_dec(timeout_ms/1000);
    print_str(" seconds...\r\n");
    
    /* 清空缓冲区 */
    uint8_t dummy[64];
    while(uart_read(DEV_UART1, dummy, sizeof(dummy)) > 0);
    
    /* 等待超时或用户输入 */
    while((HAL_GetTick() - start_tick) < timeout_ms)
    {
        /* 检查PA0按键 */
        if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET)
        {
            print_str("Button detected!\r\n");
            return true;
        }
        
        /* 检查串口输入 */
        uint8_t ch;
        if(uart_read(DEV_UART1, &ch, 1) > 0)
        {
            print_str("UART input detected!\r\n");
            return true;
        }
        
        HAL_Delay(10);
    }
    
    print_str("Timeout! Starting app...\r\n");
    return false;
}

/* Bootloader命令模式主循环 */
void bootloader_cmd_mode(void)
{
    bootloader_state = BOOT_STATE_CMD_MODE;
    
    print_str("\r\nBootloader Command Mode\r\n");
    print_str("Type 'h' for help\r\n");
    print_str("> ");
    
    while(bootloader_state == BOOT_STATE_CMD_MODE)
    {
        uint8_t ch;
        if(uart_read(DEV_UART1, &ch, 1) > 0)
        {
            /* 回显 */
            uart_write(DEV_UART1, &ch, 1);
            uart_poll_dma_tx(DEV_UART1);
            
            if(ch == '\r' || ch == '\n')
            {
                if(cmd_index > 0)
                {
                    cmd_buffer[cmd_index] = '\0';
                    print_str("\r\n");
                    process_cmd(cmd_buffer);
                    cmd_index = 0;
                }
                else
                {
                    print_str("\r\n");
                }
                
                if(bootloader_state == BOOT_STATE_CMD_MODE)
                {
                    print_str("> ");
                }
            }
            else if(ch == '\b' || ch == 0x7F)
            {
                if(cmd_index > 0)
                {
                    cmd_index--;
                    print_str(" \b");
                }
            }
            else if(cmd_index < (sizeof(cmd_buffer) - 1))
            {
                cmd_buffer[cmd_index++] = ch;
            }
        }
        
        uart_poll_dma_tx(DEV_UART1);
    }
}

/* 跳转到应用程序 */
void bootloader_jump_to_app(void)
{
    if(!bootloader_validate_app())
    {
        print_str("Invalid app!\r\n");
        return;
    }
    
    print_str("Jumping to app...\r\n");
    HAL_Delay(100);
    
    __disable_irq();
    HAL_DeInit();
    
    uint32_t app_stack = *(__IO uint32_t*)APP_START_ADDR;
    uint32_t app_reset = *(__IO uint32_t*)(APP_START_ADDR + 4);
    
    __set_MSP(app_stack);
    SCB->VTOR = APP_START_ADDR;
    
    void (*app_entry)(void) = (void (*)(void))app_reset;
    app_entry();
}

/* 验证应用程序 */
bool bootloader_validate_app(void)
{
    uint32_t app_stack = *(__IO uint32_t*)APP_START_ADDR;
    uint32_t app_reset = *(__IO uint32_t*)(APP_START_ADDR + 4);
    
    return ((app_stack >= 0x20000000) && (app_stack <= 0x20010000) &&
            (app_reset >= APP_START_ADDR) && (app_reset <= (APP_START_ADDR + APP_MAX_SIZE)));
}

/* 命令处理函数 */
void cmd_help_handler(void)
{
    print_str("\r\n==================== HELP ====================\r\n");
    print_str("Available commands:\r\n\r\n");
    
    for(uint8_t i = 0; i < CMD_TABLE_SIZE; i++)
    {
        print_str("  ");
        print_str(cmd_table[i].short_name);
        print_str(" / ");
        print_str(cmd_table[i].name);
        
        /* 对齐格式 */
        uint8_t len = strlen(cmd_table[i].short_name) + strlen(cmd_table[i].name) + 3;
        for(uint8_t j = len; j < 20; j++) {
            print_str(" ");
        }
        
        print_str("- ");
        print_str(cmd_table[i].description);
        print_str("\r\n");
    }
    
    print_str("\r\nExamples:\r\n");
    print_str("  h          - Show this help\r\n");
    print_str("  u          - Update firmware (internal/external)\r\n");
    print_str("  i          - Show system info\r\n");
    print_str("  xb         - Backup current firmware to slot 1-3\r\n");
    print_str("  xr         - Restore firmware from slot 1-3\r\n");
    print_str("  xl         - List all backup slots status\r\n");
    print_str("===============================================\r\n");
}


void cmd_update_handler(void)
{
    uint8_t ch;
    
    print_str("Firmware update method:\r\n");
    print_str("1 = XMODEM to Internal Flash\r\n");
    print_str("2 = XMODEM to External Flash\r\n");
    print_str("3 = OTA to Internal Flash\r\n");
    print_str("4 = OTA to External Flash\r\n");
    print_str("Select (1-4): ");
    
    ch = read_char();
    print_str("\r\n");
    
    if(ch == '1')
    {
        /* XMODEM到内部Flash */
        print_str("WARNING! Update internal flash? (y/n): ");
        if(read_char() != 'y')
        {
            print_str("\r\nCancelled\r\n");
            return;
        }
        print_str("\r\n");
        
        print_str("Erasing...\r\n");
        if(!bootloader_flash_erase(APP_START_ADDR, APP_MAX_SIZE))
        {
            print_str("Erase failed!\r\n");
            return;
        }
        
        print_str("Start XMODEM transfer\r\n");
        int result = xmodem_receive(APP_START_ADDR, false, 0, true);
        if(result > 0)
        {
            print_str("Success: ");
            print_dec(result);
            print_str(" bytes\r\n");
        }
        else
        {
            print_str("Transfer failed!\r\n");
        }
    }
    else if(ch == '2')
    {
        /* XMODEM到外部Flash */
        print_str("Slot (1-3): ");
        ch = read_char() - '0';
        print_str("\r\n");
        
        if(ch < 1 || ch > 3)
        {
            print_str("Invalid slot!\r\n");
            return;
        }
        
        print_str("Start XMODEM transfer\r\n");
        int result = xmodem_receive(0, true, W25Q64_PARTITION_BACKUP1 + ch - 1, true);
        if(result > 0)
        {
            print_str("Success: ");
            print_dec(result);
            print_str(" bytes\r\n");
        }
        else
        {
            print_str("Transfer failed!\r\n");
        }
    }
    else if(ch == '3')
    {
        /* OTA到内部Flash */
        print_str("WARNING! Update internal flash via OTA? (y/n): ");
        if(read_char() != 'y')
        {
            print_str("\r\nCancelled\r\n");
            return;
        }
        print_str("\r\n");
        
        print_str("Initializing WiFi module...\r\n");
        ota_error_t init_result = ota_init();
        if(init_result != OTA_OK)
        {
            print_str("WiFi module init failed! Error: ");
            print_dec(init_result);
            print_str("\r\n");
            return;
        }
        print_str("WiFi module initialized successfully\r\n");
        
        /* 设置进度回调 - 暂时不使用回调函数 */
        ota_set_progress_callback(NULL);
        ota_set_state_callback(NULL);
        
        print_str("Starting OTA download...\r\n");
        print_str("Server: ");
        print_str(OTA_SERVER_IP);
        print_str(":");
        print_dec(OTA_SERVER_PORT);
        print_str("\r\n");
        
        ota_error_t result = ota_download_firmware("/api/firmware/download/latest", APP_START_ADDR, true);
        
        if(result == OTA_OK)
        {
            print_str("OTA update completed successfully!\r\n");
        }
        else
        {
            print_str("OTA update failed with error: ");
            print_dec(result);
            print_str("\r\n");
        }
    }
    else if(ch == '4')
    {
        /* OTA到外部Flash */
        print_str("Slot (1-3): ");
        ch = read_char() - '0';
        print_str("\r\n");
        
        if(ch < 1 || ch > 3)
        {
            print_str("Invalid slot!\r\n");
            return;
        }
        
        print_str("Initializing WiFi module...\r\n");
        ota_error_t init_result = ota_init();
        if(init_result != OTA_OK)
        {
            print_str("WiFi module init failed! Error: ");
            print_dec(init_result);
            print_str("\r\n");
            return;
        }
        print_str("WiFi module initialized successfully\r\n");
        
        /* 设置进度回调 - 暂时不使用回调函数 */
        ota_set_progress_callback(NULL);
        ota_set_state_callback(NULL);
        
        w25q64_partition_id_t partition = W25Q64_PARTITION_BACKUP1 + ch - 1;
        /* 获取分区地址 */
        uint32_t ext_addr = 0;
        if(partition == W25Q64_PARTITION_BACKUP1) ext_addr = 0x200000;
        else if(partition == W25Q64_PARTITION_BACKUP2) ext_addr = 0x400000;
        else if(partition == W25Q64_PARTITION_BACKUP3) ext_addr = 0x600000;
        
        print_str("Starting OTA download to slot ");
        print_dec(ch);
        print_str("...\r\n");
        print_str("Connecting to WiFi: ");
        print_str(WIFI_SSID);
        print_str("\r\n");
        print_str("Server: ");
        print_str(OTA_SERVER_IP);
        print_str(":");
        print_dec(OTA_SERVER_PORT);
        print_str("\r\n");
        
        ota_error_t result = ota_download_firmware("/api/firmware/download/latest", ext_addr, false);
        
        if(result == OTA_OK)
        {
            print_str("OTA update to external flash completed!\r\n");
        }
        else
        {
            print_str("OTA update failed with error: ");
            print_dec(result);
            print_str("\r\n");
        }
    }
    else
    {
        print_str("Invalid selection!\r\n");
    }
}

void cmd_info_handler(void)
{
    print_str("\r\n=== System Info ===\r\n");
    print_str("MCU: STM32F103ZET6\r\n");
    print_str("Flash: 512KB\r\n");
    print_str("Boot: 0x08000000 (64KB)\r\n");
    print_str("App: 0x08010000 (448KB)\r\n");
    
    if(bootloader_validate_app())
    {
        print_str("App Status: VALID\r\n");
    }
    else
    {
        print_str("App Status: INVALID\r\n");
    }
}

void cmd_erase_handler(void)
{
    print_str("Erase app? (y/n): ");
    if(read_char() != 'y')
    {
        print_str("\r\nCancelled\r\n");
        return;
    }
    print_str("\r\n");
    
    print_str("Erasing application area...\r\n");
    
    /* 分页擦除以显示进度 */
    uint32_t total_pages = APP_MAX_SIZE / FLASH_PAGE_SIZE;
    uint32_t addr = APP_START_ADDR;
    
    HAL_FLASH_Unlock();
    
    for(uint32_t page = 0; page < total_pages; page++)
    {
        show_progress(page + 1, total_pages, "Erasing");
        
        FLASH_EraseInitTypeDef erase;
        uint32_t error = 0;
        
        erase.TypeErase = FLASH_TYPEERASE_PAGES;
        erase.PageAddress = addr + (page * FLASH_PAGE_SIZE);
        erase.NbPages = 1;
        
        if(HAL_FLASHEx_Erase(&erase, &error) != HAL_OK)
        {
            HAL_FLASH_Lock();
            print_str("\r\nErase failed at page ");
            print_dec(page);
            print_str("!\r\n");
            return;
        }
        
        /* 每隔几页更新一次进度，避免刷新太频繁 */
        if((page % 10) == 0 || page == (total_pages - 1))
        {
            HAL_Delay(1); /* 给串口时间输出 */
        }
    }
    
    HAL_FLASH_Lock();
    print_str("\r\nErase completed successfully!\r\n");
}

void cmd_reset_handler(void)
{
    print_str("Resetting...\r\n");
    HAL_Delay(100);
    HAL_NVIC_SystemReset();
}

void cmd_jump_handler(void)
{
    bootloader_state = BOOT_STATE_JUMP_APP;
    bootloader_jump_to_app();
}

void cmd_extinfo_handler(void)
{
    w25q64_init();
    uint32_t id = w25q64_read_id();
    
    print_str("\r\n=== W25Q64 Info ===\r\n");
    print_str("ID: 0x");
    print_hex(id);
    print_str("\r\nSize: 8MB\r\n");
    print_str("Partitions:\r\n");
    print_str("  Download: 1MB\r\n");
    print_str("  Backup1-3: 1MB each\r\n");
}

void cmd_extbackup_handler(void)
{
    if(!bootloader_validate_app())
    {
        print_str("No valid app!\r\n");
        return;
    }
    
    print_str("Slot (1-3): ");
    uint8_t slot = read_char() - '0';
    print_str("\r\n");
    
    if(slot < 1 || slot > 3)
    {
        print_str("Invalid slot!\r\n");
        return;
    }
    
    w25q64_init();
    w25q64_partition_id_t pid = W25Q64_PARTITION_BACKUP1 + slot - 1;
    
    print_str("Erasing...\r\n");
    if(!w25q64_erase_partition(pid))
    {
        print_str("Erase failed!\r\n");
        return;
    }
    
    /* 计算当前固件实际大小 */
    print_str("Calculating current firmware size...\r\n");
    uint32_t current_fw_size = 0;
    uint8_t *fw_ptr = (uint8_t*)APP_START_ADDR;
    
    /* 从后往前查找最后一个非0xFF字节 */
    for(int32_t i = APP_MAX_SIZE - 1; i >= 0; i--)
    {
        if(fw_ptr[i] != 0xFF)
        {
            current_fw_size = i + 1;
            break;
        }
    }
    
    if(current_fw_size == 0)
    {
        print_str("No valid firmware found!\r\n");
        return;
    }
    
    /* 对齐到1KB边界 */
    current_fw_size = (current_fw_size + 1023) & ~1023;
    
    print_str("Firmware size: ");
    print_dec(current_fw_size / 1024);
    print_str("KB\r\n");
    
    print_str("Backing up firmware...\r\n");
    uint8_t buf[1024]; /* 使用更大缓冲区 */
    uint32_t offset = 0;
    uint32_t total = current_fw_size; /* 只备份实际大小 */
    
    while(offset < total)
    {
        uint32_t size = (total - offset) > 1024 ? 1024 : (total - offset);
        memcpy(buf, (uint8_t*)(APP_START_ADDR + offset), size);
        
        if(!w25q64_write_partition(pid, offset, buf, size))
        {
            print_str("\r\nWrite failed!\r\n");
            return;
        }
        
        offset += size;
        
        /* 每4KB更新一次进度 */
        if((offset % 0x1000) == 0 || offset >= total)
        {
            show_progress(offset, total, "Backup");
        }
    }
    
    print_str("\r\nBackup completed successfully!\r\n");
}

void cmd_extrestore_handler(void)
{
    print_str("Slot (1-3): ");
    uint8_t slot = read_char() - '0';
    print_str("\r\n");
    
    if(slot < 1 || slot > 3)
    {
        print_str("Invalid slot!\r\n");
        return;
    }
    
    w25q64_init();
    w25q64_partition_id_t pid = W25Q64_PARTITION_BACKUP1 + slot - 1;
    
    /* 验证备份 */
    uint8_t buf[256];
    if(!w25q64_read_partition(pid, 0, buf, 256))
    {
        print_str("Read failed!\r\n");
        return;
    }
    
    uint32_t stack = *((uint32_t*)buf);
    if(stack < 0x20000000 || stack > 0x20010000)
    {
        print_str("Invalid backup!\r\n");
        return;
    }
    
    /* 计算固件实际大小 */
    print_str("Calculating firmware size...\r\n");
    uint32_t firmware_size = calculate_firmware_size(pid);
    if(firmware_size == 0)
    {
        print_str("Cannot determine firmware size!\r\n");
        return;
    }
    
    print_str("Firmware size: ");
    print_dec(firmware_size / 1024);
    print_str("KB\r\n");
    
    /* 只擦除需要的页数 */
    uint32_t pages_needed = (firmware_size + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE;
    uint32_t erase_size = pages_needed * FLASH_PAGE_SIZE;
    
    print_str("Erasing ");
    print_dec(pages_needed);
    print_str(" pages (");
    print_dec(erase_size / 1024);
    print_str("KB)...\r\n");
    
    if(!bootloader_flash_erase(APP_START_ADDR, erase_size))
    {
        print_str("Erase failed!\r\n");
        return;
    }
    
    print_str("Restoring firmware...\r\n");
    uint32_t offset = 0;
    uint32_t total = firmware_size; /* 只恢复实际固件大小 */
    uint8_t large_buf[1024]; /* 使用更大的缓冲区提高速度 */
    
    while(offset < total)
    {
        uint32_t size = (total - offset) > 1024 ? 1024 : (total - offset);
        
        if(!w25q64_read_partition(pid, offset, large_buf, size))
        {
            print_str("\r\nRead failed!\r\n");
            return;
        }
        
        if(!bootloader_flash_write(APP_START_ADDR + offset, large_buf, size))
        {
            print_str("\r\nWrite failed!\r\n");
            return;
        }
        
        offset += size;
        
        /* 每4KB更新一次进度 */
        if((offset % 0x1000) == 0 || offset >= total)
        {
            show_progress(offset, total, "Restore");
        }
    }
    
    print_str("\r\nRestore completed successfully!\r\n");
}

void cmd_extlist_handler(void)
{
    print_str("\r\n========== External Flash Backup List ==========\r\n");
    
    w25q64_init();
    
    /* 检查每个备份槽 */
    for(uint8_t slot = 1; slot <= 3; slot++)
    {
        w25q64_partition_id_t pid = W25Q64_PARTITION_BACKUP1 + slot - 1;
        
        print_str("\nSlot ");
        print_dec(slot);
        print_str(": ");
        
        /* 读取前256字节验证 */
        uint8_t buf[256];
        if(!w25q64_read_partition(pid, 0, buf, 256))
        {
            print_str("Read error\r\n");
            continue;
        }
        
        /* 检查栈指针 */
        uint32_t stack = *((uint32_t*)buf);
        if((stack >= 0x20000000) && (stack <= 0x20010000))
        {
            /* 有效固件 */
            print_str("Valid firmware\r\n");
            print_str("  Stack: 0x");
            print_hex(stack);
            
            /* 计算固件实际大小 */
            uint32_t fw_size = calculate_firmware_size(pid);
            print_str("\r\n  Size: ");
            print_dec(fw_size / 1024);
            print_str("KB\r\n");
            
            /* 简化的CRC计算 - 只计算前256字节 */
            uint32_t crc = 0xFFFFFFFF;
            for(uint32_t i = 0; i < 256; i++)
            {
                crc ^= buf[i];
                for(uint8_t j = 0; j < 8; j++)
                {
                    if(crc & 1)
                        crc = (crc >> 1) ^ 0xEDB88320;
                    else
                        crc = crc >> 1;
                }
            }
            crc = ~crc;
            
            print_str("  CRC32: 0x");
            print_hex(crc);
            print_str(" (first 256B)\r\n");
        }
        else
        {
            print_str("Empty or invalid\r\n");
        }
    }
    
    print_str("=================================================\r\n");
}

/* Flash操作函数 */
bool bootloader_flash_erase(uint32_t addr, uint32_t size)
{
    HAL_FLASH_Unlock();
    
    FLASH_EraseInitTypeDef erase;
    uint32_t error = 0;
    
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = addr;
    erase.NbPages = size / FLASH_PAGE_SIZE;
    
    if(HAL_FLASHEx_Erase(&erase, &error) != HAL_OK)
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
        uint32_t word = 0;
        uint32_t len = (size - i) >= 4 ? 4 : (size - i);
        memcpy(&word, &data[i], len);
        
        if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return false;
        }
    }
    
    HAL_FLASH_Lock();
    return true;
}

/* 私有函数实现 */
static void print_str(const char* str)
{
    uart_write(DEV_UART1, (uint8_t*)str, strlen(str));
    uart_poll_dma_tx(DEV_UART1);
}

static void print_hex(uint32_t val)
{
    char buf[9];
    for(int i = 7; i >= 0; i--)
    {
        uint8_t digit = val & 0xF;
        buf[i] = digit < 10 ? '0' + digit : 'A' + digit - 10;
        val >>= 4;
    }
    buf[8] = '\0';
    print_str(buf);
}

static void print_dec(uint32_t val)
{
    char buf[11];
    int i = 9;
    buf[10] = '\0';
    
    do {
        buf[i--] = '0' + (val % 10);
        val /= 10;
    } while(val && i >= 0);
    
    print_str(&buf[i + 1]);
}

/* 公共输出函数 - 供其他模块使用 */
void bootloader_print(const char* str)
{
    print_str(str);
}

void bootloader_print_hex(uint32_t val)
{
    print_hex(val);
}

void bootloader_print_dec(uint32_t val)
{
    print_dec(val);
}

static void process_cmd(char* cmd)
{
    /* 转小写 */
    for(char* p = cmd; *p; p++)
    {
        if(*p >= 'A' && *p <= 'Z') *p += 32;
    }
    
    /* 查找命令 */
    for(uint8_t i = 0; i < CMD_TABLE_SIZE; i++)
    {
        if(strcmp(cmd, cmd_table[i].short_name) == 0 ||
           strcmp(cmd, cmd_table[i].name) == 0)
        {
            cmd_table[i].handler();
            return;
        }
    }
    
    print_str("Unknown command\r\n");
}

static uint8_t read_char(void)
{
    uint8_t ch;
    while(uart_read(DEV_UART1, &ch, 1) == 0);
    uart_write(DEV_UART1, &ch, 1);
    uart_poll_dma_tx(DEV_UART1);
    return ch;
}

static void show_progress(uint32_t current, uint32_t total, const char* prefix)
{
    uint32_t percent = (current * 100) / total;
    uint32_t bar_len = (current * 30) / total; /* 30个字符的进度条 */
    
    print_str("\r");
    print_str(prefix);
    print_str(": [");
    
    /* 绘制进度条 */
    for(uint32_t i = 0; i < 30; i++)
    {
        if(i < bar_len)
            print_str("#");
        else
            print_str(" ");
    }
    
    print_str("] ");
    print_dec(percent);
    print_str("% (");
    print_dec(current / 1024); /* KB */
    print_str("/");
    print_dec(total / 1024); /* KB */
    print_str("KB)");
    
    /* 如果完成，换行 */
    if(current >= total)
    {
        print_str("\r\n");
    }
}

static uint32_t calculate_firmware_size(w25q64_partition_id_t pid)
{
    uint8_t buf[1024];
    uint32_t firmware_size = 0;
    uint32_t check_size = APP_MAX_SIZE;
    
    /* 从外部flash分块读取，从后往前查找最后一个非0xFF的数据 */
    for(int32_t offset = check_size - 1024; offset >= 0; offset -= 1024)
    {
        uint32_t read_size = 1024;
        if(offset < 0)
        {
            read_size += offset; /* 处理边界情况 */
            offset = 0;
        }
        
        if(!w25q64_read_partition(pid, offset, buf, read_size))
        {
            continue; /* 读取失败，跳过这个块 */
        }
        
        /* 从这个块的末尾开始向前查找 */
        for(int32_t i = read_size - 1; i >= 0; i--)
        {
            if(buf[i] != 0xFF)
            {
                firmware_size = offset + i + 1;
                
                /* 对齐到1KB边界 */
                firmware_size = (firmware_size + 1023) & ~1023;
                
                /* 最少8KB，因为至少要包含向量表等基础内容 */
                if(firmware_size < 8192) firmware_size = 8192;
                
                return firmware_size;
            }
        }
        
        if(offset == 0) break; /* 已经检查到开头了 */
    }
    
    /* 如果没找到有效数据，返回最小固件大小 */
    return 8192;
}

/* OTA进度回调 */
static void ota_progress_callback(uint8_t progress)
{
    print_str("\rProgress: ");
    print_dec(progress);
    print_str("%");
    
    if(progress >= 100)
    {
        print_str("\r\n");
    }
}

/* OTA状态回调 */
static void ota_state_callback(ota_state_t state)
{
    switch(state)
    {
        case OTA_STATE_CONNECTING:
            print_str("Connecting to WiFi...\r\n");
            break;
        case OTA_STATE_DOWNLOADING:
            print_str("Downloading firmware...\r\n");
            break;
        case OTA_STATE_WRITING:
            print_str("Writing to flash...\r\n");
            break;
        case OTA_STATE_VERIFYING:
            print_str("Verifying firmware...\r\n");
            break;
        case OTA_STATE_SUCCESS:
            print_str("OTA update successful!\r\n");
            break;
        case OTA_STATE_FAILED:
            print_str("OTA update failed!\r\n");
            break;
        default:
            break;
    }
}

/* 命令处理函数 */
void cmd_wifi_handler(void)
{
    print_str("WiFi functions removed. Use 'u' command for OTA updates.\r\n");
}

void cmd_wifi_info_handler(void)
{
    print_str("WiFi functions removed. Use 'u' command for OTA updates.\r\n");
}

void cmd_ota_handler(void)
{
    print_str("OTA functions integrated into 'u' command.\r\n");
    print_str("Use 'u' and select option 3 or 4 for OTA updates.\r\n");
}

void cmd_ota_debug_handler(void)
{
    print_str("OTA debug mode removed.\r\n");
}

/* 以下旧的ESP8266处理函数已移除 */
#if 0
void cmd_esp_wifi_handler(void)
{
    char ssid[64];
    char password[64];
    uint8_t ssid_len = 0;
    uint8_t pwd_len = 0;
    uint8_t ch;
    
    print_str("\r\n=== WiFi Connection ===\r\n");
    
    /* 输入SSID */
    print_str("Enter WiFi SSID: ");
    while(ssid_len < sizeof(ssid) - 1)
    {
        ch = read_char();
        if(ch == '\r' || ch == '\n')
        {
            break;
        }
        else if(ch == '\b' || ch == 0x7F)
        {
            if(ssid_len > 0)
            {
                ssid_len--;
                print_str(" \b");
            }
        }
        else
        {
            ssid[ssid_len++] = ch;
        }
    }
    ssid[ssid_len] = '\0';
    print_str("\r\n");
    
    if(ssid_len == 0)
    {
        print_str("SSID cannot be empty!\r\n");
        return;
    }
    
    /* 输入密码 */
    print_str("Enter Password: ");
    while(pwd_len < sizeof(password) - 1)
    {
        ch = read_char();
        if(ch == '\r' || ch == '\n')
        {
            break;
        }
        else if(ch == '\b' || ch == 0x7F)
        {
            if(pwd_len > 0)
            {
                pwd_len--;
                print_str(" \b");
            }
        }
        else
        {
            password[pwd_len++] = ch;
            print_str("*"); /* 隐藏密码显示 */
        }
    }
    password[pwd_len] = '\0';
    print_str("\r\n");
    
    print_str("Connecting to \"");
    print_str(ssid);
    print_str("\"...\r\n");
    
    esp8266_status_t status = esp8266_connect_wifi(ssid, password);
    
    if(status == ESP8266_OK)
    {
        print_str("WiFi connected successfully!\r\n");
        
        /* 获取IP地址 */
        char ip[32];
        if(esp8266_get_ip(ip, sizeof(ip)) == ESP8266_OK)
        {
            print_str("IP Address: ");
            print_str(ip);
            print_str("\r\n");
        }
    }
    else if(status == ESP8266_TIMEOUT)
    {
        print_str("WiFi connection timeout!\r\n");
        print_str("Check SSID and password.\r\n");
    }
    else
    {
        print_str("WiFi connection failed!\r\n");
    }
}

void cmd_esp_info_handler(void)
{
    char buffer[512];
    
    print_str("\r\n=== ESP8266 Information ===\r\n");
    
    /* 获取版本信息 */
    print_str("Getting version info...\r\n");
    if(esp8266_get_version(buffer, sizeof(buffer)) == ESP8266_OK)
    {
        print_str("Version:\r\n");
        print_str(buffer);
        print_str("\r\n");
    }
    else
    {
        print_str("Failed to get version info.\r\n");
    }
    
    /* 检查连接状态 */
    print_str("Connection Status: ");
    if(esp8266_is_connected())
    {
        print_str("Connected\r\n");
        
        /* 获取IP地址 */
        if(esp8266_get_ip(buffer, sizeof(buffer)) == ESP8266_OK)
        {
            print_str("IP Address: ");
            print_str(buffer);
            print_str("\r\n");
        }
    }
    else
    {
        print_str("Disconnected\r\n");
    }
    
    /* 获取详细状态 */
    esp8266_conn_status_t conn_status = esp8266_get_connection_status();
    print_str("Detailed Status: ");
    switch(conn_status)
    {
        case ESP8266_DISCONNECTED:
            print_str("Disconnected\r\n");
            break;
        case ESP8266_CONNECTED:
            print_str("Connected but no IP\r\n");
            break;
        case ESP8266_GOT_IP:
            print_str("Connected with IP\r\n");
            break;
        default:
            print_str("Unknown\r\n");
            break;
    }
}

void cmd_esp_ping_handler(void)
{
    char host[64];
    uint8_t host_len = 0;
    uint8_t ch;
    
    print_str("\r\n=== Network Connectivity Test ===\r\n");
    
    /* 检查WiFi连接 */
    if(!esp8266_is_connected())
    {
        print_str("ERROR: WiFi not connected!\r\n");
        print_str("Use 'espwifi' command to connect first.\r\n");
        return;
    }
    
    print_str("Enter host/IP to test (e.g. 115.190.137.231): ");
    while(host_len < sizeof(host) - 1)
    {
        ch = read_char();
        if(ch == '\r' || ch == '\n')
        {
            break;
        }
        else if(ch == '\b' || ch == 0x7F)
        {
            if(host_len > 0)
            {
                host_len--;
                print_str(" \b");
            }
        }
        else
        {
            host[host_len++] = ch;
        }
    }
    host[host_len] = '\0';
    print_str("\r\n");
    
    if(host_len == 0)
    {
        /* 默认测试一些常见的服务 */
        print_str("Testing common servers...\r\n");
        
        /* 测试百度 */
        print_str("Testing baidu.com:80... ");
        esp8266_status_t status = esp8266_test_connection("baidu.com", 80);
        if(status == ESP8266_OK)
        {
            print_str("SUCCESS\r\n");
        }
        else
        {
            print_str("FAILED\r\n");
        }
        
        /* 测试Google DNS */
        print_str("Testing 8.8.8.8:53... ");
        status = esp8266_test_connection("8.8.8.8", 53);
        if(status == ESP8266_OK)
        {
            print_str("SUCCESS\r\n");
        }
        else
        {
            print_str("FAILED\r\n");
        }
        
        /* 测试HTTP服务 */
        print_str("Testing httpbin.org:80... ");
        status = esp8266_test_connection("httpbin.org", 80);
        if(status == ESP8266_OK)
        {
            print_str("SUCCESS\r\n");
        }
        else
        {
            print_str("FAILED\r\n");
        }
    }
    else
    {
        /* 测试用户指定的主机 */
        print_str("Testing ");
        print_str(host);
        print_str(":3685... ");
        
        esp8266_status_t status = esp8266_test_connection(host, 3685);
        if(status == ESP8266_OK)
        {
            print_str("SUCCESS\r\n");
            print_str("Server is reachable!\r\n");
        }
        else if(status == ESP8266_TIMEOUT)
        {
            print_str("TIMEOUT\r\n");
            print_str("Possible issues:\r\n");
            print_str("1. Server is down\r\n");
            print_str("2. Port 3685 is blocked\r\n");
            print_str("3. Network routing issue\r\n");
        }
        else
        {
            print_str("FAILED\r\n");
            print_str("Connection rejected or network error\r\n");
        }
        
        /* 也测试HTTP端口 */
        print_str("Testing ");
        print_str(host);
        print_str(":80... ");
        
        status = esp8266_test_connection(host, 80);
        if(status == ESP8266_OK)
        {
            print_str("SUCCESS\r\n");
        }
        else
        {
            print_str("FAILED\r\n");
        }
    }
}

void cmd_http_test_handler(void)
{
    esp8266_status_t status;
    char url[] = "http://115.190.137.231:3685/api/firmware/download/latest";
    char host[] = "115.190.137.231";
    char path[] = "/api/firmware/download/latest";
    uint16_t port = 3685;
    char http_request[256];
    uint8_t buffer[512];
    uint16_t received;
    
    print_str("\r\n=== HTTP Request Debug ===\r\n");
    
    /* 检查WiFi连接 */
    if(!esp8266_is_connected())
    {
        print_str("ERROR: WiFi not connected!\r\n");
        return;
    }
    
    print_str("Testing HTTP request to your server...\r\n");
    print_str("URL: ");
    print_str(url);
    print_str("\r\n\r\n");
    
    /* 建立TCP连接 */
    print_str("Step 1: Connecting to TCP...\r\n");
    status = esp8266_tcp_connect(host, port);
    if (status != ESP8266_OK) {
        print_str("TCP connection failed!\r\n");
        return;
    }
    print_str("TCP connection SUCCESS\r\n\r\n");
    
    /* 构造HTTP请求 */
    snprintf(http_request, sizeof(http_request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "\r\n",
             path, host);
    
    print_str("Step 2: Sending HTTP request:\r\n");
    print_str("--------\r\n");
    print_str(http_request);
    print_str("--------\r\n");
    
    /* 发送HTTP请求 */
    status = esp8266_tcp_send((uint8_t *)http_request, strlen(http_request));
    if (status != ESP8266_OK) {
        print_str("HTTP request send failed!\r\n");
        esp8266_tcp_close();
        return;
    }
    print_str("HTTP request sent SUCCESS\r\n\r\n");
    
    /* 读取HTTP响应 */
    print_str("Step 3: Reading HTTP response...\r\n");
    HAL_Delay(1000);  /* 等待响应 */
    
    uint32_t start_time = HAL_GetTick();
    uint16_t total_received = 0;
    
    print_str("HTTP Response:\r\n");
    print_str("--------\r\n");
    
    while ((HAL_GetTick() - start_time) < 5000 && total_received < 400) {
        received = esp8266_tcp_receive(buffer, sizeof(buffer) - 1, 500);
        if (received > 0) {
            buffer[received] = '\0';
            print_str((char*)buffer);
            total_received += received;
        } else {
            break;
        }
    }
    
    print_str("\r\n--------\r\n");
    print_str("Total received: ");
    print_dec(total_received);
    print_str(" bytes\r\n");
    
    /* 使用软关闭，避免影响WiFi连接 */
    esp8266_tcp_close_soft();
    print_str("Connection closed (soft).\r\n");
}

/**
 * @brief TCP连接详细测试
 */
void cmd_tcp_test_handler(void)
{
    esp8266_status_t status;
    char host[64];
    char port_str[16];
    uint16_t port;
    char cmd[128];
    uint16_t resp_len;
    uint32_t start_time;
    
    print_str("\r\n=== TCP Connection Test ===\r\n");
    
    /* 检查WiFi连接 */
    if(!esp8266_is_connected())
    {
        print_str("ERROR: WiFi not connected!\r\n");
        print_str("Use 'ew' command to connect first.\r\n");
        return;
    }
    
    /* 输入主机地址 */
    print_str("Enter host (IP or domain): ");
    uint8_t idx = 0;
    uint8_t ch;
    
    while(idx < sizeof(host) - 1)
    {
        if(uart_read(DEV_UART1, &ch, 1) > 0)
        {
            if(ch == '\r' || ch == '\n')
            {
                break;
            }
            else if(ch == 0x08 || ch == 0x7F)  /* 退格键 */
            {
                if(idx > 0)
                {
                    idx--;
                    print_str("\b \b");
                }
            }
            else if(ch >= ' ' && ch <= '~')
            {
                host[idx++] = ch;
                uart_write(DEV_UART1, &ch, 1);
            }
        }
        HAL_Delay(10);
    }
    host[idx] = '\0';
    print_str("\r\n");
    
    /* 输入端口 */
    print_str("Enter port: ");
    idx = 0;
    
    while(idx < sizeof(port_str) - 1)
    {
        if(uart_read(DEV_UART1, &ch, 1) > 0)
        {
            if(ch == '\r' || ch == '\n')
            {
                break;
            }
            else if(ch == 0x08 || ch == 0x7F)  /* 退格键 */
            {
                if(idx > 0)
                {
                    idx--;
                    print_str("\b \b");
                }
            }
            else if(ch >= '0' && ch <= '9')
            {
                port_str[idx++] = ch;
                uart_write(DEV_UART1, &ch, 1);
            }
        }
        HAL_Delay(10);
    }
    port_str[idx] = '\0';
    port = atoi(port_str);
    print_str("\r\n\r\n");
    
    print_str("Testing TCP connection to ");
    print_str(host);
    print_str(":");
    print_dec(port);
    print_str("\r\n");
    print_str("========================================\r\n");
    
    /* Step 1: 测试AT命令响应 */
    print_str("Step 1: Testing AT command...\r\n");
    status = esp8266_send_cmd("AT", "OK", 1000);
    if(status == ESP8266_OK)
    {
        print_str("  [OK] AT command works\r\n");
    }
    else
    {
        print_str("  [FAIL] AT command failed\r\n");
        return;
    }
    
    /* Step 2: 检查连接状态 */
    print_str("\r\nStep 2: Checking connection status...\r\n");
    status = esp8266_send_cmd("AT+CIPSTATUS", "OK", 2000);
    print_str("  [OK] Status checked\r\n");
    
    /* Step 3: 关闭之前的连接 */
    print_str("\r\nStep 3: Closing any existing connection...\r\n");
    status = esp8266_send_cmd("AT+CIPCLOSE", "OK", 1000);
    print_str("  [OK] Connection closed\r\n");
    HAL_Delay(500);
    
    /* Step 4: 设置单连接模式 */
    print_str("\r\nStep 4: Setting single connection mode...\r\n");
    status = esp8266_send_cmd("AT+CIPMUX=0", "OK", 2000);
    if(status == ESP8266_OK)
    {
        print_str("  [OK] Single connection mode set\r\n");
    }
    else
    {
        print_str("  [FAIL] Failed to set connection mode\r\n");
        return;
    }
    
    /* Step 5: 尝试建立TCP连接 */
    print_str("\r\nStep 5: Establishing TCP connection...\r\n");
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d", host, port);
    print_str("  Command: ");
    print_str(cmd);
    print_str("\r\n");
    
    /* 发送连接命令 */
    uart_write(DEV_UART2, (uint8_t *)cmd, strlen(cmd));
    uart_write(DEV_UART2, (uint8_t *)"\r\n", 2);
    uart_poll_dma_tx(DEV_UART2);
    
    /* 等待响应 */
    print_str("  Waiting for response...\r\n");
    start_time = HAL_GetTick();
    bool connected = false;
    
    while((HAL_GetTick() - start_time) < 20000)  /* 20秒超时 */
    {
        char response_buf[512];
        resp_len = esp8266_read_response(response_buf, sizeof(response_buf), 1000);
        if(resp_len > 0)
        {
            print_str("  Response: ");
            print_str(response_buf);
            print_str("\r\n");
            
            if(strstr(response_buf, "CONNECT"))
            {
                connected = true;
                print_str("  [OK] TCP connection established!\r\n");
                break;
            }
            else if(strstr(response_buf, "ERROR") || 
                   strstr(response_buf, "CLOSED") ||
                   strstr(response_buf, "FAIL"))
            {
                print_str("  [FAIL] Connection failed\r\n");
                break;
            }
        }
        else
        {
            print_str("  Still waiting... (");
            print_dec((HAL_GetTick() - start_time) / 1000);
            print_str("s)\r\n");
        }
    }
    
    if(!connected)
    {
        print_str("  [TIMEOUT] Connection timeout after 20 seconds\r\n");
        
        /* 尝试诊断问题 */
        print_str("\r\n=== Diagnostics ===\r\n");
        
        /* 检查DNS解析（如果是域名） */
        if((host[0] < '0' || host[0] > '9'))  /* 可能是域名 */
        {
            print_str("Testing DNS resolution...\r\n");
            snprintf(cmd, sizeof(cmd), "AT+CIPDOMAIN=\"%s\"", host);
            status = esp8266_send_cmd(cmd, "OK", 5000);
            if(status == ESP8266_OK)
            {
                print_str("DNS resolution completed\r\n");
            }
        }
        
        /* 检查WiFi信号强度 */
        print_str("\r\nChecking WiFi signal strength...\r\n");
        status = esp8266_send_cmd("AT+CWJAP?", "OK", 2000);
        if(status == ESP8266_OK)
        {
            print_str("WiFi connection info checked\r\n");
        }
    }
    else
    {
        /* 连接成功，尝试发送测试数据 */
        print_str("\r\nStep 6: Testing data transmission...\r\n");
        
        /* 发送HTTP GET请求作为测试 */
        char test_data[] = "GET / HTTP/1.1\r\nHost: ";
        char test_data2[] = "\r\nConnection: close\r\n\r\n";
        
        uint16_t test_len = strlen(test_data) + strlen(host) + strlen(test_data2);
        snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d", test_len);
        
        print_str("  Preparing to send ");
        print_dec(test_len);
        print_str(" bytes...\r\n");
        
        uart_write(DEV_UART2, (uint8_t *)cmd, strlen(cmd));
        uart_write(DEV_UART2, (uint8_t *)"\r\n", 2);
        uart_poll_dma_tx(DEV_UART2);
        
        /* 等待 > 提示符 */
        start_time = HAL_GetTick();
        bool prompt_received = false;
        
        while((HAL_GetTick() - start_time) < 3000)
        {
            char prompt_buf[128];
            resp_len = esp8266_read_response(prompt_buf, sizeof(prompt_buf), 500);
            if(resp_len > 0)
            {
                if(strstr(prompt_buf, ">"))
                {
                    prompt_received = true;
                    print_str("  [OK] Ready to send data\r\n");
                    break;
                }
            }
        }
        
        if(prompt_received)
        {
            /* 发送测试数据 */
            uart_write(DEV_UART2, (uint8_t *)test_data, strlen(test_data));
            uart_write(DEV_UART2, (uint8_t *)host, strlen(host));
            uart_write(DEV_UART2, (uint8_t *)test_data2, strlen(test_data2));
            uart_poll_dma_tx(DEV_UART2);
            
            /* 等待发送确认 */
            start_time = HAL_GetTick();
            bool send_ok = false;
            
            while((HAL_GetTick() - start_time) < 5000)
            {
                char send_resp[128];
                resp_len = esp8266_read_response(send_resp, sizeof(send_resp), 500);
                if(resp_len > 0)
                {
                    if(strstr(send_resp, "SEND OK"))
                    {
                        send_ok = true;
                        print_str("  [OK] Data sent successfully\r\n");
                        break;
                    }
                    else if(strstr(send_resp, "SEND FAIL"))
                    {
                        print_str("  [FAIL] Send failed\r\n");
                        break;
                    }
                }
            }
            
            if(send_ok)
            {
                /* 接收响应数据 */
                print_str("\r\nStep 7: Receiving response...\r\n");
                HAL_Delay(1000);
                
                uint16_t total_received = 0;
                start_time = HAL_GetTick();
                
                while((HAL_GetTick() - start_time) < 5000 && total_received < 500)
                {
                    uint8_t buffer[256];
                    uint16_t received = esp8266_tcp_receive(buffer, sizeof(buffer) - 1, 500);
                    if(received > 0)
                    {
                        buffer[received] = '\0';
                        print_str((char*)buffer);
                        total_received += received;
                    }
                    else
                    {
                        break;
                    }
                }
                
                print_str("\r\n  Total received: ");
                print_dec(total_received);
                print_str(" bytes\r\n");
            }
        }
        
        /* 使用软关闭，避免影响WiFi连接 */
        esp8266_tcp_close_soft();
        print_str("\r\n[OK] Connection closed (soft)\r\n");
    }
    
    print_str("\r\n========================================\r\n");
    print_str("Test completed.\r\n");
}

/**
 * @brief OTA更新到内部Flash
 * @param url 固件下载URL
 */
static void ota_update_internal(const char *url)
{
    esp8266_http_info_t http_info;
    esp8266_status_t status;
    uint8_t buffer[1024];
    uint16_t received;
    uint32_t total_written = 0;
    
    print_str("Starting OTA update to internal flash...\r\n");
    
    /* 检查WiFi连接 */
    if(!esp8266_is_connected())
    {
        print_str("ERROR: WiFi not connected!\r\n");
        print_str("Use 'espwifi' command to connect first.\r\n");
        return;
    }
    
    print_str("Connected to: ");
    print_str(url);
    print_str("\r\n");
    
    /* 开始HTTP下载 */
    status = esp8266_http_get_start(url, &http_info);
    if(status != ESP8266_OK)
    {
        if(status == ESP8266_TIMEOUT)
        {
            print_str("HTTP request timeout!\r\n");
            print_str("Check network connection and server.\r\n");
        }
        else
        {
            print_str("HTTP request failed!\r\n");
            print_str("Let's test the connection first...\r\n");
            
            /* 尝试调试连接 */
            print_str("Testing TCP connection to 115.190.137.231:3685...\r\n");
            esp8266_status_t test_status = esp8266_test_connection("115.190.137.231", 3685);
            if(test_status == ESP8266_OK)
            {
                print_str("TCP connection: SUCCESS\r\n");
                print_str("Problem might be in HTTP protocol.\r\n");
                print_str("Try using 'httptest' command for debugging.\r\n");
            }
            else
            {
                print_str("TCP connection: FAILED\r\n");
                print_str("Check WiFi connection and server status.\r\n");
            }
        }
        return;
    }
    
    print_str("HTTP 200 OK, Content-Length: ");
    if(http_info.content_length > 0)
    {
        print_dec(http_info.content_length);
        print_str(" bytes\r\n");
        
        /* 检查固件大小 */
        if(http_info.content_length > APP_MAX_SIZE)
        {
            print_str("ERROR: Firmware too large!\r\n");
            esp8266_http_get_finish();
            return;
        }
    }
    else
    {
        print_str("Unknown (chunked)\r\n");
    }
    
    /* 确认更新 */
    print_str("WARNING! This will overwrite internal flash!\r\n");
    print_str("Continue? (y/n): ");
    if(read_char() != 'y')
    {
        print_str("\r\nCancelled\r\n");
        esp8266_http_get_finish();
        return;
    }
    print_str("\r\n");
    
    /* 擦除内部Flash */
    print_str("Erasing internal flash...\r\n");
    if(!bootloader_flash_erase(APP_START_ADDR, APP_MAX_SIZE))
    {
        print_str("Flash erase failed!\r\n");
        esp8266_http_get_finish();
        return;
    }
    
    /* 下载并写入固件 */
    print_str("Downloading firmware...\r\n");
    uint32_t last_progress = 0;
    
    while(true)
    {
        received = esp8266_http_get_data(buffer, sizeof(buffer), 5000);
        if(received == 0)
        {
            break; /* 下载完成 */
        }
        
        /* 写入内部Flash */
        if(!bootloader_flash_write(APP_START_ADDR + total_written, buffer, received))
        {
            print_str("\r\nFlash write failed!\r\n");
            esp8266_http_get_finish();
            return;
        }
        
        total_written += received;
        
        /* 显示进度 */
        if(http_info.content_length > 0)
        {
            uint32_t progress = (total_written * 100) / http_info.content_length;
            if(progress != last_progress && (progress % 5) == 0)
            {
                show_progress(total_written, http_info.content_length, "Download");
                last_progress = progress;
            }
        }
        else
        {
            /* 未知大小，每32KB显示一次 */
            if((total_written % 32768) == 0 || received < sizeof(buffer))
            {
                print_str("Downloaded: ");
                print_dec(total_written / 1024);
                print_str("KB\r\n");
            }
        }
    }
    
    esp8266_http_get_finish();
    
    print_str("\r\nOTA update completed!\r\n");
    print_str("Total downloaded: ");
    print_dec(total_written);
    print_str(" bytes\r\n");
    
    /* 验证应用程序 */
    if(bootloader_validate_app())
    {
        print_str("Firmware validation: PASSED\r\n");
    }
    else
    {
        print_str("Firmware validation: FAILED\r\n");
    }
}

/**
 * @brief OTA更新到外部Flash
 * @param url 固件下载URL
 * @param slot 备份槽位(1-3)
 */
static void ota_update_external(const char *url, uint8_t slot)
{
    esp8266_http_info_t http_info;
    esp8266_status_t status;
    uint8_t buffer[1024];
    uint16_t received;
    uint32_t total_written = 0;
    w25q64_partition_id_t pid = W25Q64_PARTITION_BACKUP1 + slot - 1;
    
    print_str("Starting OTA update to external flash slot ");
    print_dec(slot);
    print_str("...\r\n");
    
    /* 检查WiFi连接 */
    if(!esp8266_is_connected())
    {
        print_str("ERROR: WiFi not connected!\r\n");
        print_str("Use 'espwifi' command to connect first.\r\n");
        return;
    }
    
    print_str("Connected to: ");
    print_str(url);
    print_str("\r\n");
    
    /* 开始HTTP下载 */
    status = esp8266_http_get_start(url, &http_info);
    if(status != ESP8266_OK)
    {
        if(status == ESP8266_TIMEOUT)
        {
            print_str("HTTP request timeout!\r\n");
            print_str("Check network connection and server.\r\n");
        }
        else
        {
            print_str("HTTP request failed!\r\n");
            print_str("Let's test the connection first...\r\n");
            
            /* 尝试调试连接 */
            print_str("Testing TCP connection to 115.190.137.231:3685...\r\n");
            esp8266_status_t test_status = esp8266_test_connection("115.190.137.231", 3685);
            if(test_status == ESP8266_OK)
            {
                print_str("TCP connection: SUCCESS\r\n");
                print_str("Problem might be in HTTP protocol.\r\n");
                print_str("Try using 'httptest' command for debugging.\r\n");
            }
            else
            {
                print_str("TCP connection: FAILED\r\n");
                print_str("Check WiFi connection and server status.\r\n");
            }
        }
        return;
    }
    
    print_str("HTTP 200 OK, Content-Length: ");
    if(http_info.content_length > 0)
    {
        print_dec(http_info.content_length);
        print_str(" bytes\r\n");
    }
    else
    {
        print_str("Unknown (chunked)\r\n");
    }
    
    /* 初始化外部Flash */
    w25q64_init();
    
    /* 擦除外部Flash分区 */
    print_str("Erasing external flash partition...\r\n");
    if(!w25q64_erase_partition(pid))
    {
        print_str("External flash erase failed!\r\n");
        esp8266_http_get_finish();
        return;
    }
    
    /* 下载并写入固件 */
    print_str("Downloading firmware...\r\n");
    uint32_t last_progress = 0;
    
    while(true)
    {
        received = esp8266_http_get_data(buffer, sizeof(buffer), 5000);
        if(received == 0)
        {
            break; /* 下载完成 */
        }
        
        /* 写入外部Flash */
        if(!w25q64_write_partition(pid, total_written, buffer, received))
        {
            print_str("\r\nExternal flash write failed!\r\n");
            esp8266_http_get_finish();
            return;
        }
        
        total_written += received;
        
        /* 显示进度 */
        if(http_info.content_length > 0)
        {
            uint32_t progress = (total_written * 100) / http_info.content_length;
            if(progress != last_progress && (progress % 5) == 0)
            {
                show_progress(total_written, http_info.content_length, "Download");
                last_progress = progress;
            }
        }
        else
        {
            /* 未知大小，每32KB显示一次 */
            if((total_written % 32768) == 0 || received < sizeof(buffer))
            {
                print_str("Downloaded: ");
                print_dec(total_written / 1024);
                print_str("KB\r\n");
            }
        }
    }
    
    esp8266_http_get_finish();
    
    print_str("\r\nOTA update completed!\r\n");
    print_str("Total downloaded: ");
    print_dec(total_written);
    print_str(" bytes to slot ");
    print_dec(slot);
    print_str("\r\n");
    
    /* 验证外部Flash中的固件 */
    uint8_t verify_buf[256];
    if(w25q64_read_partition(pid, 0, verify_buf, 256))
    {
        uint32_t stack = *((uint32_t*)verify_buf);
        if(stack >= 0x20000000 && stack <= 0x20010000)
        {
            print_str("Firmware validation: PASSED\r\n");
            print_str("Use 'xr ");
            print_dec(slot);
            print_str("' to restore this firmware.\r\n");
        }
        else
        {
            print_str("Firmware validation: FAILED\r\n");
        }
    }
    else
    {
        print_str("Firmware validation: READ ERROR\r\n");
    }
}

#endif