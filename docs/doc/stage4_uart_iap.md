# 阶段4-1：串口IAP实现

## 概述
实现基于UART串口的IAP(In-Application Programming)固件升级功能，支持Xmodem协议数据传输、固件校验和自动升级。

## IAP协议设计

### 通信协议格式
```c
/* IAP命令包结构 */
typedef struct {
    uint8_t header;         // 包头：0xAA
    uint8_t cmd;            // 命令类型
    uint16_t length;        // 数据长度 (小端序)
    uint8_t data[256];      // 数据载荷
    uint8_t checksum;       // 校验和
    uint8_t footer;         // 包尾：0x55
} __attribute__((packed)) IAP_Packet_t;

/* IAP命令定义 */
typedef enum {
    IAP_CMD_HANDSHAKE = 0x01,       // 握手
    IAP_CMD_GET_INFO = 0x02,        // 获取信息
    IAP_CMD_ERASE = 0x03,           // 擦除Flash
    IAP_CMD_WRITE = 0x04,           // 写入数据
    IAP_CMD_READ = 0x05,            // 读取数据
    IAP_CMD_VERIFY = 0x06,          // 校验数据
    IAP_CMD_JUMP = 0x07,            // 跳转应用
    IAP_CMD_RESET = 0x08,           // 系统复位
} IAP_Command_t;

/* IAP响应码 */
typedef enum {
    IAP_RESP_OK = 0x00,             // 成功
    IAP_RESP_ERROR = 0x01,          // 错误
    IAP_RESP_BUSY = 0x02,           // 忙碌
    IAP_RESP_TIMEOUT = 0x03,        // 超时
    IAP_RESP_CRC_ERROR = 0x04,      // CRC错误
    IAP_RESP_INVALID_ADDR = 0x05,   // 无效地址
    IAP_RESP_INVALID_SIZE = 0x06,   // 无效大小
} IAP_Response_t;
```

## Xmodem协议实现

### xmodem.c
```c
/**
  * @file    xmodem.c
  * @brief   Xmodem协议实现
  */

#include "xmodem.h"
#include "uart_driver.h"

/* Xmodem协议控制字符 */
#define XMODEM_SOH          0x01    // 128字节包头
#define XMODEM_STX          0x02    // 1024字节包头
#define XMODEM_EOT          0x04    // 传输结束
#define XMODEM_ACK          0x06    // 确认
#define XMODEM_NAK          0x15    // 非确认
#define XMODEM_CAN          0x18    // 取消
#define XMODEM_C            0x43    // CRC模式请求

#define XMODEM_PACKET_SIZE_128      128
#define XMODEM_PACKET_SIZE_1024     1024
#define XMODEM_TIMEOUT_MS           1000
#define XMODEM_MAX_RETRY            10

/* Xmodem状态 */
typedef enum {
    XMODEM_STATE_IDLE = 0,
    XMODEM_STATE_RECEIVING,
    XMODEM_STATE_SENDING,
    XMODEM_STATE_COMPLETE,
    XMODEM_STATE_ERROR,
    XMODEM_STATE_CANCELLED
} Xmodem_State_t;

/* Xmodem数据包结构 */
typedef struct {
    uint8_t header;         // SOH/STX
    uint8_t packet_num;     // 包序号
    uint8_t packet_num_inv; // 包序号反码
    uint8_t data[1024];     // 数据 (最大1024字节)
    uint8_t crc_high;       // CRC高字节
    uint8_t crc_low;        // CRC低字节
} __attribute__((packed)) Xmodem_Packet_t;

static Xmodem_State_t g_xmodem_state = XMODEM_STATE_IDLE;
static uint8_t g_expected_packet_num = 1;
static uint32_t g_total_received = 0;

/**
  * @brief  计算CRC16
  * @param  data: 数据缓冲区
  * @param  length: 数据长度
  * @retval CRC16值
  */
static uint16_t Xmodem_Calculate_CRC16(uint8_t* data, uint16_t length)
{
    uint16_t crc = 0;
    
    for (uint16_t i = 0; i < length; i++)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    
    return crc;
}

/**
  * @brief  等待数据接收
  * @param  timeout_ms: 超时时间(毫秒)
  * @retval 接收到的字节，-1表示超时
  */
static int16_t Xmodem_Wait_Byte(uint32_t timeout_ms)
{
    uint32_t start_time = HAL_GetTick();
    
    while (HAL_GetTick() - start_time < timeout_ms)
    {
        if (UART_Available(USART1) > 0)
        {
            uint8_t data;
            UART_ReadData(USART1, &data, 1);
            return data;
        }
        Delay_ms(1);
    }
    
    return -1;
}

/**
  * @brief  发送字节
  * @param  data: 发送的字节
  * @retval None
  */
static void Xmodem_Send_Byte(uint8_t data)
{
    UART_SendByte(USART1, data);
}

/**
  * @brief  开始Xmodem接收
  * @param  buffer: 接收缓冲区
  * @param  max_size: 最大接收大小
  * @retval 接收结果
  */
Xmodem_Result_t Xmodem_Receive(uint8_t* buffer, uint32_t max_size, uint32_t* received_size)
{
    if (buffer == NULL || received_size == NULL)
        return XMODEM_ERROR;
    
    g_xmodem_state = XMODEM_STATE_RECEIVING;
    g_expected_packet_num = 1;
    g_total_received = 0;
    *received_size = 0;
    
    printf("Xmodem: Ready to receive, send your file now...\r\n");
    
    /* 发送CRC模式请求 */
    for (int i = 0; i < 10; i++)
    {
        Xmodem_Send_Byte(XMODEM_C);
        
        int16_t response = Xmodem_Wait_Byte(3000);
        if (response == XMODEM_SOH || response == XMODEM_STX)
        {
            /* 开始接收数据包 */
            return Xmodem_Receive_Packets(buffer, max_size, received_size, response);
        }
        else if (response == XMODEM_CAN)
        {
            printf("Xmodem: Transfer cancelled by sender\r\n");
            g_xmodem_state = XMODEM_STATE_CANCELLED;
            return XMODEM_CANCELLED;
        }
    }
    
    printf("Xmodem: No response from sender\r\n");
    g_xmodem_state = XMODEM_STATE_ERROR;
    return XMODEM_TIMEOUT;
}

/**
  * @brief  接收Xmodem数据包
  * @param  buffer: 接收缓冲区
  * @param  max_size: 最大大小
  * @param  received_size: 实际接收大小
  * @param  first_byte: 第一个字节
  * @retval 接收结果
  */
static Xmodem_Result_t Xmodem_Receive_Packets(uint8_t* buffer, uint32_t max_size, 
                                             uint32_t* received_size, uint8_t first_byte)
{
    Xmodem_Packet_t packet;
    uint8_t retry_count = 0;
    uint16_t packet_size;
    
    while (g_xmodem_state == XMODEM_STATE_RECEIVING)
    {
        /* 确定包大小 */
        if (first_byte == XMODEM_SOH)
            packet_size = XMODEM_PACKET_SIZE_128;
        else if (first_byte == XMODEM_STX)
            packet_size = XMODEM_PACKET_SIZE_1024;
        else if (first_byte == XMODEM_EOT)
        {
            /* 传输完成 */
            Xmodem_Send_Byte(XMODEM_ACK);
            printf("Xmodem: Transfer completed, received %d bytes\r\n", g_total_received);
            g_xmodem_state = XMODEM_STATE_COMPLETE;
            *received_size = g_total_received;
            return XMODEM_OK;
        }
        else if (first_byte == XMODEM_CAN)
        {
            /* 传输取消 */
            printf("Xmodem: Transfer cancelled\r\n");
            g_xmodem_state = XMODEM_STATE_CANCELLED;
            return XMODEM_CANCELLED;
        }
        else
        {
            /* 无效包头 */
            Xmodem_Send_Byte(XMODEM_NAK);
            first_byte = Xmodem_Wait_Byte(XMODEM_TIMEOUT_MS);
            continue;
        }
        
        packet.header = first_byte;
        
        /* 接收包序号 */
        int16_t byte = Xmodem_Wait_Byte(XMODEM_TIMEOUT_MS);
        if (byte < 0)
        {
            retry_count++;
            if (retry_count > XMODEM_MAX_RETRY)
            {
                printf("Xmodem: Too many timeouts\r\n");
                g_xmodem_state = XMODEM_STATE_ERROR;
                return XMODEM_TIMEOUT;
            }
            Xmodem_Send_Byte(XMODEM_NAK);
            first_byte = Xmodem_Wait_Byte(XMODEM_TIMEOUT_MS);
            continue;
        }
        packet.packet_num = byte;
        
        /* 接收包序号反码 */
        byte = Xmodem_Wait_Byte(XMODEM_TIMEOUT_MS);
        if (byte < 0)
        {
            retry_count++;
            if (retry_count > XMODEM_MAX_RETRY)
            {
                g_xmodem_state = XMODEM_STATE_ERROR;
                return XMODEM_TIMEOUT;
            }
            Xmodem_Send_Byte(XMODEM_NAK);
            first_byte = Xmodem_Wait_Byte(XMODEM_TIMEOUT_MS);
            continue;
        }
        packet.packet_num_inv = byte;
        
        /* 检查包序号 */
        if (packet.packet_num != (255 - packet.packet_num_inv))
        {
            printf("Xmodem: Packet number error\r\n");
            Xmodem_Send_Byte(XMODEM_NAK);
            first_byte = Xmodem_Wait_Byte(XMODEM_TIMEOUT_MS);
            continue;
        }
        
        /* 检查包序号是否符合预期 */
        if (packet.packet_num == g_expected_packet_num - 1)
        {
            /* 重复包，发送ACK并忽略 */
            Xmodem_Send_Byte(XMODEM_ACK);
            first_byte = Xmodem_Wait_Byte(XMODEM_TIMEOUT_MS);
            continue;
        }
        else if (packet.packet_num != g_expected_packet_num)
        {
            printf("Xmodem: Unexpected packet number %d, expected %d\r\n", 
                   packet.packet_num, g_expected_packet_num);
            Xmodem_Send_Byte(XMODEM_NAK);
            first_byte = Xmodem_Wait_Byte(XMODEM_TIMEOUT_MS);
            continue;
        }
        
        /* 接收数据 */
        for (uint16_t i = 0; i < packet_size; i++)
        {
            byte = Xmodem_Wait_Byte(XMODEM_TIMEOUT_MS);
            if (byte < 0)
            {
                retry_count++;
                if (retry_count > XMODEM_MAX_RETRY)
                {
                    g_xmodem_state = XMODEM_STATE_ERROR;
                    return XMODEM_TIMEOUT;
                }
                Xmodem_Send_Byte(XMODEM_NAK);
                first_byte = Xmodem_Wait_Byte(XMODEM_TIMEOUT_MS);
                goto retry_packet;
            }
            packet.data[i] = byte;
        }
        
        /* 接收CRC */
        byte = Xmodem_Wait_Byte(XMODEM_TIMEOUT_MS);
        if (byte < 0)
        {
            Xmodem_Send_Byte(XMODEM_NAK);
            first_byte = Xmodem_Wait_Byte(XMODEM_TIMEOUT_MS);
            continue;
        }
        packet.crc_high = byte;
        
        byte = Xmodem_Wait_Byte(XMODEM_TIMEOUT_MS);
        if (byte < 0)
        {
            Xmodem_Send_Byte(XMODEM_NAK);
            first_byte = Xmodem_Wait_Byte(XMODEM_TIMEOUT_MS);
            continue;
        }
        packet.crc_low = byte;
        
        /* 校验CRC */
        uint16_t calculated_crc = Xmodem_Calculate_CRC16(packet.data, packet_size);
        uint16_t received_crc = (packet.crc_high << 8) | packet.crc_low;
        
        if (calculated_crc != received_crc)
        {
            printf("Xmodem: CRC error, calculated=0x%04X, received=0x%04X\r\n", 
                   calculated_crc, received_crc);
            Xmodem_Send_Byte(XMODEM_NAK);
            first_byte = Xmodem_Wait_Byte(XMODEM_TIMEOUT_MS);
            continue;
        }
        
        /* 检查缓冲区空间 */
        if (g_total_received + packet_size > max_size)
        {
            printf("Xmodem: Buffer overflow\r\n");
            g_xmodem_state = XMODEM_STATE_ERROR;
            return XMODEM_ERROR;
        }
        
        /* 复制数据到缓冲区 */
        memcpy(buffer + g_total_received, packet.data, packet_size);
        g_total_received += packet_size;
        g_expected_packet_num++;
        
        /* 发送ACK */
        Xmodem_Send_Byte(XMODEM_ACK);
        
        /* 显示进度 */
        if (g_expected_packet_num % 10 == 0)
        {
            printf("Xmodem: Received %d packets, %d bytes\r\n", 
                   g_expected_packet_num - 1, g_total_received);
        }
        
        /* 等待下一包 */
        first_byte = Xmodem_Wait_Byte(XMODEM_TIMEOUT_MS);
        retry_count = 0;
        
        retry_packet:;
    }
    
    return XMODEM_ERROR;
}

/**
  * @brief  获取Xmodem状态
  * @param  None
  * @retval Xmodem状态
  */
Xmodem_State_t Xmodem_Get_State(void)
{
    return g_xmodem_state;
}

/**
  * @brief  取消Xmodem传输
  * @param  None
  * @retval None
  */
void Xmodem_Cancel(void)
{
    for (int i = 0; i < 5; i++)
    {
        Xmodem_Send_Byte(XMODEM_CAN);
    }
    g_xmodem_state = XMODEM_STATE_CANCELLED;
}
```

## IAP管理器实现

### iap_manager.c
```c
/**
  * @file    iap_manager.c
  * @brief   IAP管理器实现
  */

#include "iap_manager.h"
#include "xmodem.h"
#include "flash_manager.h"
#include "app_check.h"

#define IAP_BUFFER_SIZE         (64 * 1024)    // 64KB缓冲区
#define IAP_APP_START_ADDR      0x08008000     // 应用程序起始地址
#define IAP_APP_MAX_SIZE        (472 * 1024)   // 应用程序最大大小

/* IAP状态 */
typedef enum {
    IAP_STATE_IDLE = 0,
    IAP_STATE_RECEIVING,
    IAP_STATE_VERIFYING,
    IAP_STATE_PROGRAMMING,
    IAP_STATE_COMPLETE,
    IAP_STATE_ERROR
} IAP_State_t;

static IAP_State_t g_iap_state = IAP_STATE_IDLE;
static uint8_t* g_iap_buffer = NULL;
static uint32_t g_firmware_size = 0;
static uint32_t g_firmware_crc = 0;

/**
  * @brief  IAP管理器初始化
  * @param  None
  * @retval IAP_Result_t
  */
IAP_Result_t IAP_Manager_Init(void)
{
    /* 分配IAP缓冲区 */
    g_iap_buffer = (uint8_t*)malloc(IAP_BUFFER_SIZE);
    if (g_iap_buffer == NULL)
    {
        printf("IAP: Buffer allocation failed\r\n");
        return IAP_ERROR;
    }
    
    g_iap_state = IAP_STATE_IDLE;
    printf("IAP: Manager initialized, buffer size = %dKB\r\n", IAP_BUFFER_SIZE / 1024);
    
    return IAP_OK;
}

/**
  * @brief  开始IAP升级
  * @param  None
  * @retval IAP_Result_t
  */
IAP_Result_t IAP_Start_Upgrade(void)
{
    if (g_iap_state != IAP_STATE_IDLE)
    {
        printf("IAP: Already in progress\r\n");
        return IAP_BUSY;
    }
    
    printf("IAP: Starting firmware upgrade...\r\n");
    printf("IAP: Please send firmware file using Xmodem protocol\r\n");
    
    g_iap_state = IAP_STATE_RECEIVING;
    
    /* 使用Xmodem接收固件 */
    Xmodem_Result_t result = Xmodem_Receive(g_iap_buffer, IAP_BUFFER_SIZE, &g_firmware_size);
    
    switch (result)
    {
        case XMODEM_OK:
            printf("IAP: Firmware received successfully, size = %d bytes\r\n", g_firmware_size);
            return IAP_Process_Firmware();
            
        case XMODEM_TIMEOUT:
            printf("IAP: Receive timeout\r\n");
            g_iap_state = IAP_STATE_ERROR;
            return IAP_TIMEOUT;
            
        case XMODEM_CANCELLED:
            printf("IAP: Receive cancelled\r\n");
            g_iap_state = IAP_STATE_IDLE;
            return IAP_CANCELLED;
            
        default:
            printf("IAP: Receive error\r\n");
            g_iap_state = IAP_STATE_ERROR;
            return IAP_ERROR;
    }
}

/**
  * @brief  处理接收到的固件
  * @param  None
  * @retval IAP_Result_t
  */
static IAP_Result_t IAP_Process_Firmware(void)
{
    g_iap_state = IAP_STATE_VERIFYING;
    
    /* 检查固件大小 */
    if (g_firmware_size == 0 || g_firmware_size > IAP_APP_MAX_SIZE)
    {
        printf("IAP: Invalid firmware size %d\r\n", g_firmware_size);
        g_iap_state = IAP_STATE_ERROR;
        return IAP_ERROR;
    }
    
    /* 检查固件头部信息 */
    uint32_t stack_pointer = *(uint32_t*)g_iap_buffer;
    uint32_t reset_handler = *(uint32_t*)(g_iap_buffer + 4);
    
    if (stack_pointer < 0x20000000 || stack_pointer > 0x20010000)
    {
        printf("IAP: Invalid stack pointer 0x%08X\r\n", stack_pointer);
        g_iap_state = IAP_STATE_ERROR;
        return IAP_ERROR;
    }
    
    if (reset_handler < IAP_APP_START_ADDR || reset_handler > 0x0807EFFF)
    {
        printf("IAP: Invalid reset handler 0x%08X\r\n", reset_handler);
        g_iap_state = IAP_STATE_ERROR;
        return IAP_ERROR;
    }
    
    /* 计算CRC */
    g_firmware_crc = Flash_Calculate_CRC32(g_iap_buffer, g_firmware_size);
    printf("IAP: Firmware CRC32 = 0x%08X\r\n", g_firmware_crc);
    
    printf("IAP: Firmware verification passed\r\n");
    printf("  - Size: %d bytes\r\n", g_firmware_size);
    printf("  - Stack: 0x%08X\r\n", stack_pointer);
    printf("  - Reset: 0x%08X\r\n", reset_handler);
    printf("  - CRC32: 0x%08X\r\n", g_firmware_crc);
    
    /* 开始编程 */
    return IAP_Program_Firmware();
}

/**
  * @brief  编程固件到Flash
  * @param  None
  * @retval IAP_Result_t
  */
static IAP_Result_t IAP_Program_Firmware(void)
{
    g_iap_state = IAP_STATE_PROGRAMMING;
    
    printf("IAP: Programming firmware to internal flash...\r\n");
    
    /* 计算需要擦除的页数 */
    uint32_t pages_to_erase = (g_firmware_size + 2047) / 2048;
    printf("IAP: Erasing %d pages...\r\n", pages_to_erase);
    
    /* 擦除应用程序区域 */
    if (Flash_Internal_Erase(IAP_APP_START_ADDR, pages_to_erase) != FLASH_OK)
    {
        printf("IAP: Flash erase failed\r\n");
        g_iap_state = IAP_STATE_ERROR;
        return IAP_ERROR;
    }
    
    /* 编程固件数据 */
    printf("IAP: Writing firmware data...\r\n");
    
    uint32_t bytes_written = 0;
    uint32_t write_addr = IAP_APP_START_ADDR;
    
    while (bytes_written < g_firmware_size)
    {
        uint32_t chunk_size = g_firmware_size - bytes_written;
        if (chunk_size > 256)
            chunk_size = 256;
        
        /* 确保是4字节对齐 */
        if (chunk_size % 4 != 0)
            chunk_size = (chunk_size + 3) & ~3;
        
        if (Flash_Internal_Write(write_addr, g_iap_buffer + bytes_written, chunk_size) != FLASH_OK)
        {
            printf("IAP: Flash write failed at address 0x%08X\r\n", write_addr);
            g_iap_state = IAP_STATE_ERROR;
            return IAP_ERROR;
        }
        
        bytes_written += chunk_size;
        write_addr += chunk_size;
        
        /* 显示进度 */
        uint8_t progress = (bytes_written * 100) / g_firmware_size;
        printf("IAP: Programming... %d%%\r", progress);
    }
    
    printf("\r\nIAP: Firmware programming completed\r\n");
    
    /* 验证编程结果 */
    return IAP_Verify_Firmware();
}

/**
  * @brief  验证编程的固件
  * @param  None
  * @retval IAP_Result_t
  */
static IAP_Result_t IAP_Verify_Firmware(void)
{
    printf("IAP: Verifying programmed firmware...\r\n");
    
    /* 读取Flash中的数据并计算CRC */
    uint32_t calculated_crc = 0;
    uint8_t verify_buffer[256];
    uint32_t bytes_verified = 0;
    
    while (bytes_verified < g_firmware_size)
    {
        uint32_t chunk_size = g_firmware_size - bytes_verified;
        if (chunk_size > 256)
            chunk_size = 256;
        
        if (Flash_Internal_Read(IAP_APP_START_ADDR + bytes_verified, 
                               verify_buffer, chunk_size) != FLASH_OK)
        {
            printf("IAP: Flash read failed\r\n");
            g_iap_state = IAP_STATE_ERROR;
            return IAP_ERROR;
        }
        
        /* 比较数据 */
        if (memcmp(verify_buffer, g_iap_buffer + bytes_verified, chunk_size) != 0)
        {
            printf("IAP: Data verification failed at offset %d\r\n", bytes_verified);
            g_iap_state = IAP_STATE_ERROR;
            return IAP_ERROR;
        }
        
        bytes_verified += chunk_size;
    }
    
    /* 检查应用程序有效性 */
    if (App_Check_Validity() != APP_VALID)
    {
        printf("IAP: Application validity check failed\r\n");
        g_iap_state = IAP_STATE_ERROR;
        return IAP_ERROR;
    }
    
    printf("IAP: Firmware verification passed\r\n");
    g_iap_state = IAP_STATE_COMPLETE;
    
    /* 保存升级记录 */
    IAP_Save_Upgrade_Record();
    
    printf("IAP: Firmware upgrade completed successfully!\r\n");
    printf("IAP: You can now reset the system or type 'jump' to start the new application\r\n");
    
    return IAP_OK;
}

/**
  * @brief  保存升级记录
  * @param  None
  * @retval None
  */
static void IAP_Save_Upgrade_Record(void)
{
    /* 这里可以在配置区保存升级记录 */
    printf("IAP: Upgrade record saved\r\n");
}

/**
  * @brief  取消IAP升级
  * @param  None
  * @retval None
  */
void IAP_Cancel_Upgrade(void)
{
    if (g_iap_state == IAP_STATE_RECEIVING)
    {
        Xmodem_Cancel();
    }
    
    g_iap_state = IAP_STATE_IDLE;
    printf("IAP: Upgrade cancelled\r\n");
}

/**
  * @brief  获取IAP状态
  * @param  None
  * @retval IAP状态
  */
IAP_State_t IAP_Get_State(void)
{
    return g_iap_state;
}

/**
  * @brief  清理IAP资源
  * @param  None
  * @retval None
  */
void IAP_Cleanup(void)
{
    if (g_iap_buffer != NULL)
    {
        free(g_iap_buffer);
        g_iap_buffer = NULL;
    }
    
    g_iap_state = IAP_STATE_IDLE;
}
```

## 头文件定义

### xmodem.h
```c
#ifndef __XMODEM_H
#define __XMODEM_H

#include "stm32f10x.h"

/* Xmodem结果 */
typedef enum {
    XMODEM_OK = 0,
    XMODEM_ERROR,
    XMODEM_TIMEOUT,
    XMODEM_CANCELLED,
    XMODEM_CRC_ERROR
} Xmodem_Result_t;

/* 函数声明 */
Xmodem_Result_t Xmodem_Receive(uint8_t* buffer, uint32_t max_size, uint32_t* received_size);
void Xmodem_Cancel(void);

/* 私有函数声明 */
static uint16_t Xmodem_Calculate_CRC16(uint8_t* data, uint16_t length);
static int16_t Xmodem_Wait_Byte(uint32_t timeout_ms);
static void Xmodem_Send_Byte(uint8_t data);
static Xmodem_Result_t Xmodem_Receive_Packets(uint8_t* buffer, uint32_t max_size, 
                                             uint32_t* received_size, uint8_t first_byte);

#endif /* __XMODEM_H */
```

### iap_manager.h
```c
#ifndef __IAP_MANAGER_H
#define __IAP_MANAGER_H

#include "stm32f10x.h"
#include <stdlib.h>
#include <string.h>

/* IAP结果 */
typedef enum {
    IAP_OK = 0,
    IAP_ERROR,
    IAP_BUSY,
    IAP_TIMEOUT,
    IAP_CANCELLED
} IAP_Result_t;

/* 函数声明 */
IAP_Result_t IAP_Manager_Init(void);
IAP_Result_t IAP_Start_Upgrade(void);
void IAP_Cancel_Upgrade(void);
void IAP_Cleanup(void);

/* 私有函数声明 */
static IAP_Result_t IAP_Process_Firmware(void);
static IAP_Result_t IAP_Program_Firmware(void);
static IAP_Result_t IAP_Verify_Firmware(void);
static void IAP_Save_Upgrade_Record(void);

#endif /* __IAP_MANAGER_H */
```

## 使用示例

### 在命令系统中集成IAP
```c
// 在command_system.c中添加IAP命令
static void Cmd_IAP_Start(int argc, char* argv[])
{
    printf("Starting IAP upgrade...\r\n");
    
    if (IAP_Manager_Init() != IAP_OK)
    {
        printf("IAP initialization failed\r\n");
        return;
    }
    
    IAP_Result_t result = IAP_Start_Upgrade();
    
    switch (result)
    {
        case IAP_OK:
            printf("IAP upgrade completed successfully\r\n");
            break;
        case IAP_ERROR:
            printf("IAP upgrade failed\r\n");
            break;
        case IAP_TIMEOUT:
            printf("IAP upgrade timeout\r\n");
            break;
        case IAP_CANCELLED:
            printf("IAP upgrade cancelled\r\n");
            break;
        default:
            break;
    }
    
    IAP_Cleanup();
}
```

## 测试验证

### IAP功能测试步骤
1. **命令测试**：
   ```
   Bootloader> iap
   Starting IAP upgrade...
   IAP: Ready to receive, send your file now...
   ```

2. **固件发送**：使用支持Xmodem的工具发送.bin文件

3. **升级验证**：
   ```
   IAP: Firmware received successfully, size = 12345 bytes
   IAP: Programming firmware to internal flash...
   IAP: Firmware upgrade completed successfully!
   ```

4. **应用测试**：
   ```
   Bootloader> jump
   Jumping to application...
   ```

## 验证标准
1. Xmodem协议接收成功率 > 99%
2. 大文件传输稳定，支持断点重传
3. 固件校验准确，无误编程
4. 升级失败能正确恢复
5. 整个升级过程用户体验良好

## 下一步行动
串口IAP实现完成后，继续开发固件管理功能。