/*
 * STM32F4 Port - W25Q16 SPI Flash ops (SPI1)
 *
 * DevEBox MCUDev STM32F407 核心板 板载 U3 = W25Q16, 接 SPI1:
 *   PA15 = CS   (GPIO output, 软件控制)
 *   PB3  = SCK  (AF5_SPI1)
 *   PB4  = MISO (AF5_SPI1, 内部上拉避免空闲飘)
 *   PB5  = MOSI (AF5_SPI1)
 *
 * 注意 PA15/PB3/PB4 上电默认是 JTAG 引脚 (JTDI/JTDO/NTRST). F4 调用
 * HAL_GPIO_Init 把它们改成 AF/GPIO 后, JTAG 不可用; SWD (PA13/PA14)
 * 仍正常 — 本项目只用 SWD 烧录调试, 不受影响.
 *
 * 容量 2MB (16Mbit), sector=4KB, page=256B, JEDEC ID = 0xEF4015.
 *
 * 该文件内部完成 SPI1 初始化, 不引 spi.c.
 */
#include "openload/ops/flash_ops.h"
#include "openload/ops/sys_ops.h"
#include "openload/errno.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdint.h>

#define W25Q16_SIZE            (2u * 1024u * 1024u)   /* 16 Mbit */
#define W25Q16_SECTOR_SIZE     (4u * 1024u)
#define W25Q16_PAGE_SIZE       256u

#define CMD_WRITE_ENABLE       0x06
#define CMD_READ_STATUS1       0x05
#define CMD_READ_DATA          0x03
#define CMD_PAGE_PROGRAM       0x02
#define CMD_SECTOR_ERASE_4K    0x20
#define CMD_RELEASE_PD         0xAB
#define CMD_JEDEC_ID           0x9F

#define SR_BUSY                0x01

/* CS 在 PA15. */
#define CS_LOW()               HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET)
#define CS_HIGH()              HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET)

static SPI_HandleTypeDef s_hspi1;

/* -------- SPI/GPIO 初始化 -------- */
static void spi_msp_init(void)
{
    GPIO_InitTypeDef gi = {0};
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* PB3/PB5 — SCK/MOSI, F4 必须显式设 AF5 = SPI1.
     * 写 AF 时 HAL 会自动覆盖 PB3 的 JTDO 默认功能. */
    gi.Pin       = GPIO_PIN_3 | GPIO_PIN_5;
    gi.Mode      = GPIO_MODE_AF_PP;
    gi.Pull      = GPIO_NOPULL;
    gi.Speed     = GPIO_SPEED_FREQ_HIGH;
    gi.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOB, &gi);

    /* PB4 — MISO, 加内部上拉避免空闲飘 (覆盖 NTRST 默认) */
    gi.Pin       = GPIO_PIN_4;
    gi.Mode      = GPIO_MODE_AF_PP;
    gi.Pull      = GPIO_PULLUP;
    gi.Speed     = GPIO_SPEED_FREQ_HIGH;
    gi.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOB, &gi);

    /* PA15 — CS (软件控制), 普通输出 (覆盖 JTDI 默认) */
    gi.Pin       = GPIO_PIN_15;
    gi.Mode      = GPIO_MODE_OUTPUT_PP;
    gi.Pull      = GPIO_NOPULL;
    gi.Speed     = GPIO_SPEED_FREQ_HIGH;
    gi.Alternate = 0;
    HAL_GPIO_Init(GPIOA, &gi);
    CS_HIGH();
}

static int spi_init(void)
{
    spi_msp_init();

    s_hspi1.Instance               = SPI1;
    s_hspi1.Init.Mode              = SPI_MODE_MASTER;
    s_hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    s_hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    s_hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;
    s_hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;
    s_hspi1.Init.NSS               = SPI_NSS_SOFT;
    /* SPI1 挂 APB2 (84MHz). 分频 16 = 5.25MHz, W25Q16 极限 104MHz, 留余量. */
    s_hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
    s_hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    s_hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    s_hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    s_hspi1.Init.CRCPolynomial     = 7;
    return (HAL_SPI_Init(&s_hspi1) == HAL_OK) ? OL_OK : OL_E_IO;
}

static uint8_t spi_xfer(uint8_t b)
{
    uint8_t rx;
    HAL_SPI_TransmitReceive(&s_hspi1, &b, &rx, 1, 100);
    return rx;
}

static void spi_write(const uint8_t *p, uint32_t n)
{
    HAL_SPI_Transmit(&s_hspi1, (uint8_t *)p, (uint16_t)n, 1000);
}

static void spi_read(uint8_t *p, uint32_t n)
{
    HAL_SPI_Receive(&s_hspi1, p, (uint16_t)n, 1000);
}

/* -------- 基础动作 -------- */
static void write_enable(void)
{
    CS_LOW();
    spi_xfer(CMD_WRITE_ENABLE);
    CS_HIGH();
}

static int wait_busy(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    for (;;) {
        CS_LOW();
        spi_xfer(CMD_READ_STATUS1);
        uint8_t st = spi_xfer(0xFF);
        CS_HIGH();
        if (!(st & SR_BUSY)) { return OL_OK; }
        if ((HAL_GetTick() - start) >= timeout_ms) { return OL_E_TIMEOUT; }
    }
}

/* -------- ops 实现 -------- */
static int w25_read(ol_flash_dev_t *dev, uint32_t off, void *buf, uint32_t len)
{
    (void)dev;
    if (!buf || off + len > W25Q16_SIZE) { return OL_E_PART_OUT_OF_RANGE; }
    uint8_t cmd[4] = { CMD_READ_DATA,
                       (uint8_t)(off >> 16), (uint8_t)(off >> 8), (uint8_t)off };
    CS_LOW();
    spi_write(cmd, 4);
    spi_read(buf, len);
    CS_HIGH();
    return OL_OK;
}

static int w25_write_page(uint32_t off, const uint8_t *data, uint32_t len)
{
    write_enable();
    uint8_t cmd[4] = { CMD_PAGE_PROGRAM,
                       (uint8_t)(off >> 16), (uint8_t)(off >> 8), (uint8_t)off };
    CS_LOW();
    spi_write(cmd, 4);
    spi_write(data, len);
    CS_HIGH();
    return wait_busy(50);
}

static int w25_write(ol_flash_dev_t *dev, uint32_t off, const void *buf, uint32_t len)
{
    (void)dev;
    if (!buf || off + len > W25Q16_SIZE) { return OL_E_PART_OUT_OF_RANGE; }
    const uint8_t *p = (const uint8_t *)buf;
    while (len) {
        uint32_t page_off = off & (W25Q16_PAGE_SIZE - 1);
        uint32_t chunk    = W25Q16_PAGE_SIZE - page_off;
        if (chunk > len) { chunk = len; }
        int rc = w25_write_page(off, p, chunk);
        if (rc != OL_OK) { return rc; }
        off += chunk;
        p   += chunk;
        len -= chunk;
    }
    return OL_OK;
}

static int w25_erase(ol_flash_dev_t *dev, uint32_t off, uint32_t len)
{
    (void)dev;
    if ((off | len) & (W25Q16_SECTOR_SIZE - 1)) { return OL_E_PART_ALIGN; }
    if (off + len > W25Q16_SIZE) { return OL_E_PART_OUT_OF_RANGE; }
    while (len) {
        write_enable();
        uint8_t cmd[4] = { CMD_SECTOR_ERASE_4K,
                           (uint8_t)(off >> 16), (uint8_t)(off >> 8), (uint8_t)off };
        CS_LOW();
        spi_write(cmd, 4);
        CS_HIGH();
        int rc = wait_busy(500);
        if (rc != OL_OK) { return rc; }
        off += W25Q16_SECTOR_SIZE;
        len -= W25Q16_SECTOR_SIZE;
    }
    return OL_OK;
}

static const ol_flash_ops_t s_w25_ops = {
    .read   = w25_read,
    .write  = w25_write,
    .erase  = w25_erase,
    .lock   = 0,
    .unlock = 0,
};

static ol_flash_dev_t s_w25_dev = {
    .name              = "w25q16",
    .base              = 0,             /* 非 XIP, base 无意义 */
    .size              = W25Q16_SIZE,
    .sector_size       = W25Q16_SECTOR_SIZE,
    .write_granularity = 1,             /* SPI Flash 允许任意字节写, 仅需擦后 */
    .xip               = false,
    .ops               = &s_w25_ops,
    .priv              = 0,
};

OL_FLASH_DEV_REGISTER(w25q16, &s_w25_dev);

/* 由 board_init 在 SPI/GPIO 初始化完成后调用. */
int port_w25q16_init(void)
{
    int rc = spi_init();
    if (rc != OL_OK) { return rc; }
    HAL_Delay(10);
    /* 解除深度掉电 */
    CS_LOW();
    spi_xfer(CMD_RELEASE_PD);
    CS_HIGH();
    HAL_Delay(1);
    /* 读 JEDEC ID 验证存在性 (Winbond W25Q16 = 0xEF4015) */
    CS_LOW();
    spi_xfer(CMD_JEDEC_ID);
    uint32_t id = ((uint32_t)spi_xfer(0xFF) << 16) |
                  ((uint32_t)spi_xfer(0xFF) <<  8) |
                  ((uint32_t)spi_xfer(0xFF));
    CS_HIGH();
    return (id != 0 && id != 0xFFFFFFu) ? OL_OK : OL_E_IO;
}
