#include <string.h>
#include "bootloader.h"
#include "bootloader_cmd.h"
#include "gpio.h"
#include "dma.h"
#include "spi.h"
#include "usart.h"
#include "dev_usart.h"
#include "dev_flash.h"
#include "config.h"
#include "bsp_key.h"
#include "bsp_led.h"

static bootloader_state_t bootloader_state = BOOT_STATE_IDLE;
static uint8_t cmd_index = 0;
static char cmd_buffer[128];





/* 初始化基本的外设 */
static void base_device_init(void)
{
	led_init();                         /* 初始化LED */
	key_init();                         /* 初始化按键 */
	MX_GPIO_Init();
	MX_DMA_Init();
	MX_SPI2_Init();
	MX_USART1_UART_Init();
	MX_USART2_UART_Init();   

	uart_device_init(DEV_UART1);
	uart_device_init(DEV_UART2);
	/* 等待串口稳定 */
	HAL_Delay(100);
	
	flash_init();
}

void bootloader_init(void)
{
	/* 初始化基本的外设 */
	base_device_init();
	
	/* 初始化基本配置 */
	
}


int bootloader_check_entry(uint32_t timeout_ms)
{
	uint8_t value; 
	uint32_t start_tick = HAL_GetTick();
	log_f("======= STM32 Bootloader %s =======", BOOTLOADER_VERSIOIN);
	log_f("Press any key within %d seconds....", timeout_ms/1000);	
	
	/* 等待超时或用户输入 */
	while((HAL_GetTick() - start_tick) < timeout_ms)
	{
		/* 检查按键 */
		value = key_scan(0);
		if(value > 0)
		{
			print_cmd("Button detected!\r\n");
			return 0;
		}
		
		/* 检查串口输入 */
		if(uart_read(DEV_UART1, &value, 1) > 0)
		{
			print_cmd("UART input detected!\r\n");
			return 0;
		}
		
		HAL_Delay(10);
	}
	
	print_cmd("Starting app ...\r\n");
	return -1;
}




/* Bootloader命令模式主循环 */
void bootloader_cmd_mode(void)
{
    bootloader_state = BOOT_STATE_CMD_MODE;
    print_cmd("Bootloader Command Mode\r\nType 'h' for help\r\n");
    
    // 定义提示符
    const char prompt[] = "> ";
    
    // 显示初始提示符
    print_cmd(prompt);
    
    // 初始化命令缓冲区
    cmd_index = 0;
    cmd_buffer[0] = '\0';
    
    uint8_t ch;
    while(bootloader_state == BOOT_STATE_CMD_MODE)
    {
        if(uart_read(DEV_UART1, &ch, 1) > 0)
        {
            // 处理回车或换行
            if(ch == '\r' || ch == '\n')
            {
                // 只在有命令时处理
                if(cmd_index > 0)
                {
                    cmd_buffer[cmd_index] = '\0';
                    print_cmd("\r\n");
                    process_cmd(cmd_buffer);
                    cmd_index = 0;
                    cmd_buffer[0] = '\0';
                }
                else
                {
                    print_cmd("\r\n");
                }
                
                // 如果仍在命令模式，显示新提示符
                if(bootloader_state == BOOT_STATE_CMD_MODE)
                {
                    print_cmd(prompt);
                }
            }
            // 处理退格或删除键
            else if(ch == '\b' || ch == 0x7F)
            {
                // 只有当有字符可删除时才处理退格
                if(cmd_index > 0)
                {
                    cmd_index--;
                    // 发送退格序列：退格、空格、再退格
                    uart_write(DEV_UART1, "\b \b", 3);
                }
                // 如果已经在提示符位置，忽略退格键
                // 不执行任何操作，防止回退到提示符之前
            }
            // 处理普通字符输入
            else if(cmd_index < (sizeof(cmd_buffer) - 1))
            {
                // 存储字符并回显
                cmd_buffer[cmd_index++] = ch;
                uart_write(DEV_UART1, &ch, 1);
            }
            
            // 确保数据发送
            uart_poll_dma_tx(DEV_UART1);
        }
        
        // 保持轮询DMA传输
        uart_poll_dma_tx(DEV_UART1);
    }
}

void bootloader_jump_to_app(void)
{
	
}

