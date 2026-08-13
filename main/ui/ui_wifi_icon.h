#pragma once

#include "lvgl.h"
#include "wifi/wifi_sta.h"

#ifdef __cplusplus
extern "C" {
#endif

// Overlay (36x28) on the given screen. Recreate after each lv_obj_clean.
void ui_wifi_icon_attach(lv_obj_t *parent, lv_coord_t x, lv_coord_t y);
void ui_wifi_icon_invalidate(void);
void ui_wifi_icon_set_state(wifi_sta_state_t st);

#ifdef __cplusplus
}
#endif
