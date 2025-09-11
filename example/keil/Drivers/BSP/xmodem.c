#include "xmodem.h"
#include "main.h"

static void uart_putc(uint8_t ch)
{
    uart_write(DEV_UART1, &ch, 1);
    uart_poll_dma_tx(DEV_UART1);
}

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



int xmodem_receive(const struct flash_partition *partition, data_block_select_t use_1k)
{
//	int ret;
//	// 最大包大小 + 头部和CRC
//	uint8_t packet_buffer[1024 + 5];
//	uint8_t packet_num = 1;
//	uint8_t packet_num_comp;
//	uint32_t total_received = 0;
//	uint32_t retry_count;
//	uint32_t error_count = 0;
//	uint16_t packet_size = use_1k;
//	bool use_crc = true;
//	uint8_t ch;	
//	stream_writer_t writer;
//	
//	ret = flash_partition_erase_all(partition);
//	
//	if(ret)
//		log_f("Erase (%s) partition finish!", DOWNLOAD);	
//	log_i("Start XMODEM transfer\r\n");
//	
//    /* 清空串口缓冲区 */
//    flush_uart();	
//	
//	log_f("XMODEM: Ready to receive, using %s mode", use_1k ? "XMODEM-1K" : "XMODEM-128");
//	
//	/* 发送初始字符开始传输 */
//	retry_count = 0;
//	while(retry_count < 60)
//	{		
//		if(use_crc)
//		{
//			uart_putc('C');		//请求CRC模式
//		}
//        else
//        {
//            uart_putc(NAK);  // 请求Checksum模式
//        }	

//		/* 等待响应 */
//		uint32_t wait_start = HAL_GetTick();
//		while((HAL_GetTick() - wait_start) < 1000)
//        {
//            if(uart_getc_timeout(&ch, 10))
//            {
//                if(ch == SOH || ch == STX)
//                {
//                    /* 收到数据包开始 */
//                    packet_buffer[0] = ch;
//                    goto start_transfer;
//                }
//                else if(ch == EOT)
//                {
//                    /* 没有数据传输 */
//                    uart_putc(ACK);
//                    log_i("XMODEM: No data received");
//                    return 0;
//                }
//                else if(ch == CAN)
//                {
//                    /* 发送方取消 */
//                    log_i("XMODEM: Transfer cancelled by sender");
//                    return -1;
//                }
//                /* 忽略其他字符，继续等待 */
//            }
//        }
//        
//        retry_count++;

//        /* 3次后切换到Checksum模式（兼容更多终端） */
//        if(retry_count == 3 && use_crc)
//        {
//            use_crc = false;
//            log_i("XMODEM: Switching to checksum mode for compatibility");
//        }
//        /* 打印进度 */
//        if(use_crc)
//        {
//            log_i("C");
//        }
//        else
//        {
//            log_i(".");
//        }				
//	}
//    log_i("XMODEM: Timeout waiting for sender\r\n");
//    return -1;	

//start_transfer:
//    /* 主接收循环 */
//	while(1)
//	{
//		uint8_t header = packet_buffer[0];
//		
//		/* 已经有第一个字节，读取剩余的包 */
//		if(header == EOT)
//        {
//            /* 传输结束 */
//            uart_putc(ACK);
//            stream_writer_flush(&writer);
//            bootloader_print("\r\nXMODEM: Transfer complete, received ");
//            bootloader_print_dec(total_received);
//            bootloader_print(" bytes\r\n");
//            return total_received;
//        }
//        else if(header == CAN)
//        {
//            /* 接收到取消 */
//            if(uart_getc_timeout(&ch, 1000) && ch == CAN)
//            {
//                bootloader_print("\r\nXMODEM: Transfer cancelled by sender\r\n");
//                return -1;
//            }
//        }
//	}
}













