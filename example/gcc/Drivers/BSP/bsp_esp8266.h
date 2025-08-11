#ifndef BSP_ESP8266_H
#define BSP_ESP8266_H

#include "usart.h"

extern UCB uart2;
extern uint8_t U2_RxBuff[U2_RX_SIZE];
extern uint8_t U2_TxBuff[U2_TX_SIZE];

#define RESET_IO(x) HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, (GPIO_PinState)x) // PA4控制WiFi的复位

#define WiFi_printf u2_printf          // 串口2控制 WiFi
#define WiFi_RxCounter uart2.RxCounter // 串口2控制 WiFi
#define WiFi_RX_BUF U2_RxBuff          // 串口2控制 WiFi
#define WiFi_RXBUFF_SIZE U2_RX_SIZE    // 串口2控制 WiFi

#define SSID "Huike-Group" // 路由器SSID名称
#define PASS "huike0826"   // 路由器密码

extern int ServerPort;
extern char ServerIP[128]; // 存放服务器IP或是域名

void WiFi_ResetIO_Init(void);
char WiFi_SendCmd(char *cmd, int timeout);
char WiFi_Reset(int timeout);
char WiFi_JoinAP(int timeout);
char WiFi_Connect_Server(int timeout);
char WiFi_Smartconfig(int timeout);
char WiFi_WaitAP(int timeout);
char WiFi_Connect_IoTServer(void);

#endif
