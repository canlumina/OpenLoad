#include "network_mgr.h"
#include "config.h"
#include "bootloader_cmd.h"
#include <string.h>
#include <stdio.h>

bool network_mgr_init(network_mgr_t* mgr, esp8266_device_t* wifi_device)
{
    if (!mgr || !wifi_device) return false;
    
    mgr->wifi_device = wifi_device;
    mgr->state = NET_STATE_DISCONNECTED;
    mgr->auto_reconnect = true;
    memset(mgr->ip_address, 0, sizeof(mgr->ip_address));
    
    return true;
}

void network_mgr_deinit(network_mgr_t* mgr)
{
    if (!mgr) return;
    
    if (mgr->state == NET_STATE_CONNECTED) {
        network_mgr_disconnect_wifi(mgr);
    }
}

bool network_mgr_connect_wifi(network_mgr_t* mgr)
{
    if (!mgr) return false;
    
    const bootloader_config_t* cfg = config_get();
    if (!cfg) return false;
    
    return network_mgr_connect_wifi_custom(mgr, cfg->wifi.ssid, cfg->wifi.password);
}

bool network_mgr_connect_wifi_custom(network_mgr_t* mgr, const char* ssid, const char* password)
{
    if (!mgr || !ssid || !password) return false;
    
    mgr->state = NET_STATE_CONNECTING;
    
    if (esp8266_connect_wifi(mgr->wifi_device, ssid, password)) {
        mgr->state = NET_STATE_CONNECTED;
        strncpy(mgr->ip_address, mgr->wifi_device->ip_addr, sizeof(mgr->ip_address) - 1);
        return true;
    } else {
        mgr->state = NET_STATE_ERROR;
        return false;
    }
}

bool network_mgr_disconnect_wifi(network_mgr_t* mgr)
{
    if (!mgr) return false;
    
    esp8266_disconnect_wifi(mgr->wifi_device);
    mgr->state = NET_STATE_DISCONNECTED;
    memset(mgr->ip_address, 0, sizeof(mgr->ip_address));
    
    return true;
}

network_state_t network_mgr_get_state(network_mgr_t* mgr)
{
    return mgr ? mgr->state : NET_STATE_ERROR;
}

const char* network_mgr_get_ip(network_mgr_t* mgr)
{
    return mgr ? mgr->ip_address : NULL;
}

bool network_mgr_download_firmware(network_mgr_t* mgr, ota_target_t target, 
                                  uint32_t target_addr, uint32_t max_size,
                                  ota_progress_callback_t progress_callback)
{
    if (!mgr || mgr->state != NET_STATE_CONNECTED) {
        return false;
    }
    
    const bootloader_config_t* cfg = config_get();
    if (!cfg) return false;
    
    /* 构建完整的URL */
    char firmware_url[256];
    snprintf(firmware_url, sizeof(firmware_url), "http://%s:%d%s", 
             cfg->ota.host, cfg->ota.port, cfg->ota.path);
    
    /* 初始化OTA */
    ota_context_t ota_ctx;
    if (!ota_init(&ota_ctx, mgr->wifi_device)) {
        return false;
    }
    
    /* 设置进度回调 */
    if (progress_callback) {
        ota_set_progress_callback(&ota_ctx, progress_callback);
    }
    
    /* 开始下载 */
    bootloader_print("Starting OTA download...\r\n");
    bootloader_print("URL: ");
    bootloader_print(firmware_url);
    bootloader_print("\r\n");
    
    ota_status_t result = ota_download_firmware(&ota_ctx, firmware_url, target, target_addr, max_size);
    
    ota_deinit(&ota_ctx);
    
    return (result == OTA_STATUS_OK);
}

bool network_mgr_is_connected(network_mgr_t* mgr)
{
    return mgr && (mgr->state == NET_STATE_CONNECTED);
}