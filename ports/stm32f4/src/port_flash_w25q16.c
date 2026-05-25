/*
 * STM32F4 Port - W25Q16 SPI Flash 绑定 (SPI1)
 *
 * 实际命令集/状态机/ops 实现都在通用 driver: openload/devices/w25qxx.c.
 * 这里只负责: SPI/GPIO 后端 + chip 参数注入 + 注册到 ol_flash_devs.
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
 */
#include "openload/devices/w25qxx.h"
#include "openload/ops/flash_ops.h"
#include "openload/errno.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>

static SPI_HandleTypeDef s_hspi1;

/* -------- SPI/GPIO MspInit + Init -------- */

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
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);    /* CS 初始 HIGH */
}

/* -------- SPI 后端 (注入 driver 的函数指针) -------- */

static int port_spi_init(void)
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

static uint8_t port_spi_xfer(uint8_t b)
{
    uint8_t rx;
    HAL_SPI_TransmitReceive(&s_hspi1, &b, &rx, 1, 100);
    return rx;
}

static void port_spi_write(const uint8_t *p, uint32_t n)
{
    HAL_SPI_Transmit(&s_hspi1, (uint8_t *)p, (uint16_t)n, 1000);
}

static void port_spi_read(uint8_t *p, uint32_t n)
{
    HAL_SPI_Receive(&s_hspi1, p, (uint16_t)n, 1000);
}

static void port_cs_low(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);
}

static void port_cs_high(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
}

/* -------- chip 配置 + dev 注册 -------- */

static const w25qxx_config_t s_cfg = {
    .name           = "w25q16",
    .size           = 2u * 1024u * 1024u,   /* 16 Mbit */
    .expected_jedec = 0xEF4015u,            /* Winbond W25Q16 */
    .spi_init       = port_spi_init,
    .spi_xfer       = port_spi_xfer,
    .spi_write      = port_spi_write,
    .spi_read       = port_spi_read,
    .cs_low         = port_cs_low,
    .cs_high        = port_cs_high,
};

static ol_flash_dev_t s_dev = {
    .name              = "w25q16",
    .base              = 0,                  /* 非 XIP, base 无意义 */
    .size              = 2u * 1024u * 1024u,
    .sector_size       = W25QXX_SECTOR_SIZE,
    .write_granularity = 1,                  /* SPI Flash 允许任意字节写, 仅需擦后 */
    .xip               = false,
    .ops               = &w25qxx_ops,
    .priv              = (void *)&s_cfg,     /* driver ops 从这里取后端 */
};

OL_FLASH_DEV_REGISTER(w25q16, &s_dev);

/* 由 board_init 在 SPI/GPIO 初始化完成后调用. */
int port_w25q16_init(void)
{
    return w25qxx_chip_init(&s_cfg);
}
