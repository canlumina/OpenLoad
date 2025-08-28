#include "bootloader_cmd.h"
#include "esp8266.h"
#include "dev_usart.h"
#include "w25q64.h"
#include "ota_manager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* 私有函数 */
static void print_str(const char* str)
{
    uart_write(DEV_UART1, (uint8_t*)str, strlen(str));
    uart_poll_dma_tx(DEV_UART1);
}

static void print_dec(uint32_t val)
{
    char buf[12];
    snprintf(buf, sizeof(buf), "%lu", val);
    print_str(buf);
}

static uint8_t read_char(void)
{
    uint8_t ch;
    while(uart_read(DEV_UART1, &ch, 1) == 0) {
        HAL_Delay(10);
    }
    uart_write(DEV_UART1, &ch, 1);
    uart_poll_dma_tx(DEV_UART1);
    return ch;
}

static uint16_t read_string(char *buffer, uint16_t max_len, bool echo)
{
    uint16_t len = 0;
    uint8_t ch;
    
    while(len < max_len - 1) {
        if(uart_read(DEV_UART1, &ch, 1) > 0) {
            if(ch == '\r' || ch == '\n') {
                break;
            }
            else if(ch == '\b' || ch == 0x7F) {
                if(len > 0) {
                    len--;
                    if(echo) {
                        print_str("\b \b");
                    }
                }
            }
            else if(ch >= ' ' && ch <= '~') {
                buffer[len++] = ch;
                if(echo) {
                    uart_write(DEV_UART1, &ch, 1);
                    uart_poll_dma_tx(DEV_UART1);
                }
            }
        }
        HAL_Delay(10);
    }
    buffer[len] = '\0';
    return len;
}

static void show_progress(uint32_t current, uint32_t total, const char* prefix)
{
    uint8_t percent = (total > 0) ? ((current * 100) / total) : 0;
    
    print_str("\r");
    print_str(prefix);
    print_str(": ");
    print_dec(percent);
    print_str("% [");
    
    uint8_t bar_width = 40;
    uint8_t filled = (percent * bar_width) / 100;
    
    for(uint8_t i = 0; i < bar_width; i++) {
        if(i < filled) {
            print_str("=");
        } else {
            print_str(" ");
        }
    }
    
    print_str("] ");
    print_dec(current / 1024);
    print_str("KB/");
    print_dec(total / 1024);
    print_str("KB");
}

/**
 * @brief WiFi连接/断开处理
 */
void cmd_wifi_handler(void)
{
    char ssid[64];
    char password[64];
    char option;
    
    print_str("\r\n=== WiFi Management ===\r\n");
    print_str("1. Connect to WiFi\r\n");
    print_str("2. Disconnect WiFi\r\n");
    print_str("3. Initialize ESP8266\r\n");
    print_str("Select option: ");
    
    option = read_char();
    print_str("\r\n");
    
    switch(option) {
    case '1':
        /* 连接WiFi */
        print_str("\r\nEnter WiFi SSID: ");
        if(read_string(ssid, sizeof(ssid), true) == 0) {
            print_str("\r\nSSID cannot be empty!\r\n");
            return;
        }
        print_str("\r\n");
        
        print_str("Enter Password: ");
        uint16_t pwd_len = 0;
        uint8_t ch;
        
        while(pwd_len < sizeof(password) - 1) {
            if(uart_read(DEV_UART1, &ch, 1) > 0) {
                if(ch == '\r' || ch == '\n') {
                    break;
                }
                else if(ch == '\b' || ch == 0x7F) {
                    if(pwd_len > 0) {
                        pwd_len--;
                        print_str("\b \b");
                    }
                }
                else if(ch >= ' ' && ch <= '~') {
                    password[pwd_len++] = ch;
                    print_str("*");  /* 隐藏密码 */
                }
            }
            HAL_Delay(10);
        }
        password[pwd_len] = '\0';
        print_str("\r\n");
        
        print_str("Connecting to \"");
        print_str(ssid);
        print_str("\"...\r\n");
        
        if(esp_wifi_connect(ssid, password) == ESP_OK) {
            print_str("WiFi connected successfully!\r\n");
            
            char ip[32];
            if(esp_wifi_get_ip(ip, sizeof(ip)) == ESP_OK) {
                print_str("IP Address: ");
                print_str(ip);
                print_str("\r\n");
            }
        } else {
            print_str("WiFi connection failed!\r\n");
        }
        break;
        
    case '2':
        /* 断开WiFi */
        print_str("Disconnecting WiFi...\r\n");
        if(esp_wifi_disconnect() == ESP_OK) {
            print_str("WiFi disconnected.\r\n");
        } else {
            print_str("Failed to disconnect WiFi.\r\n");
        }
        break;
        
    case '3':
        /* 初始化ESP8266 */
        print_str("Initializing ESP8266...\r\n");
        if(esp_init() == ESP_OK) {
            print_str("ESP8266 initialized successfully!\r\n");
        } else {
            print_str("ESP8266 initialization failed!\r\n");
        }
        break;
        
    default:
        print_str("Invalid option!\r\n");
        break;
    }
}

/**
 * @brief WiFi/ESP8266信息显示
 */
void cmd_wifi_info_handler(void)
{
    char buffer[256];
    
    print_str("\r\n=== WiFi/ESP8266 Information ===\r\n");
    
    /* 测试通信 */
    print_str("Communication: ");
    if(esp_test() == ESP_OK) {
        print_str("OK\r\n");
    } else {
        print_str("FAILED\r\n");
        return;
    }
    
    /* 获取版本信息 */
    if(esp_get_version(buffer, sizeof(buffer)) == ESP_OK) {
        print_str("Version:\r\n");
        print_str(buffer);
        print_str("\r\n");
    }
    
    /* 检查WiFi连接状态 */
    print_str("WiFi Status: ");
    switch(esp_wifi_get_status()) {
    case WIFI_DISCONNECTED:
        print_str("Disconnected\r\n");
        break;
    case WIFI_CONNECTING:
        print_str("Connecting...\r\n");
        break;
    case WIFI_CONNECTED:
        print_str("Connected (no IP)\r\n");
        break;
    case WIFI_GOT_IP:
        print_str("Connected with IP\r\n");
        if(esp_wifi_get_ip(buffer, sizeof(buffer)) == ESP_OK) {
            print_str("IP Address: ");
            print_str(buffer);
            print_str("\r\n");
        }
        break;
    }
    
    /* 检查TCP连接状态 */
    print_str("TCP Status: ");
    switch(esp_tcp_get_status()) {
    case TCP_DISCONNECTED:
        print_str("Disconnected\r\n");
        break;
    case TCP_CONNECTING:
        print_str("Connecting...\r\n");
        break;
    case TCP_CONNECTED:
        print_str("Connected\r\n");
        break;
    case TCP_CLOSING:
        print_str("Closing...\r\n");
        break;
    }
}

/**
 * @brief OTA进度回调函数
 */
static void ota_progress_callback(uint32_t current, uint32_t total, uint8_t percent)
{
    show_progress(current, total, "OTA Progress");
}

/**
 * @brief OTA固件更新处理（增强版）
 */
void cmd_ota_advanced_handler(void)
{
    char option;
    ota_firmware_info_t info;
    ota_config_t config = {0};
    
    print_str("\r\n=== Advanced OTA Update ===\r\n");
    
    /* 检查WiFi连接 */
    if(!esp_wifi_is_connected()) {
        print_str("WiFi not connected!\r\n");
        print_str("Connecting to default WiFi...\r\n");
        /* 可以在这里添加自动连接默认WiFi的代码 */
        return;
    }
    
    /* 初始化OTA管理器 */
    ota_manager_init();
    
    /* 显示菜单 */
    print_str("1. Check latest firmware\r\n");
    print_str("2. List all firmware versions\r\n");
    print_str("3. Download latest to internal flash\r\n");
    print_str("4. Download latest to external flash\r\n");
    print_str("5. Download specific version\r\n");
    print_str("Select option: ");
    
    option = read_char();
    print_str("\r\n");
    
    switch(option) {
    case '1':
        /* 检查最新固件 */
        print_str("Checking latest firmware...\r\n");
        if(ota_manager_check_update(&info)) {
            print_str("Latest version: ");
            print_str(info.version);
            print_str("\r\nSize: ");
            print_dec(info.size);
            print_str(" bytes\r\n");
            if(strlen(info.description) > 0) {
                print_str("Description: ");
                print_str(info.description);
                print_str("\r\n");
            }
        } else {
            print_str("Failed to check update: ");
            print_str(ota_manager_get_error());
            print_str("\r\n");
        }
        break;
        
    case '2':
        /* 列出所有固件版本 */
        print_str("Fetching firmware list...\r\n");
        if(!ota_manager_get_firmware_list()) {
            print_str("Failed to get firmware list.\r\n");
        }
        break;
        
    case '3':
        /* 下载最新固件到内部Flash */
        print_str("Downloading latest firmware to internal flash...\r\n");
        print_str("WARNING: This will overwrite current app!\r\n");
        print_str("Continue? (y/n): ");
        if(read_char() != 'y') {
            print_str("\r\nCancelled.\r\n");
            break;
        }
        print_str("\r\n\r\n");
        
        config.target = OTA_TARGET_INTERNAL_FLASH;
        config.auto_retry = true;
        config.retry_count = 3;
        config.chunk_size = 1024;
        
        if(ota_manager_download_firmware("latest", &config, ota_progress_callback)) {
            print_str("\r\n\r\nOTA completed successfully!\r\n");
            print_str("Reset to run new firmware.\r\n");
        } else {
            print_str("\r\n\r\nOTA failed: ");
            print_str(ota_manager_get_error());
            print_str("\r\n");
        }
        break;
        
    case '4':
        /* 下载最新固件到外部Flash */
        print_str("External Flash Slot (1-3): ");
        uint8_t slot = read_char() - '0';
        print_str("\r\n");
        
        if(slot < 1 || slot > 3) {
            print_str("Invalid slot!\r\n");
            break;
        }
        
        print_str("Downloading latest firmware to external flash...\r\n");
        
        config.target = OTA_TARGET_EXTERNAL_FLASH;
        config.external_slot = slot;
        config.auto_retry = true;
        config.retry_count = 3;
        config.chunk_size = 1024;
        
        if(ota_manager_download_firmware("latest", &config, ota_progress_callback)) {
            print_str("\r\n\r\nOTA completed successfully!\r\n");
            print_str("Use 'xr ");
            print_dec(slot);
            print_str("' to restore this firmware.\r\n");
        } else {
            print_str("\r\n\r\nOTA failed: ");
            print_str(ota_manager_get_error());
            print_str("\r\n");
        }
        break;
        
    case '5':
        /* 下载指定版本 */
        {
            char version[32];
            print_str("Enter version (e.g., v1.0.0): ");
            if(read_string(version, sizeof(version), true) == 0) {
                print_str("\r\nVersion cannot be empty!\r\n");
                break;
            }
            print_str("\r\n");
            
            print_str("Target:\r\n");
            print_str("1. Internal Flash\r\n");
            print_str("2. External Flash\r\n");
            print_str("Select: ");
            char target = read_char();
            print_str("\r\n");
            
            if(target == '1') {
                config.target = OTA_TARGET_INTERNAL_FLASH;
            } else if(target == '2') {
                print_str("Slot (1-3): ");
                config.external_slot = read_char() - '0';
                print_str("\r\n");
                if(config.external_slot < 1 || config.external_slot > 3) {
                    print_str("Invalid slot!\r\n");
                    break;
                }
                config.target = OTA_TARGET_EXTERNAL_FLASH;
            } else {
                print_str("Invalid option!\r\n");
                break;
            }
            
            config.auto_retry = true;
            config.retry_count = 3;
            config.chunk_size = 1024;
            
            print_str("Downloading firmware version ");
            print_str(version);
            print_str("...\r\n");
            
            if(ota_manager_download_firmware(version, &config, ota_progress_callback)) {
                print_str("\r\n\r\nOTA completed successfully!\r\n");
            } else {
                print_str("\r\n\r\nOTA failed: ");
                print_str(ota_manager_get_error());
                print_str("\r\n");
            }
        }
        break;
        
    default:
        print_str("Invalid option!\r\n");
        break;
    }
}

/**
 * @brief OTA固件更新处理（简化版）
 */
void cmd_ota_handler(void)
{
    char url[256];
    char option;
    uint8_t slot = 1;  /* 默认槽位为1 */
    ota_info_t ota_info;
    uint8_t buffer[1024];
    uint16_t received;
    uint32_t total_written = 0;
    esp_result_t result;
    
    print_str("\r\n=== OTA Firmware Update ===\r\n");
    
    /* 检查WiFi连接 */
    if(!esp_wifi_is_connected()) {
        print_str("ERROR: WiFi not connected!\r\n");
        print_str("Use 'w' command to connect first.\r\n");
        return;
    }
    
    /* 选择更新目标 */
    print_str("Update target:\r\n");
    print_str("1. Internal Flash\r\n");
    print_str("2. External Flash\r\n");
    print_str("Select: ");
    
    option = read_char();
    print_str("\r\n");
    
    if(option != '1' && option != '2') {
        print_str("Invalid option!\r\n");
        return;
    }
    
    /* 外部Flash需要选择槽位 */
    if(option == '2') {
        print_str("External Flash Slot (1-3): ");
        slot = read_char() - '0';
        print_str("\r\n");
        
        if(slot < 1 || slot > 3) {
            print_str("Invalid slot!\r\n");
            return;
        }
    }
    
    /* 输入固件URL */
    print_str("Enter firmware URL (or 'default' for test server): ");
    if(read_string(url, sizeof(url), true) == 0) {
        print_str("\r\nURL cannot be empty!\r\n");
        return;
    }
    print_str("\r\n");
    
    /* 使用默认URL或构建完整URL */
    if(strcmp(url, "default") == 0 || strcmp(url, "latest") == 0) {
        strcpy(url, "http://115.190.137.231:3685/api/firmware/download/latest");
    } else if(strncmp(url, "v", 1) == 0 || (url[0] >= '0' && url[0] <= '9')) {
        /* 如果只输入版本号，构建完整URL */
        char temp[256];
        snprintf(temp, sizeof(temp), "http://115.190.137.231:3685/api/firmware/download/%s", url);
        strcpy(url, temp);
    }
    
    /* 开始OTA下载 */
    print_str("Starting OTA download from:\r\n");
    print_str(url);
    print_str("\r\n\r\n");
    
    result = esp_ota_start(url, &ota_info);
    if(result != ESP_OK) {
        print_str("Failed to start OTA download!\r\n");
        if(result == ESP_NO_WIFI) {
            print_str("WiFi disconnected during operation.\r\n");
        } else if(result == ESP_TIMEOUT) {
            print_str("Connection timeout.\r\n");
        } else {
            print_str("HTTP error or server unreachable.\r\n");
        }
        return;
    }
    
    print_str("Firmware size: ");
    if(ota_info.total_size > 0) {
        print_dec(ota_info.total_size);
        print_str(" bytes\r\n");
    } else {
        print_str("Unknown\r\n");
    }
    
    /* 流式写入用的变量 */
    uint8_t page_buffer[256];
    uint32_t page_offset = 0;
    uint32_t last_erased_sector = 0xFFFFFFFF;
    w25q64_partition_id_t pid = 0;
    
    /* 内部Flash更新 */
    if(option == '1') {
        /* 确认操作 */
        print_str("\r\nWARNING: This will erase internal flash!\r\n");
        print_str("Continue? (y/n): ");
        if(read_char() != 'y') {
            print_str("\r\nCancelled.\r\n");
            esp_ota_finish();
            return;
        }
        print_str("\r\n\r\n");
        
        /* 擦除内部Flash */
        print_str("Erasing internal flash...\r\n");
        if(!bootloader_flash_erase(APP_START_ADDR, APP_MAX_SIZE)) {
            print_str("Flash erase failed!\r\n");
            esp_ota_finish();
            return;
        }
        
        /* 下载并写入固件 */
        print_str("Downloading firmware...\r\n");
        while((received = esp_ota_read(buffer, sizeof(buffer))) > 0) {
            /* 写入Flash */
            if(!bootloader_flash_write(APP_START_ADDR + total_written, buffer, received)) {
                print_str("\r\nFlash write failed!\r\n");
                esp_ota_finish();
                return;
            }
            
            total_written += received;
            
            /* 显示进度 */
            if(ota_info.total_size > 0) {
                show_progress(total_written, ota_info.total_size, "Progress");
            } else if((total_written % 32768) == 0) {
                print_str("\r\nDownloaded: ");
                print_dec(total_written / 1024);
                print_str(" KB");
            }
        }
    }
    /* 外部Flash更新 */
    else {
        pid = W25Q64_PARTITION_BACKUP1 + slot - 1;
        
        /* 初始化外部Flash */
        w25q64_init();
        
        /* 擦除外部Flash分区 */
        print_str("Erasing external flash partition...\r\n");
        if(!w25q64_erase_partition(pid)) {
            print_str("External flash erase failed!\r\n");
            esp_ota_finish();
            return;
        }
        
        /* 下载并写入固件 - 使用流式写入 */
        print_str("Downloading firmware...\r\n");
        
        /* 重置页缓冲区状态 */
        page_offset = 0;
        last_erased_sector = 0xFFFFFFFF;
        
        /* 使用小缓冲区接收数据 */
        uint8_t recv_buffer[256];
        while((received = esp_ota_read(recv_buffer, sizeof(recv_buffer))) > 0) {
            uint32_t data_offset = 0;
            
            while(data_offset < received) {
                /* 计算可以复制到页缓冲区的字节数 */
                uint32_t copy_len = received - data_offset;
                if(copy_len > (256 - page_offset)) {
                    copy_len = 256 - page_offset;
                }
                
                /* 复制到页缓冲区 */
                memcpy(&page_buffer[page_offset], &recv_buffer[data_offset], copy_len);
                page_offset += copy_len;
                data_offset += copy_len;
                
                /* 如果页缓冲区满了，写入Flash */
                if(page_offset >= 256) {
                    /* 检查是否需要擦除新扇区 */
                    uint32_t current_sector = total_written / W25Q64_SECTOR_SIZE;
                    if(current_sector != last_erased_sector) {
                        uint32_t sector_addr = current_sector * W25Q64_SECTOR_SIZE;
                        w25q64_erase_sector(sector_addr);
                        last_erased_sector = current_sector;
                    }
                    
                    /* 写入外部Flash */
                    if(!w25q64_write_partition(pid, total_written, page_buffer, 256)) {
                        print_str("\r\nExternal flash write failed!\r\n");
                        esp_ota_finish();
                        return;
                    }
                    
                    total_written += 256;
                    page_offset = 0;
                }
            }
            
            /* 显示进度 */
            if(ota_info.total_size > 0) {
                show_progress(total_written, ota_info.total_size, "Progress");
            } else if((total_written % 32768) == 0) {
                print_str("\r\nDownloaded: ");
                print_dec(total_written / 1024);
                print_str(" KB");
            }
        }
    }
    
    /* 完成OTA */
    esp_ota_finish();
    
    /* 外部Flash - 写入最后的数据 */
    if(option == '2' && page_offset > 0) {
        /* 填充剩余部分为0xFF */
        memset(&page_buffer[page_offset], 0xFF, 256 - page_offset);
        
        /* 写入最后一页 */
        if(!w25q64_write_partition(pid, total_written, page_buffer, 256)) {
            print_str("Final flash write failed!\r\n");
        } else {
            total_written += page_offset;
        }
    }
    
    print_str("\r\n\r\nOTA update completed!\r\n");
    print_str("Total downloaded: ");
    print_dec(total_written);
    print_str(" bytes\r\n");
    
    /* 验证固件 */
    if(option == '1') {
        if(bootloader_validate_app()) {
            print_str("Firmware validation: PASSED\r\n");
            print_str("Reset system to run new firmware.\r\n");
        } else {
            print_str("Firmware validation: FAILED\r\n");
        }
    } else {
        uint8_t verify_buf[256];
        w25q64_partition_id_t pid = W25Q64_PARTITION_BACKUP1 + slot - 1;
        
        if(w25q64_read_partition(pid, 0, verify_buf, 256)) {
            uint32_t stack = *((uint32_t*)verify_buf);
            if(stack >= 0x20000000 && stack <= 0x20010000) {
                print_str("Firmware validation: PASSED\r\n");
                print_str("Use 'xr ");
                print_dec(slot);
                print_str("' to restore this firmware.\r\n");
            } else {
                print_str("Firmware validation: FAILED\r\n");
            }
        }
    }
}