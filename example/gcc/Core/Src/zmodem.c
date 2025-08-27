#include "zmodem.h"
#include "dev_usart.h"
#include "w25q64.h"
#include "bootloader_cmd.h"
#include <string.h>
#include <stdio.h>

/* CRC表 */
static const uint16_t crc16_tab[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
    0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
    0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
    0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

/* 私有函数声明 */
static void zmodem_process_frame(zmodem_t* zm);
static int zmodem_parse_file_header(zmodem_t* zm, const uint8_t* data);
static void zmodem_send_hex_hdr(zmodem_t* zm, uint8_t type, uint32_t pos);
static void zmodem_send_ack(zmodem_t* zm);
static void zmodem_send_nak(zmodem_t* zm);
static void zmodem_send_rpos(zmodem_t* zm, uint32_t pos);
static void zmodem_send_init(zmodem_t* zm);
static void zmodem_send_cancel_seq(zmodem_t* zm);
static void uart_send_buffer(const uint8_t* buffer, uint32_t len);
static void uart_send_byte(uint8_t byte);

/* UART发送函数 */
static void uart_send_byte(uint8_t byte)
{
    uart_write(DEV_UART1, &byte, 1);
    uart_poll_dma_tx(DEV_UART1);
}

static void uart_send_buffer(const uint8_t* buffer, uint32_t len)
{
    uart_write(DEV_UART1, (uint8_t*)buffer, len);
    uart_poll_dma_tx(DEV_UART1);
}

/* 初始化ZMODEM接收器 */
void zmodem_init(zmodem_t* zm)
{
    memset(zm, 0, sizeof(zmodem_t));
    zm->state = ZM_STATE_IDLE;
}

/* 重置ZMODEM接收器 */
void zmodem_reset(zmodem_t* zm)
{
    zm->state = ZM_STATE_IDLE;
    zm->rx_pos = 0;
    zm->tx_pos = 0;
    zm->rx_count = 0;
    zm->tx_count = 0;
    zm->can_count = 0;
    zm->timeout_count = 0;
    zm->error_count = 0;
    zm->attn_seq_detected = false;
    zm->escape_pending = false;
    memset(&zm->file_info, 0, sizeof(zmodem_file_info_t));
}

/* CRC16计算 */
uint16_t zmodem_crc16(const uint8_t* data, uint32_t len)
{
    uint16_t crc = 0;
    while(len--)
    {
        crc = (crc << 8) ^ crc16_tab[((crc >> 8) ^ *data++) & 0xFF];
    }
    return crc;
}

/* CRC32计算 */
uint32_t zmodem_crc32(const uint8_t* data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    while(len--)
    {
        crc ^= *data++;
        for(uint8_t j = 0; j < 8; j++)
        {
            if(crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc = crc >> 1;
        }
    }
    return ~crc;
}

/* 发送ZRINIT */
static void zmodem_send_init(zmodem_t* zm)
{
    uint8_t buffer[32];
    int len = 0;
    
    /* ZRINIT header */
    buffer[len++] = ZPAD;
    buffer[len++] = ZPAD;
    buffer[len++] = ZDLE;
    buffer[len++] = ZHEX;
    len += sprintf((char*)&buffer[len], "01");  // ZRINIT
    len += sprintf((char*)&buffer[len], "00000000");  // Capabilities
    
    /* CRC */
    uint16_t crc = zmodem_crc16(&buffer[4], 10);
    len += sprintf((char*)&buffer[len], "%04x\r\n", crc);
    
    /* XON */
    buffer[len++] = XON;
    
    uart_send_buffer(buffer, len);
}

/* 发送ZRPOS */
static void zmodem_send_rpos(zmodem_t* zm, uint32_t pos)
{
    uint8_t buffer[32];
    int len = 0;
    
    buffer[len++] = ZPAD;
    buffer[len++] = ZPAD;
    buffer[len++] = ZDLE;
    buffer[len++] = ZHEX;
    len += sprintf((char*)&buffer[len], "09");  // ZRPOS
    len += sprintf((char*)&buffer[len], "%08x", pos);
    
    uint16_t crc = zmodem_crc16(&buffer[4], 10);
    len += sprintf((char*)&buffer[len], "%04x\r\n", crc);
    
    uart_send_buffer(buffer, len);
}

/* 发送取消序列 */
static void zmodem_send_cancel_seq(zmodem_t* zm)
{
    uint8_t cancel_seq[8];
    for(int i = 0; i < 8; i++)
    {
        cancel_seq[i] = CAN;
    }
    uart_send_buffer(cancel_seq, 8);
}

/* 接收单个字节 */
zmodem_error_t zmodem_receive_byte(zmodem_t* zm, uint8_t byte)
{
    /* 检测取消序列 */
    if(byte == CAN)
    {
        zm->can_count++;
        if(zm->can_count >= 5)
        {
            zm->state = ZM_STATE_ABORT;
            return ZM_ERROR_CANCEL;
        }
    }
    else
    {
        zm->can_count = 0;
    }
    
    /* 状态机处理 */
    switch(zm->state)
    {
        case ZM_STATE_IDLE:
            /* 等待rz命令 */
            if(byte == 'r')
            {
                zm->state = ZM_STATE_WAIT_ZRQINIT;
            }
            break;
            
        case ZM_STATE_WAIT_ZRQINIT:
            /* 检测ZRQINIT */
            if(byte == 'z')
            {
                /* 发送ZRINIT响应 */
                zmodem_send_init(zm);
                zm->state = ZM_STATE_WAIT_ZFILE;
                if(zm->status_callback)
                {
                    zm->status_callback("Waiting for file header...");
                }
            }
            break;
            
        case ZM_STATE_WAIT_ZFILE:
        case ZM_STATE_RECEIVE_DATA:
        case ZM_STATE_WAIT_ZEOF:
            /* 接收数据到缓冲区 */
            if(zm->rx_count < sizeof(zm->rx_buffer))
            {
                zm->rx_buffer[zm->rx_count++] = byte;
            }
            
            /* 检查是否收到完整帧 */
            zmodem_process_frame(zm);
            break;
            
        case ZM_STATE_WAIT_ZFIN:
            /* 等待ZFIN */
            if(zm->rx_count < sizeof(zm->rx_buffer))
            {
                zm->rx_buffer[zm->rx_count++] = byte;
            }
            zmodem_process_frame(zm);
            break;
            
        default:
            break;
    }
    
    return ZM_OK;
}

/* 处理接收到的帧 */
static void zmodem_process_frame(zmodem_t* zm)
{
    /* 查找ZDLE序列 */
    for(uint16_t i = 0; i < zm->rx_count - 2; i++)
    {
        if(zm->rx_buffer[i] == ZPAD && zm->rx_buffer[i+1] == ZDLE)
        {
            uint8_t frame_type = zm->rx_buffer[i+2];
            
            if(frame_type == ZHEX)
            {
                /* 十六进制帧头 */
                if(i + 19 <= zm->rx_count)  // 完整帧头
                {
                    uint8_t hdr_type;
                    sscanf((char*)&zm->rx_buffer[i+3], "%02x", &hdr_type);
                    
                    switch(hdr_type)
                    {
                        case ZFILE:
                            /* 文件头 */
                            if(zm->state == ZM_STATE_WAIT_ZFILE)
                            {
                                /* 发送ZRPOS */
                                zmodem_send_rpos(zm, 0);
                                zm->state = ZM_STATE_RECEIVE_DATA;
                                if(zm->status_callback)
                                {
                                    zm->status_callback("Receiving file data...");
                                }
                            }
                            break;
                            
                        case ZDATA:
                            /* 数据帧 */
                            zm->state = ZM_STATE_RECEIVE_DATA;
                            break;
                            
                        case ZEOF:
                            /* 文件结束 */
                            if(zm->status_callback)
                            {
                                zm->status_callback("File transfer complete");
                            }
                            /* 发送ZRINIT等待下一个文件 */
                            zmodem_send_init(zm);
                            zm->state = ZM_STATE_WAIT_ZFIN;
                            break;
                            
                        case ZFIN:
                            /* 会话结束 */
                            zm->state = ZM_STATE_COMPLETE;
                            /* 发送最终确认 */
                            uart_send_byte('O');
                            uart_send_byte('O');
                            break;
                    }
                    
                    /* 清除已处理的数据 */
                    if(i + 19 < zm->rx_count)
                    {
                        memmove(zm->rx_buffer, &zm->rx_buffer[i+19], zm->rx_count - i - 19);
                        zm->rx_count -= (i + 19);
                    }
                    else
                    {
                        zm->rx_count = 0;
                    }
                    return;
                }
            }
            else if(frame_type == ZBIN || frame_type == ZBIN32)
            {
                /* 二进制数据帧 */
                if(zm->state == ZM_STATE_RECEIVE_DATA)
                {
                    /* 查找数据块结束 */
                    for(uint16_t j = i + 7; j < zm->rx_count - 2; j++)
                    {
                        if(zm->rx_buffer[j] == ZDLE)
                        {
                            uint8_t end_type = zm->rx_buffer[j+1];
                            if(end_type == 'W' || end_type == 'X' || end_type == 'Y' || end_type == 'Z')
                            {
                                /* 数据块结束 */
                                uint32_t data_len = j - i - 7;
                                
                                /* 调用写入回调 */
                                if(zm->write_data)
                                {
                                    zm->write_data(zm->rx_pos, &zm->rx_buffer[i+7], data_len);
                                }
                                
                                zm->rx_pos += data_len;
                                zm->file_info.bytes_received += data_len;
                                
                                /* 更新进度 */
                                if(zm->progress_callback)
                                {
                                    zm->progress_callback(zm->file_info.bytes_received, zm->file_info.bytes_total);
                                }
                                
                                /* 清除已处理的数据 */
                                if(j + 5 < zm->rx_count)
                                {
                                    memmove(zm->rx_buffer, &zm->rx_buffer[j+5], zm->rx_count - j - 5);
                                    zm->rx_count -= (j + 5);
                                }
                                else
                                {
                                    zm->rx_count = 0;
                                }
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
    
    /* 缓冲区溢出保护 */
    if(zm->rx_count > sizeof(zm->rx_buffer) - 256)
    {
        zm->rx_count = 0;
    }
}

/* 流式写入器初始化 */
void stream_writer_init(stream_writer_t* writer, uint32_t target_addr, bool use_external)
{
    memset(writer, 0, sizeof(stream_writer_t));
    writer->target_addr = target_addr;
    writer->use_external_flash = use_external;
    writer->last_erased_sector = 0xFFFFFFFF;
}

/* 设置分区 */
void stream_writer_set_partition(stream_writer_t* writer, uint8_t partition_id)
{
    writer->partition_id = partition_id;
}

/* 流式写入数据 */
int stream_writer_write(stream_writer_t* writer, const uint8_t* data, uint32_t len)
{
    while(len > 0)
    {
        /* 计算当前页剩余空间 */
        uint32_t page_remain = 256 - writer->page_offset;
        uint32_t copy_len = (len > page_remain) ? page_remain : len;
        
        /* 复制到页缓冲区 */
        memcpy(&writer->page_buffer[writer->page_offset], data, copy_len);
        writer->page_offset += copy_len;
        data += copy_len;
        len -= copy_len;
        
        /* 页满则写入 */
        if(writer->page_offset >= 256)
        {
            if(writer->use_external_flash)
            {
                /* 检查是否需要擦除扇区 */
                uint32_t current_sector = writer->write_offset / W25Q64_SECTOR_SIZE;
                if(current_sector != writer->last_erased_sector)
                {
                    /* 擦除新扇区 */
                    uint32_t sector_addr = current_sector * W25Q64_SECTOR_SIZE;
                    w25q64_erase_sector(sector_addr);
                    writer->last_erased_sector = current_sector;
                }
                
                /* 写入外部Flash */
                if(!w25q64_write_partition(writer->partition_id, writer->write_offset, writer->page_buffer, 256))
                {
                    return -1;
                }
            }
            else
            {
                /* 检查是否需要擦除内部Flash页 */
                uint32_t current_page = (writer->target_addr + writer->write_offset) / FLASH_PAGE_SIZE;
                uint32_t page_start = current_page * FLASH_PAGE_SIZE;
                
                if((writer->target_addr + writer->write_offset) == page_start)
                {
                    /* 擦除页 */
                    if(!bootloader_flash_erase(page_start, FLASH_PAGE_SIZE))
                    {
                        return -1;
                    }
                }
                
                /* 写入内部Flash */
                if(!bootloader_flash_write(writer->target_addr + writer->write_offset, writer->page_buffer, 256))
                {
                    return -1;
                }
            }
            
            writer->write_offset += 256;
            writer->page_offset = 0;
        }
    }
    
    return 0;
}

/* 刷新剩余数据 */
int stream_writer_flush(stream_writer_t* writer)
{
    if(writer->page_offset > 0)
    {
        /* 填充剩余部分为0xFF */
        memset(&writer->page_buffer[writer->page_offset], 0xFF, 256 - writer->page_offset);
        
        if(writer->use_external_flash)
        {
            /* 写入外部Flash */
            if(!w25q64_write_partition(writer->partition_id, writer->write_offset, writer->page_buffer, 256))
            {
                return -1;
            }
        }
        else
        {
            /* 写入内部Flash */
            if(!bootloader_flash_write(writer->target_addr + writer->write_offset, writer->page_buffer, writer->page_offset))
            {
                return -1;
            }
        }
        
        writer->page_offset = 0;
    }
    
    return 0;
}

/* 重置流式写入器 */
void stream_writer_reset(stream_writer_t* writer)
{
    writer->write_offset = 0;
    writer->page_offset = 0;
    writer->last_erased_sector = 0xFFFFFFFF;
    writer->sector_erase_pending = false;
    memset(writer->page_buffer, 0xFF, sizeof(writer->page_buffer));
}

/* IAP通过ZMODEM接收固件 */
int iap_receive_firmware_zmodem(uint32_t target_addr, bool use_external_flash, uint8_t partition_id)
{
    zmodem_t zm;
    stream_writer_t writer;
    uint8_t rx_byte;
    uint32_t timeout = 0;
    
    /* 初始化ZMODEM */
    zmodem_init(&zm);
    
    /* 初始化流式写入器 */
    stream_writer_init(&writer, target_addr, use_external_flash);
    if(use_external_flash)
    {
        stream_writer_set_partition(&writer, partition_id);
        
        /* 初始化W25Q64 */
        w25q64_init();
        
        /* 擦除目标分区 */
        printf("Erasing target partition...\r\n");
        w25q64_erase_partition(partition_id);
    }
    
    /* 设置写入回调 */
    zm.write_data = (int (*)(uint32_t, const uint8_t*, uint32_t))stream_writer_write;
    zm.progress_callback = NULL;
    zm.status_callback = NULL;
    
    printf("Starting ZMODEM receive...\r\n");
    printf("Please start file transfer (rz command on sender side)\r\n");
    
    /* 发送初始rz命令提示 */
    uart_send_buffer((uint8_t*)"rz\r\n", 4);
    
    /* 主接收循环 */
    while(!zmodem_is_complete(&zm) && !zmodem_is_error(&zm))
    {
        /* 接收字节 */
        if(uart_read(DEV_UART1, &rx_byte, 1) > 0)
        {
            timeout = 0;
            zmodem_receive_byte(&zm, rx_byte);
        }
        else
        {
            /* 超时处理 */
            HAL_Delay(10);
            timeout++;
            if(timeout > 3000)  // 30秒超时
            {
                printf("Transfer timeout!\r\n");
                zmodem_send_cancel_seq(&zm);
                break;
            }
        }
    }
    
    /* 刷新剩余数据 */
    stream_writer_flush(&writer);
    
    if(zmodem_is_complete(&zm))
    {
        printf("Transfer complete! Received %d bytes\r\n", zm.file_info.bytes_received);
        return zm.file_info.bytes_received;
    }
    else
    {
        printf("Transfer failed!\r\n");
        return -1;
    }
}

/* 从外部Flash更新固件 */
int iap_update_from_external_flash(uint8_t partition_id)
{
    uint8_t buffer[256];
    uint32_t offset = 0;
    const uint32_t total_size = APP_MAX_SIZE;
    
    printf("Updating firmware from external flash partition %d...\r\n", partition_id);
    
    /* 擦除内部Flash应用区 */
    printf("Erasing application area...\r\n");
    if(!bootloader_flash_erase(APP_START_ADDR, APP_MAX_SIZE))
    {
        printf("Failed to erase application area!\r\n");
        return -1;
    }
    
    /* 从外部Flash复制到内部Flash */
    printf("Copying firmware...\r\n");
    while(offset < total_size)
    {
        /* 读取外部Flash */
        if(!w25q64_read_partition(partition_id, offset, buffer, sizeof(buffer)))
        {
            printf("Failed to read external flash!\r\n");
            return -1;
        }
        
        /* 写入内部Flash */
        if(!bootloader_flash_write(APP_START_ADDR + offset, buffer, sizeof(buffer)))
        {
            printf("Failed to write internal flash!\r\n");
            return -1;
        }
        
        offset += sizeof(buffer);
        printf("Progress: %d%%\r", (offset * 100) / total_size);
    }
    
    printf("\r\nFirmware update complete!\r\n");
    return 0;
}

/* 验证固件 */
int iap_verify_firmware(uint32_t addr, uint32_t size, uint32_t expected_crc)
{
    uint32_t calculated_crc = bootloader_calc_crc32(addr, size);
    
    if(calculated_crc == expected_crc)
    {
        printf("Firmware verification passed (CRC32: 0x%08X)\r\n", calculated_crc);
        return 0;
    }
    else
    {
        printf("Firmware verification failed!\r\n");
        printf("Expected CRC32: 0x%08X\r\n", expected_crc);
        printf("Calculated CRC32: 0x%08X\r\n", calculated_crc);
        return -1;
    }
}

/* 辅助函数 */
bool zmodem_is_complete(const zmodem_t* zm)
{
    return zm->state == ZM_STATE_COMPLETE;
}

bool zmodem_is_error(const zmodem_t* zm)
{
    return zm->state == ZM_STATE_ERROR || zm->state == ZM_STATE_ABORT;
}

const char* zmodem_get_error_string(zmodem_error_t error)
{
    switch(error)
    {
        case ZM_OK: return "No error";
        case ZM_ERROR_TIMEOUT: return "Timeout";
        case ZM_ERROR_CANCEL: return "Cancelled";
        case ZM_ERROR_FRAME: return "Frame error";
        case ZM_ERROR_CRC: return "CRC error";
        case ZM_ERROR_SEQUENCE: return "Sequence error";
        case ZM_ERROR_SIZE: return "Size error";
        case ZM_ERROR_WRITE: return "Write error";
        default: return "Unknown error";
    }
}