/*
 * STM32F1 Port - W25Q64 SPI Flash 绑定 (SPI2)
 *
 * 实际命令集/状态机/ops 实现都在通用 driver: openload/devices/w25qxx.c.
 * 这里只负责: SPI/GPIO 后端 + chip 参数注入 + 注册到 ol_flash_devs.
 *
 * 引脚 (与 legacy 一致): PB12=CS, PB13=SCK, PB14=MISO, PB15=MOSI.
 * F1 走 AFIO 默认映射, GPIO_InitTypeDef 不需要 Alternate 字段.
 */
#include "openload/devices/w25qxx.h"
#include "openload/ops/flash_ops.h"
#include "openload/errno.h"
#include "stm32f1xx_hal.h"
#include <stdint.h>

static SPI_HandleTypeDef s_hspi2;

/* -------- SPI/GPIO MspInit + Init -------- */

static void spi_msp_init(void)
{
    GPIO_InitTypeDef gi = {0};
    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* PB13/14/15 — SCK/MISO/MOSI */
    gi.Pin   = GPIO_PIN_13 | GPIO_PIN_15;
    gi.Mode  = GPIO_MODE_AF_PP;
    gi.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gi);

    gi.Pin  = GPIO_PIN_14;
    gi.Mode = GPIO_MODE_INPUT;
    gi.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &gi);

    /* PB12 — CS (软件控制) */
    gi.Pin   = GPIO_PIN_12;
    gi.Mode  = GPIO_MODE_OUTPUT_PP;
    gi.Pull  = GPIO_NOPULL;
    gi.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gi);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);     /* CS 初始 HIGH */
}

/* -------- SPI 后端 (注入 driver 的函数指针) -------- */

static int port_spi_init(void)
{
    spi_msp_init();

    s_hspi2.Instance               = SPI2;
    s_hspi2.Init.Mode              = SPI_MODE_MASTER;
    s_hspi2.Init.Direction         = SPI_DIRECTION_2LINES;
    s_hspi2.Init.DataSize          = SPI_DATASIZE_8BIT;
    s_hspi2.Init.CLKPolarity       = SPI_POLARITY_LOW;
    s_hspi2.Init.CLKPhase          = SPI_PHASE_1EDGE;
    s_hspi2.Init.NSS               = SPI_NSS_SOFT;
    s_hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;  /* APB1=36MHz/4=9MHz */
    s_hspi2.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    s_hspi2.Init.TIMode            = SPI_TIMODE_DISABLE;
    s_hspi2.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    s_hspi2.Init.CRCPolynomial     = 7;
    return (HAL_SPI_Init(&s_hspi2) == HAL_OK) ? OL_OK : OL_E_IO;
}

static uint8_t port_spi_xfer(uint8_t b)
{
    uint8_t rx;
    HAL_SPI_TransmitReceive(&s_hspi2, &b, &rx, 1, 100);
    return rx;
}

static void port_spi_write(const uint8_t *p, uint32_t n)
{
    HAL_SPI_Transmit(&s_hspi2, (uint8_t *)p, (uint16_t)n, 1000);
}

static void port_spi_read(uint8_t *p, uint32_t n)
{
    HAL_SPI_Receive(&s_hspi2, p, (uint16_t)n, 1000);
}

static void port_cs_low(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
}

static void port_cs_high(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
}

/* -------- chip 配置 + dev 注册 -------- */

static const w25qxx_config_t s_cfg = {
    .name           = "w25q64",
    .size           = 8u * 1024u * 1024u,   /* 64 Mbit */
    .expected_jedec = 0xEF4017u,            /* Winbond W25Q64 */
    .spi_init       = port_spi_init,
    .spi_xfer       = port_spi_xfer,
    .spi_write      = port_spi_write,
    .spi_read       = port_spi_read,
    .cs_low         = port_cs_low,
    .cs_high        = port_cs_high,
};

static ol_flash_dev_t s_dev = {
    .name              = "w25q64",
    .base              = 0,                  /* 非 XIP, base 无意义 */
    .size              = 8u * 1024u * 1024u,
    .sector_size       = W25QXX_SECTOR_SIZE,
    .write_granularity = 1,                  /* SPI Flash 允许任意字节写, 仅需擦后 */
    .xip               = false,
    .ops               = &w25qxx_ops,
    .priv              = (void *)&s_cfg,     /* driver ops 从这里取后端 */
};

OL_FLASH_DEV_REGISTER(w25q64, &s_dev);

/* 由 board_init 在 SPI/GPIO 初始化完成后调用. */
int port_w25q64_init(void)
{
    return w25qxx_chip_init(&s_cfg);
}
