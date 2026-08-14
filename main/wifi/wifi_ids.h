#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Serial DEBUG IDs in the format utils/collect_hw_ids.py parses.
// Never logs the Wi-Fi password.
void wifi_ids_log(void);
void wifi_ids_log_psk_status(esp_err_t load_err);
// Bind wrap to /sdcard/nodeid.txt if present (call after SD mount).
void wifi_ids_bind_sd_node(const char *nodeid_path);

#ifdef __cplusplus
}
#endif
