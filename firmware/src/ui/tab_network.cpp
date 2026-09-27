// 6.4 Rede: download e upload nos últimos 2 minutos, com escala automática

#include "chart_panel.h"
#include "format.h"
#include "theme.h"
#include "ui_internal.h"

// Degraus da escala em KB/s. Sem 250 e 2500, para o rótulo do meio ficar curto.
static const int32_t SCALE_STEPS[] = {50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000};
constexpr int STEP_COUNT = sizeof(SCALE_STEPS) / sizeof(SCALE_STEPS[0]);

static ChartPanel s_panel;
static int32_t s_scale = 0;

// Como o máximo cobre a janela inteira, a escala sobe na hora e só desce
// quando o pico sai do gráfico, sem ficar pulando.
static void updateScale() {
  int32_t peak = 0;
  for (lv_chart_series_t* ser : s_panel.series) {
    const int32_t* ys = lv_chart_get_y_array(s_panel.chart, ser);
    for (int i = 0; i < CHART_POINTS; i++) {
      if (ys[i] != LV_CHART_POINT_NONE && ys[i] > peak) peak = ys[i];
    }
  }

  int32_t scale = SCALE_STEPS[STEP_COUNT - 1];
  for (int32_t step : SCALE_STEPS) {
    if (step >= peak) {
      scale = step;
      break;
    }
  }
  if (scale == s_scale) return;
  s_scale = scale;

  lv_chart_set_range(s_panel.chart, LV_CHART_AXIS_PRIMARY_Y, 0, scale);
  char buf[12];
  fmt_axis_kb(buf, sizeof(buf), scale);
  ui_set_text(s_panel.yTop, buf);
  fmt_axis_kb(buf, sizeof(buf), scale / 2);
  ui_set_text(s_panel.yMid, buf);
}

static void push(int32_t down, int32_t up) {
  chart_panel_push(s_panel, down, up);
  updateScale();
  chart_panel_update_markers(s_panel);  // a escala pode ter mudado
}

void tab_network_create(lv_obj_t* page) {
  const ChartLegend legend[2] = {{"Download", col::net_down}, {"Upload", col::net_up}};
  chart_panel_create(s_panel, page, legend);
  updateScale();
}

void tab_network_update(const Metrics& m) {
  // pontos em KB/s inteiros
  push((m.rx + 500) / 1000, (m.tx + 500) / 1000);

  char buf[24];
  fmt_rate(buf, sizeof(buf), m.rx);
  ui_set_text(s_panel.legendValues[0], buf);
  fmt_rate(buf, sizeof(buf), m.tx);
  ui_set_text(s_panel.legendValues[1], buf);
}

void tab_network_gap() {
  push(LV_CHART_POINT_NONE, LV_CHART_POINT_NONE);
}
