#ifndef __XMODEM_H_
#define __XMODEM_H_

#include <stdint.h>
#include <stdbool.h>
#include "dev_flash.h"

/* XMODEM 控制字符 */
#define SOH  0x01  // 128字节数据包开始
#define STX  0x02  // 1024字节数据包开始  
#define EOT  0x04  // 传输结束
#define ACK  0x06  // 确认
#define NAK  0x15  // 否认
#define CAN  0x18  // 取消传输
#define CTRLZ 0x1A // 填充字符

/* XMODEM 参数 */
#define PACKET_SIZE_128    128
#define PACKET_SIZE_1024   1024
#define PACKET_TIMEOUT     1000   // 1秒超时
#define MAX_RETRIES        10     // 最大重试次数

typedef enum{
	USE_128,
	USE_1K,
}data_block_select_t;

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
int xmodem_receive(const struct flash_partition *partition, data_block_select_t use_1k);


#endif

