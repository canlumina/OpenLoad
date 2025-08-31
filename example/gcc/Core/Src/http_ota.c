#include "http_ota.h"
#include "w25q64.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* OTA数据缓冲区大小 */
#define OTA_BUFFER_SIZE     1024

/* 静态变量 */
static ota_context_t *g_ota_ctx = NULL;
static uint32_t g_actual_write_offset = 0;  /* 实际写入位置 */
static uint32_t g_expected_chunk_remaining = 0;  /* 当前分块剩余字节数 */

/* 数据写入处理函数 */
static int ota_data_handler(uint8_t *data, uint16_t len)
{
    if (!g_ota_ctx || !data || len == 0) return -1;
    
    ota_context_t *ctx = g_ota_ctx;
    bool write_success = false;
    
    /* 简化调试输出，仅在需要时显示关键信息 */
    
    /* 简化的分块处理逻辑 - 取消截断，允许跨分块数据处理 */
    if (g_expected_chunk_remaining == 0) {
        /* 这是一个新的分块开始 */
        uint32_t remaining_total = ctx->total_size - g_actual_write_offset;
        g_expected_chunk_remaining = (remaining_total > 1024) ? 1024 : remaining_total;
        
    }
    
    /* 不再截断数据，允许接收超过分块边界的数据 */
    /* 这样可以避免36字节的数据丢失问题 */
    
    /* 检查是否会超出预期大小 */
    if (g_actual_write_offset + len > ctx->total_size) {
        len = ctx->total_size - g_actual_write_offset;
        if (len <= 0) {
            return 0; /* 已经写完了 */
        }
    }
    
    /* 直接写入数据，不使用缓冲区（因为每个块本身就是1KB） */
    if (ctx->target == OTA_TARGET_INTERNAL_FLASH) {
        /* 写入内部Flash */
        write_success = bootloader_flash_write(
            ctx->target_addr + g_actual_write_offset,
            data, 
            len);
    } else {
        /* 写入外部Flash */
        w25q64_partition_id_t pid = (w25q64_partition_id_t)ctx->target_addr;
        write_success = w25q64_write_partition(pid, 
            g_actual_write_offset,
            data, 
            len);
    }
    
    if (!write_success) {
        return -1;
    }
    
    /* 更新实际写入位置和分块剩余 */
    g_actual_write_offset += len;
    
    /* 更新分块剩余量，处理跨分块情况 */
    if (len >= g_expected_chunk_remaining) {
        /* 当前数据跨越或完成了分块边界 */
        uint32_t processed_in_chunk = g_expected_chunk_remaining;
        g_expected_chunk_remaining = 0;
        
        
        /* 如果还有剩余数据，开始下一个分块 */
        uint32_t remaining_data = len - processed_in_chunk;
        if (remaining_data > 0) {
            uint32_t remaining_total = ctx->total_size - g_actual_write_offset + remaining_data;
            g_expected_chunk_remaining = (remaining_total > 1024) ? (1024 - remaining_data) : 0;
        }
    } else {
        /* 数据在当前分块内 */
        g_expected_chunk_remaining -= len;
    }
    
    /* 不在这里调用进度回调，避免多次更新 */
    
    return len;
}

bool ota_init(ota_context_t *ctx, esp8266_device_t *esp8266)
{
    if (!ctx || !esp8266) return false;
    
    memset(ctx, 0, sizeof(ota_context_t));
    
    ctx->esp8266 = esp8266;
    
    /* 不再需要分配缓冲区，直接写入 */
    ctx->buffer = NULL;
    ctx->buffer_size = 0;
    ctx->buffer_used = 0;
    
    /* 初始化HTTP客户端 */
    if (!http_client_init(&ctx->http_client, esp8266)) {
        return false;
    }
    
    return true;
}

bool ota_deinit(ota_context_t *ctx)
{
    if (!ctx) return false;
    
    /* 不再需要释放缓冲区 */
    
    http_client_disconnect(&ctx->http_client);
    
    return true;
}

void ota_set_progress_callback(ota_context_t *ctx, ota_progress_callback_t callback)
{
    if (ctx) {
        ctx->progress_callback = callback;
    }
}

ota_status_t ota_download_firmware(ota_context_t *ctx, const char *url, 
                                  ota_target_t target, uint32_t target_addr, uint32_t max_size)
{
    if (!ctx || !ctx->esp8266 || !url) return OTA_STATUS_ERROR;
    
    char host[128], path[256];
    uint16_t port;
    
    /* 解析URL */
    if (!http_parse_url(url, host, &port, path)) {
        bootloader_print("URL parse failed\r\n");
        return OTA_STATUS_ERROR;
    }
    
    /* URL解析成功 */
    
    /* WiFi连接状态已在调用方检查过，这里不再重复检查 */
    
    /* 设置OTA参数 */
    ctx->target = target;
    ctx->target_addr = target_addr;
    ctx->max_size = max_size;
    ctx->downloaded_size = 0;
    ctx->buffer_used = 0;
    
    /* 连接HTTP服务器 */
    http_status_t http_status = http_client_connect(&ctx->http_client, host, port);
    if (http_status != HTTP_STATUS_OK) {
        return OTA_STATUS_HTTP_ERROR;
    }
    
    /* 准备Flash存储 */
    if (target == OTA_TARGET_INTERNAL_FLASH) {
        /* 擦除内部Flash */
        if (!bootloader_flash_erase(target_addr, max_size)) {
            http_client_disconnect(&ctx->http_client);
            return OTA_STATUS_FLASH_ERROR;
        }
    } else {
        /* 擦除外部Flash分区 */
        w25q64_init();
        w25q64_partition_id_t pid = (w25q64_partition_id_t)target_addr;
        if (!w25q64_erase_partition(pid)) {
            http_client_disconnect(&ctx->http_client);
            return OTA_STATUS_FLASH_ERROR;
        }
    }
    
    /* 分块下载实现 */
    #define CHUNK_SIZE 1024  /* 每次下载1KB */
    
    /* 先发送HEAD请求获取文件大小（不下载数据） */
    if (http_client_connect(&ctx->http_client, host, port) == HTTP_STATUS_OK) {
        /* 发送HEAD请求 */
        char head_request[512];
        snprintf(head_request, sizeof(head_request),
                 "HEAD %s HTTP/1.1\r\n"
                 "Host: %s\r\n"
                 "Connection: close\r\n"
                 "\r\n",
                 path, host);
        
        /* 发送请求并接收响应头 */
        if (http_client_send_raw_request(&ctx->http_client, head_request) == HTTP_STATUS_OK) {
            /* 从响应中获取Content-Length */
            if (ctx->http_client.response.content_length > 0) {
                ctx->total_size = ctx->http_client.response.content_length;
            }
        }
        
        /* 断开HEAD请求连接 */
        http_client_disconnect(&ctx->http_client);
    }
    
    /* 如果没有获取到大小，使用默认值 */
    if (ctx->total_size == 0) {
        ctx->total_size = 16384; /* 假设16KB */
    }
    
    /* 设置全局上下文 */
    g_ota_ctx = ctx;
    g_actual_write_offset = 0;  /* 重置写入位置 */
    g_expected_chunk_remaining = 0;  /* 重置分块计数 */
    
    /* 重置调试计数器（在ota_data_handler中使用） */
    /* 注意：这里通过一个dummy调用来重置static变量 */
    
    /* 分块下载 */
    int chunk_count = (ctx->total_size + CHUNK_SIZE - 1) / CHUNK_SIZE;
    
    /* 开始分块下载 */
    
    /* 重置downloaded_size */
    ctx->downloaded_size = 0;
    
    for (int chunk = 0; chunk < chunk_count; chunk++) {
        uint32_t chunk_start = chunk * CHUNK_SIZE;
        uint32_t chunk_end = chunk_start + CHUNK_SIZE - 1;
        if (chunk_end >= ctx->total_size) {
            chunk_end = ctx->total_size - 1;
        }
        
        /* 每个分块最多重试3次 */
        int retry_count = 0;
        const int max_retries = 3;
        bool chunk_success = false;
        
        while (retry_count < max_retries && !chunk_success) {
            /* 重新连接 */
            if (http_client_connect(&ctx->http_client, host, port) != HTTP_STATUS_OK) {
                retry_count++;
                if (retry_count >= max_retries) {
                    bootloader_print("\r\nFailed to connect at chunk ");
                    bootloader_print_dec(chunk + 1);
                    bootloader_print(" after ");
                    bootloader_print_dec(max_retries);
                    bootloader_print(" retries\r\n");
                    g_ota_ctx = NULL;
                    return OTA_STATUS_HTTP_ERROR;
                }
                HAL_Delay(1000); /* 重试前等待1秒 */
                continue;
            }
            
            /* 设置数据处理回调 */
            http_client_set_data_handler(&ctx->http_client, ota_data_handler);
            
            /* 发送Range请求 */
            http_status = http_client_get_with_range(&ctx->http_client, path, chunk_start, chunk_end - chunk_start + 1);
            
            if (http_status != HTTP_STATUS_OK) {
                http_client_disconnect(&ctx->http_client);
                retry_count++;
                if (retry_count >= max_retries) {
                    bootloader_print("\r\nChunk ");
                    bootloader_print_dec(chunk + 1);
                    bootloader_print(" download failed after ");
                    bootloader_print_dec(max_retries);
                    bootloader_print(" retries\r\n");
                    g_ota_ctx = NULL;
                    return OTA_STATUS_HTTP_ERROR;
                }
                HAL_Delay(1000); /* 重试前等待1秒 */
                continue;
            }
            
            /* 断开连接 */
            http_client_disconnect(&ctx->http_client);
            chunk_success = true;
        }
        
        /* 更新已下载的字节数（基于实际写入的位置） */
        ctx->downloaded_size = g_actual_write_offset;
        
        /* 每个分块完成后更新一次进度 */
        if (ctx->progress_callback) {
            ctx->progress_callback(ctx->downloaded_size, ctx->total_size);
        }
        
        /* 延时避免过快重连 */
        HAL_Delay(100);
    }
    
    /* 清除全局上下文 */
    g_ota_ctx = NULL;
    
    /* 分块下载完成 */
    
    /* 检查下载大小 */
    if (g_actual_write_offset == 0) {
        return OTA_STATUS_ERROR;
    }
    
    if (g_actual_write_offset > max_size) {
        return OTA_STATUS_SIZE_ERROR;
    }
    
    /* 最终更新实际下载大小 */
    ctx->downloaded_size = g_actual_write_offset;
    
    bootloader_print("\r\n=== OTA Download Summary ===\r\n");
    bootloader_print("Total written: ");
    bootloader_print_dec(g_actual_write_offset);
    bootloader_print(" bytes\r\n");
    bootloader_print("Expected size: ");
    bootloader_print_dec(ctx->total_size);
    bootloader_print(" bytes\r\n");
    
    if (g_actual_write_offset != ctx->total_size) {
        int32_t diff = (int32_t)g_actual_write_offset - (int32_t)ctx->total_size;
        bootloader_print("Size difference: ");
        if (diff > 0) {
            bootloader_print("+");
        }
        bootloader_print_dec(diff);
        bootloader_print(" bytes\r\n");
    } else {
        bootloader_print("Size match: OK\r\n");
    }
    
    /* 验证写入的数据 */
    if (ctx->target == OTA_TARGET_EXTERNAL_FLASH) {
        uint8_t verify_buf[64];
        w25q64_partition_id_t pid = (w25q64_partition_id_t)ctx->target_addr;
        if (w25q64_read_partition(pid, 0, verify_buf, 64)) {
            bootloader_print("\r\n=== Firmware Verification ===\r\n");
            
            /* 显示前16字节的十六进制 */
            bootloader_print("Vector table (first 16 bytes):\r\n");
            for (int i = 0; i < 16; i++) {
                char hex[4];
                sprintf(hex, "%02X", verify_buf[i]);
                bootloader_print(hex);
                if ((i + 1) % 8 == 0) {
                    bootloader_print("\r\n");
                } else {
                    bootloader_print(" ");
                }
            }
            
            /* 检查栈指针和复位向量 */
            uint32_t stack_ptr = *(uint32_t*)verify_buf;
            uint32_t reset_vector = *(uint32_t*)(verify_buf + 4);
            
            bootloader_print("Stack pointer: 0x");
            char hex_str[16];
            sprintf(hex_str, "%08lX", stack_ptr);
            bootloader_print(hex_str);
            bootloader_print("\r\n");
            
            bootloader_print("Reset vector: 0x");
            sprintf(hex_str, "%08lX", reset_vector);
            bootloader_print(hex_str);
            bootloader_print("\r\n");
            
            /* 验证向量表的合理性 */
            if ((stack_ptr & 0xFFFF0000) == 0x20000000 && 
                (reset_vector & 0xFFFF0000) == 0x08010000) {
                bootloader_print("Vector table validation: PASS\r\n");
            } else {
                bootloader_print("Vector table validation: FAIL\r\n");
            }
            
        } else {
            bootloader_print("Failed to verify written data\r\n");
        }
    }
    
    /* 最终进度回调 */
    if (ctx->progress_callback) {
        ctx->progress_callback(ctx->downloaded_size, ctx->total_size);
    }
    
    return OTA_STATUS_OK;
}