/*
 * OpenLoad bootloader 链接脚本 (STM32F407VGT6, DevEBox MCUDev)
 *
 * Flash 布局 (1 MB):
 *   0x08000000 - 0x0800FFFF (64 KB,  sector 0-3)  BOOT  ← 本工程
 *   0x08010000 - 0x0801FFFF (64 KB,  sector 4)    spare  (跳过, 简化对齐)
 *   0x08020000 - 0x0809FFFF (512 KB, sector 5-8)  APP   ← 用户的 App 工程
 *   0x080A0000 - 0x080FFFFF (384 KB, sector 9-11) spare (留 dual bank / future)
 *
 * RAM 布局 (128 KB 主 SRAM, CCM 64K @ 0x10000000 不用):
 *   0x20000000 .. _estack-4         普通段 (.data/.bss/.heap/.stack)
 *   末尾 4 字节                     .ol_magic (NOLOAD, App↔Bootloader 通信)
 */
ENTRY(Reset_Handler)

MEMORY
{
    RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K  /* BOOT 分区 = sector 0-3 */
}

/* 栈与堆 */
_estack         = ORIGIN(RAM) + LENGTH(RAM) - 4;     /* 末 4 字节给 .ol_magic */
_Min_Heap_Size  = 0;
_Min_Stack_Size = 0x1000;                            /* 4 KB 栈 (Ed25519 verify 用栈较多) */

SECTIONS
{
    .isr_vector : {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text : {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)
        KEEP (*(.init))
        KEEP (*(.fini))
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .rodata : {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    /* OpenLoad 链接段: 设备/命令/receiver 静态注册表
       KEEP 强制保留 (否则 --gc-sections 会因为没人 reference 而删掉) */
    .ol_io_devs : {
        . = ALIGN(4);
        PROVIDE(__ol_io_devs_start = .);
        KEEP(*(.ol_io_devs))
        PROVIDE(__ol_io_devs_end = .);
        . = ALIGN(4);
    } > FLASH

    .ol_flash_devs : {
        . = ALIGN(4);
        PROVIDE(__ol_flash_devs_start = .);
        KEEP(*(.ol_flash_devs))
        PROVIDE(__ol_flash_devs_end = .);
        . = ALIGN(4);
    } > FLASH

    .ol_cmds : {
        . = ALIGN(4);
        PROVIDE(__ol_cmds_start = .);
        KEEP(*(.ol_cmds))
        PROVIDE(__ol_cmds_end = .);
        . = ALIGN(4);
    } > FLASH

    .ol_receivers : {
        . = ALIGN(4);
        PROVIDE(__ol_receivers_start = .);
        KEEP(*(.ol_receivers))
        PROVIDE(__ol_receivers_end = .);
        . = ALIGN(4);
    } > FLASH

    .ARM.extab (READONLY) : {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM (READONLY) : {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    .preinit_array (READONLY) : {
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP (*(.preinit_array*))
        PROVIDE_HIDDEN (__preinit_array_end = .);
    } > FLASH

    .init_array (READONLY) : {
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array*))
        PROVIDE_HIDDEN (__init_array_end = .);
    } > FLASH

    .fini_array (READONLY) : {
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP (*(SORT(.fini_array.*)))
        KEEP (*(.fini_array*))
        PROVIDE_HIDDEN (__fini_array_end = .);
    } > FLASH

    _sidata = LOADADDR(.data);

    .data : {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        *(.RamFunc)
        *(.RamFunc*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    .bss (NOLOAD) : {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } > RAM

    ._user_heap_stack (NOLOAD) : {
        . = ALIGN(8);
        PROVIDE (end = .);
        PROVIDE (_end = .);
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } > RAM

    /* App ↔ Bootloader 通信魔数. NOLOAD 保证 startup 的 bss/data 初始化不清它. */
    .ol_magic (NOLOAD) : {
        . = ALIGN(4);
        KEEP(*(.ol_magic))
        . = ALIGN(4);
    } > RAM

    /DISCARD/ : {
        libc.a:* ( * )
        libm.a:* ( * )
        libgcc.a:* ( * )
    }
}
