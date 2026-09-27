#pragma once

#include "metrics.h"

// Recebe as mesmas linhas JSON da serial via Bluetooth LE (Nordic UART Service).
void ble_link_begin();

// Mesma semântica de serial_link_poll(): true a cada linha válida recebida.
bool ble_link_poll(Metrics& m);
