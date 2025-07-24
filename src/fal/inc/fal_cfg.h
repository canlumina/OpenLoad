//
// Created by Ythle on 25-7-24.
//

#ifndef FAL_CFG_H
#define FAL_CFG_H
#define FAL_NOR_FLASH_NAME "norflash"

extern struct fal_flash_dev nor_flash0;
extern struct fal_flash_dev w60x_onchip;
#define FAL_PART_TABLE_FLASH_DEV_NAME "w60x_onchip"
#define FAL_PART_TABLE_END_OFFSET 65536
/* flash device table */
#define FAL_FLASH_DEV_TABLE \
    {                       \
        &w60x_onchip,       \
        &nor_flash0,        \
    }
#endif // FAL_CFG_H
