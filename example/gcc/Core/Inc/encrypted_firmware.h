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




#endif /* ENCRYPTED_FIRMWARE_H */