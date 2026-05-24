/*
 * WiFi 凭据持久化实现
 *
 * 走 OpenLoad partition API, 不直接依赖 W25Q64 驱动 — 换 flash 设备只需调整
 * partitions.def, 本文件无需修改.
 */
#include "wifi_cfg.h"
#include "openload/partition.h"
#include "openload/crypto.h"
#include "openload/errno.h"
#include <string.h>

#define WIFI_CFG_MAGIC      0x49464957u    /* 'WIFI' little-endian */
#define WIFI_CFG_VERSION    1u

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    char     ssid[64];
    char     pass[64];
    uint32_t reserved;
    uint32_t crc32;        /* 前面字段的 CRC32 */
} wifi_cfg_t;

_Static_assert(sizeof(wifi_cfg_t) == 144, "wifi_cfg_t layout");

int wifi_cfg_save(const char *ssid, const char *pass)
{
    if (!ssid || !pass) { return OL_E_INVAL; }
    if (strlen(ssid) >= 64 || strlen(pass) >= 64) { return OL_E_INVAL; }

    const ol_partition_t *p = ol_part_find("wifi_cfg");
    if (!p) { return OL_E_PART_NOT_FOUND; }

    wifi_cfg_t cfg = {0};
    cfg.magic   = WIFI_CFG_MAGIC;
    cfg.version = WIFI_CFG_VERSION;
    strncpy(cfg.ssid, ssid, sizeof(cfg.ssid) - 1);
    strncpy(cfg.pass, pass, sizeof(cfg.pass) - 1);
    cfg.crc32 = ol_crc32(0, &cfg, sizeof(cfg) - sizeof(cfg.crc32));

    int rc = ol_part_erase(p, 0, p->size);
    if (rc != OL_OK) { return rc; }
    return ol_part_write(p, 0, &cfg, sizeof(cfg));
}

int wifi_cfg_load(char *ssid_out, uint32_t ssid_size,
                  char *pass_out, uint32_t pass_size)
{
    if (!ssid_out || !pass_out || ssid_size < 64 || pass_size < 64) {
        return OL_E_INVAL;
    }
    const ol_partition_t *p = ol_part_find("wifi_cfg");
    if (!p) { return OL_E_PART_NOT_FOUND; }

    wifi_cfg_t cfg;
    int rc = ol_part_read(p, 0, &cfg, sizeof(cfg));
    if (rc != OL_OK) { return rc; }

    if (cfg.magic != WIFI_CFG_MAGIC || cfg.version != WIFI_CFG_VERSION) {
        return OL_E_NOT_FOUND;
    }
    uint32_t want = ol_crc32(0, &cfg, sizeof(cfg) - sizeof(cfg.crc32));
    if (want != cfg.crc32) { return OL_E_IMAGE_HDR_CRC; }

    memcpy(ssid_out, cfg.ssid, 64);
    memcpy(pass_out, cfg.pass, 64);
    ssid_out[63] = 0;
    pass_out[63] = 0;
    return OL_OK;
}

int wifi_cfg_clear(void)
{
    const ol_partition_t *p = ol_part_find("wifi_cfg");
    if (!p) { return OL_E_PART_NOT_FOUND; }
    return ol_part_erase_all(p);
}
