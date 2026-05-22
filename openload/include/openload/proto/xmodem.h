/*
 * OpenLoad - XMODEM / XMODEM-1K Receiver
 *
 * 实现经典 XMODEM-CRC16 协议:
 *   - 128 byte packet (SOH 0x01) — 始终支持
 *   - 1024 byte packet (STX 0x02) — OPENLOAD_ENABLE_XMODEM_1K 启用
 * 流式写入 staging 分区, 不缓存整个固件。
 */
#pragma once

#include "openload/receiver.h"

/* 全局 receiver 实例, 通过 OL_RECEIVER_REGISTER 自动加入注册表. */
extern ol_receiver_t ol_xmodem_receiver;

/* 控制字符 (协议层公开仅为方便用户实现自定义变种) */
#define XMODEM_SOH   0x01
#define XMODEM_STX   0x02
#define XMODEM_EOT   0x04
#define XMODEM_ACK   0x06
#define XMODEM_NAK   0x15
#define XMODEM_CAN   0x18
#define XMODEM_C     0x43   /* 'C' CRC mode advertisement */
