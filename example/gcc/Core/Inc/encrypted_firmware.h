#ifndef ENCRYPTED_FIRMWARE_H
#define ENCRYPTED_FIRMWARE_H

#include <stdint.h>
#include <stdbool.h>
#include "w25q64.h"

/**
 * @brief 解密外部Flash中的加密固件到内部Flash
 * @param source_partition 源分区（外部Flash）
 * @param password 解密密码
 * @return true=成功，false=失败
 */
bool decrypt_external_firmware_to_internal(w25q64_partition_id_t source_partition, const char* password);

/**
 * @brief 解密外部Flash中的加密固件到另一个外部Flash分区
 * @param source_partition 源分区（外部Flash）  
 * @param dest_partition 目标分区（外部Flash）
 * @param password 解密密码
 * @return true=成功，false=失败
 */
bool decrypt_external_firmware_to_external(w25q64_partition_id_t source_partition, w25q64_partition_id_t dest_partition, const char* password);

/**
 * @brief 从HTTP下载加密固件并解密到内部Flash
 * @param url 固件下载URL
 * @param password 解密密码
 * @return true=成功，false=失败
 */
bool download_and_decrypt_to_internal(const char* url, const char* password);

/**
 * @brief 从HTTP下载加密固件并解密到外部Flash
 * @param url 固件下载URL
 * @param dest_partition 目标分区（外部Flash）
 * @param password 解密密码
 * @return true=成功，false=失败
 */
bool download_and_decrypt_to_external(const char* url, w25q64_partition_id_t dest_partition, const char* password);

#endif /* ENCRYPTED_FIRMWARE_H */