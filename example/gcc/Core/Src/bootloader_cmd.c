#include "bootloader_cmd.h"
#include "main.h"
#include "dev_usart.h"
#include "gpio.h"
#include "w25q64.h"
#include "xmodem.h"
#include "esp8266.h"
#include <string.h>
#include <stdio.h>

/* 私有变量 */
static bootloader_state_t bootloader_state = BOOT_STATE_IDLE;
static char cmd_buffer[128];  // 减小缓冲区
static uint8_t cmd_index = 0;

/* 命令表 */
static const bootloader_cmd_t cmd_table[] = {
    {"help",    "h",  "Show command help",           CMD_HELP,       cmd_help_handler},
    {"update",  "u",  "Update firmware via XMODEM", CMD_UPDATE,     cmd_update_handler},
    {"info",    "i",  "Show system information",    CMD_INFO,       cmd_info_handler},
    {"erase",   "e",  "Erase application area",     CMD_ERASE,      cmd_erase_handler},
    {"reset",   "r",  "Reset system",               CMD_RESET,      cmd_reset_handler},
    {"jump",    "j",  "Jump to application",        CMD_JUMP,       cmd_jump_handler},
    {"xinfo",   "xi", "Show external flash info",   CMD_EXTINFO,    cmd_extinfo_handler},
    {"xbackup", "xb", "Backup to external flash",   CMD_EXTBACKUP,  cmd_extbackup_handler},
    {"xrestore","xr", "Restore from external flash",CMD_EXTRESTORE, cmd_extrestore_handler},
    {"xlist",   "xl", "List external flash backups",CMD_EXTLIST,    cmd_extlist_handler},
    {"espinit", "ei", "Initialize ESP8266 module",  CMD_ESP_INIT,   cmd_esp_init_handler},
    {"esptest", "et", "Test ESP8266 communication", CMD_ESP_TEST,   cmd_esp_test_handler},
    {"espwifi", "ew", "Connect to WiFi network",    CMD_ESP_WIFI,   cmd_esp_wifi_handler},
    {"espinfo", "ef", "Show ESP8266 information",   CMD_ESP_INFO,   cmd_esp_info_handler},
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
    print_str("Firmware update via XMODEM\r\n");
    print_str("1=Internal 2=External: ");
    
    uint8_t ch = read_char();
    print_str("\r\n");
    
    if(ch == '1')
    {
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

/* ESP8266命令处理函数 */
void cmd_esp_init_handler(void)
{
    print_str("\r\n=== ESP8266 Initialization ===\r\n");
    
    esp8266_status_t status = esp8266_init();
    
    if(status == ESP8266_OK)
    {
        print_str("ESP8266 initialized successfully!\r\n");
    }
    else if(status == ESP8266_TIMEOUT)
    {
        print_str("ESP8266 initialization timeout!\r\n");
        print_str("Check module connection and power.\r\n");
    }
    else
    {
        print_str("ESP8266 initialization failed!\r\n");
    }
}

void cmd_esp_test_handler(void)
{
    print_str("\r\n=== ESP8266 Communication Test ===\r\n");
    
    esp8266_status_t status = esp8266_test();
    
    if(status == ESP8266_OK)
    {
        print_str("ESP8266 communication test passed!\r\n");
    }
    else if(status == ESP8266_TIMEOUT)
    {
        print_str("ESP8266 communication timeout!\r\n");
        print_str("Check serial connection and baud rate.\r\n");
    }
    else
    {
        print_str("ESP8266 communication test failed!\r\n");
    }
}

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