/*
 * WiFi 凭据持久化 (W25Q64 上 wifi_cfg 分区)
 *
 * 单条记录, 4KB sector. 格式:
 *   { magic, version, ssid[64], pass[64], reserved, crc32 } 共 144 字节,
 *   余下空间 0xFF 保留. 写入前整 sector 擦除.
 */
#pragma once

#include <stdint.h>

int wifi_cfg_save(const char *ssid, const char *pass);

/**
 * @param ssid_out / pass_out  缓冲, 至少 64 字节
 * @return OL_OK / OL_E_NOT_FOUND (无有效记录) / OL_E_IMAGE_HDR_CRC (CRC 错)
 */
int wifi_cfg_load(char *ssid_out, uint32_t ssid_size,
                  char *pass_out, uint32_t pass_size);

int wifi_cfg_clear(void);
