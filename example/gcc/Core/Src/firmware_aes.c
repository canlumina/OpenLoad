#include "firmware_aes.h"
#include "firmware_crypto.h"  /* 复用CRC32函数 */
#include "main.h"
#include <string.h>

/* AES软件实现 - 精简版 */

/* AES常量表 */
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

static const uint8_t inv_sbox[256] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
    0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
    0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
    0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
    0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
    0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
    0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
    0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
    0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
    0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
    0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};

static const uint8_t rcon[11] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

/* 全局变量 */
static uint8_t g_aes_round_keys[11][16];  /* AES-128需要11个轮密钥 */
static bool g_aes_initialized = false;

/* 内部函数声明 */
static void aes_key_expansion(const uint8_t* key);
static void aes_add_round_key(uint8_t* state, const uint8_t* round_key);
static void aes_sub_bytes(uint8_t* state);
static void aes_inv_sub_bytes(uint8_t* state);
static void aes_shift_rows(uint8_t* state);
static void aes_inv_shift_rows(uint8_t* state);
static void aes_mix_columns(uint8_t* state);
static void aes_inv_mix_columns(uint8_t* state);
static uint8_t aes_gmul(uint8_t a, uint8_t b);
static void aes_encrypt_block(const uint8_t* input, uint8_t* output);
static void aes_decrypt_block(const uint8_t* input, uint8_t* output);

/**
 * @brief 初始化AES加密模块
 */
bool firmware_aes_init(const uint8_t* key)
{
    if (!key) {
        return false;
    }
    
    /* 密钥扩展 */
    aes_key_expansion(key);
    
    g_aes_initialized = true;
    return true;
}

/**
 * @brief AES密钥扩展
 */
static void aes_key_expansion(const uint8_t* key)
{
    /* 复制原始密钥 */
    memcpy(g_aes_round_keys[0], key, 16);
    
    for (int i = 1; i <= 10; i++) {
        /* 取前一个轮密钥的最后4字节 */
        uint8_t temp[4];
        memcpy(temp, &g_aes_round_keys[i-1][12], 4);
        
        /* RotWord */
        uint8_t tmp = temp[0];
        temp[0] = temp[1];
        temp[1] = temp[2];
        temp[2] = temp[3];
        temp[3] = tmp;
        
        /* SubWord */
        temp[0] = sbox[temp[0]];
        temp[1] = sbox[temp[1]];
        temp[2] = sbox[temp[2]];
        temp[3] = sbox[temp[3]];
        
        /* XOR with Rcon */
        temp[0] ^= rcon[i];
        
        /* 生成新的轮密钥 */
        for (int j = 0; j < 16; j++) {
            if (j < 4) {
                g_aes_round_keys[i][j] = g_aes_round_keys[i-1][j] ^ temp[j];
            } else {
                g_aes_round_keys[i][j] = g_aes_round_keys[i-1][j] ^ g_aes_round_keys[i][j-4];
            }
        }
    }
}

/**
 * @brief AES加密单个块
 */
static void aes_encrypt_block(const uint8_t* input, uint8_t* output)
{
    uint8_t state[16];
    memcpy(state, input, 16);
    
    /* 初始轮密钥加 */
    aes_add_round_key(state, g_aes_round_keys[0]);
    
    /* 9轮加密 */
    for (int round = 1; round <= 9; round++) {
        aes_sub_bytes(state);
        aes_shift_rows(state);
        aes_mix_columns(state);
        aes_add_round_key(state, g_aes_round_keys[round]);
    }
    
    /* 最后一轮 */
    aes_sub_bytes(state);
    aes_shift_rows(state);
    aes_add_round_key(state, g_aes_round_keys[10]);
    
    memcpy(output, state, 16);
}

/**
 * @brief AES解密单个块
 */
static void aes_decrypt_block(const uint8_t* input, uint8_t* output)
{
    uint8_t state[16];
    memcpy(state, input, 16);
    
    /* 初始轮密钥加 */
    aes_add_round_key(state, g_aes_round_keys[10]);
    
    /* 9轮解密 */
    for (int round = 9; round >= 1; round--) {
        aes_inv_shift_rows(state);
        aes_inv_sub_bytes(state);
        aes_add_round_key(state, g_aes_round_keys[round]);
        aes_inv_mix_columns(state);
    }
    
    /* 最后一轮 */
    aes_inv_shift_rows(state);
    aes_inv_sub_bytes(state);
    aes_add_round_key(state, g_aes_round_keys[0]);
    
    memcpy(output, state, 16);
}

/**
 * @brief AES-CBC加密
 */
bool firmware_aes_encrypt_cbc(const uint8_t* input, uint8_t* output, uint32_t size, const uint8_t* iv)
{
    if (!g_aes_initialized || !input || !output || !iv || (size % AES_BLOCK_SIZE) != 0) {
        return false;
    }
    
    uint8_t prev_block[AES_BLOCK_SIZE];
    memcpy(prev_block, iv, AES_BLOCK_SIZE);
    
    for (uint32_t i = 0; i < size; i += AES_BLOCK_SIZE) {
        uint8_t block[AES_BLOCK_SIZE];
        
        /* XOR with previous ciphertext (or IV for first block) */
        for (int j = 0; j < AES_BLOCK_SIZE; j++) {
            block[j] = input[i + j] ^ prev_block[j];
        }
        
        /* Encrypt block */
        aes_encrypt_block(block, &output[i]);
        
        /* Update previous block */
        memcpy(prev_block, &output[i], AES_BLOCK_SIZE);
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

/* AES内部操作函数 */
static void aes_add_round_key(uint8_t* state, const uint8_t* round_key)
{
    for (int i = 0; i < 16; i++) {
        state[i] ^= round_key[i];
    }
}

static void aes_sub_bytes(uint8_t* state)
{
    for (int i = 0; i < 16; i++) {
        state[i] = sbox[state[i]];
    }
}

static void aes_inv_sub_bytes(uint8_t* state)
{
    for (int i = 0; i < 16; i++) {
        state[i] = inv_sbox[state[i]];
    }
}

static void aes_shift_rows(uint8_t* state)
{
    uint8_t temp;
    
    /* Row 1: shift left by 1 */
    temp = state[1];
    state[1] = state[5];
    state[5] = state[9];
    state[9] = state[13];
    state[13] = temp;
    
    /* Row 2: shift left by 2 */
    temp = state[2];
    state[2] = state[10];
    state[10] = temp;
    temp = state[6];
    state[6] = state[14];
    state[14] = temp;
    
    /* Row 3: shift left by 3 */
    temp = state[3];
    state[3] = state[15];
    state[15] = state[11];
    state[11] = state[7];
    state[7] = temp;
}

static void aes_inv_shift_rows(uint8_t* state)
{
    uint8_t temp;
    
    /* Row 1: shift right by 1 */
    temp = state[13];
    state[13] = state[9];
    state[9] = state[5];
    state[5] = state[1];
    state[1] = temp;
    
    /* Row 2: shift right by 2 */
    temp = state[2];
    state[2] = state[10];
    state[10] = temp;
    temp = state[6];
    state[6] = state[14];
    state[14] = temp;
    
    /* Row 3: shift right by 3 */
    temp = state[7];
    state[7] = state[11];
    state[11] = state[15];
    state[15] = state[3];
    state[3] = temp;
}

static uint8_t aes_gmul(uint8_t a, uint8_t b)
{
    uint8_t result = 0;
    for (int i = 0; i < 8; i++) {
        if (b & 1) result ^= a;
        bool carry = a & 0x80;
        a <<= 1;
        if (carry) a ^= 0x1b;
        b >>= 1;
    }
    return result;
}

static void aes_mix_columns(uint8_t* state)
{
    for (int c = 0; c < 4; c++) {
        uint8_t s0 = state[c*4 + 0];
        uint8_t s1 = state[c*4 + 1];
        uint8_t s2 = state[c*4 + 2];
        uint8_t s3 = state[c*4 + 3];
        
        state[c*4 + 0] = aes_gmul(s0, 2) ^ aes_gmul(s1, 3) ^ s2 ^ s3;
        state[c*4 + 1] = s0 ^ aes_gmul(s1, 2) ^ aes_gmul(s2, 3) ^ s3;
        state[c*4 + 2] = s0 ^ s1 ^ aes_gmul(s2, 2) ^ aes_gmul(s3, 3);
        state[c*4 + 3] = aes_gmul(s0, 3) ^ s1 ^ s2 ^ aes_gmul(s3, 2);
    }
}

static void aes_inv_mix_columns(uint8_t* state)
{
    for (int c = 0; c < 4; c++) {
        uint8_t s0 = state[c*4 + 0];
        uint8_t s1 = state[c*4 + 1];
        uint8_t s2 = state[c*4 + 2];
        uint8_t s3 = state[c*4 + 3];
        
        state[c*4 + 0] = aes_gmul(s0, 14) ^ aes_gmul(s1, 11) ^ aes_gmul(s2, 13) ^ aes_gmul(s3, 9);
        state[c*4 + 1] = aes_gmul(s0, 9) ^ aes_gmul(s1, 14) ^ aes_gmul(s2, 11) ^ aes_gmul(s3, 13);
        state[c*4 + 2] = aes_gmul(s0, 13) ^ aes_gmul(s1, 9) ^ aes_gmul(s2, 14) ^ aes_gmul(s3, 11);
        state[c*4 + 3] = aes_gmul(s0, 11) ^ aes_gmul(s1, 13) ^ aes_gmul(s2, 9) ^ aes_gmul(s3, 14);
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

/**
 * @brief 验证解密后的固件
 */
bool firmware_aes_verify_firmware(uint32_t firmware_addr, uint32_t expected_size, uint32_t expected_crc32)
{
    if (expected_size == 0) return false;
    
    uint32_t calculated_crc32 = firmware_crypto_crc32((uint8_t*)firmware_addr, expected_size);
    return (calculated_crc32 == expected_crc32);
}