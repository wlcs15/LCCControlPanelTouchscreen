#include "ui_wifi_icon.h"

#include "ui_common.h"

#define ICON_W 36
#define ICON_H 28

static lv_obj_t *s_box;
static lv_obj_t *s_bar[3];
static lv_obj_t *s_slash;
static lv_point_t s_slash_pts[2];
static wifi_sta_state_t s_state = WIFI_STA_IDLE;

static lv_obj_t *make_bar(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                         lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, w, h);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(o, 2, LV_PART_MAIN);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static void set_bar(lv_obj_t *bar, lv_color_t c, lv_opa_t opa)
{
    lv_obj_set_style_bg_color(bar, c, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, opa, LV_PART_MAIN);
}

static void redraw(void)
{
    if (!s_box)
    {
        return;
    }

    const lv_color_t dim = lv_color_hex(0x9E9E9E);
    const lv_color_t yellow = lv_color_hex(0xFFE000);
    const lv_color_t green = lv_color_hex(0x00E676);
    const lv_color_t red = lv_color_hex(0xFF1744);
    const lv_color_t bar_fail = lv_color_hex(0xB71C1C);

    lv_obj_add_flag(s_slash, LV_OBJ_FLAG_HIDDEN);

    if (s_state == WIFI_STA_IDLE || s_state == WIFI_STA_NO_PSK)
    {
        set_bar(s_bar[0], dim, LV_OPA_70);
        set_bar(s_bar[1], dim, LV_OPA_40);
        set_bar(s_bar[2], dim, LV_OPA_40);
        return;
    }

    if (s_state == WIFI_STA_FAILED)
    {
        set_bar(s_bar[0], bar_fail, LV_OPA_COVER);
        set_bar(s_bar[1], bar_fail, LV_OPA_COVER);
        set_bar(s_bar[2], bar_fail, LV_OPA_COVER);
        lv_obj_set_style_line_color(s_slash, red, LV_PART_MAIN);
        lv_obj_clear_flag(s_slash, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (s_state == WIFI_STA_SEARCHING)
    {
        set_bar(s_bar[0], yellow, LV_OPA_COVER);
        set_bar(s_bar[1], dim, LV_OPA_40);
        set_bar(s_bar[2], dim, LV_OPA_40);
        return;
    }

    set_bar(s_bar[0], green, LV_OPA_COVER);
    set_bar(s_bar[1], green, LV_OPA_COVER);
    set_bar(s_bar[2], green, LV_OPA_COVER);
}

void ui_wifi_icon_invalidate(void)
{
    s_box = NULL;
    s_bar[0] = s_bar[1] = s_bar[2] = NULL;
    s_slash = NULL;
}

void ui_wifi_icon_set_state(wifi_sta_state_t st)
{
    s_state = st;
    if (!s_box)
    {
        return;
    }
    if (!ui_lock())
    {
        return;
    }
    redraw();
    ui_unlock();
}

void ui_wifi_icon_attach(lv_obj_t *parent, lv_coord_t x, lv_coord_t y)
{
    if (!parent)
    {
        return;
    }
    s_state = wifi_sta_state();

    s_box = lv_obj_create(parent);
    lv_obj_remove_style_all(s_box);
    lv_obj_set_size(s_box, ICON_W, ICON_H);
    lv_obj_set_pos(s_box, x, y);
    lv_obj_set_style_bg_opa(s_box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_box, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_box, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_box, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    s_bar[0] = make_bar(s_box, 4, 20, 6, 6);
    s_bar[1] = make_bar(s_box, 13, 12, 6, 14);
    s_bar[2] = make_bar(s_box, 22, 4, 6, 22);

    s_slash_pts[0].x = 2;
    s_slash_pts[0].y = 2;
    s_slash_pts[1].x = 33;
    s_slash_pts[1].y = 25;
    s_slash = lv_line_create(s_box);
    lv_line_set_points(s_slash, s_slash_pts, 2);
    lv_obj_set_size(s_slash, ICON_W, ICON_H);
    lv_obj_set_pos(s_slash, 0, 0);
    lv_obj_set_style_line_width(s_slash, 3, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(s_slash, true, LV_PART_MAIN);
    lv_obj_set_style_line_opa(s_slash, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(s_slash, LV_OBJ_FLAG_CLICKABLE);

    redraw();
}
