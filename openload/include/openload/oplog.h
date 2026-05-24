/*
 * OpenLoad - 持久化操作日志 (oplog)
 *
 * 把关键事件 (boot / jump / update / install / verify failed) 写到一个
 * 独立分区, 掉电不丢. 启动后可通过 CLI dump 出来排查现场.
 *
 * 设计:
 *   - 固定 64B slot, slot N 占 [N*64 .. N*64+64) 字节偏移
 *   - 分区按 4KB sector 组织; SLOTS_PER_SECTOR = 64
 *   - 环形写入, 写指针跨 sector 边界时先 erase 目标 sector
 *   - 启动扫描所有 slot 找最大 seq, 确定 write_idx/next_seq
 *
 * Hook:
 *   logger.c 在 OPENLOAD_ENABLE_OPLOG=1 时, 对 ERR/WRN 自动 append.
 *   接入方也可显式调 ol_oplog_append.
 */
#pragma once

#include <stdint.h>

#define OL_OPLOG_SLOT_SIZE          64u
#define OL_OPLOG_MSG_MAX            44u     /* slot 内 msg 上限, 超长截断 */
#define OL_OPLOG_MAGIC              0x474F4C4Fu  /* "OLOG" 小端 */

/* 与 ol_log_level_t 对齐 */
#define OL_OPLOG_LVL_ERR            1
#define OL_OPLOG_LVL_WRN            2
#define OL_OPLOG_LVL_INF            3

typedef struct {
    uint32_t write_idx;     /* 下一条写入的 slot 号 (0..total_slots-1) */
    uint32_t next_seq;      /* 下一条 seq, 从 1 起 */
    uint32_t total_slots;
    uint32_t valid_count;   /* init 时统计的有效记录数 */
    uint8_t  ready;
} ol_oplog_stat_t;

/**
 * @brief 初始化 oplog. 找名为 "oplog" 的分区, 扫描定位写指针.
 * @return OL_OK 成功; 分区缺失 / 大小不合法 → silently disable, 返回错误码.
 */
int ol_oplog_init(void);

/**
 * @brief 追加一条记录. 协议层/logger 钩子调.
 * @param  level    OL_OPLOG_LVL_*
 * @param  msg      文本 (无需 null-term, 超长截断到 OL_OPLOG_MSG_MAX)
 * @param  len      msg 字节数
 * @return OL_OK / OL_E_NOT_FOUND (未 init) / 底层错误
 */
int ol_oplog_append(uint8_t level, const char *msg, uint32_t len);

/** 回调签名: 返回非 0 终止遍历. */
typedef int (*ol_oplog_iter_cb_t)(uint32_t seq, uint32_t ts_ms,
                                  uint8_t level, const char *msg, uint8_t msg_len,
                                  void *user);

/**
 * @brief 按时序遍历有效记录 (最旧 → 最新).
 * @param  cb     每条调一次
 * @param  user   传给 cb
 * @param  max    最多输出条数, 0 = 全部
 * @return 实际遍历条数 (>=0) 或错误码 (<0).
 */
int ol_oplog_iter(ol_oplog_iter_cb_t cb, void *user, uint32_t max);

/** 全擦. 不可恢复. */
int ol_oplog_clear(void);

/** 查询状态. */
int ol_oplog_get_stat(ol_oplog_stat_t *out);
