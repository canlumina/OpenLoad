#include "xmodem.h"
#include "main.h"
#include <string.h>

/* CRC16-CCITT查找表 */
static const uint16_t crc16_table[256] = {
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

/* CRC16-CCITT计算 */
uint16_t crc16_ccitt(const uint8_t *data, size_t len, uint16_t crc)
{
    while (len--) {
        crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ *data++) & 0xff];
    }
    return crc;
}

/* 计算校验和 */
static uint8_t calc_checksum(const uint8_t *data, size_t len)
{
    uint8_t sum = 0;
    while (len--) {
        sum += *data++;
    }
    return sum;
}

/* 串口发送单字节 */
static void uart_putc(uint8_t ch)
{
    uart_write(DEV_UART1, &ch, 1);
    uart_poll_dma_tx(DEV_UART1);
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

/* 带超时的串口接收单字节 */
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

/* ===================== 流式写入API实现 ===================== */

/**
 * @brief 初始化流式写入器
 * @param writer 写入器结构体指针
 * @param partition 目标分区
 * @return 0-成功，<0-失败
 */
int stream_writer_init(stream_writer_t *writer, const struct flash_partition *partition)
{
    if (!writer || !partition) {
        return -1;
    }
    
    /* 清零结构体 */
    memset(writer, 0, sizeof(stream_writer_t));
    
    /* 设置分区信息 */
    writer->partition = partition;
    
    /* 初始化状态 */
    writer->write_addr = 0;
    writer->total_written = 0;
    writer->page_buffer_offset = 0;
    writer->page_buffer_addr = 0;
    writer->page_buffer_dirty = false;
    
    /* 设置擦除参数 */
    writer->next_erase_addr = 0;
    writer->erase_block_size = 4096;  /* 默认使用4KB扇区擦除 */
    writer->initial_erase_done = false;
    
    /* 初始化校验值 */
    writer->crc16 = 0;
    writer->checksum = 0;
    
    writer->initialized = true;
    
    return 0;
}

/**
 * @brief 擦除下一个块
 */
static int stream_writer_erase_next_block(stream_writer_t *writer)
{
    int ret;
    
    /* 如果已经超出分区范围，不再擦除 */
    if (writer->next_erase_addr >= writer->partition->len) {
        return 0;
    }
    
    /* 擦除一个块 */
    ret = flash_partition_erase(writer->partition, writer->next_erase_addr, writer->erase_block_size);
    if (ret < 0) {
        log_f("Failed to erase at addr 0x%08X", writer->next_erase_addr);
        return ret;
    }
    
    /* 更新下一个擦除地址 */
    writer->next_erase_addr += writer->erase_block_size;
    
    return 0;
}

/**
 * @brief 刷新页缓冲区到Flash
 */
static int stream_writer_flush_page_buffer(stream_writer_t *writer)
{
    int ret;
    
    if (!writer->page_buffer_dirty || writer->page_buffer_offset == 0) {
        return 0;
    }
    
    /* 如果缓冲区不满，用0xFF填充 */
    if (writer->page_buffer_offset < 256) {
        memset(&writer->page_buffer[writer->page_buffer_offset], 
               0xFF, 
               256 - writer->page_buffer_offset);
    }
    
    /* 写入Flash */
    ret = flash_partition_write(writer->partition, 
                                writer->page_buffer_addr,
                                writer->page_buffer, 
                                256);
    if (ret < 0) {
        log_f("Failed to write page at addr 0x%08X", writer->page_buffer_addr);
        return ret;
    }
    
    /* 清除脏标志 */
    writer->page_buffer_dirty = false;
    writer->page_buffer_offset = 0;
    
    return 0;
}

/**
 * @brief 流式写入数据
 * @param writer 写入器结构体指针
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return 写入的字节数，<0-失败
 */
int stream_writer_write(stream_writer_t *writer, const uint8_t *data, size_t len)
{
    size_t written = 0;
    int ret;
    
    if (!writer || !writer->initialized || !data || len == 0) {
        return -1;
    }
    
    /* 更新校验值 */
    writer->crc16 = crc16_ccitt(data, len, writer->crc16);
    writer->checksum = calc_checksum(data, len) + writer->checksum;
    
    while (written < len) {
        /* 检查是否需要擦除新的块 */
        if (writer->write_addr >= writer->next_erase_addr) {
            ret = stream_writer_erase_next_block(writer);
            if (ret < 0) {
                return ret;
            }
        }
        
        /* 计算当前页的起始地址 */
        uint32_t page_addr = writer->write_addr & ~0xFF;  /* 256字节对齐 */
        
        /* 如果是新的页，先刷新旧的页缓冲区 */
        if (page_addr != writer->page_buffer_addr && writer->page_buffer_dirty) {
            ret = stream_writer_flush_page_buffer(writer);
            if (ret < 0) {
                return ret;
            }
        }
        
        /* 如果是新的页，设置新的页地址 */
        if (page_addr != writer->page_buffer_addr) {
            writer->page_buffer_addr = page_addr;
            writer->page_buffer_offset = writer->write_addr & 0xFF;
            memset(writer->page_buffer, 0xFF, 256);
        }
        
        /* 计算本次可以写入页缓冲区的字节数 */
        size_t page_remain = 256 - writer->page_buffer_offset;
        size_t to_write = (len - written) < page_remain ? (len - written) : page_remain;
        
        /* 复制数据到页缓冲区 */
        memcpy(&writer->page_buffer[writer->page_buffer_offset], 
               &data[written], 
               to_write);
        
        writer->page_buffer_offset += to_write;
        writer->page_buffer_dirty = true;
        writer->write_addr += to_write;
        writer->total_written += to_write;
        written += to_write;
        
        /* 如果页缓冲区满了，立即写入 */
        if (writer->page_buffer_offset >= 256) {
            ret = stream_writer_flush_page_buffer(writer);
            if (ret < 0) {
                return ret;
            }
            writer->page_buffer_offset = 0;
        }
    }
    
    return written;
}

/**
 * @brief 刷新所有待写入数据
 * @param writer 写入器结构体指针
 * @return 0-成功，<0-失败
 */
int stream_writer_flush(stream_writer_t *writer)
{
    if (!writer || !writer->initialized) {
        return -1;
    }
    
    /* 刷新页缓冲区 */
    return stream_writer_flush_page_buffer(writer);
}

/**
 * @brief 反初始化流式写入器
 * @param writer 写入器结构体指针
 */
void stream_writer_deinit(stream_writer_t *writer)
{
    if (writer) {
        /* 尝试刷新剩余数据 */
        stream_writer_flush(writer);
        
        /* 清除初始化标志 */
        writer->initialized = false;
    }
}

/**
 * @brief 获取已写入的数据大小
 * @param writer 写入器结构体指针
 * @return 已写入的字节数
 */
uint32_t stream_writer_get_written_size(stream_writer_t *writer)
{
    if (!writer || !writer->initialized) {
        return 0;
    }
    return writer->total_written;
}

/**
 * @brief 获取CRC16校验值
 * @param writer 写入器结构体指针
 * @return CRC16值
 */
uint16_t stream_writer_get_crc16(stream_writer_t *writer)
{
    if (!writer || !writer->initialized) {
        return 0;
    }
    return writer->crc16;
}

/* ===================== XMODEM接收实现 ===================== */

/**
 * @brief XMODEM协议接收文件并流式写入Flash
 * @param partition 目标分区
 * @param use_1k 是否使用1K模式
 * @return 接收的字节数，<0-失败
 */
int xmodem_receive(const struct flash_partition *partition, data_block_select_t use_1k)
{
    int ret;
    uint8_t packet_buffer[1024 + 5];  /* 最大包大小 + 头部和CRC */
    uint8_t packet_num = 1;
    uint8_t packet_num_comp;
    uint32_t retry_count;
    uint32_t error_count = 0;
    uint16_t packet_size = (use_1k == USE_1K) ? 1024 : 128;
    bool use_crc = true;
    uint8_t ch;
    stream_writer_t writer;
    
    /* 初始化流式写入器 */
    ret = stream_writer_init(&writer, partition);
    if (ret < 0) {
        log_f("Failed to init stream writer");
        return ret;
    }
    
    log_i("Start XMODEM transfer");
    log_f("Target partition: %s", partition->name);
    log_f("Mode: %s", use_1k == USE_1K ? "XMODEM-1K" : "XMODEM-128");
    
    /* 清空串口缓冲区 */
    flush_uart();
    
    /* 发送初始字符开始传输 */
    retry_count = 0;
    while(retry_count < 60)
    {
        if(use_crc)
        {
            uart_putc('C');  /* 请求CRC模式 */
        }
        else
        {
            uart_putc(NAK);  /* 请求Checksum模式 */
        }
        
        /* 等待响应 */
        if(uart_getc_timeout(&ch, 1000))
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
                log_i("XMODEM: No data received");
                stream_writer_deinit(&writer);
                return 0;
            }
            else if(ch == CAN)
            {
                /* 发送方取消 */
                log_i("XMODEM: Transfer cancelled by sender");
                stream_writer_deinit(&writer);
                return -1;
            }
        }
        
        retry_count++;
        
        /* 3次后切换到Checksum模式 */
        if(retry_count == 3 && use_crc)
        {
            use_crc = false;
            log_i("XMODEM: Switching to checksum mode");
        }
    }
    
    log_i("XMODEM: Timeout waiting for sender");
    stream_writer_deinit(&writer);
    return -1;

start_transfer:
    /* 主接收循环 */
    while(1)
    {
        uint8_t header = packet_buffer[0];
        
        /* 处理不同的控制字符 */
        if(header == SOH || header == STX)
        {
            /* 确定包大小 */
            packet_size = (header == SOH) ? 128 : 1024;
            
            /* 读取包序号和反码 */
            if(!uart_getc_timeout(&packet_buffer[1], 1000) ||
               !uart_getc_timeout(&packet_buffer[2], 1000))
            {
                log_i("XMODEM: Timeout reading packet header");
                uart_putc(NAK);
                error_count++;
                if(error_count > MAX_RETRIES)
                {
                    log_i("XMODEM: Too many errors");
                    stream_writer_deinit(&writer);
                    return -1;
                }
                continue;
            }
            
            /* 读取数据 */
            for(int i = 0; i < packet_size; i++)
            {
                if(!uart_getc_timeout(&packet_buffer[3 + i], 1000))
                {
                    log_i("XMODEM: Timeout reading data");
                    uart_putc(NAK);
                    error_count++;
                    if(error_count > MAX_RETRIES)
                    {
                        stream_writer_deinit(&writer);
                        return -1;
                    }
                    goto wait_next_packet;
                }
            }
            
            /* 读取校验 */
            if(use_crc)
            {
                /* CRC16 - 2字节 */
                if(!uart_getc_timeout(&packet_buffer[3 + packet_size], 1000) ||
                   !uart_getc_timeout(&packet_buffer[4 + packet_size], 1000))
                {
                    log_i("XMODEM: Timeout reading CRC");
                    uart_putc(NAK);
                    error_count++;
                    continue;
                }
            }
            else
            {
                /* Checksum - 1字节 */
                if(!uart_getc_timeout(&packet_buffer[3 + packet_size], 1000))
                {
                    log_i("XMODEM: Timeout reading checksum");
                    uart_putc(NAK);
                    error_count++;
                    continue;
                }
            }
            
            /* 验证包序号 */
            packet_num_comp = packet_buffer[1];
            if(packet_buffer[2] != (uint8_t)(~packet_num_comp))
            {
                log_i("XMODEM: Packet number error");
                uart_putc(NAK);
                error_count++;
                continue;
            }
            
            /* 检查包序号 */
            if(packet_num_comp == packet_num)
            {
                /* 正确的包序号，验证数据 */
                bool data_valid = false;
                
                if(use_crc)
                {
                    /* 验证CRC16 */
                    uint16_t crc_received = (packet_buffer[3 + packet_size] << 8) | 
                                           packet_buffer[4 + packet_size];
                    uint16_t crc_calc = crc16_ccitt(&packet_buffer[3], packet_size, 0);
                    data_valid = (crc_received == crc_calc);
                }
                else
                {
                    /* 验证Checksum */
                    uint8_t checksum_received = packet_buffer[3 + packet_size];
                    uint8_t checksum_calc = calc_checksum(&packet_buffer[3], packet_size);
                    data_valid = (checksum_received == checksum_calc);
                }
                
                if(data_valid)
                {
                    /* 数据有效，写入Flash */
                    ret = stream_writer_write(&writer, &packet_buffer[3], packet_size);
                    if(ret < 0)
                    {
                        log_f("XMODEM: Flash write error");
                        uart_putc(CAN);
                        uart_putc(CAN);
                        stream_writer_deinit(&writer);
                        return -1;
                    }
                    
                    /* 发送ACK */
                    uart_putc(ACK);
                    packet_num++;
                    error_count = 0;
                    
                    /* 显示进度 */
                    if((packet_num & 0x0F) == 0)
                    {
                        log_f("Received %d KB", stream_writer_get_written_size(&writer) / 1024);
                    }
                }
                else
                {
                    /* 数据校验失败 */
                    log_i("XMODEM: Data validation failed");
                    uart_putc(NAK);
                    error_count++;
                }
            }
            else if(packet_num_comp == (uint8_t)(packet_num - 1))
            {
                /* 重复的包，直接ACK */
                uart_putc(ACK);
            }
            else
            {
                /* 包序号错误 */
                log_f("XMODEM: Wrong packet number, expected %d, got %d", 
                      packet_num, packet_num_comp);
                uart_putc(NAK);
                error_count++;
            }
        }
        else if(header == EOT)
        {
            /* 传输结束 */
            uart_putc(ACK);
            
            /* 刷新剩余数据 */
            stream_writer_flush(&writer);
            
            uint32_t total_size = stream_writer_get_written_size(&writer);
            uint16_t crc = stream_writer_get_crc16(&writer);
            
            log_i("");
            log_f("XMODEM: Transfer complete");
            log_f("Received: %d bytes", total_size);
            log_f("CRC16: 0x%04X", crc);
            
            stream_writer_deinit(&writer);
            return total_size;
        }
        else if(header == CAN)
        {
            /* 接收到取消 */
            if(uart_getc_timeout(&ch, 1000) && ch == CAN)
            {
                log_i("XMODEM: Transfer cancelled by sender");
                stream_writer_deinit(&writer);
                return -1;
            }
        }
        else
        {
            /* 未知字符，忽略 */
        }

wait_next_packet:
        /* 等待下一个包 */
        if(!uart_getc_timeout(&packet_buffer[0], 10000))
        {
            log_i("XMODEM: Timeout waiting for next packet");
            stream_writer_deinit(&writer);
            return -1;
        }
        
        /* 检查错误次数 */
        if(error_count > MAX_RETRIES)
        {
            log_i("XMODEM: Too many errors, aborting");
            uart_putc(CAN);
            uart_putc(CAN);
            stream_writer_deinit(&writer);
            return -1;
        }
    }
}