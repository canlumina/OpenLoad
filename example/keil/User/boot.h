#ifndef __BOOT_H
#define __BOOT_H

// 定义外部Flash相关宏

// 默认固件的分区信息
// 默认固件的起始地址和大小(扇区从0~111)一共112个扇区
#define STM32_DEFAULT_FLASH_BASE 0x0
#define STM32_DEFAULT_FLASH_SIZE 0x70000
#define STM32_DEFAULT_FLASH_SECTOR_SIZE 112

// 新固件的分区信息
//  新固件的起始地址和大小(扇区从112~223)一共112个扇区
#define STM32_NEW_FLASH_BASE 0x70000
#define STM32_NEW_FLASH_SIZE 0x70000
#define STM32_NEW_FLASH_SECTOR_SIZE 112

void boot_main(void);
void show_usage(void);
#endif
