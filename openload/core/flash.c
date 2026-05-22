/*
 * OpenLoad - Flash 设备查找
 *
 * 与 io.c 同样, 通过链接段 .ol_flash_devs 枚举设备。
 */
#include "openload/ops/flash_ops.h"
#include <string.h>
#include <stddef.h>

extern ol_flash_dev_t * const __ol_flash_devs_start[];
extern ol_flash_dev_t * const __ol_flash_devs_end[];

ol_flash_dev_t *ol_flash_dev_find(const char *name)
{
    if (!name) {
        return NULL;
    }
    for (ol_flash_dev_t * const *it = __ol_flash_devs_start;
         it != __ol_flash_devs_end; ++it) {
        if (*it && (*it)->name && strcmp((*it)->name, name) == 0) {
            return *it;
        }
    }
    return NULL;
}
