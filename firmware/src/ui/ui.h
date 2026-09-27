#pragma once

#include <lvgl.h>

#include "../metrics.h"

void ui_init(lv_display_t* disp);
void ui_update(const Metrics& m);
void ui_set_online(bool online);
void ui_tick_offline();  // 1x por segundo enquanto offline

// Índice da aba ativa (0=Geral .. 3=Rede); a fase 6 vai persistir esse valor
uint32_t ui_active_tab();
