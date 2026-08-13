#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    WIFI_STA_IDLE = 0,
    WIFI_STA_NO_PSK,
    WIFI_STA_SEARCHING,
    WIFI_STA_CONNECTED,
    WIFI_STA_FAILED,
} wifi_sta_state_t;

typedef void (*wifi_sta_status_cb_t)(wifi_sta_state_t st, void *ctx);

// Start STA. Never logs the PSK. SSID may be logged (it is public).
esp_err_t wifi_sta_start(const char *ssid, const char *psk);
wifi_sta_state_t wifi_sta_state(void);
const char *wifi_sta_state_name(wifi_sta_state_t st);
const char *wifi_sta_ip(void);
int wifi_sta_rssi(void);
void wifi_sta_set_status_cb(wifi_sta_status_cb_t cb, void *ctx);

#ifdef __cplusplus
}
#endif
