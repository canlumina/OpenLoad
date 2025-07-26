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
    HAL_Init();                         /* 初始化HAL库 */
    sys_stm32_clock_init(RCC_PLL_MUL9); /* 设置时钟, 72Mhz */
    delay_init(72);                     /* 延时初始化 */
    usart_init(115200);                 /* 串口初始化为115200 */
    led_init();                         /* 初始化LED */
    key_init();                         /* 初始化按键 */
    W25QXX_Init();                      // W25QXX初始化();

    while(1)
    {

    }

    return 0;
}
