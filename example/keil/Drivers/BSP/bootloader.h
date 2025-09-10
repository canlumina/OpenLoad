#ifndef __BOOTLOADER_H_
#define __BOOTLOADER_H_

#include <stdint.h>

/* Bootloader状态 */
typedef enum {
    BOOT_STATE_IDLE,
    BOOT_STATE_CMD_MODE,
    BOOT_STATE_JUMP_APP
} bootloader_state_t;


void bootloader_init(void);

int bootloader_check_entry(uint32_t timeout_ms);
void bootloader_cmd_mode(void);
void bootloader_jump_to_app(void);


#endif

