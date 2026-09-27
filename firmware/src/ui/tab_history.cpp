// 6.2 Histórico: CPU e GPU nos últimos 2 minutos

#include <stdio.h>

#include "chart_panel.h"
#include "format.h"
#include "theme.h"
#include "ui_internal.h"

static ChartPanel s_panel;

void tab_history_create(lv_obj_t* page) {
  const ChartLegend legend[2] = {{"CPU", col::cpu}, {"GPU", col::gpu}};
  chart_panel_create(s_panel, page, legend);
}

void tab_history_update(const Metrics& m) {
  int cpu = fmt_pct(m.cpu);
  int gpu = fmt_pct(m.gpu);
  chart_panel_push(s_panel, cpu, gpu);

  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", cpu);
  ui_set_text(s_panel.legendValues[0], buf);
  snprintf(buf, sizeof(buf), "%d%%", gpu);
  ui_set_text(s_panel.legendValues[1], buf);
}

void tab_history_gap() {
  chart_panel_push(s_panel, LV_CHART_POINT_NONE, LV_CHART_POINT_NONE);
}
