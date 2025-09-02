#include "firmware_aes.h"
#include "firmware_crypto.h"  /* 复用CRC32函数 */
#include "main.h"
#include "mbedtls/aes.h"
#include <string.h>

/* mbedTLS AES上下文和状态 */
static mbedtls_aes_context g_aes_enc_ctx;
static mbedtls_aes_context g_aes_dec_ctx;
static uint8_t g_saved_key[AES_KEY_SIZE];
static bool g_aes_initialized = false;

bool firmware_aes_init(const uint8_t* key)
{
    if (!key) {
        return false;
    }
    
    /* 保存密钥用于后续解密 */
    memcpy(g_saved_key, key, AES_KEY_SIZE);
    
    /* 初始化mbedTLS AES上下文 */
    mbedtls_aes_init(&g_aes_enc_ctx);
    mbedtls_aes_init(&g_aes_dec_ctx);
    
    /* 设置加密和解密密钥 */
    int enc_ret = mbedtls_aes_setkey_enc(&g_aes_enc_ctx, key, 128);
    int dec_ret = mbedtls_aes_setkey_dec(&g_aes_dec_ctx, key, 128);
    
    if (enc_ret != 0 || dec_ret != 0) {
        mbedtls_aes_free(&g_aes_enc_ctx);
        mbedtls_aes_free(&g_aes_dec_ctx);
        g_aes_initialized = false;
        return false;
    }
    
    g_aes_initialized = true;
    return true;
}




/**
 * @brief AES-CBC加密 - 使用mbedTLS实现
 */
bool firmware_aes_encrypt_cbc(const uint8_t* input, uint8_t* output, uint32_t size, const uint8_t* iv)
{
    if (!g_aes_initialized || !input || !output || !iv || (size % AES_BLOCK_SIZE) != 0) {
        return false;
    }
    
    /* 复制IV到临时缓冲区（mbedTLS会修改IV） */
    uint8_t temp_iv[AES_BLOCK_SIZE];
    memcpy(temp_iv, iv, AES_BLOCK_SIZE);
    
    /* 使用mbedTLS进行AES-CBC加密 */
    int ret = mbedtls_aes_crypt_cbc(&g_aes_enc_ctx, MBEDTLS_AES_ENCRYPT, size, temp_iv, input, output);
    
    return (ret == 0);
}

/**
 * @brief AES-CBC解密 - 使用mbedTLS实现
 */
bool firmware_aes_decrypt_cbc(const uint8_t* input, uint8_t* output, uint32_t size, const uint8_t* iv)
{
    if (!g_aes_initialized || !input || !output || !iv || (size % AES_BLOCK_SIZE) != 0) {
        return false;
    }
    
    /* 调试：输出前16字节的加密数据和IV */
    print_str("Debug: First encrypted block: ");
    for (int i = 0; i < 16; i++) {
        uint8_t byte_val = input[i];
        uint8_t high = (byte_val >> 4) & 0xF;
        char h_char = high < 10 ? ('0' + high) : ('A' + high - 10);
        char h_str[2] = {h_char, '\0'};
        print_str(h_str);
        
        uint8_t low = byte_val & 0xF;
        char l_char = low < 10 ? ('0' + low) : ('A' + low - 10);
        char l_str[2] = {l_char, '\0'};
        print_str(l_str);
    }
    print_str("\r\n");
    
    /* 复制IV到临时缓冲区 */
    uint8_t temp_iv[AES_BLOCK_SIZE];
    memcpy(temp_iv, iv, AES_BLOCK_SIZE);
    
    /* 使用mbedTLS进行AES-CBC解密 */
    int ret = mbedtls_aes_crypt_cbc(&g_aes_dec_ctx, MBEDTLS_AES_DECRYPT, size, temp_iv, input, output);
    
    /* 调试：输出解密结果的前16字节 */
    if (ret == 0) {
        print_str("Debug: First decrypted block: ");
        for (int i = 0; i < 16; i++) {
            uint8_t byte_val = output[i];
            uint8_t high = (byte_val >> 4) & 0xF;
            char h_char = high < 10 ? ('0' + high) : ('A' + high - 10);
            char h_str[2] = {h_char, '\0'};
            print_str(h_str);
            
            uint8_t low = byte_val & 0xF;
            char l_char = low < 10 ? ('0' + low) : ('A' + low - 10);
            char l_str[2] = {l_char, '\0'};
            print_str(l_str);
        }
        print_str("\r\n");
    }
    
    return (ret == 0);
}

/**
 * @brief 生成AES密钥 - 使用mbedTLS AES进行密钥强化
 */
void firmware_aes_derive_key(const char* password, const uint32_t* salt, uint8_t* key)
{
    /* 简单的密钥派生：使用mbedTLS AES进行强化 */
    uint8_t temp_key[32];
    memset(temp_key, 0, sizeof(temp_key));
    
    /* 复制密码 */
    uint32_t pwd_len = strlen(password);
    if (pwd_len > 24) pwd_len = 24;
    memcpy(temp_key, password, pwd_len);
    
    /* 混合盐值（STM32 unique ID） */
    for (int i = 0; i < 2; i++) {
        temp_key[24 + i*4] = (salt[i] >> 0) & 0xFF;
        temp_key[24 + i*4 + 1] = (salt[i] >> 8) & 0xFF;
        temp_key[24 + i*4 + 2] = (salt[i] >> 16) & 0xFF;
        temp_key[24 + i*4 + 3] = (salt[i] >> 24) & 0xFF;
    }
    
    /* 使用mbedTLS AES进行密钥强化 */
    mbedtls_aes_context temp_ctx;
    mbedtls_aes_init(&temp_ctx);
    
    if (mbedtls_aes_setkey_enc(&temp_ctx, temp_key, 128) == 0) {
        uint8_t block[AES_BLOCK_SIZE];
        memcpy(block, temp_key, AES_BLOCK_SIZE);
        
        /* 使用简化的密钥强化算法（与Python工具保持一致） */
        for (int round = 0; round < 10; round++) {
            for (int i = 0; i < AES_KEY_SIZE; i++) {
                key[i] = temp_key[i] ^ temp_key[i + 16] ^ (uint8_t)(round & 0xFF);
                /* 增加一些随机性，避免全部相同的值 */
                key[i] ^= (uint8_t)((i + round) & 0xFF);
            }
            /* 更新temp_key用于下一轮 */
            memcpy(temp_key, key, AES_KEY_SIZE);
            memcpy(&temp_key[16], key, AES_KEY_SIZE);
        }
    } else {
        /* 如果AES初始化失败，使用简单的XOR混合 */
        for (int i = 0; i < AES_KEY_SIZE; i++) {
            key[i] = temp_key[i] ^ temp_key[i + 16];
        }
    }
    
    mbedtls_aes_free(&temp_ctx);
}


/**
 * @brief PKCS7填充
 */
uint32_t firmware_aes_pkcs7_pad(uint8_t* data, uint32_t data_len, uint32_t block_size)
{
    uint32_t padding = block_size - (data_len % block_size);
    if (padding == 0) padding = block_size;
    
    for (uint32_t i = 0; i < padding; i++) {
        data[data_len + i] = (uint8_t)padding;
    }
    
    return data_len + padding;
}

/**
 * @brief 移除PKCS7填充
 */
uint32_t firmware_aes_pkcs7_unpad(const uint8_t* data, uint32_t data_len)
{
    if (data_len == 0) return 0;
    
    uint8_t padding = data[data_len - 1];
    if (padding > data_len || padding == 0) return 0;
    
    /* 验证填充 */
    for (uint32_t i = data_len - padding; i < data_len; i++) {
        if (data[i] != padding) return 0;
    }
    
    return data_len - padding;
}

/**
 * @brief 验证AES加密固件头部
 */
bool firmware_aes_validate_header(const firmware_aes_header_t* header)
{
    if (!header) return false;
    
    return (header->magic == FIRMWARE_AES_MAGIC && 
            header->header_version == FIRMWARE_AES_VERSION &&
            header->firmware_size > 0 && 
            header->firmware_size < 512*1024);
}

/**
 * @brief 检查固件是否为AES加密固件
 */
bool firmware_aes_is_encrypted(uint32_t firmware_addr)
{
    firmware_aes_header_t* header = (firmware_aes_header_t*)firmware_addr;
    return firmware_aes_validate_header(header);
}

/**
 * @brief 解密AES加密固件到指定地址
 */
uint32_t firmware_aes_decrypt_firmware(uint32_t encrypted_addr, uint32_t output_addr, uint32_t max_size, const char* password)
{
    firmware_aes_header_t* header = (firmware_aes_header_t*)encrypted_addr;
    
    if (!firmware_aes_validate_header(header)) return 0;
    if (header->firmware_size > max_size) return 0;
    
    /* 生成AES密钥 */
    uint8_t aes_key[AES_KEY_SIZE];
    uint32_t* unique_id = (uint32_t*)0x1FFFF7E8;
    
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
    for (int i = 0; i < AES_KEY_SIZE; i++) {
        uint32_t byte_val = aes_key[i];
        if (byte_val < 0x10) print_str("0");
        print_hex(byte_val);
    }
    print_str("\r\n");
    
    /* 初始化mbedTLS AES */
    if (!firmware_aes_init(aes_key)) return 0;
    
    /* 使用临时RAM区域进行解密 */
    uint8_t* encrypted_data = (uint8_t*)(encrypted_addr + sizeof(firmware_aes_header_t));
    uint8_t* temp_buffer = (uint8_t*)(0x20000000 + 0xC000);
    
    /* 使用mbedTLS进行AES-CBC解密 */
    if (!firmware_aes_decrypt_cbc(encrypted_data, temp_buffer, header->encrypted_size, header->iv)) {
        return 0;
    }
    
    /* 移除PKCS7填充 */
    uint32_t actual_size = firmware_aes_pkcs7_unpad(temp_buffer, header->encrypted_size);
    if (actual_size != header->firmware_size) return 0;
    
    /* 写入Flash需要解锁并使用专门的写入函数 */
    extern bool bootloader_flash_write(uint32_t addr, const uint8_t* data, uint32_t size);
    
    HAL_FLASH_Unlock();
    bool write_success = bootloader_flash_write(output_addr, temp_buffer, actual_size);
    HAL_FLASH_Lock();
    
    if (!write_success) return 0;
    
    return actual_size;
}

