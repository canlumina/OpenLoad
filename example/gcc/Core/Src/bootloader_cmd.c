#include "bootloader_cmd.h"
#include "main.h"
#include "dev_usart.h"
#include "gpio.h"
#include "w25q64.h"
#include "xmodem.h"
#include "http_ota.h"
#include "esp8266_wifi.h"
#include "config.h"
#include "firmware_crypto.h"
#include "firmware_aes.h"
#include "streaming_aes.h"
#include "encrypted_firmware.h"
#include "firmware_version.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* 私有变量 */
static bootloader_state_t bootloader_state = BOOT_STATE_IDLE;
static char cmd_buffer[128];
static uint8_t cmd_index = 0;

/* WiFi相关变量 */
static esp8266_device_t g_wifi_device;
static bool g_wifi_initialized = false;

/* OTA和加密相关变量 */
static ota_context_t g_ota_context;
static char g_dynamic_password[64] = "yangcan";  /* 默认密码，可动态更新，支持32字符十六进制密钥 */

/* 配置命令处理函数前向声明 */
static void cmd_config_show_handler(void);
static void cmd_config_wifi_handler(void);
static void cmd_config_ota_handler(void);
static void cmd_config_save_handler(void);
static void cmd_config_reset_handler(void);

/* 动态密码管理函数 */
static void set_dynamic_password(const char* password);
static const char* get_dynamic_password(void);
static void update_password_from_ota(ota_context_t* ctx);

/* 版本管理命令处理函数前向声明 */
static void cmd_version_handler(void);
static void cmd_version_compare_handler(void);

/* 命令表 */
static const bootloader_cmd_t cmd_table[] = {
    {"help",    "h",  "Show command help",           CMD_HELP,         cmd_help_handler},
    {"update",  "u",  "Update firmware (XMODEM/OTA)", CMD_UPDATE,       cmd_update_handler},
    {"info",    "i",  "Show system information",    CMD_INFO,         cmd_info_handler},
    {"erase",   "e",  "Erase application area",     CMD_ERASE,        cmd_erase_handler},
    {"reset",   "r",  "Reset system",               CMD_RESET,        cmd_reset_handler},
    {"jump",    "j",  "Jump to application",        CMD_JUMP,         cmd_jump_handler},
    {"xinfo",   "xi", "Show external flash info",   CMD_EXTINFO,      cmd_extinfo_handler},
    {"xbackup", "xb", "Backup to external flash",   CMD_EXTBACKUP,    cmd_extbackup_handler},
    {"xrestore","xr", "Restore from external flash",CMD_EXTRESTORE,   cmd_extrestore_handler},
    {"xlist",   "xl", "List external flash backups",CMD_EXTLIST,      cmd_extlist_handler},
    {"wifi",    "w",  "Connect to WiFi network",    CMD_WIFI_CONNECT, cmd_wifi_connect_handler},
    {"wstatus", "ws", "Show WiFi connection status", CMD_WIFI_STATUS,  cmd_wifi_status_handler},
    {"wdebug",  "wd", "WiFi debug information",     CMD_WIFI_DEBUG,   cmd_wifi_debug_handler},
    {"cfgshow", "cs", "Show current configuration",  CMD_CONFIG_SHOW,  cmd_config_show_handler},
    {"cfgwifi", "cw", "Configure WiFi settings",     CMD_CONFIG_WIFI,  cmd_config_wifi_handler},
    {"cfgota",  "co", "Configure OTA settings",      CMD_CONFIG_OTA,   cmd_config_ota_handler},
    {"cfgsave", "cS", "Save configuration",          CMD_CONFIG_SAVE,  cmd_config_save_handler},
    {"cfgreset","cR", "Reset to default config",     CMD_CONFIG_RESET, cmd_config_reset_handler},
    {"version", "v",  "Show firmware version info",  CMD_VERSION,      cmd_version_handler},
    {"vcompare","vc", "Compare firmware versions",   CMD_VERSION_COMPARE, cmd_version_compare_handler},
};

#define CMD_TABLE_SIZE (sizeof(cmd_table)/sizeof(cmd_table[0]))

/* 私有函数声明 */
void print_str(const char* str);
void print_hex(uint32_t val);
static void print_dec(uint32_t val);
static void process_cmd(char* cmd);
static uint8_t read_char(void);
static void read_line(char* buffer, uint16_t size);
static void show_progress(uint32_t current, uint32_t total, const char* prefix);
static uint32_t calculate_firmware_size(w25q64_partition_id_t pid);


/* 初始化Bootloader */
void bootloader_init(void)
{
    bootloader_state = BOOT_STATE_IDLE;
    cmd_index = 0;
    
    /* 初始化配置系统 */
    if (!config_init()) {
        print_str("WARNING: Config system init failed!\r\n");
    }
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
    
    /* 检查是否为加密固件，需要先解密 */
    if (firmware_crypto_is_encrypted(APP_START_ADDR)) {
        print_str("Encrypted firmware detected, full decryption required...\r\n");
        
        /* 初始化加密模块 */
        const char* crypto_key = "yangcan";
        if (!firmware_crypto_init((uint8_t*)crypto_key, strlen(crypto_key))) {
            print_str("Crypto init failed!\r\n");
            return;
        }
        
        /* 将整个加密固件解密到外部Flash，然后再复制回内部Flash */
        print_str("This feature requires implementation of full decryption.\r\n");
        print_str("Please use XMODEM encrypted option 5 to pre-decrypt firmware.\r\n");
        return;
    }
    
    /* 验证恢复的应用程序向量表 */
    uint32_t app_stack = *(__IO uint32_t*)APP_START_ADDR;
    uint32_t app_reset = *(__IO uint32_t*)(APP_START_ADDR + 4);
    
    print_str("App stack: 0x");
    print_hex(app_stack);
    print_str("\r\nApp reset: 0x");
    print_hex(app_reset);
    print_str("\r\n");
    
    /* 检查复位向量是否在合理范围内 */
    if (app_reset < APP_START_ADDR || app_reset > (APP_START_ADDR + APP_MAX_SIZE)) {
        print_str("Invalid reset vector!\r\n");
        return;
    }
    
    print_str("Jumping to app...\r\n");
    HAL_Delay(100);
    
    /* 完全禁用所有中断 */
    __disable_irq();
    
    /* 复位所有外设到默认状态 */
    HAL_DeInit();
    
    /* 禁用SysTick */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
    
    /* 清除所有挂起的中断 */
    for (int i = 0; i < 8; i++) {
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }
    
    /* 禁用所有中断 */
    for (int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
    }
    
    /* 重置时钟到默认状态（HSI） */
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY));
    
    RCC->CFGR = 0x00000000; /* HSI作为系统时钟 */
    RCC->CR &= 0xFEF6FFFF;  /* 关闭HSE, CSS, PLL */
    RCC->CR &= 0xFFFBFFFF;  /* 关闭HSE旁路 */
    RCC->CFGR &= 0xFF80FFFF; /* 重置PLL配置 */
    RCC->CIR = 0x00000000;   /* 禁用所有RCC中断 */
    
    /* 设置向量表和栈指针 */
    SCB->VTOR = APP_START_ADDR;
    __set_MSP(app_stack);
    
    /* 确保所有缓存操作完成 */
    __DSB();
    __ISB();
    
    /* 跳转到应用程序 */
    void (*app_entry)(void) = (void (*)(void))app_reset;
    app_entry();
}

/* 验证应用程序 */
bool bootloader_validate_app(void)
{
    /* 首先检查是否为加密固件 */
    if (firmware_crypto_is_encrypted(APP_START_ADDR)) {
        print_str("Encrypted firmware detected, decrypting for validation...\r\n");
        
        /* 初始化加密模块 */
        const char* crypto_key = "yangcan";
        if (!firmware_crypto_init((uint8_t*)crypto_key, strlen(crypto_key))) {
            print_str("Crypto init failed!\r\n");
            return false;
        }
        
        /* 验证加密固件头部 */
        firmware_crypto_header_t* header = (firmware_crypto_header_t*)APP_START_ADDR;
        if (!firmware_crypto_validate_header(header)) {
            print_str("Invalid encrypted firmware header!\r\n");
            return false;
        }
        
        /* 解密前几个字节检查栈指针和复位向量 */
        uint8_t decrypted_start[256];
        uint8_t* encrypted_data = (uint8_t*)(APP_START_ADDR + sizeof(firmware_crypto_header_t));
        memcpy(decrypted_start, encrypted_data, 256);
        firmware_crypto_xor(decrypted_start, 256, 0);
        
        uint32_t app_stack = *((uint32_t*)decrypted_start);
        uint32_t app_reset = *((uint32_t*)(decrypted_start + 4));
        
        bool valid = ((app_stack >= 0x20000000) && (app_stack <= 0x20010000) &&
                      (app_reset >= APP_START_ADDR) && (app_reset <= (APP_START_ADDR + APP_MAX_SIZE)));
        
        if (valid) {
            print_str("Encrypted firmware validation passed.\r\n");
        } else {
            print_str("Encrypted firmware validation failed.\r\n");
        }
        
        return valid;
    } else {
        /* 普通未加密固件验证 */
        uint32_t app_stack = *(__IO uint32_t*)APP_START_ADDR;
        uint32_t app_reset = *(__IO uint32_t*)(APP_START_ADDR + 4);
        
        return ((app_stack >= 0x20000000) && (app_stack <= 0x20010000) &&
                (app_reset >= APP_START_ADDR) && (app_reset <= (APP_START_ADDR + APP_MAX_SIZE)));
    }
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
    
    print_str("\r\n===== Firmware Update Methods =====\r\n");
    print_str("XMODEM Options (u -> 1):\r\n");
    print_str("  1 = Internal Flash\r\n");
    print_str("  2 = External Flash (to backup slots)\r\n");
    print_str("  3 = Internal Flash (Encrypted XOR/AES)\r\n");
    print_str("  4 = External Flash (Encrypted + Decrypt)\r\n");
    print_str("\r\nOTA Options (u -> 2):\r\n");
    print_str("  1 = Internal Flash\r\n");
    print_str("  2 = External Flash (to backup slots)\r\n");
    print_str("  3 = Internal Flash (Encrypted XOR/AES)\r\n");
    print_str("  4 = External Flash (Encrypted XOR/AES)\r\n");
    print_str("\r\nExamples:\r\n");
    print_str("  h          - Show this help\r\n");
    print_str("  u          - Update firmware (select method)\r\n");
    print_str("  i          - Show system info\r\n");
    print_str("  w          - Connect to WiFi network\r\n");
    print_str("  ws         - Show WiFi connection status\r\n");
    print_str("  wd         - WiFi debug information\r\n");
    print_str("  xb         - Backup current firmware to slot 1-3\r\n");
    print_str("  xr         - Restore firmware from slot 0-3 (0=download)\r\n");
    print_str("  xl         - List all backup slots status\r\n");
    print_str("===============================================\r\n");
}

/* OTA进度回调函数 */
static void ota_progress_callback(uint32_t current, uint32_t total)
{
    if (total > 0) {
        show_progress(current, total, "Download");
    }
}

/* XMODEM子菜单处理函数前向声明 */
static void cmd_update_xmodem_handler(void);
static void cmd_update_ota_handler(void);

void cmd_update_handler(void)
{
    uint8_t ch;
    
    print_str("===== Firmware Update =====\r\n");
    print_str("Select transfer method:\r\n");
    print_str("1 = XMODEM\r\n");
    print_str("2 = HTTP OTA\r\n");
    print_str("Select (1-2): ");
    
    ch = read_char();
    print_str("\r\n");
    
    if(ch == '1')
    {
        cmd_update_xmodem_handler();
    }
    else if(ch == '2')
    {
        cmd_update_ota_handler();
    }
    else
    {
        print_str("Invalid selection!\r\n");
    }
}

/* XMODEM固件更新子菜单 */
static void cmd_update_xmodem_handler(void)
{
    uint8_t ch;
    
    print_str("===== XMODEM Firmware Update =====\r\n");
    print_str("Select destination and type:\r\n");
    print_str("1 = Internal Flash\r\n");
    print_str("2 = External Flash\r\n");
    print_str("3 = Internal Flash (Encrypted XOR/AES)\r\n");
    print_str("4 = External Flash (Encrypted XOR/AES)\r\n");
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
        uint8_t slot = read_char() - '0';
        print_str("\r\n");
        
        if(slot < 1 || slot > 3)
        {
            print_str("Invalid slot!\r\n");
            return;
        }
        
        print_str("Start XMODEM transfer to slot ");
        print_dec(slot);
        print_str("\r\n");
        int result = xmodem_receive(0, true, W25Q64_PARTITION_BACKUP1 + slot - 1, true);
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
        /* XMODEM加密固件到内部Flash */
        print_str("WARNING! Update internal flash with encrypted firmware? (y/n): ");
        if(read_char() != 'y')
        {
            print_str("\r\nCancelled\r\n");
            return;
        }
        print_str("\r\n");
        
        /* 选择加密算法 */
        print_str("Select encryption algorithm:\r\n");
        print_str("1. XOR encryption\r\n");
        print_str("2. AES-128-CBC encryption\r\n");
        print_str("Choice (1-2): ");
        uint8_t encrypt_choice = read_char();
        print_str("\r\n");
        
        if (encrypt_choice != '1' && encrypt_choice != '2') {
            print_str("Invalid choice!\r\n");
            return;
        }
        
        bool use_aes = (encrypt_choice == '2');
        
        /* 初始化加密模块 */
        const char* crypto_key = "yangcan";
        if (!firmware_crypto_init((uint8_t*)crypto_key, strlen(crypto_key))) {
            print_str("Crypto init failed!\r\n");
            return;
        }
        
        /* 如果选择AES，还需要初始化AES */
        if (use_aes) {
            uint32_t* unique_id = (uint32_t*)0x1FFFF7E8;
            uint8_t aes_key[16];
            firmware_aes_derive_key(crypto_key, unique_id, aes_key);
            if (!firmware_aes_init(aes_key)) {
                print_str("AES init failed!\r\n");
                return;
            }
            print_str("Using AES-128-CBC encryption\r\n");
        } else {
            print_str("Using XOR encryption\r\n");
        }
        
        print_str("Erasing app area...\r\n");
        if(!bootloader_flash_erase(APP_START_ADDR, APP_MAX_SIZE))
        {
            print_str("Erase failed!\r\n");
            return;
        }
        
        print_str("Start XMODEM transfer (encrypted firmware)\r\n");
        int result = xmodem_receive(APP_START_ADDR, false, 0, true);
        if(result > 0)
        {
            print_str("Transfer complete: ");
            print_dec(result);
            print_str(" bytes\r\n");
            
            /* 检查是否为加密固件 */
            bool is_xor_encrypted = firmware_crypto_is_encrypted(APP_START_ADDR);
            bool is_aes_encrypted = firmware_aes_is_encrypted(APP_START_ADDR);
            
            if (is_xor_encrypted || is_aes_encrypted) {
                if (is_aes_encrypted) {
                    print_str("AES encrypted firmware detected, decrypting...\r\n");
                } else {
                    print_str("XOR encrypted firmware detected, decrypting...\r\n");
                }
                
                /* 将加密固件移动到临时区域（使用外部Flash） */
                w25q64_init();
                if (!w25q64_erase_partition(W25Q64_PARTITION_DOWNLOAD)) {
                    print_str("Failed to prepare temp area!\r\n");
                    return;
                }
                
                /* 备份加密固件到外部Flash */
                uint8_t buf[1024];
                for (uint32_t offset = 0; offset < result; offset += 1024) {
                    uint32_t size = (result - offset > 1024) ? 1024 : (result - offset);
                    memcpy(buf, (uint8_t*)(APP_START_ADDR + offset), size);
                    if (!w25q64_write_partition(W25Q64_PARTITION_DOWNLOAD, offset, buf, size)) {
                        print_str("Backup to external flash failed!\r\n");
                        return;
                    }
                }
                
                print_str("\r\nEncrypted firmware downloaded to temporary storage!\r\n");
                print_str("Use 'xr 0' to decrypt and restore to internal flash.\r\n");
                print_str("Use 'j' to jump to the firmware after restore.\r\n");
            } else {
                print_str("Warning: Firmware is not encrypted!\r\n");
            }
        }
        else
        {
            print_str("Transfer failed!\r\n");
        }
    }
    else if(ch == '4')
    {
        /* XMODEM加密固件到外部Flash */
        print_str("Slot (1-3): ");
        uint8_t slot = read_char() - '0';
        print_str("\r\n");
        
        if(slot < 1 || slot > 3)
        {
            print_str("Invalid slot!\r\n");
            return;
        }
        
        /* 选择加密算法 */
        print_str("Select encryption algorithm:\r\n");
        print_str("1. XOR encryption\r\n");
        print_str("2. AES-128-CBC encryption\r\n");
        print_str("Choice (1-2): ");
        uint8_t encrypt_choice = read_char();
        print_str("\r\n");
        
        if (encrypt_choice != '1' && encrypt_choice != '2') {
            print_str("Invalid choice!\r\n");
            return;
        }
        
        if (encrypt_choice == '2') {
            print_str("Using AES-128-CBC encryption\r\n");
        } else {
            print_str("Using XOR encryption\r\n");
        }
        
        print_str("Start XMODEM transfer (encrypted firmware to external flash slot ");
        print_dec(slot);
        print_str(")\r\n");
        int result = xmodem_receive(0, true, W25Q64_PARTITION_BACKUP1 + slot - 1, true);
        if(result > 0)
        {
            print_str("Success: ");
            print_dec(result);
            print_str(" bytes\r\n");
            print_str("Use 'xr ");
            print_dec(slot);
            print_str("' to decrypt and restore to internal flash.\r\n");
            print_str("Use 'j' to jump to the firmware after restore.\r\n");
        }
        else
        {
            print_str("Transfer failed!\r\n");
        }
    }
    else if(ch == '3')
    {
        /* XMODEM加密固件到内部Flash */
        print_str("Download encrypted firmware to temporary storage via XMODEM? (y/n): ");
        if(read_char() != 'y')
        {
            print_str("\r\nCancelled\r\n");
            return;
        }
        print_str("\r\n");
        
        /* 选择加密算法 */
        print_str("Select encryption algorithm:\r\n");
        print_str("1. XOR encryption\r\n");
        print_str("2. AES-128-CBC encryption\r\n");
        print_str("Choice (1-2): ");
        uint8_t encrypt_choice = read_char();
        print_str("\r\n");
        
        if (encrypt_choice != '1' && encrypt_choice != '2') {
            print_str("Invalid choice!\r\n");
            return;
        }
        
        if (encrypt_choice == '2') {
            print_str("Using AES-128-CBC encryption\r\n");
        } else {
            print_str("Using XOR encryption\r\n");
        }
        
        print_str("Start XMODEM transfer to temporary storage\r\n");
        int result = xmodem_receive(0, true, W25Q64_PARTITION_DOWNLOAD, true);
        if(result > 0)
        {
            print_str("Success: ");
            print_dec(result);
            print_str(" bytes\r\n");
            print_str("Use 'xr 0' to decrypt and restore to internal flash.\r\n");
            print_str("Use 'j' to jump to the firmware after restore.\r\n");
        }
        else
        {
            print_str("Transfer failed!\r\n");
        }
    }
    else if(ch == '4')
    {
        /* HTTP OTA加密固件到外部Flash */
        print_str("Slot (1-3): ");
        uint8_t slot = read_char() - '0';
        print_str("\r\n");
        
        if(slot < 1 || slot > 3)
        {
            print_str("Invalid slot!\r\n");
            return;
        }
        
        /* 选择加密算法 */
        print_str("Select encryption algorithm:\r\n");
        print_str("1. XOR encryption\r\n");
        print_str("2. AES-128-CBC encryption\r\n");
        print_str("Choice (1-2): ");
        uint8_t encrypt_choice = read_char();
        print_str("\r\n");
        
        if (encrypt_choice != '1' && encrypt_choice != '2') {
            print_str("Invalid choice!\r\n");
            return;
        }
        
        if (encrypt_choice == '2') {
            print_str("Using AES-128-CBC encryption\r\n");
        } else {
            print_str("Using XOR encryption\r\n");
        }
        
        /* 检查ESP8266是否已初始化 */
        if (!g_wifi_initialized) {
            print_str("ESP8266 not initialized! Please use 'w' command first.\r\n");
            return;
        }
        
        /* 检查WiFi连接状态 */
        if (esp8266_get_wifi_status(&g_wifi_device) != ESP8266_WIFI_GOT_IP) {
            print_str("WiFi not connected! Please connect WiFi first.\r\n");
            return;
        }
        
        /* 初始化OTA */
        static ota_context_t ota_ctx;
        print_str("Initializing OTA...\r\n");
        if (!ota_init(&ota_ctx, &g_wifi_device)) {
            print_str("OTA init failed!\r\n");
            return;
        }
        
        /* 设置进度回调 */
        ota_set_progress_callback(&ota_ctx, ota_progress_callback);
        
        /* 使用配置的OTA服务器 */
        const bootloader_config_t* cfg = config_get();
        if (!cfg) {
            print_str("Configuration not loaded!\r\n");
            ota_deinit(&ota_ctx);
            return;
        }
        
        /* 构建完整的URL */
        char firmware_url[256];
        snprintf(firmware_url, sizeof(firmware_url), "http://%s:%d%s", 
                 cfg->ota.host, cfg->ota.port, cfg->ota.path);
        
        print_str("Starting OTA download to external flash slot ");
        print_dec(slot);
        print_str("...\r\n");
        print_str("URL: ");
        print_str(firmware_url);
        print_str("\r\n");
        
        w25q64_partition_id_t target_partition = W25Q64_PARTITION_BACKUP1 + slot - 1;
        
        ota_status_t result = ota_download_firmware(&ota_ctx, firmware_url,
                                                   OTA_TARGET_EXTERNAL_FLASH, 
                                                   target_partition, 1024*1024, true);
        
        ota_deinit(&ota_ctx);
        
        if (result == OTA_STATUS_OK) {
            print_str("\r\nEncrypted OTA download to external flash completed!\r\n");
            print_str("Use 'xr ");
            print_dec(slot);
            print_str("' to decrypt and restore to internal flash.\r\n");
            print_str("Use 'j' to jump to the firmware after restore.\r\n");
        } else {
            print_str("\r\nOTA download failed with error: ");
            print_dec(result);
            print_str("\r\n");
        }
    }
    else
    {
        print_str("Invalid selection!\r\n");
    }
}

/* HTTP OTA固件更新子菜单 */
static void cmd_update_ota_handler(void)
{
    uint8_t ch;
    
    print_str("===== HTTP OTA Firmware Update =====\r\n");
    print_str("Select destination and type:\r\n");
    print_str("1 = Internal Flash\r\n");
    print_str("2 = External Flash\r\n");
    print_str("3 = Internal Flash (Encrypted XOR/AES)\r\n");
    print_str("4 = External Flash (Encrypted XOR/AES)\r\n");
    print_str("Select (1-4): ");
    
    ch = read_char();
    print_str("\r\n");
    
    if(ch == '1')
    {
        /* HTTP OTA到内部Flash */
        print_str("WARNING! Update internal flash via OTA? (y/n): ");
        if(read_char() != 'y')
        {
            print_str("\r\nCancelled\r\n");
            return;
        }
        print_str("\r\n");
        
        /* 检查ESP8266是否已初始化 */
        if (!g_wifi_initialized) {
            print_str("ESP8266 not initialized! Please use 'w' command first.\r\n");
            return;
        }
        
        /* 检查WiFi连接状态 */
        if (esp8266_get_wifi_status(&g_wifi_device) != ESP8266_WIFI_GOT_IP) {
            print_str("WiFi not connected! Please connect WiFi first.\r\n");
            return;
        }
        
        /* 初始化OTA */
        static ota_context_t ota_ctx;
        
        print_str("Initializing OTA...\r\n");
        if (!ota_init(&ota_ctx, &g_wifi_device)) {
            print_str("OTA init failed!\r\n");
            return;
        }
        
        /* 设置进度回调 */
        ota_set_progress_callback(&ota_ctx, ota_progress_callback);
        
        /* 使用配置的OTA服务器 */
        const bootloader_config_t* cfg = config_get();
        if (!cfg) {
            print_str("Configuration not loaded!\r\n");
            ota_deinit(&ota_ctx);
            return;
        }
        
        /* 构建完整的URL */
        char firmware_url[256];
        snprintf(firmware_url, sizeof(firmware_url), "http://%s:%d%s", 
                 cfg->ota.host, cfg->ota.port, cfg->ota.path);
        
        print_str("Starting OTA download...\r\n");
        print_str("URL: ");
        print_str(firmware_url);
        print_str("\r\n");
        
        ota_status_t result = ota_download_firmware(&ota_ctx, firmware_url,
                                                   OTA_TARGET_INTERNAL_FLASH, 
                                                   APP_START_ADDR, APP_MAX_SIZE, false);
        
        ota_deinit(&ota_ctx);
        
        if (result == OTA_STATUS_OK) {
            print_str("\r\nOTA update completed successfully!\r\n");
        } else {
            print_str("\r\nOTA update failed with error: ");
            print_dec(result);
            print_str("\r\n");
        }
    }
    else if(ch == '2')
    {
        /* HTTP OTA到外部Flash */
        print_str("Slot (1-3): ");
        uint8_t slot = read_char() - '0';
        print_str("\r\n");
        
        if(slot < 1 || slot > 3)
        {
            print_str("Invalid slot!\r\n");
            return;
        }
        
        /* 检查ESP8266是否已初始化 */
        if (!g_wifi_initialized) {
            print_str("ESP8266 not initialized! Please use 'w' command first.\r\n");
            return;
        }
        
        /* 检查WiFi连接状态 */
        if (esp8266_get_wifi_status(&g_wifi_device) != ESP8266_WIFI_GOT_IP) {
            print_str("WiFi not connected! Please connect WiFi first.\r\n");
            return;
        }
        
        /* 初始化OTA */
        static ota_context_t ota_ctx;
        
        print_str("Initializing OTA...\r\n");
        if (!ota_init(&ota_ctx, &g_wifi_device)) {
            print_str("OTA init failed!\r\n");
            return;
        }
        
        /* 设置进度回调 */
        ota_set_progress_callback(&ota_ctx, ota_progress_callback);
        
        /* 使用配置的OTA服务器 */
        const bootloader_config_t* cfg = config_get();
        if (!cfg) {
            print_str("Configuration not loaded!\r\n");
            ota_deinit(&ota_ctx);
            return;
        }
        
        /* 构建完整的URL */
        char firmware_url[256];
        snprintf(firmware_url, sizeof(firmware_url), "http://%s:%d%s", 
                 cfg->ota.host, cfg->ota.port, cfg->ota.path);
        
        print_str("Starting OTA download to slot ");
        print_dec(ch);
        print_str("...\r\n");
        print_str("URL: ");
        print_str(firmware_url);
        print_str("\r\n");
        print_str("WiFi Status: ");
        esp8266_wifi_status_t wifi_status = esp8266_get_wifi_status(&g_wifi_device);
        switch(wifi_status) {
            case ESP8266_WIFI_GOT_IP:
                print_str("Connected with IP (");
                print_str(g_wifi_device.ip_addr);
                print_str(")\r\n");
                break;
            default:
                print_str("Not ready for OTA\r\n");
                return;
        }
        
        w25q64_partition_id_t partition = W25Q64_PARTITION_BACKUP1 + slot - 1;
        
        ota_status_t result = ota_download_firmware(&ota_ctx, firmware_url,
                                                   OTA_TARGET_EXTERNAL_FLASH, 
                                                   partition, 1024*1024, false);  /* 1MB最大 */
        
        ota_deinit(&ota_ctx);
        
        if (result == OTA_STATUS_OK) {
            print_str("\r\nOTA update to external flash completed!\r\n");
            print_str("Use 'xr ");
            print_dec(slot);
            print_str("' to restore this firmware.\r\n");
        } else {
            print_str("\r\nOTA update failed with error: ");
            print_dec(result);
            print_str(" (");
            switch(result) {
                case OTA_STATUS_ERROR: print_str("General Error"); break;
                case OTA_STATUS_TIMEOUT: print_str("Timeout"); break;
                case OTA_STATUS_WIFI_ERROR: print_str("WiFi Error"); break;
                case OTA_STATUS_HTTP_ERROR: print_str("HTTP Error"); break;
                case OTA_STATUS_FLASH_ERROR: print_str("Flash Error"); break;
                case OTA_STATUS_SIZE_ERROR: print_str("Size Error"); break;
                default: print_str("Unknown Error"); break;
            }
            print_str(")\r\n");
        }
    }
    else if(ch == '3')
    {
        /* HTTP OTA加密固件到内部Flash */
        print_str("WARNING! Update internal flash with encrypted firmware? (y/n): ");
        if(read_char() != 'y')
        {
            print_str("\r\nCancelled\r\n");
            return;
        }
        print_str("\r\n");
        
        /* 选择加密算法 */
        print_str("Select encryption algorithm:\r\n");
        print_str("1. XOR encryption\r\n");
        print_str("2. AES-128-CBC encryption\r\n");
        print_str("Choice (1-2): ");
        uint8_t encrypt_choice = read_char();
        print_str("\r\n");
        
        if (encrypt_choice != '1' && encrypt_choice != '2') {
            print_str("Invalid choice!\r\n");
            return;
        }
        
        bool use_aes = (encrypt_choice == '2');
        
        /* 初始化加密模块 */
        const char* crypto_key = "yangcan";
        if (!firmware_crypto_init((uint8_t*)crypto_key, strlen(crypto_key))) {
            print_str("Crypto init failed!\r\n");
            return;
        }
        
        /* 如果选择AES，还需要初始化AES */
        if (use_aes) {
            uint32_t* unique_id = (uint32_t*)0x1FFFF7E8;
            uint8_t aes_key[16];
            firmware_aes_derive_key(crypto_key, unique_id, aes_key);
            if (!firmware_aes_init(aes_key)) {
                print_str("AES init failed!\r\n");
                return;
            }
            print_str("Using AES-128-CBC encryption\r\n");
        } else {
            print_str("Using XOR encryption\r\n");
        }
        
        print_str("Erasing app area...\r\n");
        if(!bootloader_flash_erase(APP_START_ADDR, APP_MAX_SIZE))
        {
            print_str("Erase failed!\r\n");
            return;
        }
        
        print_str("Start XMODEM transfer (encrypted firmware)\r\n");
        int result = xmodem_receive(APP_START_ADDR, false, 0, true);
        if(result > 0)
        {
            print_str("Transfer complete: ");
            print_dec(result);
            print_str(" bytes\r\n");
            
            /* 检查是否为加密固件 */
            bool is_xor_encrypted = firmware_crypto_is_encrypted(APP_START_ADDR);
            bool is_aes_encrypted = firmware_aes_is_encrypted(APP_START_ADDR);
            
            if (is_xor_encrypted || is_aes_encrypted) {
                if (is_aes_encrypted) {
                    print_str("AES encrypted firmware detected, decrypting...\r\n");
                } else {
                    print_str("XOR encrypted firmware detected, decrypting...\r\n");
                }
                
                /* 将加密固件移动到临时区域（使用外部Flash） */
                w25q64_init();
                if (!w25q64_erase_partition(W25Q64_PARTITION_DOWNLOAD)) {
                    print_str("Failed to prepare temp area!\r\n");
                    return;
                }
                
                /* 备份加密固件到外部Flash */
                uint8_t buf[1024];
                for (uint32_t offset = 0; offset < result; offset += 1024) {
                    uint32_t size = (result - offset > 1024) ? 1024 : (result - offset);
                    memcpy(buf, (uint8_t*)(APP_START_ADDR + offset), size);
                    if (!w25q64_write_partition(W25Q64_PARTITION_DOWNLOAD, offset, buf, size)) {
                        print_str("Backup to external flash failed!\r\n");
                        return;
                    }
                }
                
                /* 擦除内部Flash准备存储解密固件 */
                print_str("Erasing for decrypted firmware...\r\n");
                if(!bootloader_flash_erase(APP_START_ADDR, APP_MAX_SIZE))
                {
                    print_str("Erase failed!\r\n");
                    return;
                }
                
                /* 从外部Flash解密到内部Flash */
                uint32_t decrypted_size = 0;
                uint32_t expected_crc32 = 0;
                uint32_t header_size = 0;
                uint8_t aes_iv[AES_IV_SIZE];
                
                if (is_aes_encrypted) {
                    firmware_aes_header_t aes_header;
                    if (w25q64_read_partition(W25Q64_PARTITION_DOWNLOAD, 0, (uint8_t*)&aes_header, sizeof(aes_header))) {
                        decrypted_size = aes_header.firmware_size;
                        expected_crc32 = aes_header.crc32;
                        header_size = sizeof(firmware_aes_header_t);
                        memcpy(aes_iv, aes_header.iv, AES_IV_SIZE);
                    }
                } else {
                    firmware_crypto_header_t xor_header;
                    if (w25q64_read_partition(W25Q64_PARTITION_DOWNLOAD, 0, (uint8_t*)&xor_header, sizeof(xor_header))) {
                        decrypted_size = xor_header.firmware_size;
                        expected_crc32 = xor_header.crc32;
                        header_size = sizeof(firmware_crypto_header_t);
                    }
                }
                
                if (decrypted_size > 0) {
                    print_str("Decrypting firmware (");
                    print_dec(decrypted_size);
                    print_str(" bytes)...\r\n");
                    
                    if (is_aes_encrypted) {
                        /* 内存优化的AES解密方案 - 分块处理避免RAM不足 */
                        print_str("Using memory-optimized AES decryption...\r\n");
                        
                        /* 读取AES头部信息 */
                        firmware_aes_header_t aes_header;
                        if (!w25q64_read_partition(W25Q64_PARTITION_DOWNLOAD, 0, (uint8_t*)&aes_header, sizeof(aes_header))) {
                            print_str("Failed to read AES header!\r\n");
                            return;
                        }
                        
                        print_str("Header - Magic: ");
                        print_hex(aes_header.magic);
                        print_str(", FW Size: ");
                        print_dec(aes_header.firmware_size);
                        print_str(", Enc Size: ");
                        print_dec(aes_header.encrypted_size);
                        print_str("\r\n");
                        
                        /* 初始化流式AES解密器 */
                        print_str("Initializing streaming AES decryption...\r\n");
                        
                        streaming_aes_ctx_t aes_ctx;
                        uint32_t* unique_id = (uint32_t*)0x1FFFF7E8;
                        uint8_t aes_key[16];
                        firmware_aes_derive_key("yangcan", unique_id, aes_key);
                        
                        if (!streaming_aes_init(&aes_ctx, aes_key, aes_header.iv)) {
                            print_str("Failed to initialize streaming AES!\r\n");
                            return;
                        }
                        
                        print_str("Starting streaming AES-CBC decryption...\r\n");
                        uint8_t* work_buffer = (uint8_t*)(0x20000000 + 0x8000);  /* 4KB工作缓冲区 */
                        uint8_t* decrypt_buffer = work_buffer + 2048;  /* 2KB解密输出缓冲区 */
                        uint32_t buffer_size = 2048;  /* 减少到2KB确保16字节对齐 */
                        uint32_t decrypted_total = 0;
                        
                        HAL_FLASH_Unlock();
                        
                        /* 分块读取、解密、写入 */
                        for (uint32_t offset = 0; offset < aes_header.encrypted_size; offset += buffer_size) {
                            uint32_t chunk_size = (aes_header.encrypted_size - offset > buffer_size) ? 
                                                 buffer_size : (aes_header.encrypted_size - offset);
                            
                            /* 确保chunk_size是16字节的倍数 */
                            chunk_size = (chunk_size / 16) * 16;
                            if (chunk_size == 0) chunk_size = 16;
                            
                            /* 从外部Flash读取加密数据块 */
                            if (!w25q64_read_partition(W25Q64_PARTITION_DOWNLOAD, 
                                                     sizeof(firmware_aes_header_t) + offset, 
                                                     work_buffer, chunk_size)) {
                                print_str("\r\nRead encrypted chunk failed!\r\n");
                                HAL_FLASH_Lock();
                                return;
                            }
                            
                            /* 使用流式AES-CBC解密 */
                            uint32_t decrypted_chunk_size = streaming_aes_decrypt(&aes_ctx, work_buffer, decrypt_buffer, chunk_size);
                            if (decrypted_chunk_size == 0) {
                                print_str("\r\nAES decryption failed!\r\n");
                                HAL_FLASH_Lock();
                                return;
                            }
                            
                            /* 计算实际写入大小（处理最后一块的情况） */
                            uint32_t write_size = decrypted_chunk_size;
                            if (decrypted_total + decrypted_chunk_size > aes_header.firmware_size) {
                                write_size = aes_header.firmware_size - decrypted_total;
                            }
                            
                            /* 如果是最后一块，需要移除PKCS7填充 */
                            if (offset + chunk_size >= aes_header.encrypted_size) {
                                uint32_t unpadded_size = firmware_aes_pkcs7_unpad(decrypt_buffer, decrypted_chunk_size);
                                if (unpadded_size > 0 && decrypted_total + unpadded_size <= aes_header.firmware_size) {
                                    write_size = unpadded_size;
                                }
                            }
                            
                            /* 写入解密数据到Flash */
                            if (write_size > 0) {
                                if (!bootloader_flash_write(APP_START_ADDR + decrypted_total, decrypt_buffer, write_size)) {
                                    print_str("\r\nFlash write failed!\r\n");
                                    HAL_FLASH_Lock();
                                    return;
                                }
                                decrypted_total += write_size;
                            }
                            
                            /* 显示进度 */
                            if ((offset % 8192) == 0 || offset + chunk_size >= aes_header.encrypted_size) {
                                show_progress(offset + chunk_size, aes_header.encrypted_size, "AES-CBC");
                            }
                            
                            /* 如果已经解密完所有原始数据，提前退出 */
                            if (decrypted_total >= aes_header.firmware_size) {
                                break;
                            }
                        }
                        
                        HAL_FLASH_Lock();
                        
                        print_str("\r\nMemory-optimized decryption completed: ");
                        print_dec(decrypted_total);
                        print_str(" bytes\r\n");
                        
                        /* 验证解密后的固件 */
                        if (firmware_crypto_verify_firmware(APP_START_ADDR, decrypted_total, expected_crc32)) {
                            print_str("AES firmware verification successful!\r\n");
                        } else {
                            print_str("AES firmware verification failed!\r\n");
                        }
                    } else {
                        /* XOR解密：保持原来的分块处理方式 */
                        uint8_t* buffer = (uint8_t*)(0x20000000 + 0x5000); /* 临时RAM区域 */
                        uint32_t buffer_size = 2048;
                        
                        HAL_FLASH_Unlock();
                        for (uint32_t offset = 0; offset < decrypted_size; offset += buffer_size) {
                            uint32_t chunk_size = (decrypted_size - offset > buffer_size) ? buffer_size : (decrypted_size - offset);
                            
                            /* 从外部Flash读取加密数据 */
                            if (!w25q64_read_partition(W25Q64_PARTITION_DOWNLOAD, header_size + offset, buffer, chunk_size)) {
                                print_str("\r\nRead encrypted data failed!\r\n");
                                HAL_FLASH_Lock();
                                return;
                            }
                            
                            /* XOR解密 */
                            firmware_crypto_xor(buffer, chunk_size, offset);
                            
                            /* 写入内部Flash */
                            if (!bootloader_flash_write(APP_START_ADDR + offset, buffer, chunk_size)) {
                                print_str("\r\nWrite decrypted firmware failed!\r\n");
                                HAL_FLASH_Lock();
                                return;
                            }
                            
                            if ((offset % 0x2000) == 0 || offset + chunk_size >= decrypted_size) {
                                show_progress(offset + chunk_size, decrypted_size, "Decrypt");
                            }
                        }
                        HAL_FLASH_Lock();
                        
                        /* 验证解密后的固件 */
                        if (firmware_crypto_verify_firmware(APP_START_ADDR, decrypted_size, expected_crc32)) {
                            print_str("\r\nXOR decryption and verification successful!\r\n");
                        } else {
                            print_str("\r\nXOR firmware verification failed!\r\n");
                        }
                    }
                } else {
                    print_str("Read header failed!\r\n");
                }
            } else {
                print_str("Warning: Firmware is not encrypted!\r\n");
            }
        }
        else
        {
            print_str("Transfer failed!\r\n");
        }
    }
    else if(ch == '6')
    {
        /* XMODEM加密固件到外部Flash 或 从外部Flash解密到内部Flash */
        print_str("External flash encrypted firmware options:\r\n");
        print_str("1. Upload encrypted firmware to external flash\r\n");
        print_str("2. Decrypt external flash firmware to internal flash\r\n");
        print_str("Choice (1-2): ");
        uint8_t operation = read_char();
        print_str("\r\n");
        
        if (operation == '1') {
            /* 上传加密固件到外部Flash */
            print_str("Slot (1-3): ");
            ch = read_char() - '0';
            print_str("\r\n");
            
            if(ch < 1 || ch > 3)
            {
                print_str("Invalid slot!\r\n");
                return;
            }
            
            /* 选择加密算法 */
            print_str("Select encryption algorithm:\r\n");
            print_str("1. XOR encryption\r\n");
            print_str("2. AES-128-CBC encryption\r\n");
            print_str("Choice (1-2): ");
            uint8_t encrypt_choice = read_char();
            print_str("\r\n");
            
            if (encrypt_choice != '1' && encrypt_choice != '2') {
                print_str("Invalid choice!\r\n");
                return;
            }
            
            if (encrypt_choice == '2') {
                print_str("Using AES-128-CBC encryption\r\n");
            } else {
                print_str("Using XOR encryption\r\n");
            }
            
            print_str("Start XMODEM transfer (encrypted firmware to external flash slot ");
            print_dec(ch);
            print_str(")\r\n");
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
        } else if (operation == '2') {
            /* 从外部Flash解密到内部Flash */
            print_str("WARNING! This will overwrite internal flash! (y/n): ");
            if(read_char() != 'y')
            {
                print_str("\r\nCancelled\r\n");
                return;
            }
            print_str("\r\n");
            
            print_str("Select source slot (1-3): ");
            ch = read_char() - '0';
            print_str("\r\n");
            
            if(ch < 1 || ch > 3)
            {
                print_str("Invalid slot!\r\n");
                return;
            }
            
            w25q64_partition_id_t source_partition = W25Q64_PARTITION_BACKUP1 + ch - 1;
            
            /* 解密外部Flash固件到内部Flash */
            if (decrypt_external_firmware_to_internal(source_partition, get_dynamic_password())) {
                print_str("External firmware decryption successful!\r\n");
                print_str("You can now use 'j' to jump to the new firmware.\r\n");
            } else {
                print_str("External firmware decryption failed!\r\n");
            }
        } else {
            print_str("Invalid operation!\r\n");
        }
    }
    else if(ch >= '7' && ch <= '8')
    {
        /* HTTP OTA加密固件 */
        bool to_internal = (ch == '7');
        uint8_t slot = 0;
        
        if (to_internal) {
            print_str("WARNING! Update internal flash via OTA (encrypted)? (y/n): ");
            if(read_char() != 'y')
            {
                print_str("\r\nCancelled\r\n");
                return;
            }
            print_str("\r\n");
        } else {
            print_str("Slot (1-3): ");
            slot = read_char() - '0';
            print_str("\r\n");
            
            if(slot < 1 || slot > 3)
            {
                print_str("Invalid slot!\r\n");
                return;
            }
        }
        
        /* 选择加密算法 */
        print_str("Select encryption algorithm:\r\n");
        print_str("1. XOR encryption\r\n");
        print_str("2. AES-128-CBC encryption\r\n");
        print_str("Choice (1-2): ");
        uint8_t encrypt_choice = read_char();
        print_str("\r\n");
        
        if (encrypt_choice != '1' && encrypt_choice != '2') {
            print_str("Invalid choice!\r\n");
            return;
        }
        
        if (encrypt_choice == '2') {
            print_str("Using AES-128-CBC encryption\r\n");
        } else {
            print_str("Using XOR encryption\r\n");
        }
        
        /* 检查ESP8266是否已初始化 */
        if (!g_wifi_initialized) {
            print_str("ESP8266 not initialized! Please use 'w' command first.\r\n");
            return;
        }
        
        /* 检查WiFi连接状态 */
        if (esp8266_get_wifi_status(&g_wifi_device) != ESP8266_WIFI_GOT_IP) {
            print_str("WiFi not connected! Please connect WiFi first.\r\n");
            return;
        }
        
        print_str("HTTP OTA encrypted firmware - Phase 1 Implementation:\r\n");
        print_str("1. Download encrypted firmware to temporary partition\r\n");
        print_str("2. Decrypt and install automatically\r\n");
        print_str("\r\n");
        
        /* 使用配置的OTA服务器 */
        const bootloader_config_t* cfg = config_get();
        if (!cfg) {
            print_str("Configuration not loaded!\r\n");
            return;
        }
        
        /* 构建完整的URL */
        char firmware_url[256];
        snprintf(firmware_url, sizeof(firmware_url), "http://%s:%d%s", 
                 cfg->ota.host, cfg->ota.port, cfg->ota.path);
        
        print_str("URL: ");
        print_str(firmware_url);
        print_str("\r\n");
        
        /* 初始化OTA */
        static ota_context_t ota_ctx;
        if (!ota_init(&ota_ctx, &g_wifi_device)) {
            print_str("OTA init failed!\r\n");
            return;
        }
        
        /* 设置进度回调 */
        ota_set_progress_callback(&ota_ctx, ota_progress_callback);
        
        print_str("Starting encrypted OTA download...\r\n");
        
        if (to_internal) {
            /* 下载到临时分区，然后解密到内部Flash */
            ota_status_t result = ota_download_firmware(&ota_ctx, firmware_url,
                                                       OTA_TARGET_EXTERNAL_FLASH, 
                                                       W25Q64_PARTITION_DOWNLOAD, 1024*1024, true);
            
            if (result == OTA_STATUS_OK) {
                print_str("\r\nEncrypted OTA download completed!\r\n");
                print_str("Use 'xr 0' to decrypt and restore to internal flash.\r\n");
                print_str("Use 'j' to jump to the firmware after restore.\r\n");
            } else {
                print_str("\r\nOTA download failed: ");
                print_dec(result);
                print_str("\r\n");
            }
        } else {
            /* 下载到指定外部Flash分区 */
            w25q64_partition_id_t target_partition = W25Q64_PARTITION_BACKUP1 + slot - 1;
            
            ota_status_t result = ota_download_firmware(&ota_ctx, firmware_url,
                                                       OTA_TARGET_EXTERNAL_FLASH, 
                                                       target_partition, 1024*1024, true);
            
            if (result == OTA_STATUS_OK) {
                print_str("\r\nEncrypted OTA download to external flash completed!\r\n");
                print_str("Use 'xr ");
                print_dec(slot);
                print_str("' to decrypt and restore to internal flash.\r\n");
                print_str("Use 'j' to jump to the firmware after restore.\r\n");
            } else {
                print_str("\r\nOTA download failed: ");
                print_dec(result);
                print_str("\r\n");
            }
        }
        
        ota_deinit(&ota_ctx);
    }
    else if(ch == '3')
    {
        /* HTTP OTA加密固件到内部Flash */
        print_str("Download encrypted firmware to temporary storage via OTA? (y/n): ");
        if(read_char() != 'y')
        {
            print_str("\r\nCancelled\r\n");
            return;
        }
        print_str("\r\n");
        
        /* 选择加密算法 */
        print_str("Select encryption algorithm:\r\n");
        print_str("1. XOR encryption\r\n");
        print_str("2. AES-128-CBC encryption\r\n");
        print_str("Choice (1-2): ");
        uint8_t encrypt_choice = read_char();
        print_str("\r\n");
        
        if (encrypt_choice != '1' && encrypt_choice != '2') {
            print_str("Invalid choice!\r\n");
            return;
        }
        
        if (encrypt_choice == '2') {
            print_str("Using AES-128-CBC encryption\r\n");
        } else {
            print_str("Using XOR encryption\r\n");
        }
        
        /* 检查ESP8266是否已初始化 */
        if (!g_wifi_initialized) {
            print_str("ESP8266 not initialized! Please use 'w' command first.\r\n");
            return;
        }
        
        /* 检查WiFi连接状态 */
        if (esp8266_get_wifi_status(&g_wifi_device) != ESP8266_WIFI_GOT_IP) {
            print_str("WiFi not connected! Please connect WiFi first.\r\n");
            return;
        }
        
        /* 初始化OTA */
        static ota_context_t ota_ctx;
        print_str("Initializing OTA...\r\n");
        if (!ota_init(&ota_ctx, &g_wifi_device)) {
            print_str("OTA init failed!\r\n");
            return;
        }
        
        /* 设置进度回调 */
        ota_set_progress_callback(&ota_ctx, ota_progress_callback);
        
        /* 使用配置的OTA服务器 */
        const bootloader_config_t* cfg = config_get();
        if (!cfg) {
            print_str("Configuration not loaded!\r\n");
            ota_deinit(&ota_ctx);
            return;
        }
        
        /* 构建完整的URL */
        char firmware_url[256];
        snprintf(firmware_url, sizeof(firmware_url), "http://%s:%d%s", 
                 cfg->ota.host, cfg->ota.port, cfg->ota.path);
        
        print_str("Starting OTA download to temporary storage...\r\n");
        print_str("URL: ");
        print_str(firmware_url);
        print_str("\r\n");
        
        /* 下载到下载分区 */
        ota_status_t result = ota_download_firmware(&ota_ctx, firmware_url,
                                                   OTA_TARGET_EXTERNAL_FLASH, 
                                                   W25Q64_PARTITION_DOWNLOAD, 1024*1024, true);
        
        ota_deinit(&ota_ctx);
        
        if (result == OTA_STATUS_OK) {
            print_str("\r\nEncrypted OTA download completed!\r\n");
            print_str("Use 'xr 0' to decrypt and restore to internal flash.\r\n");
            print_str("Use 'j' to jump to the firmware after restore.\r\n");
        } else {
            print_str("\r\nOTA download failed with error: ");
            print_dec(result);
            print_str("\r\n");
        }
    }
    else if(ch == '4')
    {
        /* HTTP OTA加密固件到外部Flash */
        print_str("Slot (1-3): ");
        uint8_t slot = read_char() - '0';
        print_str("\r\n");
        
        if(slot < 1 || slot > 3)
        {
            print_str("Invalid slot!\r\n");
            return;
        }
        
        /* 选择加密算法 */
        print_str("Select encryption algorithm:\r\n");
        print_str("1. XOR encryption\r\n");
        print_str("2. AES-128-CBC encryption\r\n");
        print_str("Choice (1-2): ");
        uint8_t encrypt_choice = read_char();
        print_str("\r\n");
        
        if (encrypt_choice != '1' && encrypt_choice != '2') {
            print_str("Invalid choice!\r\n");
            return;
        }
        
        if (encrypt_choice == '2') {
            print_str("Using AES-128-CBC encryption\r\n");
        } else {
            print_str("Using XOR encryption\r\n");
        }
        
        /* 检查ESP8266是否已初始化 */
        if (!g_wifi_initialized) {
            print_str("ESP8266 not initialized! Please use 'w' command first.\r\n");
            return;
        }
        
        /* 检查WiFi连接状态 */
        if (esp8266_get_wifi_status(&g_wifi_device) != ESP8266_WIFI_GOT_IP) {
            print_str("WiFi not connected! Please connect WiFi first.\r\n");
            return;
        }
        
        /* 初始化OTA */
        static ota_context_t ota_ctx;
        print_str("Initializing OTA...\r\n");
        if (!ota_init(&ota_ctx, &g_wifi_device)) {
            print_str("OTA init failed!\r\n");
            return;
        }
        
        /* 设置进度回调 */
        ota_set_progress_callback(&ota_ctx, ota_progress_callback);
        
        /* 使用配置的OTA服务器 */
        const bootloader_config_t* cfg = config_get();
        if (!cfg) {
            print_str("Configuration not loaded!\r\n");
            ota_deinit(&ota_ctx);
            return;
        }
        
        /* 构建完整的URL */
        char firmware_url[256];
        snprintf(firmware_url, sizeof(firmware_url), "http://%s:%d%s", 
                 cfg->ota.host, cfg->ota.port, cfg->ota.path);
        
        print_str("Starting OTA download to external flash slot ");
        print_dec(slot);
        print_str("...\r\n");
        print_str("URL: ");
        print_str(firmware_url);
        print_str("\r\n");
        
        w25q64_partition_id_t target_partition = W25Q64_PARTITION_BACKUP1 + slot - 1;
        
        ota_status_t result = ota_download_firmware(&ota_ctx, firmware_url,
                                                   OTA_TARGET_EXTERNAL_FLASH, 
                                                   target_partition, 1024*1024, true);
        
        ota_deinit(&ota_ctx);
        
        if (result == OTA_STATUS_OK) {
            print_str("\r\nEncrypted OTA download to external flash completed!\r\n");
            print_str("Use 'xr ");
            print_dec(slot);
            print_str("' to decrypt and restore to internal flash.\r\n");
            print_str("Use 'j' to jump to the firmware after restore.\r\n");
        } else {
            print_str("\r\nOTA download failed with error: ");
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
    
    /* 显示STM32唯一ID */
    uint32_t* unique_id = (uint32_t*)0x1FFFF7E8;
    print_str("Unique ID: ");
    for(int i = 0; i < 3; i++) {
        print_hex(unique_id[i]);
        if(i < 2) print_str(",");
    }
    print_str("\r\n");
    
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
    print_str("Slot (0-3, 0=download partition): ");
    char ch = read_char();
    print_str("\r\n");
    
    /* 验证输入字符 */
    if(ch < '0' || ch > '3')
    {
        print_str("Invalid slot! Please enter 0-3.\r\n");
        return;
    }
    
    uint8_t slot = ch - '0';
    print_str("Debug: slot = ");
    print_dec(slot);
    print_str("\r\n");
    
    w25q64_init();
    w25q64_partition_id_t pid;
    if (slot == 0) {
        pid = W25Q64_PARTITION_DOWNLOAD;
        print_str("Restoring from download partition...\r\n");
    } else {
        pid = W25Q64_PARTITION_BACKUP1 + slot - 1;
        print_str("Restoring from backup slot ");
        print_dec(slot);
        print_str("...\r\n");
    }
    
    /* 验证备份 - 检查是否为加密固件 */
    uint8_t buf[256];
    if(!w25q64_read_partition(pid, 0, buf, 256))
    {
        print_str("Read failed!\r\n");
        return;
    }
    
    /* 检查是否为加密固件 */
    bool is_encrypted_firmware = false;
    firmware_crypto_header_t* xor_header = (firmware_crypto_header_t*)buf;
    firmware_aes_header_t* aes_header = (firmware_aes_header_t*)buf;
    
    if (xor_header->magic == FIRMWARE_CRYPTO_MAGIC) {
        print_str("XOR encrypted firmware detected in slot ");
        print_dec(slot);
        print_str("\r\n");
        is_encrypted_firmware = true;
    } else if (aes_header->magic == FIRMWARE_AES_MAGIC) {
        print_str("AES encrypted firmware detected in slot ");
        print_dec(slot);
        print_str("\r\n");
        is_encrypted_firmware = true;
    } else {
        /* 普通固件验证 */
        uint32_t stack = *((uint32_t*)buf);
        if(stack < 0x20000000 || stack > 0x20010000)
        {
            print_str("Invalid backup!\r\n");
            return;
        }
        print_str("Standard firmware detected in slot ");
        print_dec(slot);
        print_str("\r\n");
    }
    
    if (is_encrypted_firmware) {
        /* 处理加密固件 - 直接调用解密函数 */
        print_str("Starting decryption from external flash...\r\n");
        
        if (decrypt_external_firmware_to_internal(pid, get_dynamic_password())) {
            print_str("\r\nEncrypted firmware restore completed successfully!\r\n");
            print_str("Use 'j' to jump to the new firmware.\r\n");
        } else {
            print_str("\r\nEncrypted firmware restore failed!\r\n");
        }
    } else {
        /* 处理普通固件 */
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
void print_str(const char* str)
{
    uart_write(DEV_UART1, (uint8_t*)str, strlen(str));
    uart_poll_dma_tx(DEV_UART1);
}

void print_hex(uint32_t val)
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

static void read_line(char* buffer, uint16_t size)
{
    uint16_t index = 0;
    uint8_t ch;
    
    while(index < size - 1) {
        ch = read_char();
        
        if (ch == '\r' || ch == '\n') {
            break;
        } else if (ch == '\b' || ch == 0x7F) {  /* 退格键 */
            if (index > 0) {
                index--;
                print_str("\b \b");  /* 退格并清除字符 */
            }
        } else {
            buffer[index++] = ch;
        }
    }
    
    buffer[index] = '\0';
    print_str("\r\n");
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

/* WiFi连接处理函数 */
void cmd_wifi_connect_handler(void)
{
    print_str("\r\n=== WiFi Connection ===\r\n");
    
    /* 初始化ESP8266 */
    if (!g_wifi_initialized) {
        print_str("Initializing ESP8266...\r\n");
        if (!esp8266_init(&g_wifi_device, DEV_UART2, 9)) {  /* UART2, PE9复位引脚 */
            print_str("ESP8266 initialization failed!\r\n");
            return;
        }
        g_wifi_initialized = true;
        print_str("ESP8266 initialized successfully.\r\n");
    }
    
    print_str("\r\nWiFi Connection Options:\r\n");
    print_str("1 = Connect to default WiFi (SSID: YANG)\r\n");
    print_str("2 = Connect to custom WiFi\r\n");
    print_str("Select (1-2): ");
    
    uint8_t choice = read_char();
    print_str("\r\n");
    
    if (choice == '1') {
        /* 连接到固定的调试WiFi */
        print_str("Connecting to debug WiFi (YANG)...\r\n");
        if (esp8266_connect_wifi(&g_wifi_device, "YANG", "yang123456789")) {
            print_str("WiFi connected successfully!\r\n");
            print_str("IP Address: ");
            print_str(g_wifi_device.ip_addr);
            print_str("\r\n");
        } else {
            print_str("WiFi connection failed!\r\n");
        }
    }
    else if (choice == '2') {
        /* 连接到自定义WiFi */
        char ssid[64], password[64];
        
        print_str("Enter SSID: ");
        /* 读取SSID */
        uint8_t ssid_len = 0;
        while (ssid_len < sizeof(ssid) - 1) {
            uint8_t ch = read_char();
            if (ch == '\r' || ch == '\n') {
                break;
            } else if (ch == '\b' || ch == 0x7F) {
                if (ssid_len > 0) {
                    ssid_len--;
                    print_str(" \b");
                }
                continue;
            }
            ssid[ssid_len++] = ch;
        }
        ssid[ssid_len] = '\0';
        print_str("\r\n");
        
        if (ssid_len == 0) {
            print_str("Invalid SSID!\r\n");
            return;
        }
        
        print_str("Enter Password: ");
        /* 读取密码 */
        uint8_t pwd_len = 0;
        while (pwd_len < sizeof(password) - 1) {
            uint8_t ch = read_char();
            if (ch == '\r' || ch == '\n') {
                break;
            } else if (ch == '\b' || ch == 0x7F) {
                if (pwd_len > 0) {
                    pwd_len--;
                    print_str(" \b");
                }
                continue;
            }
            password[pwd_len++] = ch;
            /* 密码显示为* */
            print_str("*");
        }
        password[pwd_len] = '\0';
        print_str("\r\n");
        
        print_str("Connecting to WiFi: ");
        print_str(ssid);
        print_str("...\r\n");
        
        if (esp8266_connect_wifi(&g_wifi_device, ssid, password)) {
            print_str("WiFi connected successfully!\r\n");
            print_str("IP Address: ");
            print_str(g_wifi_device.ip_addr);
            print_str("\r\n");
        } else {
            print_str("WiFi connection failed!\r\n");
        }
    }
    else {
        print_str("Invalid selection!\r\n");
    }
}

/* WiFi状态查询处理函数 */
void cmd_wifi_status_handler(void)
{
    print_str("\r\n=== WiFi Status ===\r\n");
    
    if (!g_wifi_initialized) {
        print_str("ESP8266 not initialized\r\n");
        return;
    }
    
    /* 刷新IP信息 */
    if (g_wifi_device.wifi_status == ESP8266_WIFI_GOT_IP) {
        esp8266_get_ip_info(&g_wifi_device);
    }
    
    esp8266_wifi_status_t status = esp8266_get_wifi_status(&g_wifi_device);
    
    print_str("ESP8266 Status: ");
    switch (status) {
        case ESP8266_WIFI_DISCONNECTED:
            print_str("Disconnected\r\n");
            break;
        case ESP8266_WIFI_CONNECTED:
            print_str("Connected\r\n");
            break;
        case ESP8266_WIFI_GOT_IP:
            print_str("Connected with IP\r\n");
            print_str("SSID: ");
            print_str(g_wifi_device.ssid);
            print_str("\r\nIP Address: ");
            print_str(g_wifi_device.ip_addr);
            print_str("\r\nGateway: ");
            print_str(g_wifi_device.gateway);
            print_str("\r\nNetmask: ");
            print_str(g_wifi_device.netmask);
            print_str("\r\n");
            break;
        case ESP8266_WIFI_ERROR:
            print_str("Error\r\n");
            break;
        default:
            print_str("Unknown\r\n");
            break;
    }
}

/* WiFi调试处理函数 */
void cmd_wifi_debug_handler(void)
{
    print_str("\r\n=== WiFi Debug Info ===\r\n");
    
    if (!g_wifi_initialized) {
        print_str("ESP8266 not initialized\r\n");
        return;
    }
    
    char resp[512];
    
    /* 发送AT+CIFSR命令并显示原始响应 */
    print_str("Sending AT+CIFSR...\r\n");
    if (at_send_cmd_get_resp(&g_wifi_device.at_client, "AT+CIFSR", resp, sizeof(resp), 3000) == AT_STATUS_OK) {
        print_str("Raw response:\r\n");
        print_str(resp);
        print_str("\r\n--- End of response ---\r\n");
        
        /* 尝试解析IP地址 */
        const char *ip_start = strstr(resp, "STAIP,\"");
        if (ip_start) {
            print_str("Found STAIP format: ");
            ip_start += 7;
            const char *ip_end = strchr(ip_start, '\"');
            if (ip_end) {
                int len = ip_end - ip_start;
                char temp_ip[32];
                if (len < sizeof(temp_ip)) {
                    memcpy(temp_ip, ip_start, len);
                    temp_ip[len] = '\0';
                    print_str(temp_ip);
                }
            }
            print_str("\r\n");
        } else {
            ip_start = strstr(resp, "+CIFSR:STAIP,\"");
            if (ip_start) {
                print_str("Found +CIFSR:STAIP format: ");
                ip_start += 14;
                const char *ip_end = strchr(ip_start, '\"');
                if (ip_end) {
                    int len = ip_end - ip_start;
                    char temp_ip[32];
                    if (len < sizeof(temp_ip)) {
                        memcpy(temp_ip, ip_start, len);
                        temp_ip[len] = '\0';
                        print_str(temp_ip);
                    }
                }
                print_str("\r\n");
            } else {
                print_str("No IP address format found in response\r\n");
            }
        }
    } else {
        print_str("AT+CIFSR command failed\r\n");
    }
    
    /* 检查WiFi连接状态 */
    print_str("\r\nChecking WiFi status...\r\n");
    if (at_send_cmd_get_resp(&g_wifi_device.at_client, "AT+CWJAP?", resp, sizeof(resp), 3000) == AT_STATUS_OK) {
        print_str("WiFi status response:\r\n");
        print_str(resp);
        print_str("\r\n");
    }
}

/* 配置命令处理函数 */
static void cmd_config_show_handler(void)
{
    const bootloader_config_t* cfg = config_get();
    if (!cfg) {
        print_str("Configuration not loaded!\r\n");
        return;
    }
    
    print_str("\r\n===== Current Configuration =====\r\n");
    print_str("WiFi Settings:\r\n");
    print_str("  SSID: ");
    print_str(cfg->wifi.ssid);
    print_str("\r\n  Password: ");
    /* 隐藏密码显示 */
    for (int i = 0; i < strlen(cfg->wifi.password); i++) {
        print_str("*");
    }
    print_str("\r\n  Timeout: ");
    print_dec(cfg->wifi.timeout_ms);
    print_str(" ms\r\n");
    
    print_str("\r\nOTA Settings:\r\n");
    print_str("  Server: ");
    print_str(cfg->ota.host);
    print_str(":");
    print_dec(cfg->ota.port);
    print_str("\r\n  Path: ");
    print_str(cfg->ota.path);
    print_str("\r\n  Timeout: ");
    print_dec(cfg->ota.timeout_ms);
    print_str(" ms\r\n");
    
    print_str("\r\nSystem Settings:\r\n");
    print_str("  Bootloader delay: ");
    print_dec(cfg->system.bootloader_delay_ms);
    print_str(" ms\r\n");
    print_str("  UART baudrate: ");
    print_dec(cfg->system.uart_baudrate);
    print_str("\r\n  Auto OTA: ");
    print_str(cfg->system.auto_ota_enable ? "Enabled" : "Disabled");
    print_str("\r\n  Max retries: ");
    print_dec(cfg->system.max_retry_count);
    print_str("\r\n");
}

static void cmd_config_wifi_handler(void)
{
    char ssid[64], password[64];
    
    print_str("Configure WiFi settings:\r\n");
    print_str("SSID: ");
    read_line(ssid, sizeof(ssid));
    
    print_str("Password: ");
    read_line(password, sizeof(password));
    
    if (strlen(ssid) == 0) {
        print_str("Invalid SSID!\r\n");
        return;
    }
    
    if (strlen(password) < 8) {
        print_str("Password must be at least 8 characters!\r\n");
        return;
    }
    
    if (config_set_wifi(ssid, password)) {
        print_str("WiFi configuration updated successfully!\r\n");
        print_str("Use 'cfgsave' to save changes.\r\n");
    } else {
        print_str("Failed to update WiFi configuration!\r\n");
    }
}

static void cmd_config_ota_handler(void)
{
    char host[64], path[128], port_str[8];
    uint16_t port;
    
    print_str("Configure OTA settings:\r\n");
    print_str("Server host/IP: ");
    read_line(host, sizeof(host));
    
    print_str("Port: ");
    read_line(port_str, sizeof(port_str));
    port = atoi(port_str);
    
    print_str("Firmware path: ");
    read_line(path, sizeof(path));
    
    if (strlen(host) == 0 || port == 0 || strlen(path) == 0) {
        print_str("Invalid parameters!\r\n");
        return;
    }
    
    if (config_set_ota_server(host, port, path)) {
        print_str("OTA configuration updated successfully!\r\n");
        print_str("Use 'cfgsave' to save changes.\r\n");
    } else {
        print_str("Failed to update OTA configuration!\r\n");
    }
}

static void cmd_config_save_handler(void)
{
    print_str("Saving configuration...\r\n");
    if (config_save()) {
        print_str("Configuration saved successfully!\r\n");
    } else {
        print_str("Failed to save configuration!\r\n");
    }
}

static void cmd_config_reset_handler(void)
{
    print_str("Reset configuration to defaults? (y/N): ");
    char ch = read_char();
    print_str("\r\n");
    
    if (ch == 'y' || ch == 'Y') {
        config_load_default();
        if (config_save()) {
            print_str("Configuration reset to defaults and saved!\r\n");
        } else {
            print_str("Configuration reset but save failed!\r\n");
        }
    } else {
        print_str("Operation cancelled.\r\n");
    }
}

/* 版本管理命令实现 */
static void cmd_version_handler(void)
{
    firmware_info_t current_info;
    
    print_str("=== Firmware Version Information ===\r\n\r\n");
    
    /* 显示当前运行的固件版本 */
    print_str("Current Running Firmware:\r\n");
    if (firmware_version_get_current(&current_info)) {
        firmware_version_print_info(&current_info);
    } else {
        print_str("  No version information found in current firmware\r\n");
        print_str("  (Legacy firmware without version header)\r\n");
    }
    
    print_str("\r\n");
    
    /* 显示Bootloader版本信息 */
    print_str("Bootloader Information:\r\n");
    print_str("  Version: v2.0.0 (AES+Version)\r\n");
    print_str("  Build Date: " __DATE__ " " __TIME__ "\r\n");
    print_str("  Features: XMODEM, OTA, AES-128-CBC, Version Management\r\n");
    print_str("  Flash Layout: 64KB Bootloader + 448KB App\r\n");
    
    print_str("\r\n");
    
    /* 显示系统信息 */
    print_str("System Information:\r\n");
    print_str("  STM32 Unique ID: ");
    uint32_t* unique_id = (uint32_t*)0x1FFFF7E8;
    print_hex(unique_id[0]);
    print_str("-");
    print_hex(unique_id[1]);
    print_str("-");
    print_hex(unique_id[2]);
    print_str("\r\n");
    
    print_str("  Flash Size: 512KB\r\n");
    print_str("  RAM Size: 64KB\r\n");
    print_str("  App Start Address: 0x");
    print_hex(APP_START_ADDR);
    print_str("\r\n");
    print_str("  App Max Size: ");
    print_dec(APP_MAX_SIZE / 1024);
    print_str("KB\r\n");
}

static void cmd_version_compare_handler(void)
{
    firmware_info_t current_info;
    bool current_valid = false;
    
    print_str("=== Firmware Version Comparison ===\r\n\r\n");
    
    /* 获取当前固件版本信息 */
    current_valid = firmware_version_get_current(&current_info);
    if (current_valid) {
        print_str("Current Firmware: ");
        print_str(current_info.version_string);
        print_str(" (");
        print_str(current_info.build_date);
        print_str(")\r\n");
    } else {
        print_str("Current Firmware: No version info (Legacy)\r\n");
    }
    
    /* 检查外部Flash中的备份固件版本 */
    print_str("\r\nExternal Flash Backup Versions:\r\n");
    
    for (int slot = 1; slot <= 3; slot++) {
        w25q64_partition_id_t pid = W25Q64_PARTITION_BACKUP1 + slot - 1;
        
        print_str("  Slot ");
        print_dec(slot);
        print_str(": ");
        
        /* 检查是否有加密固件 */
        firmware_aes_header_t aes_header;
        if (w25q64_read_partition(pid, 0, (uint8_t*)&aes_header, sizeof(aes_header))) {
            if (firmware_aes_validate_header(&aes_header)) {
                /* AES加密固件，显示版本信息 */
                print_str("v");
                print_dec(aes_header.fw_version.major);
                print_str(".");
                print_dec(aes_header.fw_version.minor);
                print_str(".");
                print_dec(aes_header.fw_version.patch);
                print_str(".");
                print_dec(aes_header.fw_version.build);
                print_str(" (AES Encrypted, ");
                print_dec(aes_header.firmware_size);
                print_str(" bytes)\r\n");
                
                /* 如果当前固件有版本信息，进行比较 */
                if (current_valid) {
                    int cmp = firmware_version_compare(&current_info.version, &aes_header.fw_version);
                    if (cmp > 0) {
                        print_str("    Status: Older than current\r\n");
                    } else if (cmp < 0) {
                        print_str("    Status: Newer than current\r\n");
                    } else {
                        print_str("    Status: Same as current\r\n");
                    }
                }
                continue;
            }
        }
        
        /* 检查是否有普通固件 */
        uint32_t size = calculate_firmware_size(pid);
        if (size > 0) {
            print_str("Unknown version (Legacy, ");
            print_dec(size);
            print_str(" bytes)\r\n");
        } else {
            print_str("Empty\r\n");
        }
    }
    
    print_str("\r\nUse 'u' command to update firmware.\r\n");
}

/* 动态密码管理函数实现 */
static void set_dynamic_password(const char* password)
{
    if (password && strlen(password) > 0) {
        strncpy(g_dynamic_password, password, sizeof(g_dynamic_password) - 1);
        g_dynamic_password[sizeof(g_dynamic_password) - 1] = '\0';
    }
}

static const char* get_dynamic_password(void)
{
    return g_dynamic_password;
}

static void update_password_from_ota(ota_context_t* ctx)
{
    if (ctx && strlen(ctx->encryption_password) > 0) {
        set_dynamic_password(ctx->encryption_password);
        print_str("Dynamic password updated from OTA response: ");
        print_str(ctx->encryption_password);
        print_str("\r\n");
    }
}

/* 公共动态密码管理函数 - 供其他模块调用 */
void bootloader_set_dynamic_password(const char* password)
{
    set_dynamic_password(password);
}

const char* bootloader_get_dynamic_password(void)
{
    return get_dynamic_password();
}