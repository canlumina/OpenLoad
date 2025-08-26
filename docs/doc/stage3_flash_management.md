# 阶段3-3：Flash存储管理

## 概述
实现STM32F103ZET6内部Flash和W25Q64外部Flash的统一管理，包括分区管理、擦写操作和数据完整性保护。

## Flash架构设计

### 分区定义
```c
/* 内部Flash分区 */
typedef enum {
    FLASH_PARTITION_BOOTLOADER = 0,  // 0x08000000-0x0800FFFF (64KB)
    FLASH_PARTITION_APPLICATION,     // 0x08010000-0x0807FFFF (448KB)
    FLASH_PARTITION_COUNT
} Internal_Flash_Partition_t;

/* 外部Flash分区 */
typedef enum {
    EXT_FLASH_PARTITION_DOWNLOAD = 0, // 0x000000-0x1FFFFF (2MB)
    EXT_FLASH_PARTITION_BACKUP,       // 0x200000-0x3FFFFF (2MB)
    EXT_FLASH_PARTITION_USER_DATA,    // 0x400000-0x7FFFFF (4MB)
    EXT_FLASH_PARTITION_COUNT
} External_Flash_Partition_t;

/* 分区信息结构 */
typedef struct {
    uint32_t start_address;
    uint32_t size;
    uint32_t sector_size;
    char name[16];
} Flash_Partition_Info_t;
```

## W25Q64驱动实现

### SPI驱动 (spi_driver.c)
```c
/**
  * @file    spi_driver.c
  * @brief   SPI驱动程序 - 用于W25Q64通信
  */

#include "spi_driver.h"
#include "gpio_driver.h"

#define W25Q64_CS_LOW()     GPIO_ResetBits(GPIOB, GPIO_Pin_12)
#define W25Q64_CS_HIGH()    GPIO_SetBits(GPIOB, GPIO_Pin_12)

/**
  * @brief  SPI2初始化
  * @param  None
  * @retval None
  */
void SPI2_Init(void)
{
    /* 使能时钟 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    
    /* GPIO配置 */
    GPIO_InitTypeDef GPIO_InitStructure;
    
    /* SPI2 SCK (PB13) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    /* SPI2 MISO (PB14) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    /* SPI2 MOSI (PB15) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    /* SPI2 CS (PB12) - 软件控制 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    W25Q64_CS_HIGH(); // 默认不选中
    
    /* SPI配置 */
    SPI_InitTypeDef SPI_InitStructure;
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4; // 18MHz
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI2, &SPI_InitStructure);
    
    /* 使能SPI2 */
    SPI_Cmd(SPI2, ENABLE);
}

/**
  * @brief  SPI发送接收字节
  * @param  data: 发送的数据
  * @retval 接收的数据
  */
uint8_t SPI2_ReadWriteByte(uint8_t data)
{
    /* 等待发送缓冲区空 */
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET);
    
    /* 发送数据 */
    SPI_I2S_SendData(SPI2, data);
    
    /* 等待接收缓冲区非空 */
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET);
    
    /* 读取数据 */
    return SPI_I2S_ReceiveData(SPI2);
}
```

### W25Q64驱动 (w25q64_driver.c)
```c
/**
  * @file    w25q64_driver.c
  * @brief   W25Q64 Flash驱动程序
  */

#include "w25q64_driver.h"
#include "spi_driver.h"

/* W25Q64命令定义 */
#define W25Q64_CMD_WRITE_ENABLE         0x06
#define W25Q64_CMD_WRITE_DISABLE        0x04
#define W25Q64_CMD_READ_STATUS_REG      0x05
#define W25Q64_CMD_WRITE_STATUS_REG     0x01
#define W25Q64_CMD_READ_DATA            0x03
#define W25Q64_CMD_FAST_READ            0x0B
#define W25Q64_CMD_PAGE_PROGRAM         0x02
#define W25Q64_CMD_SECTOR_ERASE         0x20
#define W25Q64_CMD_BLOCK_ERASE_32K      0x52
#define W25Q64_CMD_BLOCK_ERASE_64K      0xD8
#define W25Q64_CMD_CHIP_ERASE           0xC7
#define W25Q64_CMD_POWER_DOWN           0xB9
#define W25Q64_CMD_RELEASE_POWER_DOWN   0xAB
#define W25Q64_CMD_DEVICE_ID            0x90
#define W25Q64_CMD_JEDEC_ID             0x9F

/* W25Q64状态寄存器位定义 */
#define W25Q64_STATUS_BUSY              0x01
#define W25Q64_STATUS_WEL               0x02

/* 器件信息 */
#define W25Q64_JEDEC_ID                 0xEF4017
#define W25Q64_PAGE_SIZE                256
#define W25Q64_SECTOR_SIZE              4096
#define W25Q64_BLOCK_SIZE_32K           32768
#define W25Q64_BLOCK_SIZE_64K           65536
#define W25Q64_CHIP_SIZE                8388608  // 8MB

/**
  * @brief  W25Q64初始化
  * @param  None
  * @retval W25Q64_OK: 成功, W25Q64_ERROR: 失败
  */
W25Q64_Result_t W25Q64_Init(void)
{
    /* 初始化SPI */
    SPI2_Init();
    
    /* 释放掉电模式 */
    W25Q64_CS_LOW();
    SPI2_ReadWriteByte(W25Q64_CMD_RELEASE_POWER_DOWN);
    W25Q64_CS_HIGH();
    
    Delay_us(3); // 等待tRES1
    
    /* 读取器件ID */
    uint32_t jedec_id = W25Q64_ReadJEDEC_ID();
    
    if (jedec_id == W25Q64_JEDEC_ID)
    {
        printf("W25Q64: Detected, JEDEC ID = 0x%06X\r\n", jedec_id);
        return W25Q64_OK;
    }
    else
    {
        printf("W25Q64: Not detected, JEDEC ID = 0x%06X\r\n", jedec_id);
        return W25Q64_ERROR;
    }
}

/**
  * @brief  读取JEDEC ID
  * @param  None
  * @retval JEDEC ID
  */
uint32_t W25Q64_ReadJEDEC_ID(void)
{
    uint32_t jedec_id = 0;
    
    W25Q64_CS_LOW();
    SPI2_ReadWriteByte(W25Q64_CMD_JEDEC_ID);
    jedec_id |= (SPI2_ReadWriteByte(0xFF) << 16);
    jedec_id |= (SPI2_ReadWriteByte(0xFF) << 8);
    jedec_id |= SPI2_ReadWriteByte(0xFF);
    W25Q64_CS_HIGH();
    
    return jedec_id;
}

/**
  * @brief  读取状态寄存器
  * @param  None
  * @retval 状态寄存器值
  */
uint8_t W25Q64_ReadStatusReg(void)
{
    uint8_t status;
    
    W25Q64_CS_LOW();
    SPI2_ReadWriteByte(W25Q64_CMD_READ_STATUS_REG);
    status = SPI2_ReadWriteByte(0xFF);
    W25Q64_CS_HIGH();
    
    return status;
}

/**
  * @brief  等待操作完成
  * @param  None
  * @retval None
  */
void W25Q64_WaitForReady(void)
{
    while (W25Q64_ReadStatusReg() & W25Q64_STATUS_BUSY);
}

/**
  * @brief  写使能
  * @param  None
  * @retval None
  */
void W25Q64_WriteEnable(void)
{
    W25Q64_CS_LOW();
    SPI2_ReadWriteByte(W25Q64_CMD_WRITE_ENABLE);
    W25Q64_CS_HIGH();
}

/**
  * @brief  写禁止
  * @param  None
  * @retval None
  */
void W25Q64_WriteDisable(void)
{
    W25Q64_CS_LOW();
    SPI2_ReadWriteByte(W25Q64_CMD_WRITE_DISABLE);
    W25Q64_CS_HIGH();
}

/**
  * @brief  读取数据
  * @param  address: 读取地址
  * @param  buffer: 数据缓冲区
  * @param  length: 读取长度
  * @retval W25Q64_OK: 成功, W25Q64_ERROR: 失败
  */
W25Q64_Result_t W25Q64_ReadData(uint32_t address, uint8_t* buffer, uint32_t length)
{
    if (address + length > W25Q64_CHIP_SIZE)
        return W25Q64_ERROR;
    
    W25Q64_WaitForReady();
    
    W25Q64_CS_LOW();
    SPI2_ReadWriteByte(W25Q64_CMD_READ_DATA);
    SPI2_ReadWriteByte((address >> 16) & 0xFF);
    SPI2_ReadWriteByte((address >> 8) & 0xFF);
    SPI2_ReadWriteByte(address & 0xFF);
    
    for (uint32_t i = 0; i < length; i++)
    {
        buffer[i] = SPI2_ReadWriteByte(0xFF);
    }
    
    W25Q64_CS_HIGH();
    
    return W25Q64_OK;
}

/**
  * @brief  页编程 (最大256字节)
  * @param  address: 写入地址
  * @param  buffer: 数据缓冲区
  * @param  length: 写入长度 (1-256)
  * @retval W25Q64_OK: 成功, W25Q64_ERROR: 失败
  */
W25Q64_Result_t W25Q64_PageProgram(uint32_t address, uint8_t* buffer, uint32_t length)
{
    if (length == 0 || length > W25Q64_PAGE_SIZE)
        return W25Q64_ERROR;
    
    if (address + length > W25Q64_CHIP_SIZE)
        return W25Q64_ERROR;
    
    W25Q64_WaitForReady();
    W25Q64_WriteEnable();
    
    W25Q64_CS_LOW();
    SPI2_ReadWriteByte(W25Q64_CMD_PAGE_PROGRAM);
    SPI2_ReadWriteByte((address >> 16) & 0xFF);
    SPI2_ReadWriteByte((address >> 8) & 0xFF);
    SPI2_ReadWriteByte(address & 0xFF);
    
    for (uint32_t i = 0; i < length; i++)
    {
        SPI2_ReadWriteByte(buffer[i]);
    }
    
    W25Q64_CS_HIGH();
    
    W25Q64_WaitForReady();
    W25Q64_WriteDisable();
    
    return W25Q64_OK;
}

/**
  * @brief  扇区擦除 (4KB)
  * @param  address: 扇区地址
  * @retval W25Q64_OK: 成功, W25Q64_ERROR: 失败
  */
W25Q64_Result_t W25Q64_SectorErase(uint32_t address)
{
    if (address >= W25Q64_CHIP_SIZE)
        return W25Q64_ERROR;
    
    W25Q64_WaitForReady();
    W25Q64_WriteEnable();
    
    W25Q64_CS_LOW();
    SPI2_ReadWriteByte(W25Q64_CMD_SECTOR_ERASE);
    SPI2_ReadWriteByte((address >> 16) & 0xFF);
    SPI2_ReadWriteByte((address >> 8) & 0xFF);
    SPI2_ReadWriteByte(address & 0xFF);
    W25Q64_CS_HIGH();
    
    W25Q64_WaitForReady();
    W25Q64_WriteDisable();
    
    return W25Q64_OK;
}

/**
  * @brief  块擦除 (64KB)
  * @param  address: 块地址
  * @retval W25Q64_OK: 成功, W25Q64_ERROR: 失败
  */
W25Q64_Result_t W25Q64_BlockErase64K(uint32_t address)
{
    if (address >= W25Q64_CHIP_SIZE)
        return W25Q64_ERROR;
    
    W25Q64_WaitForReady();
    W25Q64_WriteEnable();
    
    W25Q64_CS_LOW();
    SPI2_ReadWriteByte(W25Q64_CMD_BLOCK_ERASE_64K);
    SPI2_ReadWriteByte((address >> 16) & 0xFF);
    SPI2_ReadWriteByte((address >> 8) & 0xFF);
    SPI2_ReadWriteByte(address & 0xFF);
    W25Q64_CS_HIGH();
    
    W25Q64_WaitForReady();
    W25Q64_WriteDisable();
    
    return W25Q64_OK;
}

/**
  * @brief  芯片擦除 (全片)
  * @param  None
  * @retval W25Q64_OK: 成功, W25Q64_ERROR: 失败
  */
W25Q64_Result_t W25Q64_ChipErase(void)
{
    W25Q64_WaitForReady();
    W25Q64_WriteEnable();
    
    W25Q64_CS_LOW();
    SPI2_ReadWriteByte(W25Q64_CMD_CHIP_ERASE);
    W25Q64_CS_HIGH();
    
    W25Q64_WaitForReady();
    W25Q64_WriteDisable();
    
    return W25Q64_OK;
}
```

## Flash管理器实现

### flash_manager.c
```c
/**
  * @file    flash_manager.c
  * @brief   Flash存储管理器
  */

#include "flash_manager.h"
#include "w25q64_driver.h"

/* 内部Flash分区表 */
static const Flash_Partition_Info_t internal_partitions[FLASH_PARTITION_COUNT] = {
    {0x08000000, 65536,  2048, "Bootloader"},    // 64KB, 2KB页
    {0x08010000, 458752, 2048, "Application"},   // 448KB, 2KB页
};

/* 外部Flash分区表 */
static const Flash_Partition_Info_t external_partitions[EXT_FLASH_PARTITION_COUNT] = {
    {0x000000, 2097152, 4096, "Download"},       // 2MB, 4KB扇区
    {0x200000, 2097152, 4096, "Backup"},         // 2MB, 4KB扇区
    {0x400000, 4194304, 4096, "UserData"}        // 4MB, 4KB扇区
};

/**
  * @brief  Flash管理器初始化
  * @param  None
  * @retval Flash_Result_t
  */
Flash_Result_t Flash_Manager_Init(void)
{
    printf("Flash Manager: Initializing...\r\n");
    
    /* 解锁内部Flash */
    FLASH_Unlock();
    
    /* 初始化外部Flash */
    if (W25Q64_Init() != W25Q64_OK)
    {
        printf("Flash Manager: External flash init failed\r\n");
        return FLASH_ERROR;
    }
    
    printf("Flash Manager: Initialized successfully\r\n");
    Flash_Print_PartitionInfo();
    
    return FLASH_OK;
}

/**
  * @brief  内部Flash擦除
  * @param  address: 擦除地址
  * @param  pages: 擦除页数
  * @retval Flash_Result_t
  */
Flash_Result_t Flash_Internal_Erase(uint32_t address, uint32_t pages)
{
    FLASH_Status flash_status = FLASH_COMPLETE;
    
    /* 地址检查 */
    if (address < 0x08000000 || address >= 0x08080000)
        return FLASH_ERROR;
    
    /* 检查是否为页边界 */
    if (address % 2048 != 0)
        return FLASH_ERROR;
    
    printf("Flash: Erasing internal flash at 0x%08X, %d pages\r\n", address, pages);
    
    /* 擦除页 */
    for (uint32_t i = 0; i < pages; i++)
    {
        flash_status = FLASH_ErasePage(address + i * 2048);
        if (flash_status != FLASH_COMPLETE)
        {
            printf("Flash: Erase failed at page %d, status = %d\r\n", i, flash_status);
            return FLASH_ERROR;
        }
    }
    
    printf("Flash: Internal flash erase completed\r\n");
    return FLASH_OK;
}

/**
  * @brief  内部Flash写入
  * @param  address: 写入地址
  * @param  buffer: 数据缓冲区
  * @param  length: 写入长度 (必须为4的倍数)
  * @retval Flash_Result_t
  */
Flash_Result_t Flash_Internal_Write(uint32_t address, uint8_t* buffer, uint32_t length)
{
    FLASH_Status flash_status = FLASH_COMPLETE;
    
    /* 参数检查 */
    if (address < 0x08000000 || address + length > 0x08080000)
        return FLASH_ERROR;
    
    if (length % 4 != 0)
        return FLASH_ERROR;
    
    printf("Flash: Writing to internal flash at 0x%08X, length %d\r\n", address, length);
    
    /* 按32位写入 */
    uint32_t* data = (uint32_t*)buffer;
    uint32_t words = length / 4;
    
    for (uint32_t i = 0; i < words; i++)
    {
        flash_status = FLASH_ProgramWord(address + i * 4, data[i]);
        if (flash_status != FLASH_COMPLETE)
        {
            printf("Flash: Write failed at word %d, status = %d\r\n", i, flash_status);
            return FLASH_ERROR;
        }
    }
    
    printf("Flash: Internal flash write completed\r\n");
    return FLASH_OK;
}

/**
  * @brief  内部Flash读取
  * @param  address: 读取地址
  * @param  buffer: 数据缓冲区
  * @param  length: 读取长度
  * @retval Flash_Result_t
  */
Flash_Result_t Flash_Internal_Read(uint32_t address, uint8_t* buffer, uint32_t length)
{
    /* 地址检查 */
    if (address < 0x08000000 || address + length > 0x08080000)
        return FLASH_ERROR;
    
    /* 直接从Flash读取 */
    uint8_t* flash_ptr = (uint8_t*)address;
    for (uint32_t i = 0; i < length; i++)
    {
        buffer[i] = flash_ptr[i];
    }
    
    return FLASH_OK;
}

/**
  * @brief  外部Flash擦除
  * @param  address: 擦除地址
  * @param  length: 擦除长度
  * @retval Flash_Result_t
  */
Flash_Result_t Flash_External_Erase(uint32_t address, uint32_t length)
{
    /* 地址检查 */
    if (address + length > W25Q64_CHIP_SIZE)
        return FLASH_ERROR;
    
    printf("Flash: Erasing external flash at 0x%06X, length %d\r\n", address, length);
    
    uint32_t current_address = address;
    uint32_t remaining = length;
    
    /* 按扇区擦除 */
    while (remaining > 0)
    {
        if (W25Q64_SectorErase(current_address) != W25Q64_OK)
        {
            printf("Flash: External erase failed at 0x%06X\r\n", current_address);
            return FLASH_ERROR;
        }
        
        current_address += W25Q64_SECTOR_SIZE;
        remaining = (remaining > W25Q64_SECTOR_SIZE) ? 
                   (remaining - W25Q64_SECTOR_SIZE) : 0;
    }
    
    printf("Flash: External flash erase completed\r\n");
    return FLASH_OK;
}

/**
  * @brief  外部Flash写入
  * @param  address: 写入地址
  * @param  buffer: 数据缓冲区
  * @param  length: 写入长度
  * @retval Flash_Result_t
  */
Flash_Result_t Flash_External_Write(uint32_t address, uint8_t* buffer, uint32_t length)
{
    /* 地址检查 */
    if (address + length > W25Q64_CHIP_SIZE)
        return FLASH_ERROR;
    
    printf("Flash: Writing to external flash at 0x%06X, length %d\r\n", address, length);
    
    uint32_t current_address = address;
    uint32_t current_index = 0;
    uint32_t remaining = length;
    
    /* 按页写入 */
    while (remaining > 0)
    {
        uint32_t page_offset = current_address % W25Q64_PAGE_SIZE;
        uint32_t write_size = W25Q64_PAGE_SIZE - page_offset;
        
        if (write_size > remaining)
            write_size = remaining;
        
        if (W25Q64_PageProgram(current_address, 
                              &buffer[current_index], 
                              write_size) != W25Q64_OK)
        {
            printf("Flash: External write failed at 0x%06X\r\n", current_address);
            return FLASH_ERROR;
        }
        
        current_address += write_size;
        current_index += write_size;
        remaining -= write_size;
    }
    
    printf("Flash: External flash write completed\r\n");
    return FLASH_OK;
}

/**
  * @brief  外部Flash读取
  * @param  address: 读取地址
  * @param  buffer: 数据缓冲区
  * @param  length: 读取长度
  * @retval Flash_Result_t
  */
Flash_Result_t Flash_External_Read(uint32_t address, uint8_t* buffer, uint32_t length)
{
    return (W25Q64_ReadData(address, buffer, length) == W25Q64_OK) ? 
           FLASH_OK : FLASH_ERROR;
}

/**
  * @brief  获取分区信息
  * @param  partition: 分区索引
  * @param  is_external: 是否为外部Flash
  * @retval 分区信息指针
  */
const Flash_Partition_Info_t* Flash_GetPartitionInfo(uint8_t partition, uint8_t is_external)
{
    if (is_external)
    {
        if (partition < EXT_FLASH_PARTITION_COUNT)
            return &external_partitions[partition];
    }
    else
    {
        if (partition < FLASH_PARTITION_COUNT)
            return &internal_partitions[partition];
    }
    
    return NULL;
}

/**
  * @brief  打印分区信息
  * @param  None
  * @retval None
  */
void Flash_Print_PartitionInfo(void)
{
    printf("\r\nFlash Partition Information:\r\n");
    printf("============================\r\n");
    printf("Internal Flash (STM32F103ZET6):\r\n");
    
    for (int i = 0; i < FLASH_PARTITION_COUNT; i++)
    {
        printf("  %-12s: 0x%08X - 0x%08X (%dKB)\r\n",
               internal_partitions[i].name,
               internal_partitions[i].start_address,
               internal_partitions[i].start_address + internal_partitions[i].size - 1,
               internal_partitions[i].size / 1024);
    }
    
    printf("\r\nExternal Flash (W25Q64):\r\n");
    for (int i = 0; i < EXT_FLASH_PARTITION_COUNT; i++)
    {
        printf("  %-12s: 0x%06X - 0x%06X (%dMB)\r\n",
               external_partitions[i].name,
               external_partitions[i].start_address,
               external_partitions[i].start_address + external_partitions[i].size - 1,
               external_partitions[i].size / 1048576);
    }
    printf("\r\n");
}

/**
  * @brief  计算CRC32
  * @param  data: 数据缓冲区
  * @param  length: 数据长度
  * @retval CRC32值
  */
uint32_t Flash_Calculate_CRC32(uint8_t* data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    
    for (uint32_t i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    
    return ~crc;
}
```

## 头文件定义

### flash_manager.h
```c
#ifndef __FLASH_MANAGER_H
#define __FLASH_MANAGER_H

#include "stm32f10x.h"

/* Flash操作结果 */
typedef enum {
    FLASH_OK = 0,
    FLASH_ERROR,
    FLASH_BUSY,
    FLASH_TIMEOUT
} Flash_Result_t;

/* 内部Flash分区 */
typedef enum {
    FLASH_PARTITION_BOOTLOADER = 0,
    FLASH_PARTITION_APPLICATION,
    FLASH_PARTITION_CONFIG,
    FLASH_PARTITION_COUNT
} Internal_Flash_Partition_t;

/* 外部Flash分区 */
typedef enum {
    EXT_FLASH_PARTITION_DOWNLOAD = 0,
    EXT_FLASH_PARTITION_BACKUP,
    EXT_FLASH_PARTITION_USER_DATA,
    EXT_FLASH_PARTITION_COUNT
} External_Flash_Partition_t;

/* 分区信息 */
typedef struct {
    uint32_t start_address;
    uint32_t size;
    uint32_t sector_size;
    char name[16];
} Flash_Partition_Info_t;

/* 函数声明 */
Flash_Result_t Flash_Manager_Init(void);
Flash_Result_t Flash_Internal_Erase(uint32_t address, uint32_t pages);
Flash_Result_t Flash_Internal_Write(uint32_t address, uint8_t* buffer, uint32_t length);
Flash_Result_t Flash_Internal_Read(uint32_t address, uint8_t* buffer, uint32_t length);
Flash_Result_t Flash_External_Erase(uint32_t address, uint32_t length);
Flash_Result_t Flash_External_Write(uint32_t address, uint8_t* buffer, uint32_t length);
Flash_Result_t Flash_External_Read(uint32_t address, uint8_t* buffer, uint32_t length);
const Flash_Partition_Info_t* Flash_GetPartitionInfo(uint8_t partition, uint8_t is_external);
void Flash_Print_PartitionInfo(void);
uint32_t Flash_Calculate_CRC32(uint8_t* data, uint32_t length);

#endif /* __FLASH_MANAGER_H */
```

### w25q64_driver.h
```c
#ifndef __W25Q64_DRIVER_H
#define __W25Q64_DRIVER_H

#include "stm32f10x.h"
#include "system_clock.h"

/* W25Q64操作结果 */
typedef enum {
    W25Q64_OK = 0,
    W25Q64_ERROR
} W25Q64_Result_t;

/* 器件参数 */
#define W25Q64_PAGE_SIZE        256
#define W25Q64_SECTOR_SIZE      4096
#define W25Q64_CHIP_SIZE        8388608

/* CS引脚控制 */
#define W25Q64_CS_LOW()         GPIO_ResetBits(GPIOB, GPIO_Pin_12)
#define W25Q64_CS_HIGH()        GPIO_SetBits(GPIOB, GPIO_Pin_12)

/* 函数声明 */
W25Q64_Result_t W25Q64_Init(void);
uint32_t W25Q64_ReadJEDEC_ID(void);
uint8_t W25Q64_ReadStatusReg(void);
void W25Q64_WaitForReady(void);
void W25Q64_WriteEnable(void);
void W25Q64_WriteDisable(void);
W25Q64_Result_t W25Q64_ReadData(uint32_t address, uint8_t* buffer, uint32_t length);
W25Q64_Result_t W25Q64_PageProgram(uint32_t address, uint8_t* buffer, uint32_t length);
W25Q64_Result_t W25Q64_SectorErase(uint32_t address);
W25Q64_Result_t W25Q64_BlockErase64K(uint32_t address);
W25Q64_Result_t W25Q64_ChipErase(void);

#endif /* __W25Q64_DRIVER_H */
```

## 测试验证

### Flash功能测试
```c
// 测试代码示例
void Flash_Test(void)
{
    uint8_t write_data[256];
    uint8_t read_data[256];
    
    /* 准备测试数据 */
    for (int i = 0; i < 256; i++)
        write_data[i] = i;
    
    /* 测试外部Flash */
    printf("Testing external flash...\r\n");
    
    Flash_External_Erase(0x000000, 4096);
    Flash_External_Write(0x000000, write_data, 256);
    Flash_External_Read(0x000000, read_data, 256);
    
    /* 验证数据 */
    for (int i = 0; i < 256; i++)
    {
        if (write_data[i] != read_data[i])
        {
            printf("Flash test failed at byte %d\r\n", i);
            return;
        }
    }
    
    printf("Flash test passed!\r\n");
}
```

## 验证标准
1. 内部Flash和外部Flash都能正确识别和初始化
2. 分区管理功能正常，地址范围检查有效
3. 擦写操作成功率100%，数据完整性验证通过
4. 大数据量读写速度达到设计要求
5. 异常处理完善，错误恢复机制可靠

## 下一步行动
Flash存储管理完成后，继续开发串口IAP功能。