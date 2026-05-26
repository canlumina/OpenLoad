/*
 * STM32F4 Port - RDP (Read-out Protection) ops
 *
 * M6-1: 软件控制 L0 → L1 锁. L2 永久不可逆, 本文件不提供 — 由 ST_LINK
 * 工具直接烧 option byte 作 production 最后一步.
 *
 * 实现要点:
 *   - sys_rdp_get_level: 走 HAL_FLASHEx_OBGetConfig, 不解锁 flash
 *   - sys_rdp_lock: 强制只接 L0→L1, 其他返 OL_E_INVAL. 内部 unlock OB +
 *     program RDP=L1 + OB_Launch + relock. OB_Launch 通常立即触发复位,
 *     代码后半段防御性处理 "极少数情况能跑回来" 场景.
 *
 * 解锁 (出测试): STM32_Programmer_CLI -c port=SWD -ob RDP=0xAA
 *   触发 mass erase + 回 L0. 整片 flash 被擦, 之后重烧 bootloader.
 *
 * 参考: RM0090 §3.6 Read-out Protection (RDP).
 */
#include "openload/ops/sys_ops.h"
#include "openload/errno.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>

int sys_rdp_get_level(uint8_t *out)
{
    if (!out) {
        return OL_E_INVAL;
    }
    FLASH_OBProgramInitTypeDef ob = {0};
    HAL_FLASHEx_OBGetConfig(&ob);

    switch (ob.RDPLevel) {
        case OB_RDP_LEVEL_0:  *out = OL_RDP_LEVEL_NONE;      return OL_OK;
        case OB_RDP_LEVEL_1:  *out = OL_RDP_LEVEL_READ_PROT; return OL_OK;
        case OB_RDP_LEVEL_2:  *out = OL_RDP_LEVEL_FULL;      return OL_OK;
        default:              return OL_E_IO;
    }
}

int sys_rdp_lock(void)
{
    uint8_t cur;
    int rc = sys_rdp_get_level(&cur);
    if (rc != OL_OK) {
        return rc;
    }
    /* 硬约束: 只允许 L0 → L1. L1→L1 无意义, L1/L2 升 L1 不存在, 都拒. */
    if (cur != OL_RDP_LEVEL_NONE) {
        return OL_E_INVAL;
    }

    /* Option byte 写入需先解锁主 flash + OB. RM0090 §3.6.2. */
    if (HAL_FLASH_Unlock() != HAL_OK) {
        return OL_E_IO;
    }
    if (HAL_FLASH_OB_Unlock() != HAL_OK) {
        HAL_FLASH_Lock();
        return OL_E_IO;
    }

    FLASH_OBProgramInitTypeDef ob = {0};
    ob.OptionType = OPTIONBYTE_RDP;
    ob.RDPLevel   = OB_RDP_LEVEL_1;
    HAL_StatusTypeDef hrc = HAL_FLASHEx_OBProgram(&ob);
    if (hrc != HAL_OK) {
        HAL_FLASH_OB_Lock();
        HAL_FLASH_Lock();
        return OL_E_IO;
    }

    /* OB_Launch 把 option byte reload 到生效寄存器. F4 通常立即触发系统复位
     * (BOR), 不返回. 但 RM0090 没强制保证 — 防御性继续往下. */
    HAL_FLASH_OB_Launch();

    /* 兜底: 如果 Launch 没复位, 把 OB / Flash 锁回去, 让用户手动 reboot. */
    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();
    return OL_OK;
}
