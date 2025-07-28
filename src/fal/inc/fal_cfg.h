#ifndef _FAL_CFG_H_
#define _FAL_CFG_H_

#define FAL_DEBUG 1
#define FAL_PART_HAS_TABLE_CFG

/* ===================== Flash device Configuration ========================= */
extern const struct fal_flash_dev stm32_onchip_flash;

/* flash device table */
#define FAL_FLASH_DEV_TABLE  \
    {                        \
        &stm32_onchip_flash, \
    }

/* ====================== Partition Configuration ========================== */
#ifdef FAL_PART_HAS_TABLE_CFG
/* partition table */
#define FAL_PART_TABLE                                                        \
    {                                                                         \
        {FAL_PART_MAGIC_WORD, "bootloader", "stm32_onchip", 0, 64 * 1024, 0}, \
        {FAL_PART_MAGIC_WORD, "app",        "stm32_onchip", 64 * 1024, (512 - 64 - 4) * 1024, 0}, \
        {FAL_PART_MAGIC_WORD, "env",        "stm32_onchip", 508 * 1024,  4 * 1024, 0},  \
    }
#endif /* FAL_PART_HAS_TABLE_CFG */

#endif /* _FAL_CFG_H_ */
