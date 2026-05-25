/*
 * 把 partitions.def 展开为框架可用的 g_partitions[] 数组.
 */
#include "openload/partition.h"

static const ol_partition_t g_partitions[] = {
#define OL_PART(_dev, _name, _off, _sz, _fl) \
    { .name = (_name), .device_name = #_dev, .offset = (_off), .size = (_sz), .flags = (_fl) },
#include "partitions.def"
#undef OL_PART
};

const ol_partition_t *ol_part_table(uint32_t *count_out)
{
    if (count_out) {
        *count_out = (uint32_t)(sizeof(g_partitions) / sizeof(g_partitions[0]));
    }
    return g_partitions;
}
