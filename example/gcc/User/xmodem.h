#ifndef __XMODEM_H
#define __XMODEM_H

#include "stm32f1xx_hal.h"
#include "stdint.h"

// XMODEM control characters
#define SOH 0x01  // Start of Header
#define STX 0x02  // Start of Header (1024 bytes)
#define EOT 0x04  // End of Transmission
#define ACK 0x06  // Acknowledge
#define NAK 0x15  // Negative Acknowledge
#define CAN 0x18  // Cancel
#define SUB 0x1A  // Substitute

// XMODEM packet sizes
#define XMODEM_PACKET_SIZE 128
#define XMODEM_PACKET_SIZE_1K 1024

// XMODEM error codes
#define XMODEM_OK           0
#define XMODEM_ERROR        -1
#define XMODEM_CANCELLED    -2
#define XMODEM_TIMEOUT      -3

// Function prototypes
int xmodem_receive(UART_HandleTypeDef *huart, uint8_t *buffer, uint32_t buffer_size, uint32_t *received_size);
int xmodem_send(UART_HandleTypeDef *huart, uint8_t *buffer, uint32_t buffer_size);

#endif // __XMODEM_H