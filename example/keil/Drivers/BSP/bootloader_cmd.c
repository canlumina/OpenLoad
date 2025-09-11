#include <string.h>
#include "bootloader_cmd.h"
#include "dev_usart.h"
#include "dev_flash.h"
#include "xmodem.h"
#include "config.h"



/* 命令表 */
const bootloader_cmd_t cmd_table[] = {
    {"help",    "h",  "Show command help",           CMD_HELP,         cmd_help_handler},
    {"update",  "u",  "Update firmware (XMODEM/OTA)", CMD_UPDATE,       cmd_update_handler},
//    {"erase",   "e",  "Erase application area",     CMD_ERASE,        cmd_erase_handler},
//    {"reset",   "r",  "Reset system",               CMD_RESET,        cmd_reset_handler},
//    {"jump",    "j",  "Jump to application",        CMD_JUMP,         cmd_jump_handler},
//    {"xinfo",   "xi", "Show external flash info",   CMD_EXTINFO,      cmd_extinfo_handler},
//    {"xbackup", "xb", "Backup to external flash",   CMD_EXTBACKUP,    cmd_extbackup_handler},
//    {"xrestore","xr", "Restore from external flash",CMD_EXTRESTORE,   cmd_extrestore_handler},
//    {"xlist",   "xl", "List external flash backups",CMD_EXTLIST,      cmd_extlist_handler},
//    {"wifi",    "w",  "Connect to WiFi network",    CMD_WIFI_CONNECT, cmd_wifi_connect_handler},
//    {"wstatus", "ws", "Show WiFi connection status", CMD_WIFI_STATUS,  cmd_wifi_status_handler},
//    {"wdebug",  "wd", "WiFi debug information",     CMD_WIFI_DEBUG,   cmd_wifi_debug_handler},
//    {"cfgshow", "cs", "Show current configuration",  CMD_CONFIG_SHOW,  cmd_config_show_handler},
//    {"cfgwifi", "cw", "Configure WiFi settings",     CMD_CONFIG_WIFI,  cmd_config_wifi_handler},
//    {"cfgota",  "co", "Configure OTA settings",      CMD_CONFIG_OTA,   cmd_config_ota_handler},
//    {"cfgsave", "cS", "Save configuration",          CMD_CONFIG_SAVE,  cmd_config_save_handler},
//    {"cfgreset","cR", "Reset to default config",     CMD_CONFIG_RESET, cmd_config_reset_handler},
//    {"version", "v",  "Show firmware version info",  CMD_VERSION,      cmd_version_handler},
//    {"vcompare","vc", "Compare firmware versions",   CMD_VERSION_COMPARE, cmd_version_compare_handler},
};


#define CMD_TABLE_SIZE (sizeof(cmd_table)/sizeof(cmd_table[0]))


void process_cmd(char* cmd)
{
    /* 转小写 */
    for(char* p = cmd; *p; p++)
    {
        if(*p >= 'A' && *p <= 'Z') *p += 32;
    }
    
    /* 查找命令 */
    for(uint8_t i = 0; i < CMD_TABLE_SIZE; i++)
    {
        if(strcmp(cmd, cmd_table[i].short_name) == 0 ||
           strcmp(cmd, cmd_table[i].name) == 0)
        {
            cmd_table[i].handler();
            return;
        }
    }
    
    print_cmd("Unknown command\r\n");
}



/* 命令处理函数 */
void cmd_help_handler(void)
{
    print_cmd("\r\n==================== HELP ====================\r\n");
    print_cmd("Available commands:\r\n\r\n");
    
    for(uint8_t i = 0; i < CMD_TABLE_SIZE; i++)
    {
        print_cmd("  ");
        print_cmd(cmd_table[i].short_name);
        print_cmd(" / ");
        print_cmd(cmd_table[i].name);
        
        /* 对齐格式 */
        uint8_t len = strlen(cmd_table[i].short_name) + strlen(cmd_table[i].name) + 3;
        for(uint8_t j = len; j < 20; j++) {
            print_cmd(" ");
        }
        
        print_cmd("- ");
        print_cmd(cmd_table[i].description);
        print_cmd("\r\n");
    }
    
    print_cmd("\r\n===== Firmware Update Methods =====\r\n");
    print_cmd("XMODEM Options (u -> 1):\r\n");
    print_cmd("  1 = Internal Flash\r\n");
    print_cmd("  2 = External Flash (to backup slots)\r\n");
    print_cmd("  3 = Internal Flash (Encrypted XOR/AES)\r\n");
    print_cmd("  4 = External Flash (Encrypted + Decrypt)\r\n");
    print_cmd("\r\nOTA Options (u -> 2):\r\n");
    print_cmd("  1 = Internal Flash\r\n");
    print_cmd("  2 = External Flash (to backup slots)\r\n");
    print_cmd("  3 = Internal Flash (Encrypted XOR/AES)\r\n");
    print_cmd("  4 = External Flash (Encrypted XOR/AES)\r\n");
    print_cmd("\r\nExamples:\r\n");
    print_cmd("  h          - Show this help\r\n");
    print_cmd("  u          - Update firmware (select method)\r\n");
    print_cmd("  i          - Show system info\r\n");
    print_cmd("  w          - Connect to WiFi network\r\n");
    print_cmd("  ws         - Show WiFi connection status\r\n");
    print_cmd("  wd         - WiFi debug information\r\n");
    print_cmd("  xb         - Backup current firmware to slot 1-3\r\n");
    print_cmd("  xr         - Restore firmware from slot 0-3 (0=download)\r\n");
    print_cmd("  xl         - List all backup slots status\r\n");
    print_cmd("===============================================\r\n");
}

static uint8_t read_char(void)
{
    uint8_t ch;
    while(uart_read(DEV_UART1, &ch, 1) == 0);
    uart_write(DEV_UART1, &ch, 1);
    uart_poll_dma_tx(DEV_UART1);
    return ch;
}


/* XMODEM固件更新子菜单 */
static void cmd_update_xmodem_handler(void)
{

    const struct flash_partition *partition = NULL;
	
	print_cmd("===== XMODEM Firmware Update =====\r\n");
    partition = flash_partition_find(DOWNLOAD);
	
	
	xmodem_receive(partition, USE_1K);

}


void cmd_update_handler(void)
{
    uint8_t ch;
    
    print_cmd("===== Firmware Update =====\r\n");
    print_cmd("Select transfer method:\r\n");
    print_cmd("1 = XMODEM\r\n");
    print_cmd("2 = HTTP OTA\r\n");
    print_cmd("Select (1-2): ");
    
    ch = read_char();
    print_cmd("\r\n");
    
    if(ch == '1')
    {
        cmd_update_xmodem_handler();
    }
    else if(ch == '2')
    {
        //cmd_update_ota_handler();
    }
    else
    {
        print_cmd("Invalid selection!\r\n");
    }
}










