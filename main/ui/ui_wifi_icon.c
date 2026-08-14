#include "ui_wifi_icon.h"

#include "ui_common.h"

#define ICON_W UI_WIFI_ICON_W
#define ICON_H UI_WIFI_ICON_H

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

    /* On lilac: black idle, green OK, red fail. Grey-blue washed out. */
    const lv_color_t black = lv_color_hex(0x000000);
    const lv_color_t yellow = lv_color_hex(0xF9A825);
    const lv_color_t green = lv_color_hex(0x1B5E20);
    const lv_color_t red = lv_color_hex(0xB71C1C);

    lv_obj_add_flag(s_slash, LV_OBJ_FLAG_HIDDEN);

    if (s_state == WIFI_STA_IDLE || s_state == WIFI_STA_NO_PSK)
    {
        set_bar(s_bar[0], black, LV_OPA_COVER);
        set_bar(s_bar[1], black, LV_OPA_50);
        set_bar(s_bar[2], black, LV_OPA_50);
        return;
    }

    if (s_state == WIFI_STA_FAILED)
    {
        set_bar(s_bar[0], red, LV_OPA_COVER);
        set_bar(s_bar[1], red, LV_OPA_COVER);
        set_bar(s_bar[2], red, LV_OPA_COVER);
        lv_obj_set_style_line_color(s_slash, red, LV_PART_MAIN);
        lv_obj_clear_flag(s_slash, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (s_state == WIFI_STA_SEARCHING)
    {
        set_bar(s_bar[0], yellow, LV_OPA_COVER);
        set_bar(s_bar[1], black, LV_OPA_40);
        set_bar(s_bar[2], black, LV_OPA_40);
        return;
    }

    set_bar(s_bar[0], green, LV_OPA_COVER);
    set_bar(s_bar[1], green, LV_OPA_COVER);
    set_bar(s_bar[2], green, LV_OPA_COVER);
}

typedef struct
{
    lv_obj_t *box;
    lv_obj_t *lbl;
    lv_obj_t *lbl_w;
    lv_obj_t *slash;
    lv_point_t *pts;
    ui_mark_icon_t st;
    lv_coord_t w;
} text_mark_t;

static lv_point_t s_jmri_pts[2];
static lv_point_t s_lcc_pts[2];
static lv_point_t s_can_pts[2];
static text_mark_t s_jmri = { .pts = s_jmri_pts };
static text_mark_t s_lcc = { .pts = s_lcc_pts };
static text_mark_t s_can = { .pts = s_can_pts };

static void mark_redraw(text_mark_t *m)
{
    if (!m || !m->box)
    {
        return;
    }
    const lv_color_t black = lv_color_hex(0x000000);
    const lv_color_t green = lv_color_hex(0x1B5E20);
    const lv_color_t red = lv_color_hex(0xB71C1C);
    lv_color_t fg = black;
    if (m->st == UI_JMRI_ICON_OK)
    {
        fg = green;
    }
    else if (m->st == UI_JMRI_ICON_FAIL)
    {
        fg = red;
    }
    lv_obj_set_style_text_color(m->lbl, fg, LV_PART_MAIN);
    if (m->lbl_w)
    {
        lv_obj_set_style_text_color(m->lbl_w, fg, LV_PART_MAIN);
    }
    if (m->st == UI_JMRI_ICON_FAIL)
    {
        lv_obj_set_style_line_color(m->slash, red, LV_PART_MAIN);
        lv_obj_clear_flag(m->slash, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(m->slash, LV_OBJ_FLAG_HIDDEN);
    }
}

static void mark_attach(text_mark_t *m, lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                        const char *label, lv_coord_t w)
{
    m->w = w;
    m->box = lv_obj_create(parent);
    lv_obj_remove_style_all(m->box);
    lv_obj_set_size(m->box, w, UI_JMRI_ICON_H);
    lv_obj_set_pos(m->box, x, y);
    lv_obj_set_style_bg_color(m->box, lv_color_hex(0xFFF8E1), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m->box, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_radius(m->box, 6, LV_PART_MAIN);
    lv_obj_set_style_border_width(m->box, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(m->box, 0, LV_PART_MAIN);
    lv_obj_clear_flag(m->box, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    m->lbl_w = lv_label_create(m->box);
    lv_label_set_text(m->lbl_w, label);
    lv_obj_set_style_text_font(m->lbl_w, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_align(m->lbl_w, LV_ALIGN_CENTER, 1, 0);

    m->lbl = lv_label_create(m->box);
    lv_label_set_text(m->lbl, label);
    lv_obj_set_style_text_font(m->lbl, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_align(m->lbl, LV_ALIGN_CENTER, 0, 0);

    m->pts[0].x = 3;
    m->pts[0].y = 3;
    m->pts[1].x = w - 4;
    m->pts[1].y = UI_JMRI_ICON_H - 4;
    m->slash = lv_line_create(m->box);
    lv_line_set_points(m->slash, m->pts, 2);
    lv_obj_set_size(m->slash, w, UI_JMRI_ICON_H);
    lv_obj_set_pos(m->slash, 0, 0);
    lv_obj_set_style_line_width(m->slash, 4, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(m->slash, true, LV_PART_MAIN);
    lv_obj_clear_flag(m->slash, LV_OBJ_FLAG_CLICKABLE);
    mark_redraw(m);
}

static void mark_set(text_mark_t *m, ui_mark_icon_t st)
{
    m->st = st;
    if (!m->box)
    {
        return;
    }
    if (!ui_lock())
    {
        return;
    }
    mark_redraw(m);
    ui_unlock();
}

static void mark_clear(text_mark_t *m)
{
    m->box = NULL;
    m->lbl = NULL;
    m->lbl_w = NULL;
    m->slash = NULL;
}

void ui_wifi_icon_invalidate(void)
{
    s_box = NULL;
    s_bar[0] = s_bar[1] = s_bar[2] = NULL;
    s_slash = NULL;
    ui_jmri_icon_invalidate();
}

void ui_jmri_icon_invalidate(void)
{
    mark_clear(&s_jmri);
    mark_clear(&s_lcc);
    mark_clear(&s_can);
}

void ui_jmri_icon_set_state(ui_jmri_icon_t st)
{
    mark_set(&s_jmri, st);
}

void ui_lcc_icon_set_state(ui_mark_icon_t st)
{
    mark_set(&s_lcc, st);
}

void ui_can_icon_set_state(ui_mark_icon_t st)
{
    mark_set(&s_can, st);
}

void ui_jmri_icon_attach(lv_obj_t *parent, lv_coord_t x, lv_coord_t y)
{
    if (parent)
    {
        mark_attach(&s_jmri, parent, x, y, "JMRI", UI_JMRI_ICON_W);
    }
}

void ui_lcc_icon_attach(lv_obj_t *parent, lv_coord_t x, lv_coord_t y)
{
    if (parent)
    {
        mark_attach(&s_lcc, parent, x, y, "LCC", UI_LCC_ICON_W);
    }
}

void ui_can_icon_attach(lv_obj_t *parent, lv_coord_t x, lv_coord_t y)
{
    if (parent)
    {
        mark_attach(&s_can, parent, x, y, "CAN", UI_CAN_ICON_W);
    }
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

    s_bar[0] = make_bar(s_box, 6, 26, 8, 8);
    s_bar[1] = make_bar(s_box, 18, 16, 8, 18);
    s_bar[2] = make_bar(s_box, 30, 6, 8, 28);

    s_slash_pts[0].x = 3;
    s_slash_pts[0].y = 3;
    s_slash_pts[1].x = ICON_W - 4;
    s_slash_pts[1].y = ICON_H - 4;
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
