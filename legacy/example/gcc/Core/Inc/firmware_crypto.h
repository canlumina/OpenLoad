#ifndef FIRMWARE_CRYPTO_H
#define FIRMWARE_CRYPTO_H

#include <stdint.h>
#include <stdbool.h>

/* 固件加密魔数，用于识别加密固件 */
#define FIRMWARE_CRYPTO_MAGIC       0x43525950  /* "CRYP" */
#define FIRMWARE_CRYPTO_VERSION     1

/* 加密密钥长度 */
#define CRYPTO_KEY_SIZE             32

/* 固件头部结构 */
typedef struct {
    uint32_t magic;              /* 魔数：FIRMWARE_CRYPTO_MAGIC */
    uint32_t version;            /* 版本号 */
    uint32_t firmware_size;      /* 原始固件大小 */
    uint32_t encrypted_size;     /* 加密后固件大小 */
    uint32_t crc32;              /* 原始固件CRC32校验 */
    uint32_t encrypted_crc32;    /* 加密固件CRC32校验 */
    uint8_t  key_hash[16];       /* 密钥哈希值（用于验证密钥正确性） */
    uint8_t  reserved[12];       /* 保留字段 */
} __attribute__((packed)) firmware_crypto_header_t;

/* 函数声明 */

/**
 * @brief 初始化固件加密模块
 * @param key 加密密钥
 * @param key_len 密钥长度
 * @return true=成功，false=失败
 */
bool firmware_crypto_init(const uint8_t* key, uint32_t key_len);

/**
 * @brief XOR加密/解密数据块
 * @param data 数据缓冲区（输入/输出）
 * @param size 数据大小
 * @param offset 在整个固件中的偏移量（用于密钥流生成）
 */
void firmware_crypto_xor(uint8_t* data, uint32_t size, uint32_t offset);

/**
 * @brief 计算CRC32校验值
 * @param data 数据指针
 * @param size 数据大小
 * @return CRC32值
 */
uint32_t firmware_crypto_crc32(const uint8_t* data, uint32_t size);

/**
 * @brief 验证加密固件头部
 * @param header 固件头部指针
 * @return true=有效，false=无效
 */
bool firmware_crypto_validate_header(const firmware_crypto_header_t* header);

/**
 * @brief 检查固件是否为加密固件
 * @param firmware_addr 固件地址
 * @return true=加密固件，false=未加密固件
 */
bool firmware_crypto_is_encrypted(uint32_t firmware_addr);

/**
 * @brief 解密固件到指定地址
 * @param encrypted_addr 加密固件地址
 * @param output_addr 输出地址
 * @param max_size 最大输出大小
 * @return 解密后固件大小，0=失败
 */
uint32_t firmware_crypto_decrypt_firmware(uint32_t encrypted_addr, uint32_t output_addr, uint32_t max_size);

/**
 * @brief 验证解密后的固件
 * @param firmware_addr 固件地址
 * @param expected_size 期望大小
 * @param expected_crc32 期望CRC32
 * @return true=验证通过，false=验证失败
 */
bool firmware_crypto_verify_firmware(uint32_t firmware_addr, uint32_t expected_size, uint32_t expected_crc32);

/**
 * @brief 调试打印函数声明
 */
void print_str(const char* str);
void print_hex(uint32_t value);

/**
 * @brief 生成简单的密钥哈希
 * @param key 密钥
 * @param key_len 密钥长度
 * @param hash 输出哈希值（16字节）
 */
void firmware_crypto_generate_key_hash(const uint8_t* key, uint32_t key_len, uint8_t* hash);

#endif /* FIRMWARE_CRYPTO_H */