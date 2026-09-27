// 6.1 Geral: quatro medidores em arco e os cards de rede e disco

#include <stdio.h>

#include "format.h"
#include "theme.h"
#include "ui_internal.h"

namespace {

constexpr int COL_W = 72;
constexpr int GAUGE_H = 68;
constexpr int CELL = 64;   // célula do arco, no topo da coluna
constexpr int ARC = 62;

struct Gauge {
  lv_obj_t* arc;
  lv_obj_t* number;
  uint32_t metricColor;
  int pct;
  uint32_t color;
};

Gauge gauges[4];
lv_obj_t* rxLabel;
lv_obj_t* txLabel;
lv_obj_t* rdLabel;
lv_obj_t* wrLabel;

void arcAnim(void* arc, int32_t value) {
  lv_arc_set_value((lv_obj_t*)arc, value);
}

void createGauge(Gauge& g, lv_obj_t* parent, const char* name, uint32_t metricColor) {
  lv_obj_t* column = ui_box(parent);
  lv_obj_set_size(column, COL_W, GAUGE_H);
  lv_obj_add_flag(column, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  g.metricColor = metricColor;
  g.color = metricColor;
  g.pct = -1;

  g.arc = lv_arc_create(column);
  lv_obj_remove_flag(g.arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(g.arc, ARC, ARC);
  lv_obj_set_pos(g.arc, (COL_W - ARC) / 2, (CELL - ARC) / 2);
  lv_arc_set_bg_angles(g.arc, 135, 45);  // 270° com a abertura para baixo
  lv_arc_set_range(g.arc, 0, 100);
  lv_arc_set_value(g.arc, 0);
  lv_obj_set_style_arc_width(g.arc, 6, LV_PART_MAIN);
  lv_obj_set_style_arc_width(g.arc, 6, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(g.arc, true, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(g.arc, true, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(g.arc, lv_color_hex(col::track), LV_PART_MAIN);
  lv_obj_set_style_arc_color(g.arc, lv_color_hex(metricColor), LV_PART_INDICATOR);
  lv_obj_remove_style(g.arc, nullptr, LV_PART_KNOB);

  // número em F20 e "%" em F10 na mesma linha de base: os dois labels ficam
  // alinhados por baixo e o "%" sobe a diferença entre as linhas de base
  lv_obj_t* value = ui_row(column, 1);
  lv_obj_set_flex_align(value, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  g.number = ui_label(value, &st_f12_t1, "0");
  lv_obj_set_style_text_font(g.number, F20, 0);
  lv_obj_t* pct = ui_label(value, &st_f10_t2, "%");
  lv_obj_set_style_pad_bottom(pct, F20->base_line - F10->base_line, 0);
  lv_obj_align(value, LV_ALIGN_TOP_MID, 0, CELL / 2 - F20->line_height / 2);

  // o topo do nome fica 8 px acima da borda inferior da célula, na abertura do arco
  lv_obj_t* label = ui_label(column, &st_caption, name);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, CELL - 8);
}

void updateGauge(Gauge& g, float value) {
  int pct = fmt_pct(value);
  if (pct == g.pct) return;
  g.pct = pct;

  char buf[8];
  snprintf(buf, sizeof(buf), "%d", pct);
  ui_set_text(g.number, buf);  // o número troca na hora; o arco anima

  uint32_t color = value_color(pct, g.metricColor);
  if (color != g.color) {
    g.color = color;
    lv_obj_set_style_arc_color(g.arc, lv_color_hex(color), LV_PART_INDICATOR);
  }

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, g.arc);
  lv_anim_set_exec_cb(&a, arcAnim);
  lv_anim_set_values(&a, lv_arc_get_value(g.arc), pct);
  lv_anim_set_duration(&a, 300);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);  // substitui uma animação anterior do mesmo arco
}

lv_obj_t* createCard(lv_obj_t* parent, const char* title) {
  lv_obj_t* card = ui_box(parent);
  lv_obj_add_style(card, &st_card, 0);
  lv_obj_set_width(card, 146);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_t* label = ui_label(card, &st_caption, title);
  lv_obj_set_height(label, 12);  // só maiúsculas, sem descendentes
  return card;
}

// Linha "rótulo ........ valor"; devolve o label do valor
lv_obj_t* createCardRow(lv_obj_t* card, const char* name, uint32_t dotColor) {
  lv_obj_t* row = ui_box(card);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t* left = ui_row(row, 5);
  if (dotColor) ui_dot(left, 6, dotColor);
  ui_label(left, &st_f10_t2, name);
  return ui_label(row, &st_f12_t1, "0 B/s");
}

void setRate(lv_obj_t* label, long bps) {
  char buf[24];
  fmt_rate(buf, sizeof(buf), bps);
  ui_set_text(label, buf);
}

}  // namespace

void tab_overview_create(lv_obj_t* page) {
  lv_obj_set_style_pad_top(page, 6, 0);
  lv_obj_set_style_pad_hor(page, 10, 0);
  lv_obj_set_style_pad_bottom(page, 10, 0);
  lv_obj_set_style_pad_row(page, 12, 0);
  lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(page, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  // Bloco A: 4 colunas de 72 px com 4 px de espaço
  lv_obj_t* gaugesRow = ui_row(page, 4);
  createGauge(gauges[0], gaugesRow, "CPU", col::cpu);
  createGauge(gauges[1], gaugesRow, "MEMÓRIA", col::mem);
  createGauge(gauges[2], gaugesRow, "GPU", col::gpu);
  createGauge(gauges[3], gaugesRow, "DISCO", col::disk);

  // Bloco B: 2 cards de 146 px com 8 px de espaço
  lv_obj_t* cardsRow = ui_box(page);
  lv_obj_set_flex_flow(cardsRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(cardsRow, 8, 0);

  lv_obj_t* net = createCard(cardsRow, "REDE");
  rxLabel = createCardRow(net, "Download", col::net_down);
  txLabel = createCardRow(net, "Upload", col::net_up);

  lv_obj_t* disk = createCard(cardsRow, "DISCO E/S");
  rdLabel = createCardRow(disk, "Leitura", 0);
  wrLabel = createCardRow(disk, "Escrita", 0);
}

void tab_overview_update(const Metrics& m) {
  updateGauge(gauges[0], m.cpu);
  updateGauge(gauges[1], m.mem);
  updateGauge(gauges[2], m.gpu);
  updateGauge(gauges[3], m.disk);

  setRate(rxLabel, m.rx);
  setRate(txLabel, m.tx);
  setRate(rdLabel, m.rd);
  setRate(wrLabel, m.wr);
}
