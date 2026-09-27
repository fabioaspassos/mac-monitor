#pragma once

#include <stdint.h>

// Brilho da tela: PWM no TFT_BL, automático pelo LDR do GPIO 34 e
// escurecimento depois de 30 s sem dados.
enum BrightnessMode : uint8_t { BL_AUTO, BL_100, BL_60, BL_30, BL_MODE_COUNT };

// Chamar depois de criar a tela (a TFT_eSPI liga o TFT_BL com digitalWrite no init)
void backlight_begin(uint8_t mode);
void backlight_poll();  // a cada loop(): lê o LDR, escurece por inatividade e faz o fade

uint8_t backlight_cycle_mode();  // Auto → 100% → 60% → 30% → Auto; devolve o novo modo
const char* backlight_mode_name();

void backlight_activity();  // chegou pacote: acende e reinicia a contagem de 30 s
// Toque na tela: reinicia a contagem de 30 s. Devolve true se a tela estava
// escurecida (esse toque só acende, não deve chegar na interface).
bool backlight_touch();
