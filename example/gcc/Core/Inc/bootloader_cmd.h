#ifndef BOOTLOADER_CMD_H
#define BOOTLOADER_CMD_H

#include <stdint.h>
#include <stdbool.h>

/* Flash分区定义 */
#define FLASH_BASE_ADDR         0x08000000
// FLASH_PAGE_SIZE已经在HAL库中定义
#define FLASH_TOTAL_SIZE        0x80000      // 512KB total
#define BOOTLOADER_SIZE         0x10000      // 64KB bootloader
#define APP_START_ADDR          0x08010000   // App start
#define APP_MAX_SIZE            0x70000      // 448KB app

/* Bootloader状态 */
typedef enum {
    BOOT_STATE_IDLE,
    BOOT_STATE_CMD_MODE,
    BOOT_STATE_JUMP_APP
} bootloader_state_t;

/* 命令ID */
typedef enum {
    CMD_HELP,
    CMD_UPDATE,
    CMD_INFO,
    CMD_ERASE,
    CMD_RESET,
    CMD_JUMP,
    CMD_EXTINFO,
    CMD_EXTBACKUP,
    CMD_EXTRESTORE,
    CMD_EXTLIST,
    CMD_WIFI_CONNECT,
    CMD_WIFI_STATUS,
    CMD_WIFI_DEBUG,
    CMD_CONFIG_SHOW,
    CMD_CONFIG_WIFI,
    CMD_CONFIG_OTA,
    CMD_CONFIG_SAVE,
    CMD_CONFIG_RESET,
    CMD_VERSION,
    CMD_VERSION_COMPARE
} cmd_id_t;

/* 命令结构 */
typedef struct {
    const char* name;
    const char* short_name;
    const char* description;
    cmd_id_t id;
    void (*handler)(void);
} bootloader_cmd_t;

/* 函数声明 */
void bootloader_init(void);
bool bootloader_check_entry(uint32_t timeout_ms);
void bootloader_cmd_mode(void);
void bootloader_jump_to_app(void);
bool bootloader_validate_app(void);

/* 命令处理函数 */
void cmd_help_handler(void);
void cmd_update_handler(void);
void cmd_info_handler(void);
void cmd_erase_handler(void);
void cmd_reset_handler(void);
void cmd_jump_handler(void);
void cmd_extinfo_handler(void);
void cmd_extbackup_handler(void);
void cmd_extrestore_handler(void);
void cmd_extlist_handler(void);
void cmd_wifi_connect_handler(void);
void cmd_wifi_status_handler(void);
void cmd_wifi_debug_handler(void);

/* Flash操作函数 */
bool bootloader_flash_erase(uint32_t addr, uint32_t size);
bool bootloader_flash_write(uint32_t addr, const uint8_t* data, uint32_t size);

/* 输出函数 - 供其他模块使用 */
void bootloader_print(const char* str);
void bootloader_print_hex(uint32_t val);
void bootloader_print_dec(uint32_t val);

#endif