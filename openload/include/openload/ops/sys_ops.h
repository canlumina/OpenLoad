/*
 * OpenLoad - System ops 接口
 *
 * 用户必须在 ports 层提供一份 ol_sys_ops_t, 通过 ol_sys_register() 注册。
 * 框架内部仅通过这里暴露的函数访问系统资源, 不直接调 HAL。
 */
#pragma once

#include <stdint.h>

typedef struct {
    /** 必填: 返回毫秒级 tick, 至少 32-bit, 允许回绕. */
    uint32_t (*tick_ms)(void);

    /** 可选: 阻塞延时; 若为 NULL, 框架内部用 tick_ms 轮询实现. */
    void (*delay_ms)(uint32_t ms);

    /** 必填: 复位 SoC, 永不返回. */
    void (*reboot)(void);

    /** 必填: 关全局中断 (跳转 App 前). */
    void (*disable_irq)(void);

    /**
     * 必填: 跳转到指定地址执行 App, 永不返回。
     * Cortex-M 默认实现: 取 *(addr) 为 MSP, *(addr+4) 为 Reset_Handler, 设 VTOR=addr。
     */
    void (*jump)(uint32_t app_addr);

    /**
     * 可选: 读持久化魔数 (用于 App ↔ Bootloader 通信).
     * 推荐用 RTC 备份寄存器或固定 RAM 段。
     * 返回 OL_OK / OL_E_NOT_SUPPORTED。
     */
    int (*magic_read)(uint32_t *out);

    /** 可选: 写持久化魔数. */
    int (*magic_write)(uint32_t value);

    /** 可选 (M6-1): 读 STM32 RDP level (0/1/2). port 未实现返 OL_E_NOT_SUPPORTED. */
    int (*rdp_get_level)(uint8_t *level_out);

    /** 可选 (M6-1): 触发 RDP L0→L1. 实现内部硬约束 — 当前必须是 L0, 其他返
     *  OL_E_INVAL. 完成后调用方应立即 reboot (OB_Launch 后设备一般自动复位). */
    int (*rdp_lock)(void);
} ol_sys_ops_t;

/**
 * @brief 注册 sys ops. 必须在 ol_boot_init() 之前调用.
 * @param  ops   指向用户静态实例, 框架仅保存指针.
 */
void ol_sys_register(const ol_sys_ops_t *ops);

/* 框架内部使用的封装. 用户代码通常不直接调. */
uint32_t ol_tick_ms(void);
void     ol_delay_ms(uint32_t ms);
void     ol_reboot(void);
void     ol_disable_irq(void);
void     ol_jump(uint32_t app_addr);
int      ol_magic_read(uint32_t *out);
int      ol_magic_write(uint32_t value);
int      ol_rdp_get(uint8_t *level_out);
int      ol_rdp_lock(void);

/* RDP level 含义 (M6-1, F4 port 实现). */
#define OL_RDP_LEVEL_NONE           0    /* 不锁, 调试器全访问 (出厂默认) */
#define OL_RDP_LEVEL_READ_PROT      1    /* SWD 连得上但读 flash 锁定 */
#define OL_RDP_LEVEL_FULL           2    /* 永久不可逆, OpenLoad 不主动设 */

/* 在 App ↔ Bootloader 通信中使用的标准魔数. */
#define OL_MAGIC_NONE           0x00000000u
#define OL_MAGIC_ENTER_BOOT     0x424F4F54u  /* "BOOT" — App 主动请求进入 bootloader */
#define OL_MAGIC_APP_OK         0x4150504Bu  /* "APPK" — 上次升级安装并跳转成功 */
#define OL_MAGIC_APP_BAD        0x42414421u  /* "BAD!" — 上次启动 App 校验失败 */
#define OL_MAGIC_INSTALLING     0x494E5354u  /* "INST" — install 进行中, 中断重启需 rollback */
