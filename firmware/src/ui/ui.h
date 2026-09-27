#pragma once

#include <lvgl.h>

#include "../metrics.h"

void ui_init(lv_display_t* disp, uint32_t initialTab);
void ui_update(const Metrics& m);
void ui_set_online(bool online);
void ui_tick_offline();  // 1x por segundo enquanto offline

// Painel de atualização OTA por cima de tudo (criado na primeira chamada)
void ui_ota_progress(int pct);
void ui_ota_message(const char* text);
void ui_ota_hide();

// Aviso curto no centro da tela, some sozinho depois de 1,5 s
void ui_toast(const char* text);

// Índice da aba ativa (0=Geral .. 3=Rede), guardado na flash por settings.cpp
uint32_t ui_active_tab();
