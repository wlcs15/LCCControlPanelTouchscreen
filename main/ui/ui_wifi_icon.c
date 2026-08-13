#include "ui_wifi_icon.h"

#include "ui_common.h"

#define ICON_W 36
#define ICON_H 28

static lv_color_t s_buf[ICON_W * ICON_H];
static lv_obj_t *s_canvas;
static wifi_sta_state_t s_state = WIFI_STA_IDLE;

static void draw_rect(int x, int y, int w, int h, lv_color_t c)
{
    lv_draw_rect_dsc_t rd;
    lv_draw_rect_dsc_init(&rd);
    rd.bg_color = c;
    rd.bg_opa = LV_OPA_COVER;
    rd.border_width = 0;
    rd.radius = 0;
    lv_canvas_draw_rect(s_canvas, x, y, w, h, &rd);
}

static void redraw(void)
{
    if (!s_canvas)
    {
        return;
    }

    const lv_color_t black = lv_color_hex(0x000000);
    const lv_color_t dim = lv_color_hex(0x424242);
    const lv_color_t yellow = lv_color_hex(0xFFE000);
    const lv_color_t green = lv_color_hex(0x00C853);
    const lv_color_t red = lv_color_hex(0xF44336);
    const lv_color_t bar_fail = lv_color_hex(0x8B0000);

    lv_canvas_fill_bg(s_canvas, black, LV_OPA_COVER);

    if (s_state == WIFI_STA_IDLE || s_state == WIFI_STA_NO_PSK)
    {
        draw_rect(16, 22, 4, 4, dim);
        return;
    }

    if (s_state == WIFI_STA_FAILED)
    {
        draw_rect(4, 20, 6, 6, bar_fail);
        draw_rect(13, 12, 6, 14, bar_fail);
        draw_rect(22, 4, 6, 22, bar_fail);
        lv_draw_line_dsc_t ld;
        lv_draw_line_dsc_init(&ld);
        ld.color = red;
        ld.width = 3;
        ld.round_start = 1;
        ld.round_end = 1;
        lv_point_t pts[2] = {{2, 2}, {33, 25}};
        lv_canvas_draw_line(s_canvas, pts, 2, &ld);
        return;
    }

    lv_color_t low = (s_state == WIFI_STA_SEARCHING) ? yellow : green;
    draw_rect(4, 20, 6, 6, low);
    if (s_state == WIFI_STA_CONNECTED)
    {
        draw_rect(13, 12, 6, 14, green);
        draw_rect(22, 4, 6, 22, green);
    }
    else
    {
        draw_rect(13, 12, 6, 14, dim);
        draw_rect(22, 4, 6, 22, dim);
    }
}

void ui_wifi_icon_invalidate(void)
{
    s_canvas = NULL;
}

void ui_wifi_icon_set_state(wifi_sta_state_t st)
{
    s_state = st;
    if (!s_canvas)
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
    s_canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(s_canvas, s_buf, ICON_W, ICON_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(s_canvas, x, y);
    lv_obj_clear_flag(s_canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(s_canvas, LV_OPA_COVER, LV_PART_MAIN);
    redraw();
}
