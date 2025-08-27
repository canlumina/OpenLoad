#ifndef ZMODEM_H
#define ZMODEM_H

#include <stdint.h>
#include <stdbool.h>

/* ZMODEM 协议常量 */
#define ZMODEM_BLOCK_SIZE       1024    // ZMODEM-1K block size
#define ZMODEM_CRC_SIZE         2        // CRC16 size
#define ZMODEM_HEADER_SIZE      7        // Frame header size

/* ZMODEM 控制字符 */
#define ZPAD                    '*'      // 0x2A Padding character
#define ZDLE                    0x18     // ZMODEM escape character
#define ZDLEE                   0x58     // Escaped ZDLE
#define ZBIN                    'A'      // Binary frame indicator
#define ZHEX                    'B'      // HEX frame indicator
#define ZBIN32                  'C'      // Binary frame with 32-bit CRC
#define ZBINR32                 'D'      // RLE compressed Binary frame with 32-bit CRC

/* ZMODEM 帧类型 */
#define ZRQINIT                 0x00     // Request receive init
#define ZRINIT                  0x01     // Receive init
#define ZSINIT                  0x02     // Send init sequence
#define ZACK                    0x03     // ACK to above
#define ZFILE                   0x04     // File name from sender
#define ZSKIP                   0x05     // Skip file
#define ZNAK                    0x06     // Last packet was garbled
#define ZABORT                  0x07     // Abort batch transfers
#define ZFIN                    0x08     // Finish session
#define ZRPOS                   0x09     // Resume data trans at this position
#define ZDATA                   0x0A     // Data packet(s) follow
#define ZEOF                    0x0B     // End of file
#define ZFERR                   0x0C     // Fatal Read or Write error Detected
#define ZCRC                    0x0D     // Request for file CRC and response
#define ZCHALLENGE              0x0E     // Receiver's Challenge
#define ZCOMPL                  0x0F     // Request is complete
#define ZCAN                    0x10     // Other end canned session with CAN*5
#define ZFREECNT                0x11     // Request for free bytes on filesystem
#define ZCOMMAND                0x12     // Command from sending program
#define ZSTDERR                 0x13     // Output to standard error

/* XON/XOFF 字符 */
#define XON                     0x11     // XON character
#define XOFF                    0x13     // XOFF character

/* CAN 字符 */
#define CAN                     0x18     // Cancel character

/* ZMODEM 接收状态 */
typedef enum {
    ZM_STATE_IDLE,
    ZM_STATE_WAIT_ZRQINIT,
    ZM_STATE_SEND_ZRINIT,
    ZM_STATE_WAIT_ZFILE,
    ZM_STATE_SEND_ZRPOS,
    ZM_STATE_RECEIVE_DATA,
    ZM_STATE_WAIT_ZEOF,
    ZM_STATE_SEND_ZRINIT_AGAIN,
    ZM_STATE_WAIT_ZFIN,
    ZM_STATE_SEND_ZFIN,
    ZM_STATE_COMPLETE,
    ZM_STATE_ERROR,
    ZM_STATE_ABORT
} zmodem_state_t;

/* ZMODEM 错误码 */
typedef enum {
    ZM_OK = 0,
    ZM_ERROR_TIMEOUT,
    ZM_ERROR_CANCEL,
    ZM_ERROR_FRAME,
    ZM_ERROR_CRC,
    ZM_ERROR_SEQUENCE,
    ZM_ERROR_SIZE,
    ZM_ERROR_WRITE,
    ZM_ERROR_UNKNOWN
} zmodem_error_t;

/* ZMODEM 文件信息 */
typedef struct {
    char name[256];          // 文件名
    uint32_t size;           // 文件大小
    uint32_t mode;           // 文件权限
    uint32_t mtime;          // 修改时间
    uint32_t bytes_received; // 已接收字节数
    uint32_t bytes_total;    // 总字节数
    uint32_t file_offset;    // 文件偏移
} zmodem_file_info_t;

/* ZMODEM 接收器上下文 */
typedef struct {
    zmodem_state_t state;           // 当前状态
    zmodem_file_info_t file_info;   // 文件信息
    uint32_t rx_pos;                 // 接收位置
    uint32_t tx_pos;                 // 发送位置
    uint8_t rx_buffer[ZMODEM_BLOCK_SIZE + 16]; // 接收缓冲区
    uint16_t rx_count;               // 接收计数
    uint8_t tx_buffer[256];          // 发送缓冲区
    uint16_t tx_count;               // 发送计数
    uint8_t can_count;               // CAN字符计数
    uint32_t timeout_count;          // 超时计数
    uint32_t error_count;            // 错误计数
    bool attn_seq_detected;         // 检测到注意序列
    bool escape_pending;            // 转义待处理
    
    /* 回调函数 */
    int (*write_data)(uint32_t offset, const uint8_t* data, uint32_t len);
    void (*progress_callback)(uint32_t received, uint32_t total);
    void (*status_callback)(const char* status);
} zmodem_t;

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

/* 函数声明 */

/* ZMODEM 核心功能 */
void zmodem_init(zmodem_t* zm);
void zmodem_reset(zmodem_t* zm);
zmodem_error_t zmodem_receive_byte(zmodem_t* zm, uint8_t byte);
zmodem_error_t zmodem_receive_buffer(zmodem_t* zm, const uint8_t* buffer, uint32_t len);
bool zmodem_is_complete(const zmodem_t* zm);
bool zmodem_is_error(const zmodem_t* zm);
const char* zmodem_get_error_string(zmodem_error_t error);

/* ZMODEM 发送功能 */
int zmodem_send_hex_header(uint8_t* buffer, uint8_t type, uint32_t pos);
int zmodem_send_binary_header(uint8_t* buffer, uint8_t type, uint32_t pos);
int zmodem_send_data(uint8_t* buffer, const uint8_t* data, uint32_t len, uint8_t frame_end);
void zmodem_send_cancel(uint8_t* buffer);

/* CRC 计算 */
uint16_t zmodem_crc16(const uint8_t* data, uint32_t len);
uint32_t zmodem_crc32(const uint8_t* data, uint32_t len);

/* 流式写入功能 */
void stream_writer_init(stream_writer_t* writer, uint32_t target_addr, bool use_external);
void stream_writer_set_partition(stream_writer_t* writer, uint8_t partition_id);
int stream_writer_write(stream_writer_t* writer, const uint8_t* data, uint32_t len);
int stream_writer_flush(stream_writer_t* writer);
void stream_writer_reset(stream_writer_t* writer);

/* IAP 功能 */
int iap_receive_firmware_zmodem(uint32_t target_addr, bool use_external_flash, uint8_t partition_id);
int iap_update_from_external_flash(uint8_t partition_id);
int iap_verify_firmware(uint32_t addr, uint32_t size, uint32_t expected_crc);

#endif /* ZMODEM_H */