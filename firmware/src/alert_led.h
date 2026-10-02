#pragma once

#include "metrics.h"

// LED RGB do CYD: verde normal, amarelo com pressão de memória em alerta,
// vermelho com CPU acima de 90% por 10 s ou pressão crítica.
void alert_led_begin();
void alert_led_update(const Metrics& m);  // a cada pacote
void alert_led_off();                     // Mac offline: apaga
