/*
 * OpenLoad - IO 设备查找与超时封装
 *
 * 通过链接段 .ol_io_devs 枚举所有 OL_IO_DEV_REGISTER 注册的设备。
 * 链接脚本需将 __ol_io_devs_start/__ol_io_devs_end 提供为符号。
 */
#include "openload/ops/io_ops.h"
#include "openload/ops/sys_ops.h"
#include "openload/errno.h"
#include <string.h>
#include <stddef.h>

extern ol_io_dev_t * const __ol_io_devs_start[];
extern ol_io_dev_t * const __ol_io_devs_end[];

ol_io_dev_t *ol_io_dev_find(const char *name)
{
    if (!name) {
        return NULL;
    }
    for (ol_io_dev_t * const *it = __ol_io_devs_start;
         it != __ol_io_devs_end; ++it) {
        if (*it && (*it)->name && strcmp((*it)->name, name) == 0) {
            return *it;
        }
    }
    return NULL;
}

int ol_io_read_timeout(ol_io_dev_t *dev, uint8_t *buf,
                       uint32_t len, uint32_t timeout_ms)
{
    if (!dev || !dev->ops || !dev->ops->read || !buf) {
        return OL_E_INVAL;
    }
    uint32_t got   = 0;
    uint32_t start = ol_tick_ms();
    while (got < len) {
        int n = dev->ops->read(dev, buf + got, len - got);
        if (n < 0) {
            return n;
        }
        got += (uint32_t)n;
        if (got >= len) {
            break;
        }
        if ((ol_tick_ms() - start) >= timeout_ms) {
            break;
        }
    }
    return (int)got;
}

int ol_io_putc(ol_io_dev_t *dev, uint8_t c)
{
    if (!dev || !dev->ops || !dev->ops->write) {
        return OL_E_INVAL;
    }
    return dev->ops->write(dev, &c, 1);
}

int ol_io_getc_timeout(ol_io_dev_t *dev, uint8_t *c, uint32_t timeout_ms)
{
    return ol_io_read_timeout(dev, c, 1, timeout_ms) == 1 ? OL_OK : OL_E_TIMEOUT;
}
