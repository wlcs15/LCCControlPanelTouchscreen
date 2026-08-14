#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Mirror ESP_LOG to /sdcard/debug.log when the card is mounted. */
void sd_log_init(void);

/** Flush the SD log (call from the heartbeat). */
void sd_log_flush(void);

#ifdef __cplusplus
}
#endif
