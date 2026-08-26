#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Record the OpenLCB node ID (from SD / LCC). Wrap key does not use it.
void wifi_cred_set_node_id(uint64_t node_id);
uint64_t wifi_node_id(void);

// Load SSID + PSK from NVS. If NVS is empty and this build contains a
// host-encrypted wrap blob, decrypt with live chip IDs and store in NVS.
esp_err_t wifi_cred_load(char *ssid, size_t ssid_len, char *psk, size_t psk_len);

void wifi_hw_ids_read(uint8_t mac[6], uint8_t flash_uid[8], bool *uid_ok);

#ifdef __cplusplus
}
#endif
