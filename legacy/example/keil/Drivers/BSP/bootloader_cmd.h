#ifndef __BOOTLOADER_CMD_H
#define __BOOTLOADER_CMD_H

#include <stdint.h>
#define print_cmd uart1_printf

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
    CMD_VERSION_COMPARE,
    CMD_SECURE_DOWNLOAD,
    CMD_SECURE_INSTALL,
    CMD_SECURE_VERIFY,
    CMD_SECURE_BACKUP,
    CMD_SECURE_RESTORE
} cmd_id_t;

/* 命令结构 */
typedef struct {
    const char* name;
    const char* short_name;
    const char* description;
    cmd_id_t id;
    void (*handler)(void);
} bootloader_cmd_t;


void process_cmd(char* cmd);


/* 命令处理函数 */
void cmd_help_handler(void);
void cmd_update_handler(void);
void cmd_info_handler(void);
void cmd_erase_handler(void);
void cmd_reset_handler(void);

/* 安全命令处理函数 */
void cmd_secure_download_handler(void);
void cmd_secure_install_handler(void);
void cmd_secure_verify_handler(void);
void cmd_secure_backup_handler(void);
void cmd_secure_restore_handler(void);
void cmd_jump_handler(void);
void cmd_extinfo_handler(void);
void cmd_extbackup_handler(void);
void cmd_extrestore_handler(void);
void cmd_extlist_handler(void);
void cmd_wifi_connect_handler(void);
void cmd_wifi_status_handler(void);
void cmd_wifi_debug_handler(void);
void cmd_config_show_handler(void);
void cmd_config_wifi_handler(void);
void cmd_config_ota_handler(void);
void cmd_config_save_handler(void);
void cmd_config_reset_handler(void);
void cmd_version_handler(void);
void cmd_version_compare_handler(void);

#endif
