#include "theme.h"

#include <string.h>

lv_style_t st_f10_t2;
lv_style_t st_f10_t3;
lv_style_t st_f12_t1;
lv_style_t st_caption;
lv_style_t st_card;
lv_style_t st_tab;
lv_style_t st_tab_checked;
lv_style_t st_tab_pressed;

static void initText(lv_style_t* st, const lv_font_t* font, uint32_t color) {
  lv_style_init(st);
  lv_style_set_text_font(st, font);
  lv_style_set_text_color(st, lv_color_hex(color));
}

void theme_init() {
  initText(&st_f10_t2, F10, col::text_2);
  initText(&st_f10_t3, F10, col::text_3);
  initText(&st_f12_t1, F12, col::text_1);
  initText(&st_caption, F10, col::text_2);
  lv_style_set_text_letter_space(&st_caption, 1);

  lv_style_init(&st_card);
  lv_style_set_bg_color(&st_card, lv_color_hex(col::card));
  lv_style_set_bg_opa(&st_card, LV_OPA_COVER);
  lv_style_set_radius(&st_card, 10);
  lv_style_set_border_width(&st_card, 0);
  lv_style_set_pad_ver(&st_card, 9);
  lv_style_set_pad_hor(&st_card, 10);
  lv_style_set_pad_row(&st_card, 6);

  lv_style_init(&st_tab);
  lv_style_set_radius(&st_tab, 8);
  lv_style_set_bg_opa(&st_tab, LV_OPA_TRANSP);
  lv_style_set_text_font(&st_tab, F12);
  lv_style_set_text_color(&st_tab, lv_color_hex(col::text_2));

  lv_style_init(&st_tab_checked);
  lv_style_set_bg_color(&st_tab_checked, lv_color_hex(col::tab_active_bg));
  lv_style_set_bg_opa(&st_tab_checked, LV_OPA_COVER);
  lv_style_set_text_color(&st_tab_checked, lv_color_hex(col::text_active));

  lv_style_init(&st_tab_pressed);
  lv_style_set_bg_color(&st_tab_pressed, lv_color_hex(col::tab_pressed_bg));
  lv_style_set_bg_opa(&st_tab_pressed, LV_OPA_COVER);
}

uint32_t value_color(int pct, uint32_t metric_color) {
  if (pct > 85) return col::crit;
#if UI_COLOR_MODE == UI_COLOR_BY_RANGE
  (void)metric_color;
  return pct > 60 ? col::warn : col::ok;
#else
  return metric_color;
#endif
}

lv_obj_t* ui_box(lv_obj_t* parent) {
  lv_obj_t* obj = lv_obj_create(parent);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  return obj;
}

lv_obj_t* ui_row(lv_obj_t* parent, int gap) {
  lv_obj_t* obj = ui_box(parent);
  lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(obj, gap, 0);
  return obj;
}

lv_obj_t* ui_dot(lv_obj_t* parent, int size, uint32_t color) {
  lv_obj_t* dot = ui_box(parent);
  lv_obj_set_size(dot, size, size);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dot, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  return dot;
}

lv_obj_t* ui_label(lv_obj_t* parent, lv_style_t* style, const char* text) {
  lv_obj_t* label = lv_label_create(parent);
  lv_obj_add_style(label, style, 0);
  lv_label_set_text(label, text);
  return label;
}

void ui_set_text(lv_obj_t* label, const char* text) {
  if (strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}
