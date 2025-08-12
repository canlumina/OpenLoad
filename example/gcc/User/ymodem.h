#ifndef __YMODEM_H
#define __YMODEM_H

#include "stm32f1xx_hal.h"
#include "stdint.h"

// YMODEM control characters (same as XMODEM plus some extras)
#define SOH 0x01  // Start of Header
#define STX 0x02  // Start of Header (1024 bytes)
#define EOT 0x04  // End of Transmission
#define ACK 0x06  // Acknowledge
#define NAK 0x15  // Negative Acknowledge
#define CAN 0x18  // Cancel
#define SUB 0x1A  // Substitute
#define C   0x43  // 'C' character for CRC mode

// YMODEM packet sizes
#define YMODEM_PACKET_SIZE 128
#define YMODEM_PACKET_SIZE_1K 1024

// YMODEM error codes
#define YMODEM_OK           0
#define YMODEM_ERROR        -1
#define YMODEM_CANCELLED    -2
#define YMODEM_TIMEOUT      -3

// Function prototypes
int ymodem_receive(UART_HandleTypeDef *huart, uint8_t *buffer, uint32_t buffer_size, uint32_t *received_size, char *filename, uint32_t *file_size);
int ymodem_send(UART_HandleTypeDef *huart, uint8_t *buffer, uint32_t buffer_size, const char *filename);

#endif // __YMODEM_H