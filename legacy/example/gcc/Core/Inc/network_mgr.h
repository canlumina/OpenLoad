#ifndef __NETWORK_MGR_H__
#define __NETWORK_MGR_H__

#include <stdint.h>
#include <stdbool.h>
#include "esp8266_wifi.h"
#include "http_ota.h"

/* 网络状态 */
typedef enum {
    NET_STATE_DISCONNECTED,
    NET_STATE_CONNECTING,
    NET_STATE_CONNECTED,
    NET_STATE_ERROR
} network_state_t;

/* 网络管理器结构 */
typedef struct {
    esp8266_device_t* wifi_device;
    network_state_t state;
    char ip_address[16];
    bool auto_reconnect;
} network_mgr_t;

/* 网络管理器初始化和销毁 */
bool network_mgr_init(network_mgr_t* mgr, esp8266_device_t* wifi_device);
void network_mgr_deinit(network_mgr_t* mgr);

/* WiFi连接管理 */
bool network_mgr_connect_wifi(network_mgr_t* mgr);
bool network_mgr_connect_wifi_custom(network_mgr_t* mgr, const char* ssid, const char* password);
bool network_mgr_disconnect_wifi(network_mgr_t* mgr);
network_state_t network_mgr_get_state(network_mgr_t* mgr);
const char* network_mgr_get_ip(network_mgr_t* mgr);

/* OTA下载功能 */
bool network_mgr_download_firmware(network_mgr_t* mgr, ota_target_t target, 
                                  uint32_t target_addr, uint32_t max_size,
                                  ota_progress_callback_t progress_callback);

/* 网络状态检查 */
bool network_mgr_is_connected(network_mgr_t* mgr);

#endif /* __NETWORK_MGR_H__ */