#pragma once

#include "line_reader.h"

void serial_link_begin(unsigned long baud);

// Lê a serial sem bloquear. A cada linha JSON válida recebida devolve o tipo,
// já convertida em `m` ou `cu`; chame em loop até devolver LineType::None.
LineType serial_link_poll(Metrics& m, ClaudeUsage& cu);
