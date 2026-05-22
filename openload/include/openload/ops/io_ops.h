/*
 * OpenLoad - IO ops 接口
 *
 * 统一抽象字节流通道, 协议层 (XMODEM/HTTP/...) 只与此接口打交道,
 * 不关心底层是 UART / USB CDC / TCP socket / RTT 等。
 */
#pragma once

#include <stdint.h>

typedef struct ol_io_dev ol_io_dev_t;

typedef struct {
    /**
     * 必填: 非阻塞读. 返回实际读到的字节数 (0 = 无数据), < 0 = 错误码.
     * 不允许阻塞, 即使没有数据也必须立即返回。
     */
    int (*read)(ol_io_dev_t *dev, uint8_t *buf, uint32_t len);

    /**
     * 必填: 阻塞写, 全部写完返回 len; < 0 = 错误码.
     * 实现可在内部排队 + 后台 DMA, 但函数语义是"调用结束后数据已进入发送通道"。
     */
    int (*write)(ol_io_dev_t *dev, const uint8_t *buf, uint32_t len);

    /** 可选: 查询立即可读字节数; NULL 时框架退化为试探性 read. */
    int (*available)(ol_io_dev_t *dev);

    /** 可选: 刷出所有挂起的发送数据 (例如等待 DMA 完成). */
    int (*flush)(ol_io_dev_t *dev);
} ol_io_ops_t;

struct ol_io_dev {
    const char        *name;       /* 设备名, 例如 "console" / "esp_net" */
    const ol_io_ops_t *ops;
    void              *priv;       /* 驱动私有上下文 */
};

/**
 * 用户通过此宏在源文件中静态注册 IO 设备:
 *     OL_IO_DEV_REGISTER(console, &g_console_dev);
 * 框架通过链接段 ".ol_io_devs" 枚举全部已注册设备。
 */
#define OL_IO_DEV_REGISTER(_id, _dev_ptr) \
    static const ol_io_dev_t * const __ol_io_##_id \
        __attribute__((used, section(".ol_io_devs"))) = (_dev_ptr)

/**
 * @brief 按名字查找已注册 IO 设备.
 * @return  设备指针, 未找到返回 NULL.
 */
ol_io_dev_t *ol_io_dev_find(const char *name);

/**
 * @brief 带超时阻塞读. 基于 ops->read + ol_tick_ms() 轮询.
 * @return  实际读到字节数 (可能小于 len 表示超时), < 0 = 错误.
 */
int ol_io_read_timeout(ol_io_dev_t *dev, uint8_t *buf,
                       uint32_t len, uint32_t timeout_ms);

/** 一字节包装. */
int ol_io_putc(ol_io_dev_t *dev, uint8_t c);
int ol_io_getc_timeout(ol_io_dev_t *dev, uint8_t *c, uint32_t timeout_ms);
