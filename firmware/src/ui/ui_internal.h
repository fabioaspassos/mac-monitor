#pragma once

// Funções de cada parte da interface, usadas só por ui.cpp

#include <lvgl.h>

#include "../metrics.h"

void status_bar_create(lv_obj_t* parent);
void status_bar_set_online(bool online);
void status_bar_update(const Metrics& m);

void tab_overview_create(lv_obj_t* page);
void tab_overview_update(const Metrics& m);

void tab_history_create(lv_obj_t* page);
void tab_history_update(const Metrics& m);
void tab_history_gap();

void tab_cores_create(lv_obj_t* page);
void tab_cores_update(const Metrics& m);

void tab_network_create(lv_obj_t* page);
void tab_network_update(const Metrics& m);
void tab_network_gap();

void tab_claude_create(lv_obj_t* page);
void tab_claude_update(const ClaudeUsage& c);
