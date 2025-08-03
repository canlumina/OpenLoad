#include "stm32f1xx_hal.h"
#include "usart.h"
#include "stdio.h"

/******************************************************************************************/
/* 加入以下代码, 支持printf函数, 而不需要选择use MicroLIB */

#if 1

#if (__ARMCC_VERSION >= 6010050)           /* 使用AC6编译器时 */
__asm(".global __use_no_semihosting\n\t"); /* 声明不使用半主机模式 */
__asm(".global __ARM_use_no_argv \n\t"); /* AC6下需要声明main函数为无参数格式，否则部分例程可能出现半主机模式 */

#else
/* 使用AC5编译器时, 要在这里定义__FILE 和 不使用半主机模式 */
#pragma import(__use_no_semihosting)

struct __FILE {
    int handle;
    /* Whatever you require here. If the only file you are using is */
    /* standard output using printf() for debugging, no file handling */
    /* is required. */
};

#endif

/* 不使用半主机模式，至少需要重定义_ttywrch\_sys_exit\_sys_command_string函数,以同时兼容AC6和AC5模式 */
int _ttywrch(int ch)
{
    ch = ch;
    return ch;
}

/* 定义_sys_exit()以避免使用半主机模式 */
void _sys_exit(int x)
{
    x = x;
}

char *_sys_command_string(char *cmd, int len)
{
    return NULL;
}

/* FILE 在 stdio.h里面定义. */
FILE __stdout;

/* MDK下需要重定义fputc函数, printf函数最终会通过调用fputc输出字符串到串口 */
int fputc(int ch, FILE *f)
{
    while ((USART_UX->SR & 0X40) == 0)
        ; /* 等待上一个字符发送完成 */

    USART_UX->DR = (uint8_t)ch; /* 将要发送的字符 ch 写入到DR寄存器 */
    return ch;
}
#endif


UCB uart1;
//UCB uart2;
//UCB uart3;

uint8_t U1_RxBuff[U1_RX_SIZE];
uint8_t U1_TxBuff[U1_TX_SIZE];

//uint8_t U2_RxBuff[U2_RX_SIZE];
//uint8_t U2_TxBuff[U2_TX_SIZE];

//uint8_t U3_RxBuff[U3_RX_SIZE];
//uint8_t U3_TxBuff[U3_TX_SIZE];

void u1_init(uint32_t bandrate)
{
	uart1.uart.Instance = USART1;
	uart1.uart.Init.BaudRate = bandrate;
	uart1.uart.Init.WordLength = UART_WORDLENGTH_8B;
	uart1.uart.Init.StopBits = UART_STOPBITS_1;
	uart1.uart.Init.Parity = UART_PARITY_NONE;
	uart1.uart.Init.Mode = UART_MODE_TX_RX;
	uart1.uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	HAL_UART_Init(&uart1.uart);
	u1_ptrinit();
}
void u1_ptrinit(void)
{
	uart1.RxInPtr = &uart1.RxLocation[0];
	uart1.RxOutPtr = &uart1.RxLocation[0];
	uart1.RxEndPtr = &uart1.RxLocation[9];
	uart1.RxCounter = 0;
	uart1.RxInPtr->start = U1_RxBuff;

	uart1.TxInPtr = &uart1.TxLocation[0];
	uart1.TxOutPtr = &uart1.TxLocation[0];
	uart1.TxEndPtr = &uart1.TxLocation[9];
	uart1.TxCounter = 0;
	uart1.TxInPtr->start = U1_TxBuff;

	__HAL_UART_ENABLE_IT(&uart1.uart, UART_IT_IDLE);
	HAL_UART_Receive_DMA(&uart1.uart, uart1.RxInPtr->start, U1_RX_MAX);
}
void u1_txdata(uint8_t *data, uint32_t data_len)
{
	if ((U1_TX_SIZE - uart1.TxCounter) >= data_len)
	{
		uart1.TxInPtr->start = &U1_TxBuff[uart1.TxCounter];
	}
	else
	{
		uart1.TxCounter = 0;
		uart1.TxInPtr->start = U1_TxBuff;
	}
	memcpy(uart1.TxInPtr->start, data, data_len);
	uart1.TxCounter += data_len;
	uart1.TxInPtr->end = &U1_TxBuff[uart1.TxCounter - 1];
	uart1.TxInPtr++;
	if (uart1.TxInPtr == uart1.TxEndPtr)
	{
		uart1.TxInPtr = &uart1.TxLocation[0];
	}
}
void u1_printf(char *fmt, ...)
{
	uint8_t tempbuff[256];
	uint16_t i;
	va_list ap;
	va_start(ap, fmt);
	vsprintf((char *)tempbuff, fmt, ap);
	va_end(ap);

	for (i = 0; i < strlen((char *)tempbuff); i++)
	{
		while (!__HAL_UART_GET_FLAG(&uart1.uart, UART_FLAG_TXE))
			;
		uart1.uart.Instance->DR = tempbuff[i];
	}
	while (!__HAL_UART_GET_FLAG(&uart1.uart, UART_FLAG_TC))
		;
}
//void u2_init(uint32_t bandrate)
//{
//	uart2.uart.Instance = USART2;
//	uart2.uart.Init.BaudRate = bandrate;
//	uart2.uart.Init.WordLength = UART_WORDLENGTH_8B;
//	uart2.uart.Init.StopBits = UART_STOPBITS_1;
//	uart2.uart.Init.Parity = UART_PARITY_NONE;
//	uart2.uart.Init.Mode = UART_MODE_TX_RX;
//	uart2.uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
//	HAL_UART_Init(&uart2.uart);
//	u2_ptrinit();
//}
//void u2_ptrinit(void)
//{
//	uart2.RxInPtr = &uart2.RxLocation[0];
//	uart2.RxOutPtr = &uart2.RxLocation[0];
//	uart2.RxEndPtr = &uart2.RxLocation[9];
//	uart2.RxCounter = 0;
//	uart2.RxInPtr->start = U2_RxBuff;

//	uart2.TxInPtr = &uart2.TxLocation[0];
//	uart2.TxOutPtr = &uart2.TxLocation[0];
//	uart2.TxEndPtr = &uart2.TxLocation[9];
//	uart2.TxCounter = 0;
//	uart2.TxInPtr->start = U2_TxBuff;

//	__HAL_UART_ENABLE_IT(&uart2.uart, UART_IT_IDLE);
//	HAL_UART_Receive_DMA(&uart2.uart, uart2.RxInPtr->start, U2_RX_MAX);
//}
//void u2_txdata(uint8_t *data, uint32_t data_len)
//{
//	if ((U2_TX_SIZE - uart2.TxCounter) >= data_len)
//	{
//		uart2.TxInPtr->start = &U2_TxBuff[uart2.TxCounter];
//	}
//	else
//	{
//		uart2.TxCounter = 0;
//		uart2.TxInPtr->start = U2_TxBuff;
//	}
//	memcpy(uart2.TxInPtr->start, data, data_len);
//	uart2.TxCounter += data_len;
//	uart2.TxInPtr->end = &U2_TxBuff[uart2.TxCounter - 1];
//	uart2.TxInPtr++;
//	if (uart2.TxInPtr == uart2.TxEndPtr)
//	{
//		uart2.TxInPtr = &uart2.TxLocation[0];
//	}
//}
//void u2_printf(char *fmt, ...)
//{
//	uint8_t tempbuff[256];
//	uint16_t i;
//	va_list ap;
//	va_start(ap, fmt);
//	vsprintf((char *)tempbuff, fmt, ap);
//	va_end(ap);

//	for (i = 0; i < strlen((char *)tempbuff); i++)
//	{
//		while (!__HAL_UART_GET_FLAG(&uart2.uart, UART_FLAG_TXE))
//			;
//		uart2.uart.Instance->DR = tempbuff[i];
//	}
//	while (!__HAL_UART_GET_FLAG(&uart2.uart, UART_FLAG_TC))
//		;
//}
//void u3_init(uint32_t bandrate)
//{
//	uart3.uart.Instance = USART3;
//	uart3.uart.Init.BaudRate = bandrate;
//	uart3.uart.Init.WordLength = UART_WORDLENGTH_8B;
//	uart3.uart.Init.StopBits = UART_STOPBITS_1;
//	uart3.uart.Init.Parity = UART_PARITY_NONE;
//	uart3.uart.Init.Mode = UART_MODE_TX_RX;
//	uart3.uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
//	HAL_UART_Init(&uart3.uart);
//	u3_ptrinit();
//}
//void u3_ptrinit(void)
//{
//	uart3.RxInPtr = &uart3.RxLocation[0];
//	uart3.RxOutPtr = &uart3.RxLocation[0];
//	uart3.RxEndPtr = &uart3.RxLocation[9];
//	uart3.RxCounter = 0;
//	uart3.RxInPtr->start = U3_RxBuff;

//	uart3.TxInPtr = &uart3.TxLocation[0];
//	uart3.TxOutPtr = &uart3.TxLocation[0];
//	uart3.TxEndPtr = &uart3.TxLocation[9];
//	uart3.TxCounter = 0;
//	uart3.TxInPtr->start = U3_TxBuff;

//	__HAL_UART_ENABLE_IT(&uart3.uart, UART_IT_IDLE);
//	HAL_UART_Receive_DMA(&uart3.uart, uart3.RxInPtr->start, U3_RX_MAX);
//}
//void u3_txdata(uint8_t *data, uint32_t data_len)
//{
//	if ((U3_TX_SIZE - uart3.TxCounter) >= data_len)
//	{
//		uart3.TxInPtr->start = &U3_TxBuff[uart3.TxCounter];
//	}
//	else
//	{
//		uart3.TxCounter = 0;
//		uart3.TxInPtr->start = U3_TxBuff;
//	}
//	memcpy(uart3.TxInPtr->start, data, data_len);
//	uart3.TxCounter += data_len;
//	uart3.TxInPtr->end = &U3_TxBuff[uart3.TxCounter - 1];
//	uart3.TxInPtr++;
//	if (uart3.TxInPtr == uart3.TxEndPtr)
//	{
//		uart3.TxInPtr = &uart3.TxLocation[0];
//	}
//}
//void u3_printf(char *fmt, ...)
//{
//	uint8_t tempbuff[256];
//	uint16_t i;
//	va_list ap;
//	va_start(ap, fmt);
//	vsprintf((char *)tempbuff, fmt, ap);
//	va_end(ap);

//	for (i = 0; i < strlen((char *)tempbuff); i++)
//	{
//		while (!__HAL_UART_GET_FLAG(&uart3.uart, UART_FLAG_TXE))
//			;
//		uart3.uart.Instance->DR = tempbuff[i];
//	}
//	while (!__HAL_UART_GET_FLAG(&uart3.uart, UART_FLAG_TC))
//		;
//}
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{

	GPIO_InitTypeDef GPIO_InitType;

	if (huart->Instance == USART1)
	{

		__HAL_RCC_GPIOA_CLK_ENABLE();
		__HAL_RCC_USART1_CLK_ENABLE();
		__HAL_RCC_DMA1_CLK_ENABLE();

		GPIO_InitType.Pin = GPIO_PIN_9;
		GPIO_InitType.Mode = GPIO_MODE_AF_PP;
		GPIO_InitType.Speed = GPIO_SPEED_FREQ_MEDIUM;
		HAL_GPIO_Init(GPIOA, &GPIO_InitType);

		GPIO_InitType.Pin = GPIO_PIN_10;
		GPIO_InitType.Mode = GPIO_MODE_AF_INPUT;
		GPIO_InitType.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(GPIOA, &GPIO_InitType);

		HAL_NVIC_SetPriority(USART1_IRQn, 3, 0);
		HAL_NVIC_EnableIRQ(USART1_IRQn);

		uart1.dmatx.Instance = DMA1_Channel4;
		uart1.dmatx.Init.Direction = DMA_MEMORY_TO_PERIPH;
		uart1.dmatx.Init.PeriphInc = DMA_PINC_DISABLE;
		uart1.dmatx.Init.MemInc = DMA_MINC_ENABLE;
		uart1.dmatx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
		uart1.dmatx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
		uart1.dmatx.Init.Mode = DMA_NORMAL;
		uart1.dmatx.Init.Priority = DMA_PRIORITY_MEDIUM;
		__HAL_LINKDMA(huart, hdmatx, uart1.dmatx);
		HAL_DMA_Init(&uart1.dmatx);

		HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 3, 0);
		HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);

		uart1.dmarx.Instance = DMA1_Channel5;
		uart1.dmarx.Init.Direction = DMA_PERIPH_TO_MEMORY;
		uart1.dmarx.Init.PeriphInc = DMA_PINC_DISABLE;
		uart1.dmarx.Init.MemInc = DMA_MINC_ENABLE;
		uart1.dmarx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
		uart1.dmarx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
		uart1.dmarx.Init.Mode = DMA_NORMAL;
		uart1.dmarx.Init.Priority = DMA_PRIORITY_MEDIUM;
		__HAL_LINKDMA(huart, hdmarx, uart1.dmarx);
		HAL_DMA_Init(&uart1.dmarx);

		HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 3, 0);
		HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
	}
//	else if (huart->Instance == USART2)
//	{
//		__HAL_RCC_GPIOA_CLK_ENABLE();
//		__HAL_RCC_USART2_CLK_ENABLE();
//		__HAL_RCC_DMA1_CLK_ENABLE();

//		GPIO_InitType.Pin = GPIO_PIN_2;
//		GPIO_InitType.Mode = GPIO_MODE_AF_OD;
//		GPIO_InitType.Speed = GPIO_SPEED_FREQ_MEDIUM;
//		HAL_GPIO_Init(GPIOA, &GPIO_InitType);

//		GPIO_InitType.Pin = GPIO_PIN_3;
//		GPIO_InitType.Mode = GPIO_MODE_AF_INPUT;
//		GPIO_InitType.Pull = GPIO_NOPULL;
//		HAL_GPIO_Init(GPIOA, &GPIO_InitType);

//		HAL_NVIC_SetPriority(USART2_IRQn, 3, 0);
//		HAL_NVIC_EnableIRQ(USART2_IRQn);

//		uart2.dmatx.Instance = DMA1_Channel7;
//		uart2.dmatx.Init.Direction = DMA_MEMORY_TO_PERIPH;
//		uart2.dmatx.Init.PeriphInc = DMA_PINC_DISABLE;
//		uart2.dmatx.Init.MemInc = DMA_MINC_ENABLE;
//		uart2.dmatx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
//		uart2.dmatx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
//		uart2.dmatx.Init.Mode = DMA_NORMAL;
//		uart2.dmatx.Init.Priority = DMA_PRIORITY_MEDIUM;
//		__HAL_LINKDMA(huart, hdmatx, uart2.dmatx);
//		HAL_DMA_Init(&uart2.dmatx);

//		HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 3, 0);
//		HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);

//		uart2.dmarx.Instance = DMA1_Channel6;
//		uart2.dmarx.Init.Direction = DMA_PERIPH_TO_MEMORY;
//		uart2.dmarx.Init.PeriphInc = DMA_PINC_DISABLE;
//		uart2.dmarx.Init.MemInc = DMA_MINC_ENABLE;
//		uart2.dmarx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
//		uart2.dmarx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
//		uart2.dmarx.Init.Mode = DMA_NORMAL;
//		uart2.dmarx.Init.Priority = DMA_PRIORITY_MEDIUM;
//		__HAL_LINKDMA(huart, hdmarx, uart2.dmarx);
//		HAL_DMA_Init(&uart2.dmarx);

//		HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 3, 0);
//		HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
//	}
//	else if (huart->Instance == USART3)
//	{
//		__HAL_RCC_GPIOB_CLK_ENABLE();
//		__HAL_RCC_USART3_CLK_ENABLE();
//		__HAL_RCC_DMA1_CLK_ENABLE();

//		GPIO_InitType.Pin = GPIO_PIN_10;
//		GPIO_InitType.Mode = GPIO_MODE_AF_OD;
//		GPIO_InitType.Speed = GPIO_SPEED_FREQ_MEDIUM;
//		HAL_GPIO_Init(GPIOB, &GPIO_InitType);

//		GPIO_InitType.Pin = GPIO_PIN_11;
//		GPIO_InitType.Mode = GPIO_MODE_AF_INPUT;
//		GPIO_InitType.Pull = GPIO_NOPULL;
//		HAL_GPIO_Init(GPIOB, &GPIO_InitType);

//		HAL_NVIC_SetPriority(USART3_IRQn, 3, 0);
//		HAL_NVIC_EnableIRQ(USART3_IRQn);

//		uart3.dmatx.Instance = DMA1_Channel2;
//		uart3.dmatx.Init.Direction = DMA_MEMORY_TO_PERIPH;
//		uart3.dmatx.Init.PeriphInc = DMA_PINC_DISABLE;
//		uart3.dmatx.Init.MemInc = DMA_MINC_ENABLE;
//		uart3.dmatx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
//		uart3.dmatx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
//		uart3.dmatx.Init.Mode = DMA_NORMAL;
//		uart3.dmatx.Init.Priority = DMA_PRIORITY_MEDIUM;
//		__HAL_LINKDMA(huart, hdmatx, uart3.dmatx);
//		HAL_DMA_Init(&uart3.dmatx);

//		HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 3, 0);
//		HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);

//		uart3.dmarx.Instance = DMA1_Channel3;
//		uart3.dmarx.Init.Direction = DMA_PERIPH_TO_MEMORY;
//		uart3.dmarx.Init.PeriphInc = DMA_PINC_DISABLE;
//		uart3.dmarx.Init.MemInc = DMA_MINC_ENABLE;
//		uart3.dmarx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
//		uart3.dmarx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
//		uart3.dmarx.Init.Mode = DMA_NORMAL;
//		uart3.dmarx.Init.Priority = DMA_PRIORITY_MEDIUM;
//		__HAL_LINKDMA(huart, hdmarx, uart3.dmarx);
//		HAL_DMA_Init(&uart3.dmarx);

//		HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 3, 0);
//		HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
//	}
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART1)
	{
	}
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART1)
	{
	}
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART1)
	{
		uart1.TxState = 0;
	}
//	else if (huart->Instance == USART2)
//	{
//		uart2.TxState = 0;
//	}
//	else if (huart->Instance == USART3)
//	{
//		uart3.TxState = 0;
//	}
}
void HAL_UART_AbortReceiveCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART1)
	{
		uart1.RxInPtr->end = &U1_RxBuff[uart1.RxCounter - 1];
		uart1.RxInPtr++;
		if (uart1.RxInPtr == uart1.RxEndPtr)
		{
			uart1.RxInPtr = &uart1.RxLocation[0];
		}
		if ((U1_RX_SIZE - uart1.RxCounter) < U1_RX_MAX)
		{
			uart1.RxCounter = 0;
			uart1.RxInPtr->start = U1_RxBuff;
		}
		else
		{
			uart1.RxInPtr->start = &U1_RxBuff[uart1.RxCounter];
		}
		HAL_UART_Receive_DMA(&uart1.uart, uart1.RxInPtr->start, U1_RX_MAX);
	}
//	else if (huart->Instance == USART2)
//	{
//		uart2.RxInPtr->end = &U2_RxBuff[uart2.RxCounter - 1];
//		uart2.RxInPtr++;
//		if (uart2.RxInPtr == uart2.RxEndPtr)
//		{
//			uart2.RxInPtr = &uart2.RxLocation[0];
//		}
//		if ((U2_RX_SIZE - uart2.RxCounter) < U2_RX_MAX)
//		{
//			uart2.RxCounter = 0;
//			uart2.RxInPtr->start = U2_RxBuff;
//		}
//		else
//		{
//			uart2.RxInPtr->start = &U2_RxBuff[uart2.RxCounter];
//		}
//		HAL_UART_Receive_DMA(&uart2.uart, uart2.RxInPtr->start, U2_RX_MAX);
//	}
//	else if (huart->Instance == USART3)
//	{
//		uart3.RxInPtr->end = &U3_RxBuff[uart3.RxCounter - 1];
//		uart3.RxInPtr++;
//		if (uart3.RxInPtr == uart3.RxEndPtr)
//		{
//			uart3.RxInPtr = &uart3.RxLocation[0];
//		}
//		if ((U3_RX_SIZE - uart3.RxCounter) < U3_RX_MAX)
//		{
//			uart3.RxCounter = 0;
//			uart3.RxInPtr->start = U3_RxBuff;
//		}
//		else
//		{
//			uart3.RxInPtr->start = &U3_RxBuff[uart3.RxCounter];
//		}
//		HAL_UART_Receive_DMA(&uart3.uart, uart3.RxInPtr->start, U3_RX_MAX);
//	}
}
