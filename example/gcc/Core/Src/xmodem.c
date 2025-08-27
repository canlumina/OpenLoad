#include "xmodem.h"
#include "dev_usart.h"
#include "w25q64.h"
#include "bootloader_cmd.h"
#include <string.h>
#include <stdio.h>

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

/* 私有函数 */
static void uart_putc(uint8_t ch)
{
    uart_write(DEV_UART1, &ch, 1);
    uart_poll_dma_tx(DEV_UART1);
}

static int uart_getc_timeout(uint8_t* ch, uint32_t timeout_ms)
{
    uint32_t start_tick = HAL_GetTick();
    
    while((HAL_GetTick() - start_tick) < timeout_ms)
    {
        if(uart_read(DEV_UART1, ch, 1) > 0)
        {
            return 1;
        }
        HAL_Delay(1);
    }
    
    return 0;  // 超时
}

static uint16_t calc_crc16(const uint8_t* data, uint32_t len)
{
    uint16_t crc = 0;
    
    while(len--)
    {
        crc = (crc << 8) ^ crc16_tab[((crc >> 8) ^ *data++) & 0xFF];
    }
    
    return crc;
}

static uint8_t calc_checksum(const uint8_t* data, uint32_t len)
{
    uint8_t sum = 0;
    
    while(len--)
    {
        sum += *data++;
    }
    
    return sum;
}

/* 清空串口缓冲区 */
static void flush_uart(void)
{
    uint8_t dummy[256];
    /* 多次读取确保清空 */
    for(int i = 0; i < 3; i++)
    {
        while(uart_read(DEV_UART1, dummy, sizeof(dummy)) > 0);
        HAL_Delay(10);
    }
}

/* 流式写入器实现 */
static void stream_writer_init(stream_writer_t* writer, uint32_t target_addr, bool use_external)
{
    memset(writer, 0, sizeof(stream_writer_t));
    writer->target_addr = target_addr;
    writer->use_external_flash = use_external;
    writer->last_erased_sector = 0xFFFFFFFF;
}

static void stream_writer_set_partition(stream_writer_t* writer, uint8_t partition_id)
{
    writer->partition_id = partition_id;
}

static int stream_writer_write(stream_writer_t* writer, const uint8_t* data, uint32_t len)
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

static int stream_writer_flush(stream_writer_t* writer)
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

/* XMODEM接收函数 */
int xmodem_receive(uint32_t dest_addr, bool use_external_flash, uint8_t partition_id, bool use_1k)
{
    uint8_t packet_buffer[1024 + 5];  // 最大包大小 + 头部和CRC
    uint8_t packet_num = 1;
    uint8_t packet_num_comp;
    uint32_t total_received = 0;
    uint32_t retry_count;
    uint32_t error_count = 0;
    uint16_t packet_size = use_1k ? PACKET_SIZE_1024 : PACKET_SIZE_128;
    bool use_crc = true;
    uint8_t ch;
    stream_writer_t writer;
    
    /* 初始化流式写入器 */
    stream_writer_init(&writer, dest_addr, use_external_flash);
    if(use_external_flash)
    {
        stream_writer_set_partition(&writer, partition_id);
        w25q64_init();
        /* 先擦除目标分区 */
        printf("XMODEM: Erasing target partition...\r\n");
        w25q64_erase_partition(partition_id);
    }
    
    /* 清空串口缓冲区 */
    flush_uart();
    
    printf("XMODEM: Ready to receive, using %s mode\r\n", use_1k ? "XMODEM-1K" : "XMODEM-128");
    
    /* 发送初始字符开始传输 */
    retry_count = 0;
    while(retry_count < 60)  // 60秒超时
    {
        /* 先清空接收缓冲区 */
        uint8_t temp[256];
        while(uart_read(DEV_UART1, temp, sizeof(temp)) > 0);
        
        if(use_crc)
        {
            uart_putc('C');  // 请求CRC模式
        }
        else
        {
            uart_putc(NAK);  // 请求Checksum模式
        }
        
        /* 等待响应 */
        uint32_t wait_start = HAL_GetTick();
        while((HAL_GetTick() - wait_start) < 1000)
        {
            if(uart_getc_timeout(&ch, 10))
            {
                if(ch == SOH || ch == STX)
                {
                    /* 收到数据包开始 */
                    packet_buffer[0] = ch;
                    goto start_transfer;
                }
                else if(ch == EOT)
                {
                    /* 没有数据传输 */
                    uart_putc(ACK);
                    printf("XMODEM: No data received\r\n");
                    return 0;
                }
                else if(ch == CAN)
                {
                    /* 发送方取消 */
                    printf("XMODEM: Transfer cancelled by sender\r\n");
                    return -1;
                }
                /* 忽略其他字符，继续等待 */
            }
        }
        
        retry_count++;
        
        /* 3次后切换到Checksum模式（兼容更多终端） */
        if(retry_count == 3 && use_crc)
        {
            use_crc = false;
            printf("XMODEM: Switching to checksum mode for compatibility\r\n");
        }
        
        /* 打印进度 */
        if(use_crc)
        {
            printf("C");
        }
        else
        {
            printf(".");
        }
        fflush(stdout);
    }
    
    printf("XMODEM: Timeout waiting for sender\r\n");
    return -1;
    
start_transfer:
    /* 主接收循环 */
    while(1)
    {
        uint8_t header = packet_buffer[0];
        
        /* 已经有第一个字节，读取剩余的包 */
        if(header == EOT)
        {
            /* 传输结束 */
            uart_putc(ACK);
            stream_writer_flush(&writer);
            printf("\r\nXMODEM: Transfer complete, received %d bytes\r\n", total_received);
            return total_received;
        }
        else if(header == CAN)
        {
            /* 接收到取消 */
            if(uart_getc_timeout(&ch, 1000) && ch == CAN)
            {
                printf("\r\nXMODEM: Transfer cancelled by sender\r\n");
                return -1;
            }
        }
        else if(header == SOH || header == STX)
        {
            /* 数据包 */
            if(header == SOH)
            {
                packet_size = PACKET_SIZE_128;
            }
            else
            {
                packet_size = PACKET_SIZE_1024;
            }
            
            /* 读取包号 */
            if(!uart_getc_timeout(&packet_buffer[1], 1000) ||
               !uart_getc_timeout(&packet_buffer[2], 1000))
            {
                printf("XMODEM: Timeout reading packet number\r\n");
                uart_putc(NAK);
                error_count++;
                if(error_count > MAX_RETRIES)
                {
                    return -1;
                }
                goto wait_next_packet;
            }
            
            /* 读取数据 */
            for(uint32_t i = 0; i < packet_size; i++)
            {
                if(!uart_getc_timeout(&packet_buffer[3 + i], 1000))
                {
                    printf("XMODEM: Timeout reading data\r\n");
                    uart_putc(NAK);
                    error_count++;
                    if(error_count > MAX_RETRIES)
                    {
                        return -1;
                    }
                    goto wait_next_packet;
                }
            }
            
            /* 读取校验 */
            if(use_crc)
            {
                if(!uart_getc_timeout(&packet_buffer[3 + packet_size], 1000) ||
                   !uart_getc_timeout(&packet_buffer[4 + packet_size], 1000))
                {
                    printf("XMODEM: Timeout reading CRC\r\n");
                    uart_putc(NAK);
                    error_count++;
                    if(error_count > MAX_RETRIES)
                    {
                        return -1;
                    }
                    goto wait_next_packet;
                }
            }
            else
            {
                if(!uart_getc_timeout(&packet_buffer[3 + packet_size], 1000))
                {
                    printf("XMODEM: Timeout reading checksum\r\n");
                    uart_putc(NAK);
                    error_count++;
                    if(error_count > MAX_RETRIES)
                    {
                        return -1;
                    }
                    goto wait_next_packet;
                }
            }
            
            /* 验证包号 */
            packet_num_comp = packet_buffer[2];
            if(packet_buffer[1] != packet_num || packet_num_comp != (255 - packet_num))
            {
                printf("XMODEM: Packet number error\r\n");
                uart_putc(NAK);
                error_count++;
                if(error_count > MAX_RETRIES)
                {
                    return -1;
                }
                goto wait_next_packet;
            }
            
            /* 验证校验 */
            if(use_crc)
            {
                uint16_t crc = calc_crc16(&packet_buffer[3], packet_size);
                uint16_t recv_crc = (packet_buffer[3 + packet_size] << 8) | packet_buffer[4 + packet_size];
                
                if(crc != recv_crc)
                {
                    /* CRC错误 */
                    printf("\r\nXMODEM: CRC error (expected: 0x%04X, got: 0x%04X)\r\n", crc, recv_crc);
                    uart_putc(NAK);
                    error_count++;
                    if(error_count > MAX_RETRIES)
                    {
                        /* 发送取消序列 */
                        for(int i = 0; i < 8; i++) uart_putc(CAN);
                        printf("XMODEM: Too many errors, transfer cancelled\r\n");
                        return -1;
                    }
                    flush_uart();
                    goto wait_next_packet;
                }
            }
            else
            {
                uint8_t checksum = calc_checksum(&packet_buffer[3], packet_size);
                
                if(checksum != packet_buffer[3 + packet_size])
                {
                    /* Checksum错误 */
                    printf("\r\nXMODEM: Checksum error (expected: 0x%02X, got: 0x%02X)\r\n", checksum, packet_buffer[3 + packet_size]);
                    uart_putc(NAK);
                    error_count++;
                    if(error_count > MAX_RETRIES)
                    {
                        /* 发送取消序列 */
                        for(int i = 0; i < 8; i++) uart_putc(CAN);
                        printf("XMODEM: Too many errors, transfer cancelled\r\n");
                        return -1;
                    }
                    flush_uart();
                    goto wait_next_packet;
                }
            }
            
            /* 写入数据 */
            if(stream_writer_write(&writer, &packet_buffer[3], packet_size) < 0)
            {
                printf("XMODEM: Write error\r\n");
                return -1;
            }
            
            total_received += packet_size;
            packet_num++;
            error_count = 0;
            
            /* 发送ACK */
            uart_putc(ACK);
            
            /* 显示进度 */
            if((packet_num % 32) == 0)
            {
                printf("#");
                fflush(stdout);
            }
            else if((packet_num % 8) == 0)
            {
                printf(".");
                fflush(stdout);
            }
        }
        else
        {
            /* 未知字符，忽略 */
            printf("XMODEM: Unknown char 0x%02X\r\n", header);
        }
        
wait_next_packet:
        /* 等待下一个包 */
        if(!uart_getc_timeout(&packet_buffer[0], 5000))  // 5秒超时
        {
            printf("\r\nXMODEM: Timeout waiting for next packet\r\n");
            return -1;
        }
    }
    
    return total_received;
}