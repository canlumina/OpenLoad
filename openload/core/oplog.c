/*
 * OpenLoad - 持久化操作日志实现
 *
 * 见 oplog.h 设计说明.
 *
 * 关键点:
 *   - record 布局固定 64B, struct __attribute__((packed)) 保证字段无 pad
 *   - CRC32 覆盖 [magic..msg] 共 60 字节, 排除自身的 crc32 字段
 *   - magic == 0xFFFFFFFF (NOR flash erased state) 视为 empty slot
 *   - in_progress 防重入: logger -> oplog_append -> 内部 OL_LOGE 不再钩
 */
#include "openload/oplog.h"
#include "openload/partition.h"
#include "openload/ops/sys_ops.h"
#include "openload/ops/flash_ops.h"
#include "openload/logger.h"
#include "openload/errno.h"
#include <string.h>
#include <stddef.h>

extern uint32_t ol_crc32(uint32_t init, const void *data, uint32_t len);

#define SLOTS_PER_SECTOR        (4096u / OL_OPLOG_SLOT_SIZE)   /* 64 */
#define ERASED_MAGIC            0xFFFFFFFFu

typedef struct __attribute__((packed)) {
    uint32_t magic;         /* 4 */
    uint32_t seq;           /* 4 */
    uint32_t ts_ms;         /* 4 */
    uint8_t  level;         /* 1 */
    uint8_t  msg_len;       /* 1 */
    uint8_t  _pad[2];       /* 2 */
    char     msg[OL_OPLOG_MSG_MAX]; /* 44 */
    uint32_t crc32;         /* 4 */
} oplog_rec_t;

_Static_assert(sizeof(oplog_rec_t) == OL_OPLOG_SLOT_SIZE,
               "oplog_rec_t must be 64 bytes");

static const ol_partition_t *s_part;
static ol_oplog_stat_t       s_stat;
static volatile uint8_t      s_in_progress;
static uint32_t              s_sector_size;

/* ---------- 工具 ---------- */

static uint32_t rec_calc_crc(const oplog_rec_t *r)
{
    /* 排除末尾的 crc32 字段 (4 字节) */
    return ol_crc32(0, r, OL_OPLOG_SLOT_SIZE - 4u);
}

static int rec_is_valid(const oplog_rec_t *r)
{
    if (r->magic != OL_OPLOG_MAGIC) { return 0; }
    return r->crc32 == rec_calc_crc(r);
}

static int read_slot(uint32_t idx, oplog_rec_t *out)
{
    return ol_part_read(s_part, idx * OL_OPLOG_SLOT_SIZE,
                        (uint8_t *)out, OL_OPLOG_SLOT_SIZE);
}

static int erase_sector_of(uint32_t slot_idx)
{
    uint32_t off = (slot_idx / SLOTS_PER_SECTOR) * s_sector_size;
    return ol_part_erase(s_part, off, s_sector_size);
}

/* ---------- public ---------- */

int ol_oplog_init(void)
{
    s_stat.ready = 0;
    s_part = ol_part_find("oplog");
    if (!s_part) { return OL_E_PART_NOT_FOUND; }

    ol_flash_dev_t *dev = ol_part_get_device(s_part);
    if (!dev || dev->sector_size == 0) { return OL_E_PART_NO_DEVICE; }
    s_sector_size = dev->sector_size;
    if (s_part->size % s_sector_size) { return OL_E_PART_ALIGN; }

    uint32_t total_slots = s_part->size / OL_OPLOG_SLOT_SIZE;
    if (total_slots == 0) { return OL_E_INVAL; }

    /* 扫描全部 slot, 找 max(seq) 的有效记录 */
    uint32_t max_seq = 0;
    uint32_t max_idx = 0;
    int      found   = 0;
    uint32_t valid   = 0;
    oplog_rec_t rec;
    for (uint32_t i = 0; i < total_slots; ++i) {
        if (read_slot(i, &rec) != OL_OK) { continue; }
        if (!rec_is_valid(&rec)) { continue; }
        valid++;
        if (!found || rec.seq > max_seq) {
            max_seq = rec.seq;
            max_idx = i;
            found   = 1;
        }
    }

    if (found) {
        s_stat.write_idx = (max_idx + 1u) % total_slots;
        s_stat.next_seq  = max_seq + 1u;
    } else {
        s_stat.write_idx = 0;
        s_stat.next_seq  = 1;
    }
    s_stat.total_slots = total_slots;
    s_stat.valid_count = valid;
    s_stat.ready       = 1;

    OL_LOGI("oplog: %u/%u used, next_seq=%u, write_idx=%u",
            (unsigned)valid, (unsigned)total_slots,
            (unsigned)s_stat.next_seq, (unsigned)s_stat.write_idx);
    return OL_OK;
}

int ol_oplog_append(uint8_t level, const char *msg, uint32_t len)
{
    if (!s_stat.ready) { return OL_E_NOT_FOUND; }
    if (s_in_progress)  { return OL_OK; }     /* 重入直接吞掉 */
    s_in_progress = 1;

    oplog_rec_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.magic   = OL_OPLOG_MAGIC;
    rec.seq     = s_stat.next_seq;
    rec.ts_ms   = ol_tick_ms();
    rec.level   = level;
    uint32_t cp = (len > OL_OPLOG_MSG_MAX) ? OL_OPLOG_MSG_MAX : len;
    if (cp && msg) { memcpy(rec.msg, msg, cp); }
    rec.msg_len = (uint8_t)cp;
    rec.crc32   = rec_calc_crc(&rec);

    /* 目标 slot 在新 sector 起点 → 先 erase. 否则信任之前的 erase 让该
     * slot 处于 0xFF 状态; 若不是 (e.g. 第一次环形回绕到部分写过的 sector),
     * 这一次写会因为位反向失败但 NOR flash 不会报错 — 仅 CRC 后续读时校验
     * 不过. 为稳妥起见: 跨 sector 边界总是 erase 一遍. */
    if ((s_stat.write_idx % SLOTS_PER_SECTOR) == 0) {
        int rc = erase_sector_of(s_stat.write_idx);
        if (rc != OL_OK) {
            s_in_progress = 0;
            return rc;
        }
    }

    int rc = ol_part_write(s_part, s_stat.write_idx * OL_OPLOG_SLOT_SIZE,
                           (const uint8_t *)&rec, sizeof(rec));
    if (rc != OL_OK) {
        s_in_progress = 0;
        return rc;
    }

    s_stat.write_idx = (s_stat.write_idx + 1u) % s_stat.total_slots;
    s_stat.next_seq++;
    if (s_stat.valid_count < s_stat.total_slots) { s_stat.valid_count++; }

    s_in_progress = 0;
    return OL_OK;
}

int ol_oplog_iter(ol_oplog_iter_cb_t cb, void *user, uint32_t max)
{
    if (!s_stat.ready) { return OL_E_NOT_FOUND; }
    if (!cb)            { return OL_E_INVAL; }

    /* 找最小 seq 的有效 slot, 从它开始按 (idx+1) % total 遍历到 write_idx
     * (排除 write_idx 自身, 因为它是下一条要写的位置). 物理顺序 == 时间
     * 顺序, 因为我们是顺序 append + 环形覆盖. */
    uint32_t min_seq = 0;
    uint32_t min_idx = 0;
    int      found   = 0;
    oplog_rec_t rec;
    for (uint32_t i = 0; i < s_stat.total_slots; ++i) {
        if (read_slot(i, &rec) != OL_OK) { continue; }
        if (!rec_is_valid(&rec)) { continue; }
        if (!found || rec.seq < min_seq) {
            min_seq = rec.seq;
            min_idx = i;
            found   = 1;
        }
    }
    if (!found) { return 0; }

    uint32_t emitted = 0;
    uint32_t i = min_idx;
    do {
        if (read_slot(i, &rec) == OL_OK && rec_is_valid(&rec)) {
            int stop = cb(rec.seq, rec.ts_ms, rec.level,
                          rec.msg, rec.msg_len, user);
            emitted++;
            if (stop) { break; }
            if (max && emitted >= max) { break; }
        }
        i = (i + 1u) % s_stat.total_slots;
    } while (i != s_stat.write_idx);

    return (int)emitted;
}

int ol_oplog_clear(void)
{
    if (!s_stat.ready) { return OL_E_NOT_FOUND; }
    int rc = ol_part_erase_all(s_part);
    if (rc != OL_OK) { return rc; }
    s_stat.write_idx   = 0;
    s_stat.next_seq    = 1;
    s_stat.valid_count = 0;
    return OL_OK;
}

int ol_oplog_get_stat(ol_oplog_stat_t *out)
{
    if (!out) { return OL_E_INVAL; }
    if (!s_stat.ready) { return OL_E_NOT_FOUND; }
    *out = s_stat;
    return OL_OK;
}
