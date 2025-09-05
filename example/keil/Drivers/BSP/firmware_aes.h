#ifndef FIRMWARE_AES_H
#define FIRMWARE_AES_H

#include <stdint.h>
#include <stdbool.h>
#include "firmware_version.h"

/* AES加密相关常量 */
#define AES_BLOCK_SIZE          16
#define AES_KEY_SIZE            16  /* AES-128 */
#define AES_IV_SIZE             16

/* AES加密固件魔数 */
#define FIRMWARE_AES_MAGIC      0x41455331  /* "AES1" */
#define FIRMWARE_AES_VERSION    1

/* AES加密固件头部结构 */
typedef struct {
    uint32_t magic;                    /* 魔数：FIRMWARE_AES_MAGIC */
    uint32_t header_version;           /* 头部版本号 */
    uint32_t firmware_size;            /* 原始固件大小 */
    uint32_t encrypted_size;           /* 加密后固件大小 */
    uint32_t crc32;                    /* 原始固件CRC32校验 */
    uint32_t encrypted_crc32;          /* 加密固件CRC32校验 */
    uint8_t  iv[AES_IV_SIZE];          /* 初始化向量 */
    uint8_t  key_hash[16];             /* 密钥哈希值 */
    firmware_version_t fw_version;     /* 固件版本信息 (8字节) */
} __attribute__((packed)) firmware_aes_header_t;

/* 函数声明 */

/**
 * @brief 初始化AES加密模块
 * @param key AES密钥（16字节）
 * @return true=成功，false=失败
 */
bool firmware_aes_init(const uint8_t* key);

/**
 * @brief AES-CBC加密
 * @param input 输入数据
 * @param output 输出缓冲区
 * @param size 数据大小（必须是16字节的倍数）
 * @param iv 初始化向量（16字节）
 * @return true=成功，false=失败
 */
bool firmware_aes_encrypt_cbc(const uint8_t* input, uint8_t* output, uint32_t size, const uint8_t* iv);

/**
 * @brief AES-CBC解密
 * @param input 输入数据
 * @param output 输出缓冲区
 * @param size 数据大小（必须是16字节的倍数）
 * @param iv 初始化向量（16字节）
 * @return true=成功，false=失败
 */
bool firmware_aes_decrypt_cbc(const uint8_t* input, uint8_t* output, uint32_t size, const uint8_t* iv);

/**
 * @brief 生成AES密钥
 * @param password 用户密码
 * @param salt 盐值（STM32唯一ID）
 * @param key 输出的AES密钥（16字节）
 */
void firmware_aes_derive_key(const char* password, const uint32_t* salt, uint8_t* key);

/**
 * @brief 验证AES加密固件头部
 * @param header 固件头部指针
 * @return true=有效，false=无效
 */
bool firmware_aes_validate_header(const firmware_aes_header_t* header);

/**
 * @brief 检查固件是否为AES加密固件
 * @param firmware_addr 固件地址
 * @return true=AES加密固件，false=其他格式固件
 */
bool firmware_aes_is_encrypted(uint32_t firmware_addr);

/**
 * @brief 解密AES加密固件到指定地址
 * @param encrypted_addr 加密固件地址
 * @param output_addr 输出地址
 * @param max_size 最大输出大小
 * @param password 解密密码
 * @return 解密后固件大小，0=失败
 */
uint32_t firmware_aes_decrypt_firmware(uint32_t encrypted_addr, uint32_t output_addr, uint32_t max_size, const char* password);


/**
 * @brief PKCS7填充
 * @param data 数据缓冲区
 * @param data_len 数据长度
 * @param block_size 块大小
 * @return 填充后的长度
 */
uint32_t firmware_aes_pkcs7_pad(uint8_t* data, uint32_t data_len, uint32_t block_size);

/**
 * @brief 移除PKCS7填充
 * @param data 数据缓冲区
 * @param data_len 数据长度
 * @return 移除填充后的长度，0=无效填充
 */
uint32_t firmware_aes_pkcs7_unpad(const uint8_t* data, uint32_t data_len);

#endif /* FIRMWARE_AES_H */