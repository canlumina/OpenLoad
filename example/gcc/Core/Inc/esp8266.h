#ifndef _ESP8266_H_
#define _ESP8266_H_

#include <stdint.h>
#include <stdbool.h>

/* ESP8266 使用串口2 */
#define ESP8266_UART_ID     DEV_UART2

/* ESP8266 复位引脚定义 (PE9) */
#define ESP8266_RESET_GPIO_PORT    GPIOE
#define ESP8266_RESET_GPIO_PIN     GPIO_PIN_9

/* ESP8266 AT命令超时设置 */
#define ESP8266_CMD_TIMEOUT_MS     3000
#define ESP8266_RESET_DELAY_MS     100
#define ESP8266_BOOT_DELAY_MS      2000

/* ESP8266 响应状态 */
typedef enum {
    ESP8266_OK = 0,
    ESP8266_ERROR,
    ESP8266_TIMEOUT,
    ESP8266_BUSY
} esp8266_status_t;

/* ESP8266 连接状态 */
typedef enum {
    ESP8266_DISCONNECTED = 0,
    ESP8266_CONNECTED,
    ESP8266_GOT_IP
} esp8266_conn_status_t;

/* ESP8266 工作模式 */
typedef enum {
    ESP8266_MODE_STA = 1,      /* Station模式 */
    ESP8266_MODE_AP = 2,       /* Access Point模式 */
    ESP8266_MODE_STA_AP = 3    /* Station + AP模式 */
} esp8266_mode_t;

/* 硬件控制函数 */
extern void esp8266_hw_reset(void);
extern void esp8266_hw_init(void);

/* 基础通信函数 */
extern esp8266_status_t esp8266_send_cmd(const char *cmd, const char *expected_resp, uint32_t timeout_ms);
extern esp8266_status_t esp8266_send_data(const uint8_t *data, uint16_t len);
extern uint16_t esp8266_read_response(char *buffer, uint16_t max_len, uint32_t timeout_ms);

/* AT命令封装函数 */
extern esp8266_status_t esp8266_test(void);
extern esp8266_status_t esp8266_reset(void);
extern esp8266_status_t esp8266_set_mode(esp8266_mode_t mode);
extern esp8266_status_t esp8266_get_version(char *version, uint16_t max_len);
extern esp8266_status_t esp8266_connect_wifi(const char *ssid, const char *password);
extern esp8266_status_t esp8266_disconnect_wifi(void);
extern esp8266_status_t esp8266_get_ip(char *ip, uint16_t max_len);

/* 高级功能函数 */
extern esp8266_status_t esp8266_init(void);
extern bool esp8266_is_connected(void);
extern esp8266_conn_status_t esp8266_get_connection_status(void);

/* TCP/HTTP客户端功能 */
extern esp8266_status_t esp8266_tcp_connect(const char *host, uint16_t port);
extern esp8266_status_t esp8266_tcp_send(const uint8_t *data, uint16_t len);
extern uint16_t esp8266_tcp_receive(uint8_t *buffer, uint16_t max_len, uint32_t timeout_ms);
extern esp8266_status_t esp8266_tcp_close(void);

/* HTTP下载功能 */
typedef struct {
    uint32_t content_length;    /* HTTP内容长度 */
    uint32_t downloaded;        /* 已下载字节数 */
    bool chunked;              /* 是否为分块传输 */
} esp8266_http_info_t;

extern esp8266_status_t esp8266_http_get_start(const char *url, esp8266_http_info_t *info);
extern uint16_t esp8266_http_get_data(uint8_t *buffer, uint16_t max_len, uint32_t timeout_ms);
extern esp8266_status_t esp8266_http_get_finish(void);

/* 调试和测试函数 */
extern esp8266_status_t esp8266_test_connection(const char *host, uint16_t port);

#endif