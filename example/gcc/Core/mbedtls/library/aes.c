#include "mbedtls/aes.h"
#include "mbedtls/platform.h"
#include <string.h>

/* AES S盒 */
static const unsigned char sbox[256] = {
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

/* AES 逆S盒 */
static const unsigned char rsbox[256] = {
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

/* Rcon表 */
static const unsigned char Rcon[10] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

/* 初始化 AES 上下文 */
void mbedtls_aes_init(mbedtls_aes_context *ctx)
{
    memset(ctx, 0, sizeof(mbedtls_aes_context));
}

/* 释放 AES 上下文 */
void mbedtls_aes_free(mbedtls_aes_context *ctx)
{
    if (ctx == NULL) {
        return;
    }
    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_aes_context));
}

/* 密钥扩展 */
static void KeyExpansion(unsigned char* key, unsigned char* w)
{
    int i, j;
    unsigned char temp[4], k;

    // 前4个字（16字节）直接复制
    for (i = 0; i < 16; i++) {
        w[i] = key[i];
    }

    // 生成其他轮密钥
    for (i = 16; i < 176; i += 4) {
        for (j = 0; j < 4; j++) {
            temp[j] = w[i - 4 + j];
        }
        
        if (i % 16 == 0) {
            // RotWord
            k = temp[0];
            temp[0] = temp[1];
            temp[1] = temp[2];
            temp[2] = temp[3];
            temp[3] = k;
            
            // SubWord
            temp[0] = sbox[temp[0]];
            temp[1] = sbox[temp[1]];
            temp[2] = sbox[temp[2]];
            temp[3] = sbox[temp[3]];
            
            // Rcon
            temp[0] ^= Rcon[i/16 - 1];
        }
        
        w[i] = w[i - 16] ^ temp[0];
        w[i + 1] = w[i - 15] ^ temp[1];
        w[i + 2] = w[i - 14] ^ temp[2];
        w[i + 3] = w[i - 13] ^ temp[3];
    }
}

/* SubBytes变换 */
static void SubBytes(unsigned char* state)
{
    int i;
    for (i = 0; i < 16; i++) {
        state[i] = sbox[state[i]];
    }
}

/* InvSubBytes变换 */
static void InvSubBytes(unsigned char* state)
{
    int i;
    for (i = 0; i < 16; i++) {
        state[i] = rsbox[state[i]];
    }
}

/* ShiftRows变换 */
static void ShiftRows(unsigned char* state)
{
    unsigned char temp;
    
    // 第二行左移1位
    temp = state[1];
    state[1] = state[5];
    state[5] = state[9];
    state[9] = state[13];
    state[13] = temp;
    
    // 第三行左移2位
    temp = state[2];
    state[2] = state[10];
    state[10] = temp;
    temp = state[6];
    state[6] = state[14];
    state[14] = temp;
    
    // 第四行左移3位
    temp = state[15];
    state[15] = state[11];
    state[11] = state[7];
    state[7] = state[3];
    state[3] = temp;
}

/* InvShiftRows变换 */
static void InvShiftRows(unsigned char* state)
{
    unsigned char temp;
    
    // 第二行右移1位
    temp = state[13];
    state[13] = state[9];
    state[9] = state[5];
    state[5] = state[1];
    state[1] = temp;
    
    // 第三行右移2位
    temp = state[2];
    state[2] = state[10];
    state[10] = temp;
    temp = state[6];
    state[6] = state[14];
    state[14] = temp;
    
    // 第四行右移3位
    temp = state[3];
    state[3] = state[7];
    state[7] = state[11];
    state[11] = state[15];
    state[15] = temp;
}

/* 有限域乘法函数 */
static unsigned char gf_mul(unsigned char a, unsigned char b)
{
    unsigned char p = 0;
    unsigned char counter;
    unsigned char hi_bit_set;
    
    for (counter = 0; counter < 8; counter++) {
        if ((b & 1) != 0) {
            p ^= a;
        }
        hi_bit_set = (a & 0x80);
        a <<= 1;
        if (hi_bit_set != 0) {
            a ^= 0x1b;
        }
        b >>= 1;
    }
    return p;
}

/* MixColumns变换 */
static void MixColumns(unsigned char* state)
{
    int i;
    unsigned char a, b, c, d;
    
    for (i = 0; i < 4; i++) {
        a = state[i*4+0];
        b = state[i*4+1];
        c = state[i*4+2];
        d = state[i*4+3];
        
        state[i*4+0] = gf_mul(0x02, a) ^ gf_mul(0x03, b) ^ c ^ d;
        state[i*4+1] = a ^ gf_mul(0x02, b) ^ gf_mul(0x03, c) ^ d;
        state[i*4+2] = a ^ b ^ gf_mul(0x02, c) ^ gf_mul(0x03, d);
        state[i*4+3] = gf_mul(0x03, a) ^ b ^ c ^ gf_mul(0x02, d);
    }
}

/* InvMixColumns变换 */
static void InvMixColumns(unsigned char* state)
{
    int i;
    unsigned char a, b, c, d;
    
    for (i = 0; i < 4; i++) {
        a = state[i*4+0];
        b = state[i*4+1];
        c = state[i*4+2];
        d = state[i*4+3];
        
        state[i*4+0] = gf_mul(0x0e, a) ^ gf_mul(0x0b, b) ^ gf_mul(0x0d, c) ^ gf_mul(0x09, d);
        state[i*4+1] = gf_mul(0x09, a) ^ gf_mul(0x0e, b) ^ gf_mul(0x0b, c) ^ gf_mul(0x0d, d);
        state[i*4+2] = gf_mul(0x0d, a) ^ gf_mul(0x09, b) ^ gf_mul(0x0e, c) ^ gf_mul(0x0b, d);
        state[i*4+3] = gf_mul(0x0b, a) ^ gf_mul(0x0d, b) ^ gf_mul(0x09, c) ^ gf_mul(0x0e, d);
    }
}

/* AddRoundKey变换 */
static void AddRoundKey(unsigned char* state, unsigned char* key)
{
    int i;
    for (i = 0; i < 16; i++) {
        state[i] ^= key[i];
    }
}

/* AES加密 */
static void AES_Encrypt(unsigned char* input, unsigned char* output, unsigned char* key)
{
    unsigned char state[16];
    unsigned char RoundKey[176];
    int round = 0;
    
    KeyExpansion(key, RoundKey);
    
    for (int i = 0; i < 16; i++) {
        state[i] = input[i];
    }
    
    AddRoundKey(state, RoundKey);
    
    for (round = 1; round <= 9; round++) {
        SubBytes(state);
        ShiftRows(state);
        MixColumns(state);
        AddRoundKey(state, RoundKey + round * 16);
    }
    
    SubBytes(state);
    ShiftRows(state);
    AddRoundKey(state, RoundKey + 10 * 16);
    
    for (int i = 0; i < 16; i++) {
        output[i] = state[i];
    }
}

/* AES解密 */
static void AES_Decrypt(unsigned char* input, unsigned char* output, unsigned char* key)
{
    unsigned char state[16];
    unsigned char RoundKey[176];
    int round = 0;
    
    KeyExpansion(key, RoundKey);
    
    for (int i = 0; i < 16; i++) {
        state[i] = input[i];
    }
    
    AddRoundKey(state, RoundKey + 10 * 16);
    
    for (round = 9; round >= 1; round--) {
        InvShiftRows(state);
        InvSubBytes(state);
        AddRoundKey(state, RoundKey + round * 16);
        InvMixColumns(state);
    }
    
    InvShiftRows(state);
    InvSubBytes(state);
    AddRoundKey(state, RoundKey);
    
    for (int i = 0; i < 16; i++) {
        output[i] = state[i];
    }
}

/* 设置加密密钥 */
int mbedtls_aes_setkey_enc(mbedtls_aes_context *ctx,
                          const unsigned char *key,
                          unsigned int keybits)
{
    if (ctx == NULL || key == NULL || keybits != 128) {
        return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
    }
    
    ctx->nr = 10;
    ctx->rk = ctx->buf;
    
    memcpy(ctx->buf, key, 16);
    
    return 0;
}

/* 设置解密密钥 */
int mbedtls_aes_setkey_dec(mbedtls_aes_context *ctx,
                          const unsigned char *key,
                          unsigned int keybits)
{
    if (ctx == NULL || key == NULL || keybits != 128) {
        return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
    }
    
    ctx->nr = 10;
    ctx->rk = ctx->buf;
    
    memcpy(ctx->buf, key, 16);
    
    return 0;
}

/* ECB 模式加密/解密 */
int mbedtls_aes_crypt_ecb(mbedtls_aes_context *ctx,
                         int mode,
                         const unsigned char input[16],
                         unsigned char output[16])
{
    if (ctx == NULL || input == NULL || output == NULL) {
        return MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;
    }
    
    if (mode == MBEDTLS_AES_ENCRYPT) {
        AES_Encrypt((unsigned char*)input, output, (unsigned char*)ctx->buf);
    } else {
        AES_Decrypt((unsigned char*)input, output, (unsigned char*)ctx->buf);
    }
    
    return 0;
}

/* CBC 模式加密/解密 */
int mbedtls_aes_crypt_cbc(mbedtls_aes_context *ctx,
                         int mode,
                         size_t length,
                         unsigned char iv[16],
                         const unsigned char *input,
                         unsigned char *output)
{
    int i;
    unsigned char temp[16];
    
    if (ctx == NULL || input == NULL || output == NULL || iv == NULL) {
        return MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;
    }
    
    if (length % 16) {
        return MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;
    }
    
    if (mode == MBEDTLS_AES_DECRYPT) {
        while (length > 0) {
            memcpy(temp, input, 16);
            mbedtls_aes_crypt_ecb(ctx, mode, input, output);
            
            for (i = 0; i < 16; i++) {
                output[i] = output[i] ^ iv[i];
            }
            
            memcpy(iv, temp, 16);
            
            input += 16;
            output += 16;
            length -= 16;
        }
    } else {
        while (length > 0) {
            for (i = 0; i < 16; i++) {
                output[i] = input[i] ^ iv[i];
            }
            
            mbedtls_aes_crypt_ecb(ctx, mode, output, output);
            memcpy(iv, output, 16);
            
            input += 16;
            output += 16;
            length -= 16;
        }
    }
    
    return 0;
}