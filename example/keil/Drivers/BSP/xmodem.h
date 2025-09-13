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
    /* 分区信息 */
    const struct flash_partition *partition;  // 当前操作的分区
    
    /* 写入位置跟踪 */
    uint32_t write_addr;                      // 当前写入地址（相对于分区起始）
    uint32_t total_written;                   // 已写入的总字节数
    
    /* 页缓冲管理 - 用于处理非页对齐的写入 */
    uint8_t page_buffer[256];                 // 页缓冲区（W25Q64页大小）
    uint16_t page_buffer_offset;              // 页缓冲区内的偏移
    uint32_t page_buffer_addr;                // 页缓冲区对应的Flash地址
    bool page_buffer_dirty;                   // 页缓冲区是否有待写入数据
    
    /* 擦除管理 */
    uint32_t next_erase_addr;                 // 下一个需要擦除的地址
    uint32_t erase_block_size;                // 擦除块大小（扇区或块）
    bool initial_erase_done;                  // 是否已完成初始擦除
    
    /* 状态和统计 */
    bool initialized;                          // 是否已初始化
    uint32_t expected_size;                    // 期望接收的总大小（如果已知）
    uint16_t crc16;                           // 数据CRC16校验值
    uint8_t checksum;                         // 数据校验和
} stream_writer_t;

/* 流式写入API */
int stream_writer_init(stream_writer_t *writer, const struct flash_partition *partition);
int stream_writer_write(stream_writer_t *writer, const uint8_t *data, size_t len);
int stream_writer_flush(stream_writer_t *writer);
void stream_writer_deinit(stream_writer_t *writer);
uint32_t stream_writer_get_written_size(stream_writer_t *writer);
uint16_t stream_writer_get_crc16(stream_writer_t *writer);

/* CRC16计算 */
uint16_t crc16_ccitt(const uint8_t *data, size_t len, uint16_t crc);

/* XMODEM接收函数 */
int xmodem_receive(const struct flash_partition *partition, data_block_select_t use_1k);


#endif

