#ifndef BOOTLOADER_CMD_H
#define BOOTLOADER_CMD_H

#include <stdint.h>
#include <stdbool.h>

/* Flash 分区定义 - STM32F103ZET6 (512KB Flash) */
#define FLASH_BASE_ADDR         0x08000000
#ifndef FLASH_PAGE_SIZE
#define FLASH_PAGE_SIZE         0x800        // 2KB per page for STM32F103
#endif
#define FLASH_TOTAL_SIZE        0x80000      // 512KB total flash
#define BOOTLOADER_SIZE         0x10000      // 64KB for bootloader (0x08000000 - 0x0800FFFF)
#define APP_START_ADDR          0x08010000   // Application start address
#define APP_MAX_SIZE            0x70000      // 448KB for application (0x08010000 - 0x0807FFFF)

/* 固件信息结构 */
typedef struct {
    uint32_t magic;          // 魔数标识 0xDEADBEEF
    uint32_t version;        // 固件版本
    uint32_t size;           // 固件大小
    uint32_t crc32;          // CRC32校验值
    uint32_t build_time;     // 编译时间戳
    char     name[32];       // 固件名称
} firmware_info_t;

/* Bootloader 状态 */
typedef enum {
    BOOT_STATE_IDLE,         // 空闲状态
    BOOT_STATE_CMD_MODE,     // 命令模式
    BOOT_STATE_UPDATE,       // 更新模式
    BOOT_STATE_DOWNLOAD,     // 下载模式
    BOOT_STATE_BACKUP,       // 备份模式
    BOOT_STATE_RESTORE,      // 恢复模式
    BOOT_STATE_JUMP_APP      // 跳转到应用
} bootloader_state_t;

/* 命令ID定义 */
typedef enum {
    CMD_HELP,
    CMD_UPDATE,
    CMD_DOWNLOAD,
    CMD_BACKUP,
    CMD_RESTORE,
    CMD_INFO,
    CMD_ERASE,
    CMD_RESET,
    CMD_JUMP,
    CMD_EXTINFO,    // 外部Flash信息
    CMD_EXTBACKUP,  // 备份到外部Flash
    CMD_EXTRESTORE, // 从外部Flash恢复
    CMD_EXTLIST,    // 列出外部Flash备份
    CMD_UNKNOWN
} cmd_id_t;

/* 命令结构 */
typedef struct {
    const char* name;
    const char* short_name;    // 命令缩写
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
void cmd_download_handler(void);
void cmd_backup_handler(void);
void cmd_restore_handler(void);
void cmd_info_handler(void);
void cmd_erase_handler(void);
void cmd_reset_handler(void);
void cmd_jump_handler(void);
void cmd_extinfo_handler(void);
void cmd_extbackup_handler(void);
void cmd_extrestore_handler(void);
void cmd_extlist_handler(void);

/* 工具函数 */
uint32_t bootloader_calc_crc32(uint32_t addr, uint32_t size);
bool bootloader_flash_erase(uint32_t addr, uint32_t size);
bool bootloader_flash_write(uint32_t addr, const uint8_t* data, uint32_t size);
bool bootloader_flash_read(uint32_t addr, uint8_t* data, uint32_t size);

#endif /* BOOTLOADER_CMD_H */