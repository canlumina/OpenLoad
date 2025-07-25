#include "boot.h"
#include "bsp_iap.h"
#include "bsp_key.h"
#include "bsp_norflash.h"
#include "delay.h"
#include "string.h"
#include "usart.h"
static uint32_t size = 0;
void show_usage(void)
{
    printf("Bootloader V1.0\r\n");
    printf("Usage:bootloader\r\n");
    printf("Press any key to start...\r\n");
}

// 请求更新固件，等待固件下载完成
static void update_firmware(void)
{
    uint32_t oldcount = 0; /* 老的串口接收数据值 */
    uint32_t applenth = 0; /* 接收到的app代码长度 */
    printf("请发送固件...\r\n");
    while (1)
    {
        if (oldcount == g_usart_rx_cnt) /* 新周期内,没有收到任何数据,认为本次数据接收完成 */
        {
            applenth = g_usart_rx_cnt;
            size = g_usart_rx_cnt;
            oldcount = 0;
            g_usart_rx_cnt = 0;
            W25QXX_Write(g_usart_rx_buf, STM32_NEW_FLASH_BASE, applenth);
            printf("User program reception completed!\r\n");
            printf("The reception length:%dBytes\r\n", applenth);
            return;
        }
        else
            oldcount = g_usart_rx_cnt;
        // TODO: OTG协议下载固件
    }
}

static void update_app(void)
{
    // 从flash中读取固件到缓存中
    memset(g_usart_rx_buf, 0, sizeof(g_usart_rx_buf));
    W25QXX_Read(g_usart_rx_buf, STM32_NEW_FLASH_BASE, size);
    iap_write_appbin(FLASH_APP1_ADDR, g_usart_rx_buf, size);
    iap_load_app(FLASH_APP1_ADDR);
}

void boot_main(void)
{
    // 打印bootloader启动信息、以及bootloader帮助信息
    show_usage();

    // 定时器定时3秒，在此时间内没有按键按下，则跳转到应用程序

    while (1)
    {
        switch (key_scan(0))
        {
        case KEY0_PRES: // 更新固件
            update_firmware();

            break;
        case KEY1_PRES: // 运行固件
                        // iap_load_app(FLASH_APP1_ADDR);
            printf("key2 press\r\n");
            update_app();
            break;
        case KEY2_PRES: // 检查更新
            break;
        case WKUP_PRES: // 恢复默认固件
            break;
        default:
            break;
        }
        delay_ms(100);
    }
}