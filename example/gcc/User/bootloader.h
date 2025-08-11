#pragma once

#include <stdint.h>

#define APP_START_ADDR   (0x08010000UL) /* 64KB offset (bootloader occupies 0x08000000-0x0800FFFF) */

void bootloader_main(void);
