#include "wifi_ids.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "wifi_cred.h"

static uint64_t parse_dotted_node(const char *s)
{
    unsigned b[6] = {0};
    if (sscanf(s, "%x.%x.%x.%x.%x.%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6)
    {
        return 0;
    }
    return ((uint64_t)b[0] << 40) | ((uint64_t)b[1] << 32) | ((uint64_t)b[2] << 24) |
           ((uint64_t)b[3] << 16) | ((uint64_t)b[4] << 8) | (uint64_t)b[5];
}

void wifi_ids_bind_sd_node(const char *nodeid_path)
{
    if (nodeid_path == NULL)
    {
        return;
    }
    FILE *f = fopen(nodeid_path, "r");
    if (!f)
    {
        return;
    }
    char buf[64] = {0};
    if (fgets(buf, sizeof(buf), f) != NULL)
    {
        const uint64_t node = parse_dotted_node(buf);
        if (node != 0)
        {
            wifi_cred_set_node_id(node);
        }
    }
    fclose(f);
}

static const char *TAG = "debug_ids";

void wifi_ids_log(void)
{
    uint8_t mac[6] = {0};
    uint8_t uid[8] = {0};
    bool uid_ok = false;
    wifi_hw_ids_read(mac, uid, &uid_ok);
    const uint64_t node = wifi_node_id();

    char mac_s[24];
    char node_s[24];
    char uid_s[24];
    snprintf(mac_s, sizeof(mac_s), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    snprintf(node_s, sizeof(node_s), "%02X.%02X.%02X.%02X.%02X.%02X",
             (unsigned)((node >> 40) & 0xFF),
             (unsigned)((node >> 32) & 0xFF),
             (unsigned)((node >> 24) & 0xFF),
             (unsigned)((node >> 16) & 0xFF),
             (unsigned)((node >> 8) & 0xFF),
             (unsigned)(node & 0xFF));
    if (uid_ok)
    {
        snprintf(uid_s, sizeof(uid_s), "%02X%02X%02X%02X%02X%02X%02X%02X",
                 uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6], uid[7]);
    }
    else
    {
        snprintf(uid_s, sizeof(uid_s), "UNAVAILABLE");
    }

    ESP_LOGI(TAG, "==== DEBUG IDs (not the WiFi PSK) ====");
    ESP_LOGI(TAG, "MAC Address: %s", mac_s);
    ESP_LOGI(TAG, "OpenLCB Node ID: %s", node_s);
    ESP_LOGI(TAG, "SPI flash unique ID: %s", uid_s);
}

void wifi_ids_log_psk_status(esp_err_t load_err)
{
    if (load_err == ESP_OK)
    {
        ESP_LOGI(TAG, "WiFi password: set (not logged)");
    }
    else if (load_err == ESP_ERR_NOT_FOUND)
    {
        ESP_LOGW(TAG, "WiFi password: NOT SET");
    }
    else
    {
        ESP_LOGE(TAG, "WiFi password: unwrap failed (%s)", esp_err_to_name(load_err));
    }
}
