#include <string.h>
#include "w25q64.h"
#include "dev_flash.h"
#include "dev_usart.h"


static const struct flash_dev * const device_table[] = FLASH_DEV_TABLE;
static const size_t device_table_len = sizeof(device_table) / sizeof(device_table[0]);

static const struct flash_partition partition_table_def[] = FLASH_PART_TABLE;
static const struct flash_partition *partition_table = NULL;
static struct part_flash_info part_flash_cache[sizeof(partition_table_def) / sizeof(partition_table_def[0])] = { 0 };

static uint8_t init_ok = 0;
static size_t partition_table_len = 0;



int flash_device_init(void)
{
    size_t i;

    if (init_ok)
    {
        return 0;
    }

    for (i = 0; i < device_table_len; i++)
    {
        assert(device_table[i]->ops.read);
        assert(device_table[i]->ops.write);
        assert(device_table[i]->ops.erase);
        /* init flash device on flash table */
        if (device_table[i]->ops.init)
        {
            device_table[i]->ops.init();
        }
        log_f("Flash device | %*.*s | addr: 0x%08x | len: 0x%08x | blk_size: 0x%08x |initialized finish.",
                FLASH_DEV_NAME_MAX, FLASH_DEV_NAME_MAX, device_table[i]->name, device_table[i]->addr, device_table[i]->len,
                device_table[i]->blk_size);
    }

    init_ok = 1;
    return 0;
}

/**
 * find flash device by name
 *
 * @param name flash device name
 *
 * @return != NULL: flash device
 *            NULL: not found
 */
const struct flash_dev *flash_device_find(const char *name)
{
    assert(init_ok);
    assert(name);

    size_t i;

    for (i = 0; i < device_table_len; i++)
    {
        if (!strncmp(name, device_table[i]->name, FLASH_DEV_NAME_MAX)) {
            return device_table[i];
        }
    }

    return NULL;
}


/**
 * print the partition table
 */
void flash_show_part_table(void)
{
    char *item1 = "name", *item2 = "flash_dev";
    size_t i, part_name_max = strlen(item1), flash_dev_name_max = strlen(item2);
    const struct flash_partition *part;

    if (partition_table_len)
    {
        for (i = 0; i < partition_table_len; i++)
        {
            part = &partition_table[i];
            if (strlen(part->name) > part_name_max)
            {
                part_name_max = strlen(part->name);
            }
            if (strlen(part->flash_name) > flash_dev_name_max)
            {
                flash_dev_name_max = strlen(part->flash_name);
            }
        }
    }
    log_i("==================== flash partition table ====================");
    log_f("| %-*.*s | %-*.*s |   offset   |    length  |", part_name_max, FLASH_DEV_NAME_MAX, item1, flash_dev_name_max,
            FLASH_DEV_NAME_MAX, item2);
    log_i("---------------------------------------------------------------");
    for (i = 0; i < partition_table_len; i++)
    {
        part = &partition_table[i];
		
        log_f("| %-*.*s | %-*.*s | 0x%08lx | 0x%08x |", part_name_max, FLASH_DEV_NAME_MAX, part->name, flash_dev_name_max,
                FLASH_DEV_NAME_MAX, part->flash_name, part->offset, part->len);
    }
    log_i("===============================================================");
}


static int check_and_update_part_cache(const struct flash_partition *table, size_t len)
{
    const struct flash_dev *flash_device = NULL;
    size_t i;

    for (i = 0; i < len; i++)
    {
        flash_device = flash_device_find(table[i].flash_name);
        if (flash_device == NULL)
        {
            log_f("Warning: Do NOT found the flash device(%s).", table[i].flash_name);
            continue;
        }

        if (table[i].offset >= (long)flash_device->len)
        {
            log_f("Initialize failed! Partition(%s) offset address(%ld) out of flash bound(<%d).",
                    table[i].name, table[i].offset, flash_device->len);
            partition_table_len = 0;

            return -1;
        }

        part_flash_cache[i].flash_dev = flash_device;
    }

    return 0;
}


/**
 * Initialize all flash partition on FAL partition table
 *
 * @return partitions total number
 */
int flash_partition_init(void)
{

    if (!init_ok)
    {
        return partition_table_len;
    }

    partition_table = &partition_table_def[0];
    partition_table_len = sizeof(partition_table_def) / sizeof(partition_table_def[0]);
 
    /* check the partition table device exists */
    if (check_and_update_part_cache(partition_table, partition_table_len) != 0)
    {
        goto _exit;
    }

    init_ok = 1;

_exit:
    flash_show_part_table();

    return partition_table_len;
}


/**
 * find the partition by name
 *
 * @param name partition name
 *
 * @return != NULL: partition
 *            NULL: not found
 */
const struct flash_partition *flash_partition_find(const char *name)
{
    assert(init_ok);

    size_t i;

    for (i = 0; i < partition_table_len; i++)
    {
        if (!strcmp(name, partition_table[i].name))
        {
            return &partition_table[i];
        }
    }

    return NULL;
}

static const struct flash_dev *flash_device_find_by_part(const struct flash_partition *part)
{
    assert(part >= partition_table);
    assert(part <= &partition_table[partition_table_len - 1]);

    return part_flash_cache[part - partition_table].flash_dev;
}

/**
 * get the partition table
 *
 * @param len return the partition table length
 *
 * @return partition table
 */
const struct flash_partition *flash_get_partition_table(size_t *len)
{
   assert(init_ok);
   assert(len);

    *len = partition_table_len;

    return partition_table;
}

/**
 * read data from partition
 *
 * @param part partition
 * @param addr relative address for partition
 * @param buf read buffer
 * @param size read size
 *
 * @return >= 0: successful read data size
 *           -1: error
 */
int flash_partition_read(const struct flash_partition *part, uint32_t addr, uint8_t *buf, size_t size)
{
    int ret = 0;
    const struct flash_dev *flash_dev = NULL;

    assert(part);
    assert(buf);

    if (addr + size > part->len)
    {
        log_i("Partition read error! Partition address out of bound.");
        return -1;
    }

    flash_dev = flash_device_find(part->flash_name);
    if (flash_dev == NULL)
    {
        log_f("Partition read error! Don't found flash device(%s) of the partition(%s).", part->flash_name, part->name);
        return -1;
    }

    ret = flash_dev->ops.read(part->offset + addr, buf, size);
    if (ret < 0)
    {
        log_f("Partition read error! Flash device(%s) read error!", part->flash_name);
    }

    return ret;
}

/**
 * write data to partition
 *
 * @param part partition
 * @param addr relative address for partition
 * @param buf write buffer
 * @param size write size
 *
 * @return >= 0: successful write data size
 *           -1: error
 */
int flash_partition_write(const struct flash_partition *part, uint32_t addr, const uint8_t *buf, size_t size)
{
    int ret = 0;
    const struct flash_dev *flash_dev = NULL;

    assert(part);
    assert(buf);

    if (addr + size > part->len)
    {
        log_i("Partition write error! Partition address out of bound.");
        return -1;
    }

    flash_dev = flash_device_find(part->flash_name);
    if (flash_dev == NULL)
    {
        log_f("Partition write error!  Don't found flash device(%s) of the partition(%s).", part->flash_name, part->name);
        return -1;
    }

    ret = flash_dev->ops.write(part->offset + addr, buf, size);
    if (ret < 0)
    {
        log_f("Partition write error! Flash device(%s) write error!", part->flash_name);
    }

    return ret;
}

/**
 * erase partition data
 *
 * @param part partition
 * @param addr relative address for partition
 * @param size erase size
 *
 * @return >= 0: successful erased data size
 *           -1: error
 */
int flash_partition_erase(const struct flash_partition *part, uint32_t addr, size_t size)
{
    int ret = 0;
    const struct flash_dev *flash_dev = NULL;

    assert(part);

    if (addr + size > part->len)
    {
        log_i("Partition erase error! Partition address out of bound.");
        return -1;
    }

    //flash_dev = fal_flash_device_find(part->flash_name);
		flash_dev = flash_device_find_by_part(part);
    if (flash_dev == NULL)
    {
        log_f("Partition erase error! Don't found flash device(%s) of the partition(%s).", part->flash_name, part->name);
        return -1;
    }

    ret = flash_dev->ops.erase(part->offset + addr, size);
    if (ret < 0)
    {
        log_f("Partition erase error! Flash device(%s) erase error!", part->flash_name);
    }

    return ret;
}


/**
 * erase partition all data
 *
 * @param part partition
 *
 * @return >= 0: successful erased data size
 *           -1: error
 */
int flash_partition_erase_all(const struct flash_partition *part)
{
    return flash_partition_erase(part, 0, part->len);
}





int flash_init(void)
{
    int result;

    /* initialize all flash device on FAL flash table */
    result = flash_device_init();

    if (result < 0) {
        goto __exit;
    }

    /* initialize all flash partition on FAL partition table */
    result = flash_partition_init();

__exit:

    if ((result > 0) && (init_ok))
    {
        init_ok = 1;
        log_i("Flash Abstraction Layer initialize success.");
    }
    else if(result <= 0)
    {
        init_ok = 0;
        log_i("Flash Abstraction Layer initialize failed.");
    }

    return result;
}


/**
 * Check if the FAL is initialized successfully
 * 
 * @return 0: not init or init failed; 1: init success
 */
int flash_init_check(void)
{
    return init_ok;
}
