#include <string.h>
#include "gpio.h"
#include "dma.h"
#include "spi.h"
#include "usart.h"
#include "dev_usart.h"
#include "dev_flash.h"

#include "bsp_key.h"
#include "bsp_led.h"

#define BUF_SIZE 512
static int fal_test(const char *partiton_name);

void SystemClock_Config(void);

int main(void)
{
	HAL_Init();                         /* 初始化HAL库 */
	SystemClock_Config(); 				/* 设置时钟, 72Mhz */

	led_init();                         /* 初始化LED */
	key_init();                         /* 初始化按键 */
	MX_GPIO_Init();
	MX_DMA_Init();
	MX_SPI2_Init();
	MX_USART1_UART_Init();
	MX_USART2_UART_Init();   

	uart_device_init(DEV_UART1);
	uart_device_init(DEV_UART2);
	flash_init();
	/* 等待串口稳定 */
	HAL_Delay(100);
	if (fal_test("env") == 0)
    {
        uart_printf("Fal partition (%s) test success!", "env");
    }
    else
    {
        uart_printf("Fal partition (%s) test failed!", "env");
    }	
	
	while(1)
	{

		HAL_Delay(1000);
	}
	
	return 0;
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	/** Initializes the RCC Oscillators according to the specified parameters
	* in the RCC_OscInitTypeDef structure.
	*/
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	*/
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
	{
		Error_Handler();
	}
}



/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}



static int fal_test(const char *partiton_name)
{
    int ret;
    int i, j, len;
    uint8_t buf[BUF_SIZE];
    const struct flash_dev *flash_dev = NULL;
    const struct flash_partition *partition = NULL;

    if (!partiton_name)
    {
        uart_printf("Input param partition name is null!\r\n");
        return -1;
    }

    partition = flash_partition_find(partiton_name);
    if (partition == NULL)
    {
        uart_printf("Find partition (%s) failed!\r\n", partiton_name);
        ret = -1;
        return ret;
    }

    flash_dev = flash_device_find(partition->flash_name);
    if (flash_dev == NULL)
    {
        uart_printf("Find flash device (%s) failed!\r\n", partition->flash_name);
        ret = -1;
        return ret;
    }

    uart_printf("Flash device : %s   "
          "Flash size : %dK   "
          "Partition : %s   "
          "Partition size: %dK\r\n",
          partition->flash_name,
          flash_dev->len / 1024,
          partition->name,
          partition->len / 1024);

    /* 擦除 `partition` 分区上的全部数据 */
    ret = flash_partition_erase_all(partition);
    if (ret < 0)
    {
        uart_printf("Partition (%s) erase failed!\r\n", partition->name);
        ret = -1;
        return ret;
    }
    uart_printf("Erase (%s) partition finish!\r\n", partiton_name);

    /* 循环读取整个分区的数据，并对内容进行检验 */
    for (i = 0; i < partition->len;)
    {
        memset(buf, 0x00, BUF_SIZE);
        len = (partition->len - i) > BUF_SIZE ? BUF_SIZE : (partition->len - i);

        /* 从 Flash 读取 len 长度的数据到 buf 缓冲区 */
        ret = flash_partition_read(partition, i, buf, len);
        if (ret < 0)
        {
            uart_printf("Partition (%s) read failed!\r\n", partition->name);
            ret = -1;
            return ret;
        }
        for (j = 0; j < len; j++)
        {
            /* 校验数据内容是否为 0xFF */
            if (buf[j] != 0xFF)
            {
                uart_printf("The erase operation did not really succeed!\r\n");
                ret = -1;
                return ret;
            }
        }
        i += len;
    }

    /* 把 0 写入指定分区 */
    for (i = 0; i < partition->len;)
    {
        /* 设置写入的数据 0x00 */
        memset(buf, 0x00, BUF_SIZE);
        len = (partition->len - i) > BUF_SIZE ? BUF_SIZE : (partition->len - i);

        /* 写入数据 */
        ret = flash_partition_write(partition, i, buf, len);
        if (ret < 0)
        {
            uart_printf("Partition (%s) write failed!\r\n", partition->name);
            ret = -1;
            return ret;
        }
        i += len;
    }
    uart_printf("Write (%s) partition finish! Write size %d(%dK).\r\n", partiton_name, i, i / 1024);

    /* 从指定的分区读取数据并校验数据 */
    for (i = 0; i < partition->len;)
    {
        /* 清空读缓冲区，以 0xFF 填充 */
        memset(buf, 0xFF, BUF_SIZE);
        len = (partition->len - i) > BUF_SIZE ? BUF_SIZE : (partition->len - i);

        /* 读取数据到 buf 缓冲区 */
        ret = flash_partition_read(partition, i, buf, len);
        if (ret < 0)
        {
            uart_printf("Partition (%s) read failed!\r\n", partition->name);
            ret = -1;
            return ret;
        }
        for (j = 0; j < len; j++)
        {
            /* 校验读取的数据是否为步骤 3 中写入的数据 0x00 */
            if (buf[j] != 0x00)
            {
                uart_printf("The write operation did not really succeed!\r\n");
                ret = -1;
                return ret;
            }
        }
        i += len;
    }

    ret = 0;
    return ret;
}
