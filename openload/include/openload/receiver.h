/*
 * OpenLoad - 升级数据接收器抽象
 *
 * 每种升级协议 (XMODEM / YMODEM / HTTP OTA / 用户自定义) 实现一份
 * ol_receiver_t 实例。Updater 通过统一接口启动协议、轮询数据、收尾。
 */
#pragma once

#include <stdint.h>
#include "openload/ops/io_ops.h"
#include "openload/partition.h"

typedef struct ol_receiver ol_receiver_t;

typedef struct {
    /**
     * 必填: 启动接收 (协议握手、解析头部等). 不阻塞太久; 重活在 poll 里做。
     * @param  io   字节流通道 (UART/网络/...)
     * @param  dst  接收数据写入的目标分区
     * @return OL_OK = 已就绪开始接收, 其它为各类错误码.
     */
    int (*begin)(ol_receiver_t *r, ol_io_dev_t *io,
                 const ol_partition_t *dst);

    /**
     * 必填: 主循环单次推进. 返回:
     *   1 = 接收完成 (全部数据已落 flash)
     *   0 = 仍在进行中
     *  <0 = 错误码
     */
    int (*poll)(ol_receiver_t *r);

    /** 必填: 收尾 (无论成功失败都必须调). 释放协议私有状态、关闭 socket 等. */
    int (*end)(ol_receiver_t *r);

    /** 可选: 返回进度百分比 0..100. NULL 时框架显示 "..." 字样. */
    uint8_t (*progress)(ol_receiver_t *r);

    /**
     * 可选: 在 begin 之前喂入额外初始化参数. 由 ol_updater_run 在
     * url_or_null 非 NULL 时调用 (HTTP OTA 用 URL 字符串). XMODEM/YMODEM
     * 等不需要外部参数的协议留 NULL.
     */
    int (*prepare)(ol_receiver_t *r, const void *arg);
} ol_receiver_ops_t;

struct ol_receiver {
    const char              *name;   /* "xmodem" / "ymodem" / "http" / ... */
    const ol_receiver_ops_t *ops;
    void                    *priv;   /* 协议私有状态 */
};

/**
 * 用户/协议作者通过此宏注册一个 receiver:
 *     OL_RECEIVER_REGISTER(xmodem, &ol_xmodem_receiver);
 * 框架通过链接段 ".ol_receivers" 枚举。
 */
#define OL_RECEIVER_REGISTER(_id, _r_ptr) \
    static ol_receiver_t * const __ol_recv_##_id \
        __attribute__((used, section(".ol_receivers"))) = (_r_ptr)

/** 按名字查找 receiver. */
ol_receiver_t *ol_receiver_find(const char *name);
