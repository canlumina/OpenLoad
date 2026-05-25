/*
 * OpenLoad - W25QXX SPI NOR Flash 通用驱动实现
 *
 * 把 F1/F4 两个 port 里 99% 重复的 W25Q* 命令集和状态机抽到这里。
 * 与平台解耦: 时间用 ol_tick_ms/ol_delay_ms, SPI/CS 由 cfg-> 函数指针给.
 *
 * ops 函数从 ol_flash_dev_t.priv 取 w25qxx_config_t — 这样同一份 ops 实例
 * 能服务多颗 chip (port 各自声明独立 dev/cfg, 互不干扰)。
 */
#include "openload/devices/w25qxx.h"
#include "openload/ops/flash_ops.h"
#include "openload/ops/sys_ops.h"
#include "openload/errno.h"
#include <stdint.h>

/* W25Q 系列通用命令集 (与 SPI Flash 标准基本一致). */
#define CMD_WRITE_ENABLE       0x06
#define CMD_READ_STATUS1       0x05
#define CMD_READ_DATA          0x03
#define CMD_PAGE_PROGRAM       0x02
#define CMD_SECTOR_ERASE_4K    0x20
#define CMD_RELEASE_PD         0xAB
#define CMD_JEDEC_ID           0x9F

#define SR_BUSY                0x01

/* -------- 基础动作 (cfg-> 后端) -------- */

static void write_enable(const w25qxx_config_t *cfg)
{
    cfg->cs_low();
    cfg->spi_xfer(CMD_WRITE_ENABLE);
    cfg->cs_high();
}

static int wait_busy(const w25qxx_config_t *cfg, uint32_t timeout_ms)
{
    uint32_t start = ol_tick_ms();
    for (;;) {
        cfg->cs_low();
        cfg->spi_xfer(CMD_READ_STATUS1);
        uint8_t st = cfg->spi_xfer(0xFF);
        cfg->cs_high();
        if (!(st & SR_BUSY)) { return OL_OK; }
        if ((ol_tick_ms() - start) >= timeout_ms) { return OL_E_TIMEOUT; }
    }
}

/* -------- ops 实现 -------- */

static int w25_read(ol_flash_dev_t *dev, uint32_t off, void *buf, uint32_t len)
{
    const w25qxx_config_t *cfg = (const w25qxx_config_t *)dev->priv;
    if (!buf || off + len > cfg->size) { return OL_E_PART_OUT_OF_RANGE; }

    uint8_t cmd[4] = { CMD_READ_DATA,
                       (uint8_t)(off >> 16), (uint8_t)(off >> 8), (uint8_t)off };
    cfg->cs_low();
    cfg->spi_write(cmd, 4);
    cfg->spi_read(buf, len);
    cfg->cs_high();
    return OL_OK;
}

static int w25_write_page(const w25qxx_config_t *cfg, uint32_t off,
                          const uint8_t *data, uint32_t len)
{
    write_enable(cfg);
    uint8_t cmd[4] = { CMD_PAGE_PROGRAM,
                       (uint8_t)(off >> 16), (uint8_t)(off >> 8), (uint8_t)off };
    cfg->cs_low();
    cfg->spi_write(cmd, 4);
    cfg->spi_write(data, len);
    cfg->cs_high();
    return wait_busy(cfg, 50);
}

static int w25_write(ol_flash_dev_t *dev, uint32_t off, const void *buf, uint32_t len)
{
    const w25qxx_config_t *cfg = (const w25qxx_config_t *)dev->priv;
    if (!buf || off + len > cfg->size) { return OL_E_PART_OUT_OF_RANGE; }

    const uint8_t *p = (const uint8_t *)buf;
    while (len) {
        uint32_t page_off = off & (W25QXX_PAGE_SIZE - 1);
        uint32_t chunk    = W25QXX_PAGE_SIZE - page_off;
        if (chunk > len) { chunk = len; }
        int rc = w25_write_page(cfg, off, p, chunk);
        if (rc != OL_OK) { return rc; }
        off += chunk;
        p   += chunk;
        len -= chunk;
    }
    return OL_OK;
}

static int w25_erase(ol_flash_dev_t *dev, uint32_t off, uint32_t len)
{
    const w25qxx_config_t *cfg = (const w25qxx_config_t *)dev->priv;
    if ((off | len) & (W25QXX_SECTOR_SIZE - 1)) { return OL_E_PART_ALIGN; }
    if (off + len > cfg->size) { return OL_E_PART_OUT_OF_RANGE; }

    while (len) {
        write_enable(cfg);
        uint8_t cmd[4] = { CMD_SECTOR_ERASE_4K,
                           (uint8_t)(off >> 16), (uint8_t)(off >> 8), (uint8_t)off };
        cfg->cs_low();
        cfg->spi_write(cmd, 4);
        cfg->cs_high();
        int rc = wait_busy(cfg, 500);
        if (rc != OL_OK) { return rc; }
        off += W25QXX_SECTOR_SIZE;
        len -= W25QXX_SECTOR_SIZE;
    }
    return OL_OK;
}

const ol_flash_ops_t w25qxx_ops = {
    .read   = w25_read,
    .write  = w25_write,
    .erase  = w25_erase,
    .lock   = 0,
    .unlock = 0,
};

/* -------- chip 初始化 -------- */

int w25qxx_chip_init(const w25qxx_config_t *cfg)
{
    int rc = cfg->spi_init();
    if (rc != OL_OK) { return rc; }
    ol_delay_ms(10);

    /* 解除深度掉电 */
    cfg->cs_low();
    cfg->spi_xfer(CMD_RELEASE_PD);
    cfg->cs_high();
    ol_delay_ms(1);

    /* 读 JEDEC ID — 严校验 cfg->expected_jedec (0 = 跳过严格匹配) */
    cfg->cs_low();
    cfg->spi_xfer(CMD_JEDEC_ID);
    uint32_t id = ((uint32_t)cfg->spi_xfer(0xFF) << 16) |
                  ((uint32_t)cfg->spi_xfer(0xFF) <<  8) |
                  ((uint32_t)cfg->spi_xfer(0xFF));
    cfg->cs_high();

    if (id == 0 || id == 0xFFFFFFu) { return OL_E_IO; }
    if (cfg->expected_jedec != 0 && id != cfg->expected_jedec) { return OL_E_IO; }
    return OL_OK;
}
