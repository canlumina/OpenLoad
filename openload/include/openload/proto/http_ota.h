/*
 * OpenLoad - HTTP OTA Receiver (M2)
 *
 * 通过 ol_io_dev_t name="net" 拉取 HTTP/1.0 资源, 流式写入 staging 分区.
 *
 * 限制 (M2 范围):
 *   - 仅支持 http:// 明文, 无 HTTPS (HTTPS 留 M3+ 接 mbedtls)
 *   - URL 格式: http://host[:port][/path]
 *   - 仅识别 Content-Length, 不支持 Transfer-Encoding: chunked
 *   - 状态码非 2xx 直接终止
 *
 * 接入步骤 (CLI):
 *   wifi join <ssid> <pass>                       (M2-12c, 一次)
 *   update http download app http://<host>/x.bin  (本 receiver)
 */
#pragma once

#include "openload/receiver.h"

extern ol_receiver_t ol_http_ota_receiver;
