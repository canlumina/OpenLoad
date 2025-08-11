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
    /* Hold a key to stay in bootloader */
    uint8_t key = key_scan(0);
    if (key == WKUP_PRES || key == KEY0_PRES) {
        return 1;
    }
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

static int program_app_from_uart(void) {
    const struct fal_partition *app_part = fal_partition_find("app");
    if (app_part == NULL) {
        printf("[BL] No 'app' partition!\r\n");
        return -1;
    }

    UART_HandleTypeDef *upd_uart = &uart2.uart; /* Use USART2 for update channel */
    printf("[BL] Upgrade on USART2. Send length in bytes then \n, then raw image.\r\n");
    printf("[BL] Max %lu bytes.\r\n", (unsigned long)app_part->len);
    printf("[BL] Enter size and press Enter: \r\n");

    char header[32];
    int n = read_line(upd_uart, header, sizeof(header), 15000);
    if (n <= 0) {
        printf("[BL] Timeout waiting size.\r\n");
        return -2;
    }

    char *endptr = NULL;
    unsigned long image_size = strtoul(header, &endptr, 0);
    if (image_size == 0 || image_size > app_part->len) {
        printf("[BL] Invalid size: %s\r\n", header);
        return -3;
    }

    printf("[BL] Erasing app partition...\r\n");
    if (fal_partition_erase(app_part, 0, app_part->len) < 0) {
        printf("[BL] Erase failed.\r\n");
        return -4;
    }

    printf("[BL] Send %lu bytes now...\r\n", image_size);
    uint8_t buf[512];
    uint32_t written = 0;
    while (written < image_size) {
        uint32_t remain = image_size - written;
        uint32_t chunk = (remain > sizeof(buf)) ? sizeof(buf) : remain;
        if (uart_receive_exact(upd_uart, buf, chunk, 2000) != HAL_OK) {
            printf("[BL] UART receive error at %lu.\r\n", (unsigned long)written);
            return -5;
        }
        if (fal_partition_write(app_part, written, buf, chunk) < 0) {
            printf("[BL] Flash write error at %lu.\r\n", (unsigned long)written);
            return -6;
        }
        written += chunk;
        if ((written & 0x3FFFU) == 0) { /* every 16KB */
            printf("[BL] %lu/%lu\r\n", (unsigned long)written, (unsigned long)image_size);
        }
    }

    printf("[BL] Upgrade OK. Written %lu bytes.\r\n", (unsigned long)written);
    return 0;
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
    printf("Press 'u' then Enter on USART2 to update, or auto-jump when app valid next reset.\r\n");

    while (1) {
        /* Poll for update command */
        uint8_t ch;
        if (HAL_UART_Receive(&uart2.uart, &ch, 1, 10) == HAL_OK) {
            if (ch == 'u' || ch == 'U') {
                int r = program_app_from_uart();
                if (r == 0) {
                    printf("[BL] Rebooting to new app...\r\n");
                    delay_ms(50);
                    if (is_app_valid(APP_START_ADDR)) {
                        jump_to_application(APP_START_ADDR);
                    } else {
                        printf("[BL] New app invalid. Stay in boot.\r\n");
                    }
                }
            }
        }

        LED0_TOGGLE();
        delay_ms(100);
    }
}
