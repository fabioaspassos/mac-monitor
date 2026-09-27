#pragma once

// Layout comum das telas Histórico (6.2) e Rede (6.4): legenda, gráfico de
// linha de 2 minutos com grade, eixo Y à direita e eixo X embaixo.

#include <lvgl.h>

constexpr int CHART_POINTS = 120;  // 1 ponto por segundo

struct ChartPanel {
  lv_obj_t* chart;
  // [0] = série principal (CPU, Download): primeira na legenda e desenhada por cima
  // [1] = série secundária (GPU, Upload)
  lv_chart_series_t* series[2];
  lv_obj_t* markers[2];
  lv_obj_t* legendValues[2];
  lv_obj_t* yTop;
  lv_obj_t* yMid;
};

struct ChartLegend {
  const char* name;
  uint32_t color;
};

void chart_panel_create(ChartPanel& p, lv_obj_t* page, const ChartLegend (&legend)[2]);

// Acrescenta um ponto em cada série (LV_CHART_POINT_NONE abre um vão)
void chart_panel_push(ChartPanel& p, int32_t primary, int32_t secondary);

// Reposiciona os marcadores do último ponto (depois de mudar a escala, por exemplo)
void chart_panel_update_markers(ChartPanel& p);
