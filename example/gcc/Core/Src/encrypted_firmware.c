#include "encrypted_firmware.h"
#include "firmware_crypto.h"
#include "firmware_aes.h"
#include "streaming_aes.h"
#include "bootloader_cmd.h"
#include "http_ota.h"
#include "esp8266_wifi.h"
#include "dev_usart.h"
#include <string.h>
#include <stdio.h>

/* 外部声明的函数 */
extern bool bootloader_flash_write(uint32_t addr, const uint8_t* data, uint32_t size);
extern bool bootloader_flash_erase(uint32_t addr, uint32_t size);
extern uint16_t uart_write(uint8_t uart_id, const uint8_t* data, uint16_t len);

/* print_str函数现在在bootloader_cmd.c中实现为全局函数 */

static void print_dec(uint32_t val) {
    char buffer[12];
    sprintf(buffer, "%lu", val);
    print_str(buffer);
}


static void show_progress(uint32_t current, uint32_t total, const char* prefix) {
    uint32_t percent = (current * 100) / total;
    char buffer[64];
    sprintf(buffer, "\r%s: %lu/%lu (%lu%%)", prefix ? prefix : "Progress", current, total, percent);
    print_str(buffer);
}

/* 内部函数声明 */
static bool decrypt_aes_firmware_to_internal(w25q64_partition_id_t source_partition, const char* password);
static bool decrypt_xor_firmware_to_internal(w25q64_partition_id_t source_partition, const char* password);

/**
 * @brief 解密外部Flash中的加密固件到内部Flash
 */
bool decrypt_external_firmware_to_internal(w25q64_partition_id_t source_partition, const char* password)
{
    print_str("Starting decryption from external flash...\r\n");
    
    /* 检查是否为XOR加密固件 */
    firmware_crypto_header_t xor_header;
    bool is_xor = false;
    if (w25q64_read_partition(source_partition, 0, (uint8_t*)&xor_header, sizeof(xor_header))) {
        is_xor = firmware_crypto_validate_header(&xor_header);
    }
    
    /* 检查是否为AES加密固件 */
    firmware_aes_header_t aes_header;
    bool is_aes = false;
    if (!is_xor && w25q64_read_partition(source_partition, 0, (uint8_t*)&aes_header, sizeof(aes_header))) {
        is_aes = firmware_aes_validate_header(&aes_header);
    }
    
    if (!is_xor && !is_aes) {
        print_str("No valid encrypted firmware found!\r\n");
        return false;
    }
    
    if (is_aes) {
        print_str("AES encrypted firmware detected\r\n");
        return decrypt_aes_firmware_to_internal(source_partition, password);
    } else {
        print_str("XOR encrypted firmware detected\r\n");
        return decrypt_xor_firmware_to_internal(source_partition, password);
    }
}

/**
 * @brief 解密AES固件到内部Flash
 */
static bool decrypt_aes_firmware_to_internal(w25q64_partition_id_t source_partition, const char* password)
{
    /* 读取AES头部 */
    firmware_aes_header_t aes_header;
    if (!w25q64_read_partition(source_partition, 0, (uint8_t*)&aes_header, sizeof(aes_header))) {
        print_str("Failed to read AES header!\r\n");
        return false;
    }
    
    print_str("AES FW Size: ");
    print_dec(aes_header.firmware_size);
    print_str(", Enc Size: ");
    print_dec(aes_header.encrypted_size);
    print_str("\r\n");
    
    /* 初始化AES解密器 */
    streaming_aes_ctx_t aes_ctx;
    uint32_t* unique_id = (uint32_t*)0x1FFFF7E8;
    uint8_t aes_key[16];
    
    /* 调试信息：显示Unique ID */
    print_str("Debug: STM32 Unique ID: ");
    print_hex(unique_id[0]);
    print_str("-");
    print_hex(unique_id[1]);
    print_str("-");
    print_hex(unique_id[2]);
    print_str("\r\n");
    
    firmware_aes_derive_key(password, unique_id, aes_key);
    
    /* 调试信息：显示生成的AES密钥 */
    print_str("Debug: Generated AES Key: ");
    for (int i = 0; i < 16; i++) {
        uint8_t byte_val = aes_key[i];
        // 显示高4位
        uint8_t high = (byte_val >> 4) & 0xF;
        char h_char = high < 10 ? ('0' + high) : ('A' + high - 10);
        char h_str[2] = {h_char, '\0'};
        print_str(h_str);
        
        // 显示低4位
        uint8_t low = byte_val & 0xF;
        char l_char = low < 10 ? ('0' + low) : ('A' + low - 10);
        char l_str[2] = {l_char, '\0'};
        print_str(l_str);
    }
    print_str("\r\n");
    
    /* 调试信息：显示IV */
    print_str("Debug: IV: ");
    for (int i = 0; i < 16; i++) {
        uint8_t byte_val = aes_header.iv[i];
        // 显示高4位
        uint8_t high = (byte_val >> 4) & 0xF;
        char h_char = high < 10 ? ('0' + high) : ('A' + high - 10);
        char h_str[2] = {h_char, '\0'};
        print_str(h_str);
        
        // 显示低4位
        uint8_t low = byte_val & 0xF;
        char l_char = low < 10 ? ('0' + low) : ('A' + low - 10);
        char l_str[2] = {l_char, '\0'};
        print_str(l_str);
    }
    print_str("\r\n");
    
    if (!streaming_aes_init(&aes_ctx, aes_key, aes_header.iv)) {
        print_str("Failed to initialize AES!\r\n");
        return false;
    }
    
    /* 擦除内部Flash */
    print_str("Erasing internal flash...\r\n");
    if (!bootloader_flash_erase(APP_START_ADDR, APP_MAX_SIZE)) {
        print_str("Flash erase failed!\r\n");
        return false;
    }
    
    /* 流式解密到内部Flash */
    uint8_t* work_buffer = (uint8_t*)(0x20000000 + 0x8000);
    uint8_t* decrypt_buffer = work_buffer + 2048;
    uint32_t buffer_size = 2048;
    uint32_t decrypted_total = 0;
    
    HAL_FLASH_Unlock();
    
    for (uint32_t offset = 0; offset < aes_header.encrypted_size; offset += buffer_size) {
        uint32_t chunk_size = (aes_header.encrypted_size - offset > buffer_size) ? 
                             buffer_size : (aes_header.encrypted_size - offset);
        
        /* 确保16字节对齐 */
        chunk_size = (chunk_size / 16) * 16;
        if (chunk_size == 0) chunk_size = 16;
        
        /* 从外部Flash读取 */
        if (!w25q64_read_partition(source_partition, sizeof(firmware_aes_header_t) + offset, work_buffer, chunk_size)) {
            print_str("\r\nRead failed!\r\n");
            HAL_FLASH_Lock();
            return false;
        }
        
        /* AES解密 */
        uint32_t decrypted_size = streaming_aes_decrypt(&aes_ctx, work_buffer, decrypt_buffer, chunk_size);
        if (decrypted_size == 0) {
            print_str("\r\nDecryption failed!\r\n");
            HAL_FLASH_Lock();
            return false;
        }
        
        /* 计算写入大小 */
        uint32_t write_size = decrypted_size;
        if (decrypted_total + decrypted_size > aes_header.firmware_size) {
            write_size = aes_header.firmware_size - decrypted_total;
        }
        
        /* 处理最后一块的填充 */
        if (offset + chunk_size >= aes_header.encrypted_size) {
            uint32_t unpadded_size = firmware_aes_pkcs7_unpad(decrypt_buffer, decrypted_size);
            if (unpadded_size > 0 && decrypted_total + unpadded_size <= aes_header.firmware_size) {
                write_size = unpadded_size;
            }
        }
        
        /* 写入Flash */
        if (write_size > 0) {
            if (!bootloader_flash_write(APP_START_ADDR + decrypted_total, decrypt_buffer, write_size)) {
                print_str("\r\nWrite failed!\r\n");
                HAL_FLASH_Lock();
                return false;
            }
            decrypted_total += write_size;
        }
        
        /* 显示进度 */
        if ((offset % 8192) == 0 || offset + chunk_size >= aes_header.encrypted_size) {
            show_progress(offset + chunk_size, aes_header.encrypted_size, "AES-CBC");
        }
        
        if (decrypted_total >= aes_header.firmware_size) break;
    }
    
    HAL_FLASH_Lock();
    
    print_str("\r\nAES decryption completed: ");
    print_dec(decrypted_total);
    print_str(" bytes\r\n");
    
    /* 调试信息：显示解密后前16字节数据 */
    print_str("Debug: First 16 bytes of decrypted data: ");
    uint8_t* flash_data = (uint8_t*)APP_START_ADDR;
    for (int i = 0; i < 16; i++) {
        uint8_t byte_val = flash_data[i];
        uint8_t high = (byte_val >> 4) & 0xF;
        char h_char = high < 10 ? ('0' + high) : ('A' + high - 10);
        char h_str[2] = {h_char, '\0'};
        print_str(h_str);
        
        uint8_t low = byte_val & 0xF;
        char l_char = low < 10 ? ('0' + low) : ('A' + low - 10);
        char l_str[2] = {l_char, '\0'};
        print_str(l_str);
        
        if (i < 15) print_str(" ");
    }
    print_str("\r\n");
    
    /* 调试信息：显示AES头中的CRC32信息 */
    print_str("Debug: AES header CRC32: 0x");
    print_hex(aes_header.crc32);
    print_str("\r\n");
    print_str("Debug: Firmware size from header: ");
    print_dec(aes_header.firmware_size);
    print_str(" bytes\r\n");
    print_str("Debug: Decrypted total: ");
    print_dec(decrypted_total);
    print_str(" bytes\r\n");
    
    /* 验证 */
    if (firmware_crypto_verify_firmware(APP_START_ADDR, decrypted_total, aes_header.crc32)) {
        print_str("AES firmware verification successful!\r\n");
        return true;
    } else {
        print_str("AES firmware verification failed!\r\n");
        return false;
    }
}

/**
 * @brief 解密XOR固件到内部Flash
 */
static bool decrypt_xor_firmware_to_internal(w25q64_partition_id_t source_partition, const char* password)
{
    /* 初始化XOR解密 */
    if (!firmware_crypto_init((uint8_t*)password, strlen(password))) {
        print_str("XOR init failed!\r\n");
        return false;
    }
    
    /* 读取XOR头部 */
    firmware_crypto_header_t xor_header;
    if (!w25q64_read_partition(source_partition, 0, (uint8_t*)&xor_header, sizeof(xor_header))) {
        print_str("Failed to read XOR header!\r\n");
        return false;
    }
    
    print_str("XOR FW Size: ");
    print_dec(xor_header.firmware_size);
    print_str(" bytes\r\n");
    
    /* 擦除内部Flash */
    print_str("Erasing internal flash...\r\n");
    if (!bootloader_flash_erase(APP_START_ADDR, APP_MAX_SIZE)) {
        print_str("Flash erase failed!\r\n");
        return false;
    }
    
    /* 流式解密到内部Flash */
    uint8_t* buffer = (uint8_t*)(0x20000000 + 0x8000);
    uint32_t buffer_size = 2048;
    uint32_t decrypted_total = 0;
    
    HAL_FLASH_Unlock();
    
    for (uint32_t offset = 0; offset < xor_header.firmware_size; offset += buffer_size) {
        uint32_t chunk_size = (xor_header.firmware_size - offset > buffer_size) ? 
                             buffer_size : (xor_header.firmware_size - offset);
        
        /* 从外部Flash读取加密数据 */
        if (!w25q64_read_partition(source_partition, sizeof(firmware_crypto_header_t) + offset, buffer, chunk_size)) {
            print_str("\r\nRead failed!\r\n");
            HAL_FLASH_Lock();
            return false;
        }
        
        /* XOR解密 */
        firmware_crypto_xor(buffer, chunk_size, offset);
        
        /* 写入内部Flash */
        if (!bootloader_flash_write(APP_START_ADDR + offset, buffer, chunk_size)) {
            print_str("\r\nWrite failed!\r\n");
            HAL_FLASH_Lock();
            return false;
        }
        
        decrypted_total += chunk_size;
        
        /* 显示进度 */
        if ((offset % 4096) == 0 || offset + chunk_size >= xor_header.firmware_size) {
            show_progress(offset + chunk_size, xor_header.firmware_size, "XOR");
        }
    }
    
    HAL_FLASH_Lock();
    
    print_str("\r\nXOR decryption completed: ");
    print_dec(decrypted_total);
    print_str(" bytes\r\n");
    
    /* 验证 */
    if (firmware_crypto_verify_firmware(APP_START_ADDR, decrypted_total, xor_header.crc32)) {
        print_str("XOR firmware verification successful!\r\n");
        return true;
    } else {
        print_str("XOR firmware verification failed!\r\n");
        return false;
    }
}

