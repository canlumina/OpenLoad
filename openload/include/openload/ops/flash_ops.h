/*
 * OpenLoad - Flash ops 接口
 *
 * 抽象一个可读/写/擦的非易失性存储设备 (内部 Flash、SPI NOR、SD 等),
 * 由 partition 层在其上叠加分区视图。框架不假设设备是否可 XIP。
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct ol_flash_dev ol_flash_dev_t;

typedef struct {
    /**
     * 必填: 读 [offset, offset+len). 成功返回 OL_OK, < 0 = 错误码.
     * 对 XIP 设备 (内部 Flash) 通常退化为 memcpy。
     */
    int (*read)(ol_flash_dev_t *dev, uint32_t offset,
                void *buf, uint32_t len);

    /**
     * 必填: 写 [offset, offset+len)。len 必须按 write_granularity 对齐;
     * 目标 sector 必须已擦除 (NOR 写入只能 1 → 0)。
     */
    int (*write)(ol_flash_dev_t *dev, uint32_t offset,
                 const void *buf, uint32_t len);

    /**
     * 必填: 擦除 [offset, offset+len)。offset、len 必须按 sector_size 对齐。
     * 一次擦除可能很慢 (内部 Flash 数十 ms / 外部 Flash 数百 ms),
     * 实现可在内部分次擦除并喂狗。
     */
    int (*erase)(ol_flash_dev_t *dev, uint32_t offset, uint32_t len);

    /** 可选: 解锁 (内部 Flash 的写保护、SPI Flash 的状态寄存器等). */
    int (*unlock)(ol_flash_dev_t *dev);

    /** 可选: 重新加锁. */
    int (*lock)(ol_flash_dev_t *dev);
} ol_flash_ops_t;

struct ol_flash_dev {
    const char           *name;              /* 设备名 (分区表引用) */
    uint32_t              base;              /* 起始绝对地址 (XIP 设备用; 非 XIP 填 0) */
    uint32_t              size;              /* 设备总大小 (bytes) */
    uint32_t              sector_size;       /* 最小擦除单元 */
    uint32_t              write_granularity; /* 最小写入粒度: 1/2/4/8/256 ... */
    bool                  xip;               /* true = 可直接 CPU 取址 (内部 Flash) */
    const ol_flash_ops_t *ops;
    void                 *priv;              /* 驱动私有数据 */
};

/**
 * 用户通过此宏在源文件中静态注册 Flash 设备:
 *     OL_FLASH_DEV_REGISTER(internal, &g_internal_flash_dev);
 * 框架通过链接段 ".ol_flash_devs" 枚举设备。
 */
#define OL_FLASH_DEV_REGISTER(_id, _dev_ptr) \
    static const ol_flash_dev_t * const __ol_flash_##_id \
        __attribute__((used, section(".ol_flash_devs"))) = (_dev_ptr)

/**
 * @brief 按名字查找已注册 Flash 设备.
 * @return  设备指针, 未找到返回 NULL.
 */
ol_flash_dev_t *ol_flash_dev_find(const char *name);
