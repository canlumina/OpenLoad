#ifndef W25Q64_H
#define W25Q64_H

#include <stdint.h>
#include <stdbool.h>
#include "spi.h"

/* W25Q64 芯片参数 */
#define W25Q64_CHIP_SIZE        0x800000    // 8MB
#define W25Q64_BLOCK_SIZE       0x10000     // 64KB
#define W25Q64_SECTOR_SIZE      0x1000      // 4KB  
#define W25Q64_PAGE_SIZE        0x100       // 256 bytes
#define W25Q64_NUM_BLOCKS       128
#define W25Q64_NUM_SECTORS      2048

/* W25Q64 命令定义 */
#define W25Q64_CMD_WRITE_ENABLE         0x06
#define W25Q64_CMD_WRITE_DISABLE        0x04
#define W25Q64_CMD_READ_STATUS_REG1     0x05
#define W25Q64_CMD_READ_STATUS_REG2     0x35
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
#define W25Q64_CMD_DEVICE_ID            0xAB
#define W25Q64_CMD_MANUFACTURER_ID      0x90
#define W25Q64_CMD_JEDEC_ID             0x9F
#define W25Q64_CMD_ENABLE_RESET         0x66
#define W25Q64_CMD_RESET                0x99

/* 状态寄存器位定义 */
#define W25Q64_SR_BUSY          0x01
#define W25Q64_SR_WEL           0x02

/* 超时定义 */
#define W25Q64_TIMEOUT_PAGE_PROGRAM    5      // 5ms
#define W25Q64_TIMEOUT_SECTOR_ERASE    400    // 400ms
#define W25Q64_TIMEOUT_BLOCK_ERASE     2000   // 2s
#define W25Q64_TIMEOUT_CHIP_ERASE      100000 // 100s

/* 外部Flash分区定义 */
typedef enum {
    W25Q64_PARTITION_DOWNLOAD = 0,  // 下载区
    W25Q64_PARTITION_BACKUP1,       // 备份区1
    W25Q64_PARTITION_BACKUP2,       // 备份区2
    W25Q64_PARTITION_BACKUP3,       // 备份区3
    W25Q64_PARTITION_LOG,           // 日志区
    W25Q64_PARTITION_CONFIG,        // 配置区
    W25Q64_PARTITION_RESERVED,      // 预留区
    W25Q64_PARTITION_MAX
} w25q64_partition_id_t;

/* 分区信息结构 */
typedef struct {
    const char* name;
    uint32_t start_addr;
    uint32_t size;
    const char* description;
} w25q64_partition_t;

/* 分区表 - 总容量8MB */
#define W25Q64_PARTITION_TABLE { \
    {"Download",  0x000000, 0x200000, "Firmware download area (2MB)"},     \
    {"Backup1",   0x200000, 0x070000, "Firmware backup slot 1 (448KB)"},   \
    {"Backup2",   0x270000, 0x070000, "Firmware backup slot 2 (448KB)"},   \
    {"Backup3",   0x2E0000, 0x070000, "Firmware backup slot 3 (448KB)"},   \
    {"Log",       0x350000, 0x080000, "System log area (512KB)"},          \
    {"Config",    0x3D0000, 0x010000, "Configuration area (64KB)"},        \
    {"Reserved",  0x3E0000, 0x420000, "Reserved area (4.125MB)"}           \
}

/* 函数声明 */

/* 基础操作 */
int w25q64_init(void);
uint32_t w25q64_read_id(void);
bool w25q64_is_busy(void);
void w25q64_wait_busy(void);
void w25q64_write_enable(void);
void w25q64_write_disable(void);

/* 读操作 */
int w25q64_read(long addr, uint8_t* buffer, size_t size);
void w25q64_fast_read(uint32_t addr, uint8_t* buffer, uint32_t size);

/* 写操作 */
bool w25q64_write_page(uint32_t addr, const uint8_t* data, uint32_t size);
int w25q64_write(long addr, const uint8_t* data, size_t size);

/* 擦除操作 */
bool w25q64_erase_sector(uint32_t addr);
bool w25q64_erase_block(uint32_t addr);
bool w25q64_erase_chip(void);

/* 分区操作 */
const w25q64_partition_t* w25q64_get_partition(w25q64_partition_id_t id);
bool w25q64_erase_partition(w25q64_partition_id_t id);
bool w25q64_read_partition(w25q64_partition_id_t id, uint32_t offset, uint8_t* buffer, uint32_t size);
bool w25q64_write_partition(w25q64_partition_id_t id, uint32_t offset, const uint8_t* data, uint32_t size);
// void w25q64_print_partition_info(void);  // 已注释掉以节省代码空间

/* 电源管理 */
void w25q64_power_down(void);
void w25q64_power_up(void);

/* 芯片复位 */
void w25q64_reset(void);

/* SPI接口函数 - 需要用户实现 */
void w25q64_cs_low(void);
void w25q64_cs_high(void);
uint8_t w25q64_spi_transmit(uint8_t data);
void w25q64_spi_transmit_buffer(const uint8_t* tx_data, uint8_t* rx_data, uint32_t size);

#endif /* W25Q64_H */

