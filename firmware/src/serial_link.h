#pragma once

#include "metrics.h"

void serial_link_begin(unsigned long baud);

// Lê a serial sem bloquear. Devolve true a cada linha JSON válida recebida,
// já convertida em `m`; chame em loop até devolver false.
bool serial_link_poll(Metrics& m);
