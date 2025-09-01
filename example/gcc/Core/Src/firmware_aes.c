#include "firmware_aes.h"
#include "firmware_crypto.h"  /* 复用CRC32函数 */
#include "main.h"
#include <string.h>

/* 简化的AES实现 - 只保留必要的常量 */
static const uint8_t sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

/* 全局变量 */
static bool g_aes_initialized = false;

/* 内部函数声明 - 简化版本 */

bool firmware_aes_init(const uint8_t* key)
{
    g_aes_initialized = (key != NULL);
    return g_aes_initialized;
}




/**
 * @brief AES-CBC加密 - 简化版本
 */
bool firmware_aes_encrypt_cbc(const uint8_t* input, uint8_t* output, uint32_t size, const uint8_t* iv)
{
    if (!g_aes_initialized || !input || !output || !iv || (size % AES_BLOCK_SIZE) != 0) {
        return false;
    }
    
    /* 简化实现 - 使用XOR代替AES加密 */
    for (uint32_t i = 0; i < size; i++) {
        output[i] = input[i] ^ 0xAA ^ iv[i % AES_BLOCK_SIZE];
    }
    
    return true;
}

/**
 * @brief AES-CBC解密
 */
bool firmware_aes_decrypt_cbc(const uint8_t* input, uint8_t* output, uint32_t size, const uint8_t* iv)
{
    if (!g_aes_initialized || !input || !output || !iv || (size % AES_BLOCK_SIZE) != 0) {
        return false;
    }
    
    uint8_t prev_block[AES_BLOCK_SIZE];
    memcpy(prev_block, iv, AES_BLOCK_SIZE);
    
    for (uint32_t i = 0; i < size; i += AES_BLOCK_SIZE) {
        uint8_t decrypted_block[AES_BLOCK_SIZE];
        
        /* Decrypt block - 简化版本使用XOR代替复杂的AES */
        for (int j = 0; j < AES_BLOCK_SIZE; j++) {
            decrypted_block[j] = input[i + j] ^ 0xAA;  /* 简单的XOR解密 */
        }
        
        /* XOR with previous ciphertext (or IV for first block) */
        for (int j = 0; j < AES_BLOCK_SIZE; j++) {
            output[i + j] = decrypted_block[j] ^ prev_block[j];
        }
        
        /* Update previous block */
        memcpy(prev_block, &input[i], AES_BLOCK_SIZE);
    }
    
    return true;
}

/**
 * @brief 生成AES密钥
 */
void firmware_aes_derive_key(const char* password, const uint32_t* salt, uint8_t* key)
{
    /* 简单的密钥派生：SHA256的简化版本 */
    uint8_t temp_key[32];
    memset(temp_key, 0, sizeof(temp_key));
    
    /* 复制密码 */
    uint32_t pwd_len = strlen(password);
    if (pwd_len > 24) pwd_len = 24;
    memcpy(temp_key, password, pwd_len);
    
    /* 混合盐值（STM32 unique ID） */
    for (int i = 0; i < 2; i++) {  /* 只使用前2个ID值，避免越界 */
        temp_key[24 + i*4] = (salt[i] >> 0) & 0xFF;
        temp_key[24 + i*4 + 1] = (salt[i] >> 8) & 0xFF;
        temp_key[24 + i*4 + 2] = (salt[i] >> 16) & 0xFF;
        temp_key[24 + i*4 + 3] = (salt[i] >> 24) & 0xFF;
    }
    
    /* 简单的哈希混合 - 简化版本用于调试 */
    for (int round = 0; round < 10; round++) {  /* 临时减少循环次数 */
        for (int i = 0; i < 16; i++) {
            key[i] = temp_key[i] ^ temp_key[i+16];
            key[i] ^= (uint8_t)(round & 0xFF);
            key[i] ^= sbox[key[i]];  /* 使用AES S盒增加非线性 */
        }
        memcpy(temp_key, key, 16);
        memcpy(&temp_key[16], key, 16);
    }
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
            header->version == FIRMWARE_AES_VERSION &&
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
    firmware_aes_derive_key(password, unique_id, aes_key);
    
    /* 初始化AES */
    if (!firmware_aes_init(aes_key)) return 0;
    
    /* 使用临时RAM区域进行解密 */
    uint8_t* encrypted_data = (uint8_t*)(encrypted_addr + sizeof(firmware_aes_header_t));
    uint8_t* temp_buffer = (uint8_t*)(0x20000000 + 0xC000);  /* 使用不同的RAM区域避免冲突 */
    
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

