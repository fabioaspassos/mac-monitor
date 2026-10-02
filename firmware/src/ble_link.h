#pragma once

#include "line_reader.h"

// Recebe as mesmas linhas JSON da serial via Bluetooth LE (Nordic UART Service).
void ble_link_begin();

// Mesma semântica de serial_link_poll(): o tipo de cada linha válida recebida.
LineType ble_link_poll(Metrics& m, ClaudeUsage& cu);
