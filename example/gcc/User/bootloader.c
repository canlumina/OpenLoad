#include "bootloader.h"
#include "sys.h"
#include "usart.h"
#include "bsp_led.h"
#include "bsp_key.h"
#include "delay.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fal.h>
#include "xmodem.h"
#include "ymodem.h"

static int is_app_valid(uint32_t app_addr) {
    uint32_t stack_pointer = *(uint32_t *)app_addr;
    /* SRAM on STM32F103xE: [0x2000_0000, 0x2001_0000) */
    if (stack_pointer < 0x20000000UL || stack_pointer >= 0x20010000UL) {
        return 0;
    }
    uint32_t reset_handler = *(uint32_t *)(app_addr + 4);
    if (reset_handler < 0x08000000UL || reset_handler >= 0x08080000UL) {
        return 0;
    }
    return 1;
}

static void jump_to_application(uint32_t app_addr) {
    typedef void (*pFunction)(void);
    uint32_t stack = *(uint32_t *)app_addr;
    uint32_t reset = *(uint32_t *)(app_addr + 4);

    __disable_irq();

    /* De-init used peripherals to leave a clean state */
    HAL_UART_DeInit(&uart1.uart);
    HAL_UART_DeInit(&uart2.uart);

    /* Disable SysTick */
    SysTick->CTRL = 0;

    /* Remap vector table to application */
    SCB->VTOR = app_addr;

    /* Set MSP to application's stack pointer */
    sys_msr_msp(stack);

    /* Jump */
    pFunction app_reset = (pFunction)reset;
    __enable_irq();
    app_reset();
}

static int should_stay_in_boot(void) {
    /* Wait for 2 seconds for a key press to stay in bootloader */
    printf("Press any key within 2 seconds to enter bootloader...\r\n");
    uint32_t start_time = HAL_GetTick();
    
    // Check if a key is already pressed at startup
    uint8_t initial_key = key_scan(1); // Use mode 1 to support continuous press
    if (initial_key == WKUP_PRES || initial_key == KEY0_PRES || initial_key == KEY1_PRES || initial_key == KEY2_PRES) {
        printf("Key pressed at startup, entering bootloader.\r\n");
        return 1;
    }
    
    while ((HAL_GetTick() - start_time) < 2000) {
        uint8_t key = key_scan(0);
        if (key == WKUP_PRES || key == KEY0_PRES || key == KEY1_PRES || key == KEY2_PRES) {
            printf("Key pressed, entering bootloader.\r\n");
            return 1;
        }
        delay_ms(10);
    }
    printf("No key pressed, booting application if valid.\r\n");
    return 0;
}

static HAL_StatusTypeDef uart_receive_exact(UART_HandleTypeDef *huart, uint8_t *buffer,
                                            uint32_t total_len, uint32_t chunk_timeout_ms) {
    uint32_t received = 0;
    while (received < total_len) {
        uint32_t remain = total_len - received;
        uint16_t this_time = (remain > 512U) ? 512U : (uint16_t)remain;
        HAL_StatusTypeDef st = HAL_UART_Receive(huart, buffer + received, this_time, chunk_timeout_ms);
        if (st != HAL_OK) return st;
        received += this_time;
    }
    return HAL_OK;
}

static int read_line(UART_HandleTypeDef *huart, char *line_buf, size_t max_len, uint32_t timeout_ms) {
    size_t idx = 0;
    uint32_t start = HAL_GetTick();
    while (idx + 1 < max_len) {
        uint8_t ch;
        HAL_StatusTypeDef st = HAL_UART_Receive(huart, &ch, 1, 10);
        if (st == HAL_OK) {
            if (ch == '\r') continue;
            if (ch == '\n') break;
            line_buf[idx++] = (char)ch;
        } else {
            if ((HAL_GetTick() - start) > timeout_ms) break;
        }
    }
    line_buf[idx] = '\0';
    return (int)idx;
}

static int program_app_from_uart_xymodem(int protocol) {
    const struct fal_partition *download_part = fal_partition_find("download");
    if (download_part == NULL) {
        printf("[BL] No 'download' partition!\r\n");
        return -1;
    }
    
    const struct fal_partition *app_part = fal_partition_find("app");
    if (app_part == NULL) {
        printf("[BL] No 'app' partition!\r\n");
        return -1;
    }

    UART_HandleTypeDef *upd_uart = &uart1.uart; /* Use USART1 for update channel */
    printf("[BL] Upgrade on USART1 using %s.\r\n", protocol ? "YMODEM" : "XMODEM");
    printf("[BL] Download partition size: %lu bytes.\r\n", (unsigned long)download_part->len);
    printf("[BL] App partition size: %lu bytes.\r\n", (unsigned long)app_part->len);
    
    // Allocate buffer for download partition
    uint8_t *download_buffer = (uint8_t *)malloc(download_part->len);
    if (download_buffer == NULL) {
        printf("[BL] Failed to allocate download buffer.\r\n");
        return -2;
    }
    
    uint32_t received_size = 0;
    int result = 0;
    
    if (protocol == 0) {
        // XMODEM
        printf("[BL] Waiting for XMODEM transfer...\r\n");
        result = xmodem_receive(upd_uart, download_buffer, download_part->len, &received_size);
    } else {
        // YMODEM
        char filename[256] = {0};
        uint32_t file_size = 0;
        printf("[BL] Waiting for YMODEM transfer...\r\n");
        result = ymodem_receive(upd_uart, download_buffer, download_part->len, &received_size, filename, &file_size);
        if (result == YMODEM_OK) {
            printf("[BL] Received file: %s, size: %lu bytes\r\n", filename, (unsigned long)file_size);
        }
    }
    
    if (result != 0) {
        printf("[BL] Transfer failed with error code: %d\r\n", result);
        free(download_buffer);
        return -3;
    }
    
    printf("[BL] Received %lu bytes.\r\n", (unsigned long)received_size);
    
    // Erase download partition
    printf("[BL] Erasing download partition...\r\n");
    if (fal_partition_erase(download_part, 0, download_part->len) < 0) {
        printf("[BL] Erase download partition failed.\r\n");
        free(download_buffer);
        return -4;
    }
    
    // Write received data to download partition
    printf("[BL] Writing to download partition...\r\n");
    uint32_t written = 0;
    while (written < received_size) {
        uint32_t chunk = (received_size - written) > 512 ? 512 : (received_size - written);
        if (fal_partition_write(download_part, written, download_buffer + written, chunk) < 0) {
            printf("[BL] Write to download partition failed at %lu.\r\n", (unsigned long)written);
            free(download_buffer);
            return -5;
        }
        written += chunk;
    }
    
    // Now copy from download partition to app partition
    printf("[BL] Copying from download to app partition...\r\n");
    
    // Erase app partition
    printf("[BL] Erasing app partition...\r\n");
    if (fal_partition_erase(app_part, 0, app_part->len) < 0) {
        printf("[BL] Erase app partition failed.\r\n");
        free(download_buffer);
        return -6;
    }
    
    // Copy data from download to app partition
    uint8_t copy_buffer[512];
    uint32_t copied = 0;
    while (copied < received_size) {
        uint32_t chunk = (received_size - copied) > sizeof(copy_buffer) ? sizeof(copy_buffer) : (received_size - copied);
        
        // Read from download partition
        if (fal_partition_read(download_part, copied, copy_buffer, chunk) < 0) {
            printf("[BL] Read from download partition failed at %lu.\r\n", (unsigned long)copied);
            free(download_buffer);
            return -7;
        }
        
        // Write to app partition
        if (fal_partition_write(app_part, copied, copy_buffer, chunk) < 0) {
            printf("[BL] Write to app partition failed at %lu.\r\n", (unsigned long)copied);
            free(download_buffer);
            return -8;
        }
        
        copied += chunk;
        if ((copied & 0x3FFFU) == 0) { /* every 16KB */
            printf("[BL] Copied %lu/%lu bytes\r\n", (unsigned long)copied, (unsigned long)received_size);
        }
    }
    
    free(download_buffer);
    printf("[BL] Upgrade completed. Copied %lu bytes to app partition.\r\n", (unsigned long)copied);
    return 0;
}

static int restore_app_from_backup(void) {
    const struct fal_partition *app_part = fal_partition_find("app");
    if (app_part == NULL) {
        printf("[BL] No 'app' partition!\r\n");
        return -1;
    }
    
    const struct fal_partition *basesys_part = fal_partition_find("basesys");
    if (basesys_part == NULL) {
        printf("[BL] No 'basesys' partition!\r\n");
        return -1;
    }
    
    printf("[BL] Restoring app from backup (basesys partition)...\r\n");
    
    // Erase app partition
    printf("[BL] Erasing app partition...\r\n");
    if (fal_partition_erase(app_part, 0, app_part->len) < 0) {
        printf("[BL] Erase app partition failed.\r\n");
        return -2;
    }
    
    // Copy data from basesys to app partition
    uint8_t buffer[512];
    uint32_t copied = 0;
    uint32_t copy_size = basesys_part->len > app_part->len ? app_part->len : basesys_part->len;
    
    while (copied < copy_size) {
        uint32_t chunk = (copy_size - copied) > sizeof(buffer) ? sizeof(buffer) : (copy_size - copied);
        
        // Read from basesys partition
        if (fal_partition_read(basesys_part, copied, buffer, chunk) < 0) {
            printf("[BL] Read from basesys partition failed at %lu.\r\n", (unsigned long)copied);
            return -3;
        }
        
        // Write to app partition
        if (fal_partition_write(app_part, copied, buffer, chunk) < 0) {
            printf("[BL] Write to app partition failed at %lu.\r\n", (unsigned long)copied);
            return -4;
        }
        
        copied += chunk;
        if ((copied & 0x3FFFU) == 0) { /* every 16KB */
            printf("[BL] Restored %lu/%lu bytes\r\n", (unsigned long)copied, (unsigned long)copy_size);
        }
    }
    
    printf("[BL] Restore completed. Copied %lu bytes to app partition.\r\n", (unsigned long)copied);
    return 0;
}

static void show_menu(void) {
    printf("\r\n===== Bootloader Menu =====\r\n");
    printf("1. UART IAP (XMODEM)\r\n");
    printf("2. UART IAP (YMODEM)\r\n");
    printf("3. Restore app from backup\r\n");
    printf("4. Jump to application\r\n");
    printf("5. Show partition info\r\n");
    printf("6. Erase app partition\r\n");
    printf("0. Exit menu (auto-boot)\r\n");
    printf("Select option: ");
}

void bootloader_main(void) {
    printf("Bootloader start\r\n");

    /* Optional: init storage abstraction */
    fal_init();

    if (!should_stay_in_boot() && is_app_valid(APP_START_ADDR)) {
        printf("Jumping to application at 0x%08lX\r\n", (unsigned long)APP_START_ADDR);
        delay_ms(50);
        jump_to_application(APP_START_ADDR);
    }

    printf("Stay in bootloader (no valid app or key held)\r\n");
    
    // Bootloader command loop
    while (1) {
        show_menu();
        
        // Read user input
        uint8_t ch;
        if (HAL_UART_Receive(&uart1.uart, &ch, 1, 30000) == HAL_OK) { // 30 second timeout
            printf("%c\r\n", ch);
            
            switch (ch) {
                case '1': // XMODEM IAP
                    {
                        int r = program_app_from_uart_xymodem(0); // 0 for XMODEM
                        if (r == 0) {
                            printf("[BL] XMODEM transfer successful.\r\n");
                        } else {
                            printf("[BL] XMODEM transfer failed with code %d.\r\n", r);
                        }
                    }
                    break;
                    
                case '2': // YMODEM IAP
                    {
                        int r = program_app_from_uart_xymodem(1); // 1 for YMODEM
                        if (r == 0) {
                            printf("[BL] YMODEM transfer successful.\r\n");
                        } else {
                            printf("[BL] YMODEM transfer failed with code %d.\r\n", r);
                        }
                    }
                    break;
                    
                case '3': // Restore from backup
                    {
                        int r = restore_app_from_backup();
                        if (r == 0) {
                            printf("[BL] Restore from backup successful.\r\n");
                        } else {
                            printf("[BL] Restore from backup failed with code %d.\r\n", r);
                        }
                    }
                    break;
                    
                case '4': // Jump to application
                    if (is_app_valid(APP_START_ADDR)) {
                        printf("[BL] Jumping to application...\r\n");
                        delay_ms(50);
                        jump_to_application(APP_START_ADDR);
                    } else {
                        printf("[BL] No valid application found.\r\n");
                    }
                    break;
                    
                case '5': // Show partition info
                    fal_show_part_table();
                    break;
                    
                case '6': // Erase app partition
                    {
                        const struct fal_partition *app_part = fal_partition_find("app");
                        if (app_part != NULL) {
                            printf("[BL] Erasing app partition...\r\n");
                            if (fal_partition_erase_all(app_part) < 0) {
                                printf("[BL] Erase app partition failed.\r\n");
                            } else {
                                printf("[BL] App partition erased successfully.\r\n");
                            }
                        } else {
                            printf("[BL] No 'app' partition found.\r\n");
                        }
                    }
                    break;
                    
                case '0': // Exit menu and try to boot app
                    if (is_app_valid(APP_START_ADDR)) {
                        printf("[BL] Jumping to application...\r\n");
                        delay_ms(50);
                        jump_to_application(APP_START_ADDR);
                    } else {
                        printf("[BL] No valid application found. Returning to menu.\r\n");
                    }
                    break;
                    
                default:
                    printf("[BL] Invalid option. Please try again.\r\n");
                    break;
            }
        } else {
            printf("\r\n[BL] Timeout. Checking for valid application...\r\n");
            if (is_app_valid(APP_START_ADDR)) {
                printf("[BL] Jumping to application...\r\n");
                delay_ms(50);
                jump_to_application(APP_START_ADDR);
            } else {
                printf("[BL] No valid application found. Returning to menu.\r\n");
            }
        }

        LED0_TOGGLE();
        delay_ms(100);
    }
}
