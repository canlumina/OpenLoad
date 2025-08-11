#include "bsp_key.h"
#include "bsp_led.h"
#include "bsp_spi.h"
#include "delay.h"
#include "easyflash.h"
#include "sfud_cfg.h"
#include "sys.h"
#include "usart.h"
#include <fal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF_SIZE 512
static int fal_test(const char *partiton_name);
static void test_env(void);

int main(void) {
    HAL_Init();                         /* 初始化HAL库 */
    sys_stm32_clock_init(RCC_PLL_MUL9); /* 设置时钟, 72Mhz */
    delay_init(72);                     /* 延时初始化 */
    u1_init(115200);                    /* 串口初始化为115200 */
    u2_init(115200);                    /* 串口初始化为115200 */
    led_init();                         /* 初始化LED */
    key_init();                         /* 初始化按键 */
    // W25QXX_Init();                      // W25QXX初始化();
    spi2_init();
    fal_init();
    spi_flash_init();

    /* 关闭stdout缓冲，确保即时输出 */

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Boot OK on USART1 @115200, use linux ttyUSB0 output...\r\n");

    if (fal_test("app") == 0) {
        log_i("Fal partition (%s) test success!", "app");
    } else {
        log_e("Fal partition (%s) test failed!", "app");
    }

    // if (fal_test("env") == 0)
    // {
    //     log_i("Fal partition (%s) test success!", "env");
    // }
    // else
    // {
    //     log_e("Fal partition (%s) test failed!", "env");
    // }

    // if (easyflash_init() == EF_NO_ERR)
    // {
    //     /* test Env demo */
    //     test_env();
    // }

    while (1) {
        delay_ms(500);
    }
    return 0;
}

static void test_env(void) {
    uint32_t i_boot_times = 0;
    char *c_old_boot_times, c_new_boot_times[11] = {0};

    /* get the boot count number from Env */
    c_old_boot_times = ef_get_env("boot_times");
    assert_param(c_old_boot_times);
    i_boot_times = atol(c_old_boot_times);
    /* boot count +1 */
    i_boot_times++;
    printf("The system now boot %lu times\n\r", (unsigned long)i_boot_times);
    /* interger to string */
    sprintf(c_new_boot_times, "%lu", (unsigned long)i_boot_times);
    /* set and store the boot count number to Env */
    ef_set_env("boot_times", c_new_boot_times);
    ef_save_env();
}

static int fal_test(const char *partiton_name) {
    int ret;
    int i, j, len;
    uint8_t buf[BUF_SIZE];
    const struct fal_flash_dev *flash_dev = NULL;
    const struct fal_partition *partition = NULL;

    if (!partiton_name) {
        log_e("Input param partition name is null!");
        return -1;
    }

    partition = fal_partition_find(partiton_name);
    if (partition == NULL) {
        log_e("Find partition (%s) failed!", partiton_name);
        ret = -1;
        return ret;
    }

    flash_dev = fal_flash_device_find(partition->flash_name);
    if (flash_dev == NULL) {
        log_e("Find flash device (%s) failed!", partition->flash_name);
        ret = -1;
        return ret;
    }

    log_i("Flash device : %s   "
          "Flash size : %dK   "
          "Partition : %s   "
          "Partition size: %dK",
          partition->flash_name, flash_dev->len / 1024, partition->name,
          partition->len / 1024);

    /* 擦除 `partition` 分区上的全部数据 */
    ret = fal_partition_erase_all(partition);
    if (ret < 0) {
        log_e("Partition (%s) erase failed!", partition->name);
        ret = -1;
        return ret;
    }
    log_i("Erase (%s) partition finish!", partiton_name);

    /* 循环读取整个分区的数据，并对内容进行检验 */
    for (i = 0; i < partition->len;) {
        memset(buf, 0x00, BUF_SIZE);
        len = (partition->len - i) > BUF_SIZE ? BUF_SIZE : (partition->len - i);

        /* 从 Flash 读取 len 长度的数据到 buf 缓冲区 */
        ret = fal_partition_read(partition, i, buf, len);
        if (ret < 0) {
            log_e("Partition (%s) read failed!", partition->name);
            ret = -1;
            return ret;
        }
        for (j = 0; j < len; j++) {
            /* 校验数据内容是否为 0xFF */
            if (buf[j] != 0xFF) {
                log_e("The erase operation did not really succeed!");
                ret = -1;
                return ret;
            }
        }
        i += len;
    }

    /* 把 0 写入指定分区 */
    for (i = 0; i < partition->len;) {
        /* 设置写入的数据 0x00 */
        memset(buf, 0x00, BUF_SIZE);
        len = (partition->len - i) > BUF_SIZE ? BUF_SIZE : (partition->len - i);

        /* 写入数据 */
        ret = fal_partition_write(partition, i, buf, len);
        if (ret < 0) {
            log_e("Partition (%s) write failed!", partition->name);
            ret = -1;
            return ret;
        }
        i += len;
    }
    log_i("Write (%s) partition finish! Write size %d(%dK).", partiton_name, i,
          i / 1024);

    /* 从指定的分区读取数据并校验数据 */
    for (i = 0; i < partition->len;) {
        /* 清空读缓冲区，以 0xFF 填充 */
        memset(buf, 0xFF, BUF_SIZE);
        len = (partition->len - i) > BUF_SIZE ? BUF_SIZE : (partition->len - i);

        /* 读取数据到 buf 缓冲区 */
        ret = fal_partition_read(partition, i, buf, len);
        if (ret < 0) {
            log_e("Partition (%s) read failed!", partition->name);
            ret = -1;
            return ret;
        }
        for (j = 0; j < len; j++) {
            /* 校验读取的数据是否为步骤 3 中写入的数据 0x00 */
            if (buf[j] != 0x00) {
                log_e("The write operation did not really succeed!");
                ret = -1;
                return ret;
            }
        }
        i += len;
    }

    ret = 0;
    return ret;
}
