/*
 * OpenLoad - 错误码定义
 *
 * 所有公开 API 返回 int: 0 = 成功, 负数 = 模块化错误码。
 * 错误码按模块分段, 便于通过数值定位来源。
 */
#pragma once

#define OL_OK                       0

/* Common (-1 ~ -15) */
#define OL_E_INVAL                  -1   /* 参数非法 */
#define OL_E_TIMEOUT                -2   /* 超时 */
#define OL_E_NOMEM                  -3   /* 内存不足 */
#define OL_E_NOT_FOUND              -4   /* 资源未找到 */
#define OL_E_BUSY                   -5   /* 设备忙 */
#define OL_E_IO                     -6   /* 底层 IO 失败 */
#define OL_E_NOT_SUPPORTED          -7   /* 接口未实现 */

/* Partition (-16 ~ -31) */
#define OL_E_PART_NOT_FOUND         -16
#define OL_E_PART_OUT_OF_RANGE      -17
#define OL_E_PART_WRITE_DENIED      -18
#define OL_E_PART_ALIGN             -19  /* 偏移/长度未按设备粒度对齐 */
#define OL_E_PART_NO_DEVICE         -20  /* 分区引用的 flash device 未注册 */

/* Image (-32 ~ -47) */
#define OL_E_IMAGE_MAGIC            -32
#define OL_E_IMAGE_HDR_CRC          -33
#define OL_E_IMAGE_PAYLOAD_CRC      -34
#define OL_E_IMAGE_SIZE             -35
#define OL_E_IMAGE_BOARD            -36
#define OL_E_IMAGE_VERSION          -37  /* 防回滚命中 */
#define OL_E_IMAGE_SIGNATURE        -38  /* 签名验证失败 */
#define OL_E_IMAGE_HASH             -39  /* SHA-256 摘要不匹配 */

/* Receiver (-48 ~ -63) */
#define OL_E_RX_CANCELED            -48
#define OL_E_RX_PROTOCOL            -49
#define OL_E_RX_CRC                 -50
#define OL_E_RX_OVERFLOW            -51

/* Crypto (-64 ~ -79) */
#define OL_E_CRYPTO_KEY             -64
#define OL_E_CRYPTO_DECRYPT         -65

/* Network / Modem (-80 ~ -95) */
#define OL_E_AT_ERROR               -80  /* AT 命令返回 ERROR / FAIL */
#define OL_E_NET_NO_LINK            -81  /* TCP 链路未建立 / 已断 */
#define OL_E_NET_HTTP               -82  /* HTTP 状态码非 2xx */

/**
 * @brief 返回错误码对应的简短英文字符串 (用于日志).
 * @param  e   错误码
 * @return     不可修改的字符串字面量, 未知错误返回 "unknown".
 */
const char *ol_strerror(int e);
