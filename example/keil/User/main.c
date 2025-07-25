#include "bsp_iap.h"
#include "bsp_key.h"
#include "bsp_led.h"
#include "bsp_norflash.h"
#include "bsp_stmflash.h"
#include "delay.h"
#include "sys.h"
#include "usart.h"
#include "boot.h"

int main(void)
{
    uint8_t t;
    uint8_t key;
    uint32_t oldcount = 0; /* 老的串口接收数据值 */
    uint32_t applenth = 0; /* 接收到的app代码长度 */
    uint8_t clearflag = 0;
    uint16_t id = 0;

    HAL_Init();                         /* 初始化HAL库 */
    sys_stm32_clock_init(RCC_PLL_MUL9); /* 设置时钟, 72Mhz */
    delay_init(72);                     /* 延时初始化 */
    usart_init(115200);                 /* 串口初始化为115200 */
    led_init();                         /* 初始化LED */
    key_init();                         /* 初始化按键 */
    W25QXX_Init();                      // W25QXX初始化();

    boot_main();

    // while (1)
    // {
    //     id = W25QXX_ReadID();
	// 	printf("id is %X\r\n", id);
    //     if (id == W25Q64 || id == NM25Q128)
    //         break;
    //     printf("W25Q64 Check Failed!");
    //     delay_ms(500);
    // }

    // while (1)
    // {
    //     if (g_usart_rx_cnt)
    //     {
    //         if (oldcount == g_usart_rx_cnt) /* 新周期内,没有收到任何数据,认为本次数据接收完成 */
    //         {
    //             applenth = g_usart_rx_cnt;
    //             oldcount = 0;
    //             g_usart_rx_cnt = 0;
    //             printf("User program reception completed!\r\n");
    //             printf("The reception length:%dBytes\r\n", applenth);
    //         }
    //         else
    //             oldcount = g_usart_rx_cnt;
    //     }

    //     t++;
    //     delay_ms(100);

    //     if (t == 3)
    //     {
    //         LED0_TOGGLE();
    //         t = 0;

    //         if (clearflag)
    //         {
    //             clearflag--;
    //         }
    //     }

    //     key = key_scan(0);

    //     if (key == WKUP_PRES) /* WKUP按下,更新固件到FLASH */
    //     {
    //         if (applenth)
    //         {
    //             printf("Starting firmware update...\r\n");
    //             if (((*(volatile uint32_t *)(0X20001000 + 4)) & 0xFF000000) == 0x08000000) /* 判断是否为0X08XXXXXX */
    //             {
    //                 iap_write_appbin(FLASH_APP1_ADDR, g_usart_rx_buf, applenth); /* 更新FLASH代码 */
    //                 printf("The firmware update is complete!\r\n");
    //             }
    //             else
    //             {
    //                 printf("This is not a firmware!\r\n");
    //             }
    //         }
    //         else
    //             printf("No firmware to update!\r\n");

    //         clearflag = 7; /* 标志更新了显示,并且设置7*300ms后清除显示 */
    //     }

    //     if (key == KEY1_PRES) /* KEY1按键按下, 运行FLASH APP代码 */
    //     {
    //         if (((*(volatile uint32_t *)(FLASH_APP1_ADDR + 4)) & 0xFF000000) ==
    //             0x08000000) /* 判断FLASH里面是否有APP,有的话执行 */
    //         {
    //             printf("Starting FLASH APP!!\r\n\r\n");
    //             delay_ms(10);
    //             iap_load_app(FLASH_APP1_ADDR); /* 执行FLASH APP代码 */
    //         }
    //         else
    //         {
    //             printf("No FLASH APP!\r\n");
    //         }

    //         clearflag = 7; /* 标志更新了显示,并且设置7*300ms后清除显示 */
    //     }

    //     if (key == KEY0_PRES) /* KEY0按下 */
    //     {
    //         printf("Starting SRAM APP!!\r\n\r\n");
    //         delay_ms(10);

    //         if (((*(volatile uint32_t *)(0x20001000 + 4)) & 0xFF000000) == 0x20000000) /* 判断是否为0X20XXXXXX */
    //         {
    //             iap_load_app(0x20001000); /* SRAM地址 */
    //         }
    //         else
    //         {
    //             printf("This is not a SRAM APP!\r\n");
    //         }

    //         clearflag = 7; /* 标志更新了显示,并且设置7*300ms后清除显示 */
    //     }
    // }
}
