/*
 * OpenLoad - YMODEM (Batch) Receiver
 *
 * 兼容 XMODEM-CRC 帧格式, 多出一个 block 0 传递文件名 + 大小. 单文件场景下
 * 与 XMODEM-1K 流程极其相似, 多了首尾两个文件信息帧.
 *
 * 与 XMODEM 一样, 收到的数据流式写入 staging 分区, 不裁剪末尾 padding ——
 * 由 image header 的 firmware_size 在后续 verify / install 阶段自然截断.
 */
#pragma once

#include "openload/receiver.h"

/* 全局 receiver 实例, 通过 OL_RECEIVER_REGISTER 自动加入注册表. */
extern ol_receiver_t ol_ymodem_receiver;
