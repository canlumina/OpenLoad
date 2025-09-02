#include "firmware_crypto.h"
#include <string.h>
#include "stm32f1xx_hal.h"

/* 全局变量 */
static uint8_t g_crypto_key[CRYPTO_KEY_SIZE];
static bool g_crypto_initialized = false;

/* CRC32查找表 */
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

/**
 * @brief 初始化固件加密模块
 */
bool firmware_crypto_init(const uint8_t* key, uint32_t key_len)
{
    if (!key || key_len == 0) {
        return false;
    }
    
    /* 清零密钥缓冲区 */
    memset(g_crypto_key, 0, sizeof(g_crypto_key));
    
    /* 复制密钥，如果密钥太短则重复填充 */
    for (uint32_t i = 0; i < CRYPTO_KEY_SIZE; i++) {
        g_crypto_key[i] = key[i % key_len];
    }
    
    /* 简单的密钥扩展：与STM32唯一ID异或 */
    uint32_t* unique_id = (uint32_t*)0x1FFFF7E8; /* STM32F103的唯一ID地址 */
    for (uint32_t i = 0; i < 3; i++) {
        uint32_t id_word = unique_id[i];
        for (uint32_t j = 0; j < 4 && (i*4+j) < CRYPTO_KEY_SIZE; j++) {
            g_crypto_key[i*4+j] ^= (id_word >> (j*8)) & 0xFF;
        }
    }
    
    g_crypto_initialized = true;
    return true;
}

/**
 * @brief XOR加密/解密数据块
 */
void firmware_crypto_xor(uint8_t* data, uint32_t size, uint32_t offset)
{
    if (!g_crypto_initialized || !data) {
        return;
    }
    
    for (uint32_t i = 0; i < size; i++) {
        /* 使用偏移量和位置计算密钥索引，增加密钥流的随机性 */
        uint32_t key_index = (offset + i) % CRYPTO_KEY_SIZE;
        uint8_t key_byte = g_crypto_key[key_index];
        
        /* 增加一些基于位置的变化 */
        key_byte ^= (uint8_t)((offset + i) & 0xFF);
        key_byte ^= (uint8_t)(((offset + i) >> 8) & 0xFF);
        
        data[i] ^= key_byte;
    }
}

/**
 * @brief 计算CRC32校验值 - 使用STM32兼容的软件CRC32
 */
uint32_t firmware_crypto_crc32(const uint8_t* data, uint32_t size)
{
    // 使用STM32兼容的CRC32算法（IEEE 802.3，不取反）
    uint32_t crc = 0xFFFFFFFF;
    
    for (uint32_t i = 0; i < size; i++) {
        uint8_t byte = data[i];
        crc = crc32_table[(crc ^ byte) & 0xFF] ^ (crc >> 8);
    }
    
    // 不取反，匹配STM32硬件CRC行为
    return crc;
}

/**
 * @brief 生成简单的密钥哈希
 */
void firmware_crypto_generate_key_hash(const uint8_t* key, uint32_t key_len, uint8_t* hash)
{
    if (!key || !hash) {
        return;
    }
    
    /* 简单的哈希算法：使用XOR和移位 */
    memset(hash, 0, 16);
    
    for (uint32_t i = 0; i < key_len; i++) {
        uint8_t byte = key[i];
        uint32_t pos = i % 16;
        
        hash[pos] ^= byte;
        hash[(pos + 1) % 16] ^= (byte >> 1) | (byte << 7);  /* 循环移位 */
        hash[(pos + 8) % 16] ^= byte ^ 0xAA;  /* 异或常数 */
    }
    
    /* 额外的混淆 */
    for (int round = 0; round < 3; round++) {
        for (int i = 0; i < 16; i++) {
            hash[i] ^= hash[(i + 1) % 16];
            hash[i] = (hash[i] << 1) | (hash[i] >> 7);  /* 循环左移1位 */
        }
    }
}

/**
 * @brief 验证加密固件头部
 */
bool firmware_crypto_validate_header(const firmware_crypto_header_t* header)
{
    if (!header) {
        return false;
    }
    
    /* 检查魔数 */
    if (header->magic != FIRMWARE_CRYPTO_MAGIC) {
        return false;
    }
    
    /* 检查版本 */
    if (header->version != FIRMWARE_CRYPTO_VERSION) {
        return false;
    }
    
    /* 检查大小的合理性 */
    if (header->firmware_size == 0 || header->firmware_size > 512*1024 ||
        header->encrypted_size == 0 || header->encrypted_size > 512*1024) {
        return false;
    }
    
    return true;
}

/**
 * @brief 检查固件是否为加密固件
 */
bool firmware_crypto_is_encrypted(uint32_t firmware_addr)
{
    firmware_crypto_header_t* header = (firmware_crypto_header_t*)firmware_addr;
    return firmware_crypto_validate_header(header);
}

/**
 * @brief 解密固件到指定地址
 */
uint32_t firmware_crypto_decrypt_firmware(uint32_t encrypted_addr, uint32_t output_addr, uint32_t max_size)
{
    if (!g_crypto_initialized) {
        return 0;
    }
    
    firmware_crypto_header_t* header = (firmware_crypto_header_t*)encrypted_addr;
    
    /* 验证头部 */
    if (!firmware_crypto_validate_header(header)) {
        return 0;
    }
    
    /* 检查输出空间是否足够 */
    if (header->firmware_size > max_size) {
        return 0;
    }
    
    /* 验证密钥 */
    uint8_t calculated_hash[16];
    firmware_crypto_generate_key_hash(g_crypto_key, CRYPTO_KEY_SIZE, calculated_hash);
    if (memcmp(calculated_hash, header->key_hash, 16) != 0) {
        return 0;  /* 密钥不匹配 */
    }
    
    /* 获取加密数据起始地址 */
    uint8_t* encrypted_data = (uint8_t*)(encrypted_addr + sizeof(firmware_crypto_header_t));
    uint8_t* output_data = (uint8_t*)output_addr;
    
    /* 解密数据 */
    uint32_t decrypt_size = header->encrypted_size;
    uint32_t buffer_size = 1024;  /* 分块处理 */
    uint8_t temp_buffer[1024];
    
    for (uint32_t offset = 0; offset < decrypt_size; offset += buffer_size) {
        uint32_t chunk_size = (decrypt_size - offset > buffer_size) ? buffer_size : (decrypt_size - offset);
        
        /* 复制加密数据到临时缓冲区 */
        memcpy(temp_buffer, encrypted_data + offset, chunk_size);
        
        /* 解密 */
        firmware_crypto_xor(temp_buffer, chunk_size, offset);
        
        /* 复制到输出 */
        memcpy(output_data + offset, temp_buffer, chunk_size);
    }
    
    return header->firmware_size;
}

/**
 * @brief 验证解密后的固件
 */
bool firmware_crypto_verify_firmware(uint32_t firmware_addr, uint32_t expected_size, uint32_t expected_crc32)
{
    if (expected_size == 0) {
        return false;
    }
    
    /* 计算固件的CRC32 */
    uint32_t calculated_crc32 = firmware_crypto_crc32((uint8_t*)firmware_addr, expected_size);
    
    /* 调试信息 */
    print_str("Debug: Expected CRC32: 0x");
    print_hex(expected_crc32);
    print_str("\r\n");
    print_str("Debug: Calculated CRC32: 0x");
    print_hex(calculated_crc32);
    print_str("\r\n");
    
    return (calculated_crc32 == expected_crc32);
}