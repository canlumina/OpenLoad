#ifndef __DEV_FLASH_H_
#define __DEV_FLASH_H_

#include <stdint.h>
#include <stddef.h>
#include "config.h"
#include "dev_usart.h"

#ifndef FLASH_DEV_NAME_MAX
#define FLASH_DEV_NAME_MAX 24
#endif


/* partition magic word */
#define FLASH_PART_MAGIC_WORD         0x45503130
#define FLASH_PART_MAGIC_WORD_H       0x4550L
#define FLASH_PART_MAGIC_WORD_L       0x3130L

/* 分区信息结构 */
struct flash_partition
{
    uint32_t magic_word;

    /* partition name */
    char name[FLASH_DEV_NAME_MAX];
    /* flash device name for partition */
    char flash_name[FLASH_DEV_NAME_MAX];

    /* partition offset address on flash device */
    long offset;
    size_t len;

    uint32_t reserved;
};
typedef struct flash_partition *flash_partition_t;;


/* flash设备基本结构 */
struct flash_dev
{
    char name[FLASH_DEV_NAME_MAX];

    /* flash device start address and len  */
    uint32_t addr;
    size_t len;
    /* the block size in the flash for erase minimum granularity */
    size_t blk_size;

    struct
    {
        int (*init)(void);
        int (*read)(long offset, uint8_t *buf, size_t size);
        int (*write)(long offset, const uint8_t *buf, size_t size);
        int (*erase)(long offset, size_t size);
    } ops;

    /* write minimum granularity, unit: bit. 
       1(nor flash)/ 8(stm32f2/f4)/ 32(stm32f1)/ 64(stm32l4)
       0 will not take effect. */
    size_t write_gran;
};
typedef struct flash_dev *flash_dev_t;

struct part_flash_info
{
    const struct flash_dev *flash_dev;
};

	
#define assert(EXPR)                                                           \
if (!(EXPR))                                                                   \
{                                                                              \
    uart_printf("(%s) has assert failed at %s.\r\n", #EXPR, __func__ );        \
    while (1);                                                                 \
}


int flash_init(void);
const struct flash_partition *flash_partition_find(const char *name);
const struct flash_dev *flash_device_find(const char *name);
int flash_partition_erase_all(const struct flash_partition *part);
int flash_partition_read(const struct flash_partition *part, uint32_t addr, uint8_t *buf, size_t size);
int flash_partition_write(const struct flash_partition *part, uint32_t addr, const uint8_t *buf, size_t size);
#endif
