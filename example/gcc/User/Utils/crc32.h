#ifndef __CRC32_H
#define __CRC32_H

#include <stdint.h>

uint32_t crc32(const uint8_t *data, uint32_t length);

/* Streaming CRC32 functions */
uint32_t crc32_init(void);
uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t length);
uint32_t crc32_final(uint32_t crc);

#endif /* __CRC32_H */
