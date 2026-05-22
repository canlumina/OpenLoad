#ifndef XMODEM_H
#define XMODEM_H

#include <stdint.h>
#include <stdbool.h>

/* 流式写入控制 */
typedef struct {
    uint32_t target_addr;            // 目标地址（内部或外部Flash）
    uint32_t write_offset;           // 写入偏移
    uint32_t total_size;             // 总大小
    uint8_t page_buffer[256];        // 页缓冲区
    uint16_t page_offset;            // 页内偏移
    bool use_external_flash;        // 使用外部Flash
    uint8_t partition_id;           // 分区ID（仅外部Flash）
    
    /* 扇区擦除跟踪 */
    uint32_t last_erased_sector;    // 最后擦除的扇区
    bool sector_erase_pending;      // 扇区擦除待处理
} stream_writer_t;

/* XMODEM接收函数 */
int xmodem_receive(uint32_t dest_addr, bool use_external_flash, uint8_t partition_id, bool use_1k);

#endif /* XMODEM_H */