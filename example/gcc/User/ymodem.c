#include "ymodem.h"
#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdlib.h>

// CRC calculation for YMODEM
static uint16_t ymodem_crc_update(uint16_t crc, uint8_t data) {
    crc = crc ^ ((uint16_t)data << 8);
    for (int i = 0; i < 8; i++) {
        if (crc & 0x8000) {
            crc = (crc << 1) ^ 0x1021;
        } else {
            crc = crc << 1;
        }
    }
    return crc;
}

static uint16_t ymodem_calc_crc(uint8_t *data, uint16_t len) {
    uint16_t crc = 0;
    for (uint16_t i = 0; i < len; i++) {
        crc = ymodem_crc_update(crc, data[i]);
    }
    return crc;
}

// Receive a single byte with timeout
static HAL_StatusTypeDef ymodem_receive_byte(UART_HandleTypeDef *huart, uint8_t *byte, uint32_t timeout_ms) {
    return HAL_UART_Receive(huart, byte, 1, timeout_ms);
}

// Send a single byte
static HAL_StatusTypeDef ymodem_send_byte(UART_HandleTypeDef *huart, uint8_t byte) {
    return HAL_UART_Transmit(huart, &byte, 1, HAL_MAX_DELAY);
}

// Parse file information from first packet
static void ymodem_parse_file_info(uint8_t *packet, char *filename, uint32_t *file_size) {
    // Filename is null-terminated string at beginning of packet
    int i = 0;
    while (packet[i] != 0 && i < 256) {
        filename[i] = packet[i];
        i++;
    }
    filename[i] = '\0';
    
    // File size follows filename, also null-terminated
    int j = i + 1;
    char size_str[16] = {0};
    int k = 0;
    while (packet[j] != 0 && k < 15) {
        size_str[k] = packet[j];
        j++;
        k++;
    }
    
    *file_size = atoi(size_str);
}

// Receive YMODEM packet
static int ymodem_receive_packet(UART_HandleTypeDef *huart, uint8_t *packet, uint16_t *packet_size, uint8_t *packet_num) {
    uint8_t header;
    uint8_t expected_packet_num = *packet_num;
    uint8_t packet_complement;
    uint16_t crc_received, crc_calculated;
    
    // Read header
    if (ymodem_receive_byte(huart, &header, 1000) != HAL_OK) {
        return YMODEM_TIMEOUT;
    }
    
    // Check for EOT
    if (header == EOT) {
        ymodem_send_byte(huart, ACK);
        return YMODEM_OK; // End of transmission
    }
    
    // Check header type
    if (header == SOH) {
        *packet_size = YMODEM_PACKET_SIZE;
    } else if (header == STX) {
        *packet_size = YMODEM_PACKET_SIZE_1K;
    } else {
        return YMODEM_ERROR;
    }
    
    // Read packet number
    if (ymodem_receive_byte(huart, packet_num, 1000) != HAL_OK) {
        return YMODEM_TIMEOUT;
    }
    
    // Read packet number complement
    if (ymodem_receive_byte(huart, &packet_complement, 1000) != HAL_OK) {
        return YMODEM_TIMEOUT;
    }
    
    // Verify packet number
    if (*packet_num != (uint8_t)(~packet_complement)) {
        return YMODEM_ERROR;
    }
    
    // Verify expected packet number (unless it's the first packet)
    if (expected_packet_num != 0 && *packet_num != expected_packet_num) {
        return YMODEM_ERROR;
    }
    
    // Read data
    for (int i = 0; i < *packet_size; i++) {
        if (ymodem_receive_byte(huart, &packet[i], 1000) != HAL_OK) {
            return YMODEM_TIMEOUT;
        }
    }
    
    // Read CRC (2 bytes)
    uint8_t crc_high, crc_low;
    if (ymodem_receive_byte(huart, &crc_high, 1000) != HAL_OK) {
        return YMODEM_TIMEOUT;
    }
    if (ymodem_receive_byte(huart, &crc_low, 1000) != HAL_OK) {
        return YMODEM_TIMEOUT;
    }
    
    crc_received = ((uint16_t)crc_high << 8) | crc_low;
    crc_calculated = ymodem_calc_crc(packet, *packet_size);
    
    if (crc_received != crc_calculated) {
        return YMODEM_ERROR;
    }
    
    return YMODEM_OK;
}

int ymodem_receive(UART_HandleTypeDef *huart, uint8_t *buffer, uint32_t buffer_size, uint32_t *received_size, char *filename, uint32_t *file_size) {
    uint8_t packet[YMODEM_PACKET_SIZE_1K];
    uint16_t packet_size;
    uint8_t packet_num = 0; // Start with packet 0 for file info
    uint32_t total_received = 0;
    int retries = 0;
    const int max_retries = 10;
    int file_info_received = 0;
    
    // Send 'C' to start transfer in CRC mode
    ymodem_send_byte(huart, C);
    
    while (total_received < buffer_size && retries < max_retries) {
        int result = ymodem_receive_packet(huart, packet, &packet_size, &packet_num);
        
        if (result == YMODEM_OK) {
            // Check for EOT
            if (packet_size == 0) {
                // Send final ACK
                ymodem_send_byte(huart, ACK);
                *received_size = total_received;
                return YMODEM_OK;
            }
            
            // Handle first packet (file info)
            if (!file_info_received) {
                if (packet_num == 0) {
                    // Parse file information
                    ymodem_parse_file_info(packet, filename, file_size);
                    file_info_received = 1;
                    packet_num = 1; // Next packet should be 1
                }
                // Send ACK for file info packet
                ymodem_send_byte(huart, ACK);
                // Send 'C' for next packet
                ymodem_send_byte(huart, C);
                retries = 0; // Reset retries
                continue;
            }
            
            // Add data to buffer
            uint32_t copy_size = packet_size;
            if (total_received + copy_size > buffer_size) {
                copy_size = buffer_size - total_received;
            }
            
            memcpy(buffer + total_received, packet, copy_size);
            total_received += copy_size;
            
            // Send ACK
            ymodem_send_byte(huart, ACK);
            
            // Move to next packet
            packet_num++;
            retries = 0; // Reset retries on successful packet
        } else if (result == YMODEM_TIMEOUT) {
            retries++;
            if (!file_info_received) {
                ymodem_send_byte(huart, C);
            } else {
                ymodem_send_byte(huart, NAK);
            }
        } else {
            retries++;
            if (!file_info_received) {
                ymodem_send_byte(huart, C);
            } else {
                ymodem_send_byte(huart, NAK);
            }
        }
    }
    
    // Send CAN to cancel if we've exceeded retries
    if (retries >= max_retries) {
        ymodem_send_byte(huart, CAN);
        ymodem_send_byte(huart, CAN);
        return YMODEM_CANCELLED;
    }
    
    *received_size = total_received;
    return YMODEM_OK;
}

int ymodem_send(UART_HandleTypeDef *huart, uint8_t *buffer, uint32_t buffer_size, const char *filename) {
    // Implementation for YMODEM send would go here
    // For now, we're focusing on receive functionality
    return YMODEM_OK;
}