/*
 * STM32F1 Port - RDP (Read-out Protection) ops
 *
 * M6 收尾: 把 M6-1 的 F4 实现搬到 F1, 让 F1/F4 RDP 行为对齐.
 *
 * 与 F4 的关键差异 (写 F1 前必须知道):
 *   1. F1 只有 L0 (0xA5) / L1 (0x00) 两级, 没有 L2. switch 仅两条 case.
 *   2. F1 写 OB 必须先 HAL_FLASHEx_OBErase 再 OBProgram (RM0008 §2.4.2).
 *      F4 可以直接 OBProgram 单字段, F1 不行.
 *   3. F1 OBErase 会把 USERConfig (IWDG_SW/STOP/STDBY) / DATAData / WRPPage
 *      **全部擦成默认值**. 若用户工程依赖软件 IWDG (USERConfig 中 OB_IWDG_SW),
 *      erase 后写回 default = 硬件 IWDG 自动跑, 上电后 app 必须 30s 内喂狗
 *      否则复位. 因此实现里必须先 OBGetConfig 把所有字段备份, 改完 RDP
 *      连同其他字段整体写回去.
 *
 * 实现要点:
 *   - sys_rdp_get_level: 走 HAL_FLASHEx_OBGetConfig, 不解锁 flash
 *   - sys_rdp_lock: 强制只接 L0→L1, 其他返 OL_E_INVAL. 内部 GetConfig 备份 →
 *     unlock OB → OBErase → 重设 RDP=L1 (其他字段保持) → OBProgram → OB_Launch
 *     → relock. OB_Launch 通常立即触发复位, 防御性处理 "极少数情况能跑回来".
 *
 * 解锁 (出测试): STM32_Programmer_CLI -c port=SWD -ob RDP=0xA5
 *   注意 F1 默认 L0 值是 0xA5, 跟 F4 的 0xAA 不一样.
 *   触发 mass erase + 回 L0. 整片 flash 被擦, 之后重烧 bootloader.
 *
 * 参考: RM0008 §2.4 Flash memory protection, §2.4.2 Programming option bytes.
 */
#include "openload/ops/sys_ops.h"
#include "openload/errno.h"
#include "stm32f1xx_hal.h"
#include <stdint.h>

int sys_rdp_get_level(uint8_t *out)
{
    if (!out) {
        return OL_E_INVAL;
    }
    FLASH_OBProgramInitTypeDef ob = {0};
    HAL_FLASHEx_OBGetConfig(&ob);

    /* F1 RDPLevel 只可能是 OB_RDP_LEVEL_0 (0xA5) 或 OB_RDP_LEVEL_1 (0x00),
     * 没有 L2 case. 任何其它值视为硬件异常. */
    switch (ob.RDPLevel) {
        case OB_RDP_LEVEL_0:  *out = OL_RDP_LEVEL_NONE;      return OL_OK;
        case OB_RDP_LEVEL_1:  *out = OL_RDP_LEVEL_READ_PROT; return OL_OK;
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
    /* 硬约束: 只允许 L0 → L1. L1→L1 无意义, F1 没 L2, 都拒. */
    if (cur != OL_RDP_LEVEL_NONE) {
        return OL_E_INVAL;
    }

    /* 关键: 先备份所有 OB 字段. F1 OBErase 会擦掉 USERConfig/DATAData/WRPPage,
     * 没备份直接写 RDP 会让 IWDG_SW 等用户配置变成 default. */
    FLASH_OBProgramInitTypeDef ob = {0};
    HAL_FLASHEx_OBGetConfig(&ob);

    /* Option byte 写入需先解锁主 flash + OB. RM0008 §2.4.2. */
    if (HAL_FLASH_Unlock() != HAL_OK) {
        return OL_E_IO;
    }
    if (HAL_FLASH_OB_Unlock() != HAL_OK) {
        HAL_FLASH_Lock();
        return OL_E_IO;
    }

    /* F1 必须先 OBErase 把整个 option byte 区擦到 0xFF, 再写新值. */
    if (HAL_FLASHEx_OBErase() != HAL_OK) {
        HAL_FLASH_OB_Lock();
        HAL_FLASH_Lock();
        return OL_E_IO;
    }

    /* 修改 RDP=L1, 其余字段沿用备份值. OptionType 必须四类全置位, 否则
     * 未置位的字段不会写回 → erase 后保持 0xFF / default, IWDG 配置丢失. */
    ob.OptionType = OPTIONBYTE_RDP | OPTIONBYTE_USER | OPTIONBYTE_DATA | OPTIONBYTE_WRP;
    ob.RDPLevel   = OB_RDP_LEVEL_1;

    HAL_StatusTypeDef hrc = HAL_FLASHEx_OBProgram(&ob);
    if (hrc != HAL_OK) {
        HAL_FLASH_OB_Lock();
        HAL_FLASH_Lock();
        return OL_E_IO;
    }

    /* OB_Launch 把 option byte reload 到生效寄存器. F1 通常立即触发系统复位,
     * 不返回. 但跟 F4 一样, RM0008 没强制保证 — 防御性继续往下. */
    HAL_FLASH_OB_Launch();

    /* 兜底: 如果 Launch 没复位, 把 OB / Flash 锁回去, 让用户手动 reboot. */
    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();
    return OL_OK;
}
