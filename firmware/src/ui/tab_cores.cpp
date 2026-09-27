// 6.3 Núcleos: uma barra vertical por núcleo da CPU

#include <stdio.h>

#include "format.h"
#include "theme.h"
#include "ui_internal.h"

namespace {

constexpr int BAR_H = 104;

struct Core {
  lv_obj_t* value;
  lv_obj_t* bar;
  int pct;
  uint32_t color;
};

lv_obj_t* totalLabel;
lv_obj_t* countLabel;
lv_obj_t* barsRow;
Core cores[MAX_CORES];
int coreCount = -1;  // força a criação no primeiro pacote

void createCore(Core& c, int index, int barW, const lv_font_t* valueFont) {
  lv_obj_t* column = ui_box(barsRow);
  lv_obj_set_flex_grow(column, 1);
  lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(column, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(column, 4, 0);

  c.value = ui_label(column, &st_f10_t2, "");
  lv_obj_set_style_text_font(c.value, valueFont, 0);
  lv_obj_set_style_text_color(c.value, lv_color_hex(col::text_core), 0);
  lv_obj_set_height(c.value, 12);

  c.bar = lv_bar_create(column);
  lv_obj_remove_flag(c.bar, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(c.bar, barW, BAR_H);  // mais alta que larga: o LVGL preenche de baixo para cima
  lv_bar_set_range(c.bar, 0, 100);
  lv_obj_set_style_radius(c.bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_radius(c.bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(c.bar, lv_color_hex(col::track), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(c.bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(c.bar, lv_color_hex(col::cpu), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(c.bar, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_anim_duration(c.bar, 300, LV_PART_MAIN);

  char buf[4];
  snprintf(buf, sizeof(buf), "%d", index + 1);
  ui_label(column, &st_f10_t3, buf);

  c.pct = -1;
  c.color = col::cpu;
}

// Recria as colunas quando o número de núcleos muda
void rebuild(int count) {
  lv_obj_clean(barsRow);
  coreCount = count;

  int barW = count <= 10 ? 14 : count <= 14 ? 12 : 10;
  const lv_font_t* valueFont = count <= 14 ? F10 : F9;
  for (int i = 0; i < count; i++) createCore(cores[i], i, barW, valueFont);

  char buf[20];
  snprintf(buf, sizeof(buf), "%d núcleos", count);
  ui_set_text(countLabel, buf);
}

void updateCore(Core& c, float value) {
  int pct = fmt_pct(value);
  if (pct == c.pct) return;
  c.pct = pct;

  char buf[4];
  snprintf(buf, sizeof(buf), "%d", pct);
  ui_set_text(c.value, buf);

  uint32_t color = value_color(pct, col::cpu);
  if (color != c.color) {
    c.color = color;
    lv_obj_set_style_bg_color(c.bar, lv_color_hex(color), LV_PART_INDICATOR);
  }
  lv_bar_set_value(c.bar, pct, LV_ANIM_ON);
}

}  // namespace

void tab_cores_create(lv_obj_t* page) {
  lv_obj_set_style_pad_top(page, 6, 0);
  lv_obj_set_style_pad_hor(page, 10, 0);
  lv_obj_set_style_pad_bottom(page, 8, 0);
  lv_obj_set_style_pad_row(page, 10, 0);
  lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);

  lv_obj_t* header = ui_box(page);
  lv_obj_set_width(header, LV_PCT(100));
  lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t* total = ui_row(header, 6);
  ui_label(total, &st_f10_t2, "CPU total");
  totalLabel = ui_label(total, &st_f12_t1, "0%");
  countLabel = ui_label(header, &st_f10_t2, "");

  barsRow = ui_box(page);
  lv_obj_set_width(barsRow, LV_PCT(100));
  lv_obj_set_flex_flow(barsRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(barsRow, 2, 0);
}

void tab_cores_update(const Metrics& m) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", fmt_pct(m.cpu));
  ui_set_text(totalLabel, buf);

  if (m.coreCount != coreCount) rebuild(m.coreCount);
  for (int i = 0; i < coreCount; i++) updateCore(cores[i], m.cores[i]);
}
