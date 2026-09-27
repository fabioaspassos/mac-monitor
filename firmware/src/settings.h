#pragma once

#include <stdint.h>

// Preferências guardadas na flash (Preferences / NVS): modo de brilho e última aba.
void settings_begin();

uint8_t settings_brightness_mode();
void settings_set_brightness_mode(uint8_t mode);  // grava na hora (só muda por toque)

uint8_t settings_tab();
// Chamar a cada loop(): grava a aba ativa depois de 5 s parada nela,
// para não gastar a flash enquanto o usuário desliza pelas abas.
void settings_poll(uint32_t activeTab);
