#pragma once

#include "lvgl.h"
#include "wifi/wifi_sta.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    UI_JMRI_ICON_OFF = 0,
    UI_JMRI_ICON_OK,
    UI_JMRI_ICON_FAIL,
} ui_jmri_icon_t;

typedef ui_jmri_icon_t ui_mark_icon_t;

// Overlay on the given screen. Recreate after each lv_obj_clean.
void ui_wifi_icon_attach(lv_obj_t *parent, lv_coord_t x, lv_coord_t y);
void ui_wifi_icon_invalidate(void);
void ui_wifi_icon_set_state(wifi_sta_state_t st);

#define UI_WIFI_ICON_W 48
#define UI_WIFI_ICON_H 36

// "JMRI" label + slash, to the left of the signal bars.
#define UI_JMRI_ICON_W 80
#define UI_JMRI_ICON_H 36
void ui_jmri_icon_attach(lv_obj_t *parent, lv_coord_t x, lv_coord_t y);
void ui_jmri_icon_invalidate(void);
void ui_jmri_icon_set_state(ui_jmri_icon_t st);

// LCC = CAN or Wi-Fi GridConnect. CAN = wired bus heard another node.
#define UI_LCC_ICON_W 60
#define UI_CAN_ICON_W 60
void ui_lcc_icon_attach(lv_obj_t *parent, lv_coord_t x, lv_coord_t y);
void ui_can_icon_attach(lv_obj_t *parent, lv_coord_t x, lv_coord_t y);
void ui_lcc_icon_set_state(ui_mark_icon_t st);
void ui_can_icon_set_state(ui_mark_icon_t st);

#ifdef __cplusplus
}
#endif
