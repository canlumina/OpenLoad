#include "crc32.h"

static uint32_t crc_table[256];
static int table_initialized = 0;

static void init_crc32_table(void) {
    if (table_initialized) {
        return;
    }
    uint32_t i, j, crc;
    for (i = 0; i < 256; i++) {
        crc = i;
        for (j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
        crc_table[i] = crc;
    }
    table_initialized = 1;
}

uint32_t crc32(const uint8_t *data, uint32_t length) {
    init_crc32_table();
    
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < length; i++) {
        crc = (crc >> 8) ^ crc_table[(crc & 0xFF) ^ data[i]];
    }
    return crc ^ 0xFFFFFFFF;
}

/* Streaming CRC32 functions */
uint32_t crc32_init(void) {
    init_crc32_table();
    return 0xFFFFFFFF;
}

uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t length) {
    init_crc32_table();
    for (uint32_t i = 0; i < length; i++) {
        crc = (crc >> 8) ^ crc_table[(crc & 0xFF) ^ data[i]];
    }
    return crc;
}

uint32_t crc32_final(uint32_t crc) {
    return crc ^ 0xFFFFFFFF;
}
