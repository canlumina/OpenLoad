/*
 * OpenLoad - W25QXX SPI NOR Flash 通用驱动接口
 *
 * 适配 Winbond W25Q 系列 (W25Q16/Q32/Q64/Q128 等), 命令集 + 时序通用化.
 * Driver 不知道具体 SPI/GPIO 怎么调, 由 port 通过 w25qxx_config_t 注入后端.
 *
 * 用法 (在 port 里):
 *   1. 实现 6 个 SPI/CS 后端函数 (spi_init/spi_xfer/spi_write/spi_read/cs_low/cs_high)
 *   2. 声明 static 的 w25qxx_config_t s_cfg = { .name, .size, .expected_jedec, 后端... }
 *   3. 声明 static 的 ol_flash_dev_t s_dev, .ops = &w25qxx_ops, .priv = &s_cfg
 *   4. OL_FLASH_DEV_REGISTER(<id>, &s_dev)
 *   5. board_init 调 w25qxx_chip_init(&s_cfg) 一次, 完成 RELEASE_PD + JEDEC ID 严校验
 */
#pragma once

#include <stdint.h>
#include "openload/ops/flash_ops.h"

/* W25Q 系列共有的固定参数. Port shim 把这俩用于 ol_flash_dev_t. */
#define W25QXX_PAGE_SIZE       256u
#define W25QXX_SECTOR_SIZE     4096u

typedef struct {
    const char *name;             /* 仅 driver 内部用; ol_flash_dev_t.name 须独立设 */
    uint32_t    size;             /* chip 总容量 (byte), 例 2MB 写 2u*1024u*1024u */
    uint32_t    expected_jedec;   /* 期望 JEDEC ID, 例 W25Q16=0xEF4015 / W25Q64=0xEF4017.
                                     0 = 不严校验, 仅检查 id != 0 && != 0xFFFFFF */

    /* --- SPI 后端 (port 实现) --- */
    /** 初始化 SPI 外设 + GPIO + CS pin. 完成后 CS 须保持 HIGH. */
    int     (*spi_init)(void);
    /** 全双工交换一字节, 返回 MISO 读到的字节. */
    uint8_t (*spi_xfer)(uint8_t b);
    /** 单向写 n 字节, 丢弃 MISO. */
    void    (*spi_write)(const uint8_t *p, uint32_t n);
    /** 单向读 n 字节 (MOSI 通常发 0xFF). */
    void    (*spi_read)(uint8_t *p, uint32_t n);
    /** CS 拉低 (片选有效, 开始一次 SPI 事务). */
    void    (*cs_low)(void);
    /** CS 拉高 (片选释放, 结束 SPI 事务). */
    void    (*cs_high)(void);
} w25qxx_config_t;

/**
 * 通用 ops 实现. Port shim 把它指给 ol_flash_dev_t.ops.
 * 注意: dev->priv 必须指向 w25qxx_config_t 实例, ops 函数从 priv 取后端.
 */
extern const ol_flash_ops_t w25qxx_ops;

/**
 * @brief 初始化 chip: 调 cfg->spi_init, 解除深度掉电, 严校验 JEDEC ID.
 * @return  OL_OK / OL_E_IO (SPI init 失败或 JEDEC 不匹配)
 */
int w25qxx_chip_init(const w25qxx_config_t *cfg);
