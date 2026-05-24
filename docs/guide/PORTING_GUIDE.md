# OpenLoad Porting Guide — 移植到新板子

把 OpenLoad 移植到一颗新单片机/新板子, 总共需要实现 5 个接口、注册 2 类设备、调整 1 个链接脚本。

工作量参考: STM32F103ZET6 + W25Q64 参考实现 = **~600 行代码**, 半天可完成。

---

## 第一步: 复制示例工程做骨架

```
cp -r examples/stm32f103zet6_gcc  examples/<your_board>
```

后续修改集中在新目录里, 不动 `openload/` 与已有 `ports/`。

---

## 第二步: 实现 `ol_sys_ops_t` (5 个必填 + 2 个可选)

`ports/<your_chip>/src/port_sys.c`:

| 字段 | 必/选 | 实现要点 |
|------|-------|---------|
| `tick_ms()` | 必 | 通常包装芯片厂的 `HAL_GetTick()` 或 SysTick handler 维护的全局变量 |
| `delay_ms(ms)` | 选 | 不实现时框架用 `tick_ms` 轮询替代 |
| `reboot()` | 必 | Cortex-M: `NVIC_SystemReset()` |
| `disable_irq()` | 必 | Cortex-M: `__disable_irq()` |
| `jump(addr)` | 必 | 见下方 Cortex-M 模板 |
| `magic_read(out)` | 选 | 读 RTC 备份寄存器 / RAM 末尾保留段 |
| `magic_write(v)` | 选 | 写同上位置 |

### Cortex-M3/M4 jump 模板

```c
static void sys_jump(uint32_t app_addr) {
    SCB->VTOR    = app_addr;
    uint32_t msp = *(volatile uint32_t *)(app_addr);
    uint32_t pc  = *(volatile uint32_t *)(app_addr + 4);
    __asm volatile (
        "msr msp, %0  \n"
        "bx  %1       \n"
        : : "r"(msp), "r"(pc) : "memory"
    );
}
```

调用 `ol_sys_register(&your_sys_ops)` 注册 (通常放在 `port_<chip>_init()` 内)。

---

## 第三步: 实现 `ol_flash_ops_t` (至少一个 — 内部 Flash)

`ports/<your_chip>/src/port_flash_int.c`:

```c
static int int_read(ol_flash_dev_t *d, uint32_t off, void *buf, uint32_t len)  {
    memcpy(buf, (const void *)(d->base + off), len);
    return OL_OK;
}
static int int_write(ol_flash_dev_t *d, uint32_t off, const void *buf, uint32_t len)  { /* HAL_FLASH_Program */ }
static int int_erase(ol_flash_dev_t *d, uint32_t off, uint32_t len)            { /* HAL_FLASHEx_Erase */ }

static const ol_flash_ops_t int_ops = { int_read, int_write, int_erase, NULL, NULL };

static ol_flash_dev_t s_dev = {
    .name              = "internal",
    .base              = 0x08000000u,
    .size              = 512 * 1024u,
    .sector_size       = 2048u,        /* 各 MCU 不同, 查 datasheet */
    .write_granularity = 4u,
    .xip               = true,
    .ops               = &int_ops,
};
OL_FLASH_DEV_REGISTER(internal, &s_dev);
```

如果还有外部 SPI Flash, 再添一个文件 `port_flash_w25q64.c` 同样套路, 注册名 `"w25q64"` (或随你定)。

---

## 第四步: 实现 `ol_io_ops_t` (至少一个 — console)

`ports/<your_chip>/src/port_io_uart.c`:

```c
static int uart_read(ol_io_dev_t *d, uint8_t *buf, uint32_t len)  { /* 非阻塞读 ringbuf 或 UART RX 寄存器 */ }
static int uart_write(ol_io_dev_t *d, const uint8_t *buf, uint32_t len) { /* 阻塞写 */ }

static const ol_io_ops_t uart_ops = { uart_read, uart_write, NULL, NULL };
static ol_io_dev_t s_console = { .name = "console", .ops = &uart_ops };
OL_IO_DEV_REGISTER(console, &s_console);
```

设备名 `"console"` 是约定俗成的: CLI/日志 默认就找这个名字。

---

## 第五步: 链接脚本

复制示例的 `linker.ld` 然后调整两处:

```ld
MEMORY
{
    RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = <your_ram_size>K
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = <your_boot_size>K   /* 仅 bootloader 区 */
}
```

`SECTIONS` 中的 4 个 `.ol_*` 段不要动 — 它们是框架静态注册机制依赖的。

---

## 第六步: 修改分区表

`partitions.def`:

```c
OL_PART(internal, "boot",     0x00000000u, 0x10000u, OL_PART_FLAG_READABLE)
OL_PART(internal, "app",      0x00010000u, 0x70000u, OL_PART_FLAG_READABLE | OL_PART_FLAG_WRITABLE | OL_PART_FLAG_EXECUTABLE)
OL_PART(w25q64,   "download", 0x00000000u, 0x80000u, OL_PART_FLAG_READABLE | OL_PART_FLAG_WRITABLE)
```

地址布局必须与 linker.ld 中的 boot 大小一致 (boot 结束地址 = app 起始)。

---

## 第七步: 配置项

`openload_config.h`:

```c
#define OPENLOAD_BOARD_ID         0x????          /* 防误刷 */
#define OPENLOAD_BOOT_DELAY_MS    3000
#define OPENLOAD_ENABLE_XMODEM    1
/* ...其它项按需 */
```

未定义的项继承 `openload/include/openload/config_default.h` 的默认值。

---

## 第八步: 注册 + main

`board_init.c` 把所有 ops 装上, `main.c` 启动:

```c
int main(void) {
    /* 1. 芯片厂 HAL 初始化 (时钟/GPIO/外设) */
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    /* 2. port 层 (注册 ops + 启动外设) */
    port_<your_chip>_init();
    /* 3. 框架 */
    ol_boot_init();
    ol_boot_run();         /* never return */
}
```

---

## 第九步: App 端约定

被跳转的 App 必须:
1. 编译时把 `.text` 链接到 `0x08000000 + boot_size + 64` (跳过 OpenLoad header)
2. App 的向量表前 64 字节其实是 OpenLoad header (PC 永远不会执行到), 第 65 字节起放 App 自己的向量表
3. 实际操作: App 用普通 linker.ld 链到 boot_size+64 偏移, 编译完用 `tools/image_tool.py` 在前面拼 64 字节 header, 烧入 app 分区即可

详见 `examples/<your_board>/README.md`。

---

## 常见坑

| 现象 | 原因 |
|------|------|
| 编译过, 但 `.ol_io_devs` 等链接段为空 | 链接脚本 `.ol_*` 段缺 `KEEP()` — GC 把它们删了 |
| `OL_IO_DEV_REGISTER` 写在 .c 文件但找不到设备 | 该 .c 没被链接 (CMake 没加进 source list) |
| 跳转 App 后 hardfault | App 链接地址没设对 / 没跳过 64 字节 header |
| 串口收发乱码 | 时钟源不是 HSE 而是 HSI, PLL 倍频未生效, 实际 baudrate 不对 |
| 内部 Flash 写失败 | 没擦就写 / 写入对齐不对 (F1 必须 halfword 对齐, F4/H7 是 word/doubleword) |
