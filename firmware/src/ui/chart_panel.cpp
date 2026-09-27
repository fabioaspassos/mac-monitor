#include "chart_panel.h"

#include "theme.h"

constexpr int CHART_W = 270;
constexpr int CHART_H = 124;
constexpr int AXIS_X = CHART_W + 6;  // coluna do eixo Y, 24 px
constexpr int MARKER = 6;

static lv_obj_t* createLegendItem(lv_obj_t* parent, const ChartLegend& item) {
  lv_obj_t* row = ui_row(parent, 5);
  ui_dot(row, 8, item.color);
  ui_label(row, &st_f10_t2, item.name);
  return ui_label(row, &st_f12_t1, "");
}

static lv_obj_t* createYLabel(lv_obj_t* parent, const char* text, int lineY) {
  lv_obj_t* label = ui_label(parent, &st_f10_t3, text);
  lv_obj_set_pos(label, AXIS_X, lineY - F10->line_height / 2);
  return label;
}

void chart_panel_create(ChartPanel& p, lv_obj_t* page, const ChartLegend (&legend)[2]) {
  lv_obj_set_style_pad_top(page, 6, 0);
  lv_obj_set_style_pad_hor(page, 10, 0);
  lv_obj_set_style_pad_bottom(page, 8, 0);
  lv_obj_set_style_pad_row(page, 6, 0);
  lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);

  // Legenda
  lv_obj_t* legendRow = ui_row(page, 14);
  p.legendValues[0] = createLegendItem(legendRow, legend[0]);
  p.legendValues[1] = createLegendItem(legendRow, legend[1]);

  // Gráfico + eixo Y; os rótulos e marcadores podem sair um pouco da área
  lv_obj_t* area = ui_box(page);
  lv_obj_set_size(area, LV_PCT(100), CHART_H);
  lv_obj_add_flag(area, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  // linha vertical do meio (−1 min): com vdiv=3 o chart também desenharia as bordas
  lv_obj_t* midLine = ui_box(area);
  lv_obj_set_size(midLine, 1, CHART_H);
  lv_obj_set_pos(midLine, CHART_W / 2, 0);
  lv_obj_set_style_bg_color(midLine, lv_color_hex(col::grid), 0);
  lv_obj_set_style_bg_opa(midLine, LV_OPA_COVER, 0);

  lv_obj_t* chart = lv_chart_create(area);
  p.chart = chart;
  lv_obj_remove_flag(chart, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(chart, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(chart, CHART_W, CHART_H);
  lv_obj_set_pos(chart, 0, 0);
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(chart, CHART_POINTS);
  lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
  lv_chart_set_div_line_count(chart, 5, 0);  // 0, 25, 50, 75 e 100%

  lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(chart, 0, 0);
  lv_obj_set_style_radius(chart, 0, 0);
  lv_obj_set_style_border_side(chart, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(chart, 1, 0);
  lv_obj_set_style_border_color(chart, lv_color_hex(col::baseline), 0);
  lv_obj_set_style_line_width(chart, 1, 0);  // grade
  lv_obj_set_style_line_color(chart, lv_color_hex(col::grid), 0);
  lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
  lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);  // sem marcadores nos pontos

  // a série criada por último fica por cima
  p.series[1] = lv_chart_add_series(chart, lv_color_hex(legend[1].color), LV_CHART_AXIS_PRIMARY_Y);
  p.series[0] = lv_chart_add_series(chart, lv_color_hex(legend[0].color), LV_CHART_AXIS_PRIMARY_Y);
  for (int i = 0; i < 2; i++) {
    lv_chart_set_all_value(chart, p.series[i], LV_CHART_POINT_NONE);  // sem zeros falsos
  }

  // marcador do último ponto: círculo na cor da série com anel de 2 px no fundo
  for (int i = 1; i >= 0; i--) {
    lv_obj_t* marker = ui_dot(area, MARKER, legend[i].color);
    lv_obj_set_style_outline_width(marker, 2, 0);
    lv_obj_set_style_outline_color(marker, lv_color_hex(col::bg), 0);
    lv_obj_set_style_outline_opa(marker, LV_OPA_COVER, 0);
    lv_obj_add_flag(marker, LV_OBJ_FLAG_HIDDEN);
    p.markers[i] = marker;
  }

  // Eixo Y: linha de cima, do meio e de base (a borda de baixo do chart)
  p.yTop = createYLabel(area, "100", 0);
  p.yMid = createYLabel(area, "50", (CHART_H - 1) / 2);
  createYLabel(area, "0", CHART_H - 1);

  // Eixo X
  lv_obj_t* xAxis = ui_box(page);
  lv_obj_set_size(xAxis, LV_PCT(100), F10->line_height);
  lv_obj_t* label = ui_label(xAxis, &st_f10_t3, "−2 min");
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);
  label = ui_label(xAxis, &st_f10_t3, "−1 min");
  lv_obj_align(label, LV_ALIGN_TOP_MID, CHART_W / 2 - 150, 0);  // centro em x = 145 da tela
  label = ui_label(xAxis, &st_f10_t3, "agora");
  lv_obj_align(label, LV_ALIGN_TOP_RIGHT, CHART_W - 300, 0);    // borda direita do chart
}

void chart_panel_update_markers(ChartPanel& p) {
  for (int i = 0; i < 2; i++) {
    const int32_t* ys = lv_chart_get_y_array(p.chart, p.series[i]);
    uint32_t last = (lv_chart_get_x_start_point(p.chart, p.series[i]) + CHART_POINTS - 1) % CHART_POINTS;
    if (ys[last] == LV_CHART_POINT_NONE) {
      lv_obj_add_flag(p.markers[i], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_point_t pos;
    lv_chart_get_point_pos_by_id(p.chart, p.series[i], CHART_POINTS - 1, &pos);
    lv_obj_set_pos(p.markers[i], pos.x - MARKER / 2, pos.y - MARKER / 2);
    lv_obj_remove_flag(p.markers[i], LV_OBJ_FLAG_HIDDEN);
  }
}

void chart_panel_push(ChartPanel& p, int32_t primary, int32_t secondary) {
  lv_chart_set_next_value(p.chart, p.series[0], primary);
  lv_chart_set_next_value(p.chart, p.series[1], secondary);
  chart_panel_update_markers(p);
}
