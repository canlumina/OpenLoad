#include "xmodem.h"
#include "stm32f1xx_hal.h"
#include <string.h>
#include <fal.h>
#include "crc32.h"

// CRC calculation for XMODEM
static uint16_t xmodem_crc_update(uint16_t crc, uint8_t data) {
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

static uint16_t xmodem_calc_crc(uint8_t *data, uint16_t len) {
    uint16_t crc = 0;
    for (uint16_t i = 0; i < len; i++) {
        crc = xmodem_crc_update(crc, data[i]);
    }
    return crc;
}

// Receive a single byte with timeout
static HAL_StatusTypeDef xmodem_receive_byte(UART_HandleTypeDef *huart, uint8_t *byte, uint32_t timeout_ms) {
    return HAL_UART_Receive(huart, byte, 1, timeout_ms);
}

// Send a single byte
static HAL_StatusTypeDef xmodem_send_byte(UART_HandleTypeDef *huart, uint8_t byte) {
    return HAL_UART_Transmit(huart, &byte, 1, HAL_MAX_DELAY);
}

// Receive XMODEM packet
static int xmodem_receive_packet(UART_HandleTypeDef *huart, uint8_t *packet, uint16_t *packet_size, uint8_t *packet_num) {
    uint8_t header;
    uint8_t expected_packet_num = *packet_num;
    uint8_t packet_complement;
    uint16_t crc_received, crc_calculated;
    
    // Read header
    if (xmodem_receive_byte(huart, &header, 1000) != HAL_OK) {
        return XMODEM_TIMEOUT;
    }
    
    // Check for EOT
    if (header == EOT) {
        xmodem_send_byte(huart, ACK);
        *packet_size = 0; // Mark as EOT
        return XMODEM_OK; 
    }
    
    // Check header type
    if (header == SOH) {
        *packet_size = XMODEM_PACKET_SIZE;
    } else if (header == STX) {
        *packet_size = XMODEM_PACKET_SIZE_1K;
    } else {
        return XMODEM_ERROR;
    }
    
    // Read packet number
    if (xmodem_receive_byte(huart, packet_num, 1000) != HAL_OK) {
        return XMODEM_TIMEOUT;
    }
    
    // Read packet number complement
    if (xmodem_receive_byte(huart, &packet_complement, 1000) != HAL_OK) {
        return XMODEM_TIMEOUT;
    }
    
    // Verify packet number
    if (*packet_num != (uint8_t)(~packet_complement)) {
        return XMODEM_ERROR;
    }
    
    // Verify expected packet number (unless it's the first packet)
    if (expected_packet_num != 0 && *packet_num != expected_packet_num) {
         // Handle retransmission of the same packet
        if (*packet_num == (uint8_t)(expected_packet_num - 1)) {
            // This is a retransmission of the previous packet, just ACK it
            xmodem_send_byte(huart, ACK);
            return XMODEM_OK; // Or a new status code for duplicate packet
        }
        return XMODEM_ERROR;
    }
    
    // Read data
    for (int i = 0; i < *packet_size; i++) {
        if (xmodem_receive_byte(huart, &packet[i], 1000) != HAL_OK) {
            return XMODEM_TIMEOUT;
        }
    }
    
    // Read CRC (2 bytes)
    uint8_t crc_high, crc_low;
    if (xmodem_receive_byte(huart, &crc_high, 1000) != HAL_OK) {
        return XMODEM_TIMEOUT;
    }
    if (xmodem_receive_byte(huart, &crc_low, 1000) != HAL_OK) {
        return XMODEM_TIMEOUT;
    }
    
    crc_received = ((uint16_t)crc_high << 8) | crc_low;
    crc_calculated = xmodem_calc_crc(packet, *packet_size);
    
    if (crc_received != crc_calculated) {
        return XMODEM_ERROR;
    }
    
    return XMODEM_OK;
}

int xmodem_receive(UART_HandleTypeDef *huart, uint8_t *buffer, uint32_t buffer_size, uint32_t *received_size) {
    uint8_t packet[XMODEM_PACKET_SIZE_1K];
    uint16_t packet_size;
    uint8_t packet_num = 1; // Start with packet 1 (packet 0 is special)
    uint32_t total_received = 0;
    int retries = 0;
    const int max_retries = 10;
    
    // Send 'C' to start transfer in CRC mode
    xmodem_send_byte(huart, 'C');
    
    while (total_received < buffer_size && retries < max_retries) {
        int result = xmodem_receive_packet(huart, packet, &packet_size, &packet_num);
        
        if (result == XMODEM_OK) {
            // Check for EOT
            if (packet_size == 0) {
                *received_size = total_received;
                return XMODEM_OK;
            }
            
            // Add data to buffer
            uint32_t copy_size = packet_size;
            if (total_received + copy_size > buffer_size) {
                copy_size = buffer_size - total_received;
            }
            
            memcpy(buffer + total_received, packet, copy_size);
            total_received += copy_size;
            
            // Send ACK
            xmodem_send_byte(huart, ACK);
            
            // Move to next packet
            packet_num++;
            retries = 0; // Reset retries on successful packet
        } else {
            retries++;
            xmodem_send_byte(huart, NAK);
        }
    }
    
    // Send CAN to cancel if we've exceeded retries
    if (retries >= max_retries) {
        xmodem_send_byte(huart, CAN);
        xmodem_send_byte(huart, CAN);
        return XMODEM_CANCELLED;
    }
    
    *received_size = total_received;
    return XMODEM_OK;
}

int xmodem_receive_to_flash(UART_HandleTypeDef *huart, const struct fal_partition *part, uint32_t *received_size, uint32_t *fw_crc) {
    uint8_t packet[XMODEM_PACKET_SIZE_1K];
    uint16_t packet_size;
    uint8_t packet_num = 1;
    uint32_t total_received = 0;
    int retries = 0;
    const int max_retries = 10;
    
    *fw_crc = crc32_init();

    // Send 'C' to start transfer in CRC mode
    xmodem_send_byte(huart, 'C');
    
    while (total_received < part->len && retries < max_retries) {
        int result = xmodem_receive_packet(huart, packet, &packet_size, &packet_num);
        
        if (result == XMODEM_OK) {
            if (packet_size == 0) { // EOT
                *received_size = total_received;
                *fw_crc = crc32_final(*fw_crc);
                return XMODEM_OK;
            }
            
            // Write data to flash
            if (total_received + packet_size > part->len) {
                // Firmware is larger than partition
                xmodem_send_byte(huart, CAN);
                xmodem_send_byte(huart, CAN);
                return XMODEM_ERROR;
            }
            
            if (fal_partition_write(part, total_received, packet, packet_size) < 0) {
                // Flash write error
                xmodem_send_byte(huart, CAN);
                xmodem_send_byte(huart, CAN);
                return XMODEM_ERROR;
            }
            
            *fw_crc = crc32_update(*fw_crc, packet, packet_size);
            total_received += packet_size;
            
            // Send ACK
            xmodem_send_byte(huart, ACK);
            
            packet_num++;
            retries = 0;
        } else {
            retries++;
            xmodem_send_byte(huart, NAK);
        }
    }
    
    if (retries >= max_retries) {
        xmodem_send_byte(huart, CAN);
        xmodem_send_byte(huart, CAN);
        return XMODEM_CANCELLED;
    }
    
    *received_size = total_received;
    *fw_crc = crc32_final(*fw_crc);
    return XMODEM_OK;
}

int xmodem_send(UART_HandleTypeDef *huart, uint8_t *buffer, uint32_t buffer_size) {
    // Implementation for XMODEM send would go here
    // For now, we're focusing on receive functionality
    return XMODEM_OK;
}