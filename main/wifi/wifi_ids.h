#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Serial DEBUG IDs in the format utils/collect_hw_ids.py parses.
// Never logs the Wi-Fi password.
void wifi_ids_log(void);
void wifi_ids_log_psk_status(esp_err_t load_err);

#ifdef __cplusplus
}
#endif
