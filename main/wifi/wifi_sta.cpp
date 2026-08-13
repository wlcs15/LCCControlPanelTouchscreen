#include "wifi_sta.h"

#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "wifi_sta";

static const EventBits_t kGotIp = BIT0;
static const EventBits_t kFail = BIT1;

static EventGroupHandle_t s_events;
static volatile wifi_sta_state_t s_state = WIFI_STA_IDLE;
static char s_ip[16];
static volatile int s_rssi;
static int s_retries;
static const int kMaxRetries = 8;
static wifi_sta_status_cb_t s_cb;
static void *s_cb_ctx;

static void set_state(wifi_sta_state_t st)
{
    s_state = st;
    ESP_LOGI(TAG, "status %s", wifi_sta_state_name(st));
    if (s_cb)
    {
        s_cb(st, s_cb_ctx);
    }
}

void wifi_sta_set_status_cb(wifi_sta_status_cb_t cb, void *ctx)
{
    s_cb = cb;
    s_cb_ctx = ctx;
    if (s_cb)
    {
        s_cb(s_state, s_cb_ctx);
    }
}

const char *wifi_sta_state_name(wifi_sta_state_t st)
{
    switch (st)
    {
    case WIFI_STA_IDLE:
        return "idle";
    case WIFI_STA_NO_PSK:
        return "no-psk";
    case WIFI_STA_SEARCHING:
        return "searching";
    case WIFI_STA_CONNECTED:
        return "connected";
    case WIFI_STA_FAILED:
        return "failed";
    }
    return "unknown";
}

wifi_sta_state_t wifi_sta_state(void)
{
    return s_state;
}

const char *wifi_sta_ip(void)
{
    return s_ip;
}

int wifi_sta_rssi(void)
{
    return s_rssi;
}

static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "STA start, connecting (password not logged)");
        set_state(WIFI_STA_SEARCHING);
        (void)esp_wifi_connect();
    }
    else if (id == WIFI_EVENT_STA_DISCONNECTED)
    {
        const wifi_event_sta_disconnected_t *d =
            static_cast<const wifi_event_sta_disconnected_t *>(data);
        const int reason = d ? d->reason : -1;
        s_ip[0] = '\0';
        if (s_retries < kMaxRetries)
        {
            ++s_retries;
            ESP_LOGW(TAG, "disconnected reason=%d, retry %d/%d", reason, s_retries, kMaxRetries);
            set_state(WIFI_STA_SEARCHING);
            (void)esp_wifi_connect();
        }
        else
        {
            ESP_LOGE(TAG, "disconnected reason=%d, giving up", reason);
            set_state(WIFI_STA_FAILED);
            if (s_events)
            {
                xEventGroupSetBits(s_events, kFail);
            }
        }
    }
}

static void on_ip(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (id != IP_EVENT_STA_GOT_IP)
    {
        return;
    }
    const ip_event_got_ip_t *event = static_cast<const ip_event_got_ip_t *>(data);
    snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&event->ip_info.ip));
    wifi_ap_record_t ap = {};
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK)
    {
        s_rssi = ap.rssi;
    }
    s_retries = 0;
    ESP_LOGI(TAG, "connected ip=%s rssi=%d dBm (SSID public, password not logged)",
             s_ip, s_rssi);
    set_state(WIFI_STA_CONNECTED);
    if (s_events)
    {
        xEventGroupSetBits(s_events, kGotIp);
    }
}

esp_err_t wifi_sta_start(const char *ssid, const char *psk)
{
    if (ssid == nullptr || ssid[0] == '\0' || psk == nullptr || psk[0] == '\0')
    {
        set_state(WIFI_STA_NO_PSK);
        ESP_LOGW(TAG, "no credentials; not starting radio");
        return ESP_ERR_NOT_FOUND;
    }

    if (s_events == nullptr)
    {
        s_events = xEventGroupCreate();
    }
    xEventGroupClearBits(s_events, kGotIp | kFail);
    s_retries = 0;
    s_ip[0] = '\0';

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi, nullptr, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_ip, nullptr, nullptr));

    wifi_config_t wc = {};
    strncpy(reinterpret_cast<char *>(wc.sta.ssid), ssid, sizeof(wc.sta.ssid) - 1);
    strncpy(reinterpret_cast<char *>(wc.sta.password), psk, sizeof(wc.sta.password) - 1);
    wc.sta.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    wc.sta.pmf_cfg.capable = true;
    wc.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    memset(&wc, 0, sizeof(wc));

    ESP_LOGI(TAG, "searching for SSID (%u chars). Password not logged.",
             (unsigned)strlen(ssid));
    set_state(WIFI_STA_SEARCHING);
    ESP_ERROR_CHECK(esp_wifi_start());
    return ESP_OK;
}
