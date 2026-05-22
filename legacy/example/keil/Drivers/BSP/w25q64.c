#include <string.h>
#include <stdio.h>
#include "dev_flash.h"
#include "w25q64.h"
#include "main.h"
#include "gpio.h"

/* SPI接口函数 - 需要用户实现 */
void w25q64_cs_low(void);
void w25q64_cs_high(void);
uint8_t w25q64_spi_transmit(uint8_t data);

/* CS引脚控制 - 使用PB12 */
static void w25q64_cs_low(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
}

static void w25q64_cs_high(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
}

/* SPI单字节传输 */
static uint8_t w25q64_spi_transmit(uint8_t data)
{
    uint8_t rx_data;
    HAL_SPI_TransmitReceive(&hspi2, &data, &rx_data, 1, 100);
    return rx_data;
}



/* 读取芯片ID */
uint32_t w25q64_read_id(void)
{
    uint32_t id = 0;
    uint8_t cmd = W25Q64_CMD_JEDEC_ID;
    uint8_t buffer[3];
    
    w25q64_cs_low();
    w25q64_spi_transmit(cmd);
    buffer[0] = w25q64_spi_transmit(0xFF);
    buffer[1] = w25q64_spi_transmit(0xFF);
    buffer[2] = w25q64_spi_transmit(0xFF);
    w25q64_cs_high();
    
    id = (buffer[0] << 16) | (buffer[1] << 8) | buffer[2];
    return id;
}

/* 检查芯片是否忙 */
static bool w25q64_is_busy(void)
{
    uint8_t status;
    
    w25q64_cs_low();
    w25q64_spi_transmit(W25Q64_CMD_READ_STATUS_REG1);
    status = w25q64_spi_transmit(0xFF);
    w25q64_cs_high();
    
    return (status & W25Q64_SR_BUSY) != 0;
}

/* 等待芯片空闲 */
static void w25q64_wait_busy(void)
{
    uint32_t timeout = 0;
    while(w25q64_is_busy())
    {
        HAL_Delay(10);
        if(++timeout > 500) // 5秒超时 (500 * 10ms)
        {
            break;
        }
    }
}

/* 写使能 */
static void w25q64_write_enable(void)
{
    w25q64_cs_low();
    w25q64_spi_transmit(W25Q64_CMD_WRITE_ENABLE);
    w25q64_cs_high();
}

/* 写禁止 */
static void w25q64_write_disable(void)
{
    w25q64_cs_low();
    w25q64_spi_transmit(W25Q64_CMD_WRITE_DISABLE);
    w25q64_cs_high();
}


/* 快速读取数据 保留*/
void w25q64_fast_read(uint32_t addr, uint8_t* buffer, uint32_t size)
{
    w25q64_cs_low();
    w25q64_spi_transmit(W25Q64_CMD_FAST_READ);
    w25q64_spi_transmit((addr >> 16) & 0xFF);
    w25q64_spi_transmit((addr >> 8) & 0xFF);
    w25q64_spi_transmit(addr & 0xFF);
    w25q64_spi_transmit(0xFF); // Dummy byte
    
    for(uint32_t i = 0; i < size; i++)
    {
        buffer[i] = w25q64_spi_transmit(0xFF);
    }
    
    w25q64_cs_high();
}

/* 写一页数据 */
static bool w25q64_write_page(uint32_t addr, const uint8_t* data, uint32_t size)
{
    if(size > W25Q64_PAGE_SIZE)
    {
        size = W25Q64_PAGE_SIZE;
    }
    
    /* 计算页内剩余空间 */
    uint32_t page_remain = W25Q64_PAGE_SIZE - (addr % W25Q64_PAGE_SIZE);
    if(size > page_remain)
    {
        size = page_remain;
    }
    
    w25q64_write_enable();
    
    w25q64_cs_low();
    w25q64_spi_transmit(W25Q64_CMD_PAGE_PROGRAM);
    w25q64_spi_transmit((addr >> 16) & 0xFF);
    w25q64_spi_transmit((addr >> 8) & 0xFF);
    w25q64_spi_transmit(addr & 0xFF);
    
    for(uint32_t i = 0; i < size; i++)
    {
        w25q64_spi_transmit(data[i]);
    }
	
    w25q64_cs_high();
    w25q64_wait_busy();
	
  	w25q64_write_disable();  
    return true;
}

/* 擦除扇区 (4KB) */
bool w25q64_erase_sector(uint32_t addr)
{
    addr = addr & 0xFFFFF000; // 对齐到扇区边界
    
    w25q64_write_enable();
    
    w25q64_cs_low();
    w25q64_spi_transmit(W25Q64_CMD_SECTOR_ERASE);
    w25q64_spi_transmit((addr >> 16) & 0xFF);
    w25q64_spi_transmit((addr >> 8) & 0xFF);
    w25q64_spi_transmit(addr & 0xFF);
    w25q64_cs_high();
    
    w25q64_wait_busy();
    
    return true;
}

/* 擦除块 (64KB) */
bool w25q64_erase_block(uint32_t addr)
{
    addr = addr & 0xFFFF0000; // 对齐到块边界
    
    w25q64_write_enable();
    
    w25q64_cs_low();
    w25q64_spi_transmit(W25Q64_CMD_BLOCK_ERASE_64K);
    w25q64_spi_transmit((addr >> 16) & 0xFF);
    w25q64_spi_transmit((addr >> 8) & 0xFF);
    w25q64_spi_transmit(addr & 0xFF);
    w25q64_cs_high();
    
    w25q64_wait_busy();
    
    return true;
}

/* 擦除整个芯片 */
bool w25q64_erase_chip(void)
{
    w25q64_write_enable();
    
    w25q64_cs_low();
    w25q64_spi_transmit(W25Q64_CMD_CHIP_ERASE);
    w25q64_cs_high();
    
    w25q64_wait_busy();
    
    return true;
}

/* 电源关闭 */
void w25q64_power_down(void)
{
    w25q64_cs_low();
    w25q64_spi_transmit(W25Q64_CMD_POWER_DOWN);
    w25q64_cs_high();
    HAL_Delay(1);
}

/* 电源唤醒 */
void w25q64_power_up(void)
{
    w25q64_cs_low();
    w25q64_spi_transmit(W25Q64_CMD_RELEASE_POWER_DOWN);
    w25q64_cs_high();
    HAL_Delay(1);
}

/* 芯片复位 */
static void w25q64_reset(void)
{
    w25q64_cs_low();
    w25q64_spi_transmit(W25Q64_CMD_ENABLE_RESET);
    w25q64_cs_high();
    
    HAL_Delay(1);
    
    w25q64_cs_low();
    w25q64_spi_transmit(W25Q64_CMD_RESET);
    w25q64_cs_high();
    
    HAL_Delay(10);
}

/* 初始化W25Q64 */
int w25q64_init(void)
{
    /* 配置CS引脚为输出 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    w25q64_cs_high();
    HAL_Delay(10);
    
    /* 复位芯片 */
    w25q64_reset();
    HAL_Delay(10);
    
    /* 唤醒芯片 */
    w25q64_power_up();
	
	return 0;
}

/* 读取数据 */
int w25q64_read(long addr, uint8_t* buffer, size_t size)
{
    w25q64_cs_low();
    w25q64_spi_transmit(W25Q64_CMD_READ_DATA);
    w25q64_spi_transmit((addr >> 16) & 0xFF);
    w25q64_spi_transmit((addr >> 8) & 0xFF);
    w25q64_spi_transmit(addr & 0xFF);
    
    for(uint32_t i = 0; i < size; i++)
    {
        buffer[i] = w25q64_spi_transmit(0xFF);
    }
    
    w25q64_cs_high();
	
	return 0;
}

/* 写多页数据 */
int w25q64_write(long addr, const uint8_t* data, size_t size)
{
    uint32_t page_remain;
    
    while(size > 0)
    {
        page_remain = W25Q64_PAGE_SIZE - (addr % W25Q64_PAGE_SIZE);
        if(page_remain > size)
        {
            page_remain = size;
        }
        
        if(!w25q64_write_page(addr, data, page_remain))
        {
            return false;
        }
        
        addr += page_remain;
        data += page_remain;
        size -= page_remain;
    }
	
    return 0;
}

/* 擦除指定分区 */
int w25q64_erase(long offset, size_t lenth)
{    
    uint32_t addr = offset;
    uint32_t size = lenth;
    
    /* 优先使用块擦除 */
    while(size >= W25Q64_BLOCK_SIZE)
    {
        if(!w25q64_erase_block(addr))
        {
            return false;
        }
        addr += W25Q64_BLOCK_SIZE;
        size -= W25Q64_BLOCK_SIZE;
    }
    
    /* 剩余部分使用扇区擦除 */
    while(size >= W25Q64_SECTOR_SIZE)
    {
        if(!w25q64_erase_sector(addr))
        {
            return false;
        }
        addr += W25Q64_SECTOR_SIZE;
        size -= W25Q64_SECTOR_SIZE;
    }
    
    return true;
}

/* w25q64设备结构体 */ 
struct flash_dev w25q64 =
{
	.name = "w25q64",
	.addr = 0x0,
	.len = 8 * 1024 * 1024,
	.blk_size = 4096,
	.ops = {w25q64_init, w25q64_read, w25q64_write, w25q64_erase},
	.write_gran = 1
};
