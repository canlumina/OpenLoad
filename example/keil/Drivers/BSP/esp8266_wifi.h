#ifndef __ESP8266_WIFI_H__
#define __ESP8266_WIFI_H__

#include "at_client.h"
#include <stdint.h>
#include <stdbool.h>

/* ESP8266 WiFi状态 */
typedef enum {
    ESP8266_WIFI_DISCONNECTED = 0,
    ESP8266_WIFI_CONNECTED,
    ESP8266_WIFI_GOT_IP,
    ESP8266_WIFI_ERROR
} esp8266_wifi_status_t;

/* ESP8266设备结构 */
typedef struct {
    at_client_t at_client;
    esp8266_wifi_status_t wifi_status;
    char ip_addr[16];
    char gateway[16];
    char netmask[16];
    char ssid[64];
    bool is_initialized;
} esp8266_device_t;

/* ESP8266 WiFi API */
bool esp8266_init(esp8266_device_t *device, uint8_t uart_dev, uint8_t reset_pin);
bool esp8266_reset(esp8266_device_t *device);
bool esp8266_connect_wifi(esp8266_device_t *device, const char *ssid, const char *password);
bool esp8266_disconnect_wifi(esp8266_device_t *device);
esp8266_wifi_status_t esp8266_get_wifi_status(esp8266_device_t *device);
bool esp8266_get_ip_info(esp8266_device_t *device);

/* TCP连接API */
bool esp8266_tcp_connect(esp8266_device_t *device, const char *host, uint16_t port);
bool esp8266_tcp_disconnect(esp8266_device_t *device);
int esp8266_tcp_send(esp8266_device_t *device, const uint8_t *data, uint16_t len);
int esp8266_tcp_receive(esp8266_device_t *device, uint8_t *buffer, uint16_t buffer_size, uint32_t timeout);

/* 内部API - 用于HTTP响应头解析 */
int esp8266_tcp_receive_for_header(esp8266_device_t *device, uint8_t *buffer, uint16_t buffer_size, uint32_t timeout);

#endif /* __ESP8266_WIFI_H__ */