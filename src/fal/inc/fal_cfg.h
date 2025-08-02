#ifndef _FAL_CFG_H_
#define _FAL_CFG_H_

#define FAL_DEBUG 1
#define FAL_PART_HAS_TABLE_CFG
#define FAL_USING_SFUD_PORT

/* ===================== Flash device Configuration ========================= */
extern const struct fal_flash_dev stm32_onchip_flash;
extern struct fal_flash_dev nor_flash0;
/* flash device table */
#define FAL_FLASH_DEV_TABLE  \
    {                        \
        &stm32_onchip_flash, \
        &nor_flash0,         \
    }

/* ====================== Partition Configuration ========================== */
#ifdef FAL_PART_HAS_TABLE_CFG
/* partition table */
#define FAL_PART_TABLE                                                                                    \
    {                                                                                                     \
        {FAL_PART_MAGIC_WORD, "bootloader", "stm32_onchip", 0                   , 64 * 1024        ,  0}, \
        {FAL_PART_MAGIC_WORD, "app"       , "stm32_onchip", 64 * 1024           , (512 - 64) * 1024,  0}, \
        {FAL_PART_MAGIC_WORD, "env"       , "norflash0"   , 0                   , 1024 * 1024      ,  0}, \
        {FAL_PART_MAGIC_WORD, "download"  , "norflash0"   , (1024) * 1024       , 1024 * 1024      ,  0}, \
        {FAL_PART_MAGIC_WORD, "basesys"   , "norflash0"   , (1024 + 1024) * 1024, 1024 * 1024      ,  0}, \
        {FAL_PART_MAGIC_WORD, "fonts"     , "norflash0"   , (1024 + 2048) * 1024, 5* 1024 * 1024   ,  0}, \
    }
#endif /* FAL_PART_HAS_TABLE_CFG */

#endif /* _FAL_CFG_H_ */
