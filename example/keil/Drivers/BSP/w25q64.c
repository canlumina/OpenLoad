#include <string.h>
#include <stdio.h>
#include "dev_flash.h"
#include "w25q64.h"
#include "main.h"
#include "gpio.h"



/* 分区表定义 */
//static const w25q64_partition_t w25q64_partitions[W25Q64_PARTITION_MAX] = W25Q64_PARTITION_TABLE;

/* CS引脚控制 - 使用PB12 */
void w25q64_cs_low(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
}

void w25q64_cs_high(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
}

/* SPI单字节传输 */
uint8_t w25q64_spi_transmit(uint8_t data)
{
    uint8_t rx_data;
    HAL_SPI_TransmitReceive(&hspi2, &data, &rx_data, 1, 100);
    return rx_data;
}

/* SPI多字节传输 */
void w25q64_spi_transmit_buffer(const uint8_t* tx_data, uint8_t* rx_data, uint32_t size)
{
    if(tx_data && rx_data)
    {
        HAL_SPI_TransmitReceive(&hspi2, (uint8_t*)tx_data, rx_data, size, 1000);
    }
    else if(tx_data)
    {
        HAL_SPI_Transmit(&hspi2, (uint8_t*)tx_data, size, 1000);
    }
    else if(rx_data)
    {
        HAL_SPI_Receive(&hspi2, rx_data, size, 1000);
    }
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
bool w25q64_is_busy(void)
{
    uint8_t status;
    
    w25q64_cs_low();
    w25q64_spi_transmit(W25Q64_CMD_READ_STATUS_REG1);
    status = w25q64_spi_transmit(0xFF);
    w25q64_cs_high();
    
    return (status & W25Q64_SR_BUSY) != 0;
}

/* 等待芯片空闲 */
void w25q64_wait_busy(void)
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
void w25q64_write_enable(void)
{
    w25q64_cs_low();
    w25q64_spi_transmit(W25Q64_CMD_WRITE_ENABLE);
    w25q64_cs_high();
}

/* 写禁止 */
void w25q64_write_disable(void)
{
    w25q64_cs_low();
    w25q64_spi_transmit(W25Q64_CMD_WRITE_DISABLE);
    w25q64_cs_high();
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

/* 快速读取数据 */
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
bool w25q64_write_page(uint32_t addr, const uint8_t* data, uint32_t size)
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
    
    return true;
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

/* 获取分区信息 */
const w25q64_partition_t* w25q64_get_partition(w25q64_partition_id_t id)
{
    if(id >= W25Q64_PARTITION_MAX)
    {
        return NULL;
    }
    
	return 0;
    //return &w25q64_partitions[id];
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

/* 从分区读取数据 */
bool w25q64_read_partition(w25q64_partition_id_t id, uint32_t offset, uint8_t* buffer, uint32_t size)
{
    const w25q64_partition_t* partition = w25q64_get_partition(id);
    if(!partition)
    {
        return false;
    }
    
    if(offset + size > partition->size)
    {
        return false;
    }
    
    w25q64_read(partition->start_addr + offset, buffer, size);
    return true;
}

/* 向分区写入数据 */
bool w25q64_write_partition(w25q64_partition_id_t id, uint32_t offset, const uint8_t* data, uint32_t size)
{
    const w25q64_partition_t* partition = w25q64_get_partition(id);
    if(!partition)
    {
        return false;
    }
    
    if(offset + size > partition->size)
    {
        return false;
    }
    
    return w25q64_write(partition->start_addr + offset, data, size);
}

/* 打印分区信息 - bootloader中不使用，节省空间 */
/*
void w25q64_print_partition_info(void)
{
    bootloader_print("\r\n========== W25Q64 Partition Table ==========\r\n");
    bootloader_print("Total Size: 8MB (0x");
    bootloader_print_hex(W25Q64_CHIP_SIZE);
    bootloader_print(")\r\n");
    bootloader_print("---------------------------------------------\r\n");
    bootloader_print("Name       | Start      | Size       | Description\r\n");
    bootloader_print("---------------------------------------------\r\n");
    
    for(uint32_t i = 0; i < W25Q64_PARTITION_MAX; i++)
    {
        const w25q64_partition_t* p = &w25q64_partitions[i];
        bootloader_print(p->name);
        bootloader_print(" | 0x");
        bootloader_print_hex(p->start_addr);
        bootloader_print(" | ");
        
        if(p->size >= 0x100000)
        {
            bootloader_print_dec(p->size / 0x100000);
            bootloader_print("MB     | ");
        }
        else
        {
            bootloader_print_dec(p->size / 0x400);
            bootloader_print("KB     | ");
        }
        
        bootloader_print(p->description);
        bootloader_print("\r\n");
    }
    
    bootloader_print("=============================================\r\n");
}
*/

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
void w25q64_reset(void)
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




struct flash_dev w25q64 =
    {
        .name = "w25q64",
        .addr = 0x0,
        .len = 8 * 1024 * 1024,
        .blk_size = 4096,
        .ops = {w25q64_init, w25q64_read, w25q64_write, w25q64_erase},
        .write_gran = 1
	};
