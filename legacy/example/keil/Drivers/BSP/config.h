#ifndef __CONFIG_H_
#define __CONFIG_H_

// bootloader相关配置
#define BOOTLOADER_VERSIOIN "1.0.0"


// 串口相关配置
#define DEV_UART1	0
#define DEV_UART2	1
#define UART1_TX_BUF_SIZE           2048
#define UART1_RX_BUF_SIZE           2048
#define	UART1_DMA_RX_BUF_SIZE		1024
#define	UART1_DMA_TX_BUF_SIZE		512

#define UART2_TX_BUF_SIZE           2048
#define UART2_RX_BUF_SIZE           2048
#define	UART2_DMA_RX_BUF_SIZE		1024
#define	UART2_DMA_TX_BUF_SIZE		512

#define UART1_BAUDRATE				115200
#define UART2_BAUDRATE				115200


// flash相关配置


/* ===================== Flash device Configuration ========================= */
extern const struct flash_dev stm32_onchip_flash;
extern struct flash_dev w25q64;
/* flash device table */
#define FLASH_DEV_TABLE  \
    {                        \
        &stm32_onchip_flash, \
        &w25q64,         \
    }


/* ====================== Partition Configuration ========================== */
/* partition table */
#define FLASH_DEV_NAME_MAX 	12
#define APP_PART			"app"
#define DOWNLOAD			"download"
#define BACKUP_APP			"backup_app"
#define FLASH_PART_TABLE                                                                                        \
    {                                                                                                           \
        {FLASH_PART_MAGIC_WORD, "bootloader"   , "stm32_onchip", 0                           , 64 * 1024     ,  0},  \
        {FLASH_PART_MAGIC_WORD, APP_PART       , "stm32_onchip", 64 * 1024                   , 448 * 1024    ,  0},  \
        {FLASH_PART_MAGIC_WORD, "env"          , "w25q64"      , 0                           , 64 * 1024     ,  0},  \
        {FLASH_PART_MAGIC_WORD, DOWNLOAD       , "w25q64"      , (0 + 64) * 1024             , 448 * 1024    ,  0},  \
        {FLASH_PART_MAGIC_WORD, "backup_bl"    , "w25q64"      , (64 + 448) * 1024           , 64 * 1024     ,  0},  \
		{FLASH_PART_MAGIC_WORD, BACKUP_APP     , "w25q64"      , (64 + 448 + 64) * 1024      , 448 * 1024    ,  0},  \
        {FLASH_PART_MAGIC_WORD, "fonts"        , "w25q64"      , (64 + 448 + 64 + 448) * 1024, 5 * 1024 * 1024, 0},  \
    }
//剩余的flash，保留

	

	
	
	
	
#endif
