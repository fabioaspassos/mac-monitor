#include "serial_link.h"

#include <Arduino.h>

#include "line_reader.h"

namespace {

LineReader reader;

}  // namespace

void serial_link_begin(unsigned long baud) {
  // O LVGL pode segurar o loop por dezenas de ms durante uma animação; com o
  // buffer padrão de 256 bytes uma linha de ~300 bytes chegaria truncada.
  Serial.setRxBufferSize(1024);
  Serial.begin(baud);
}

LineType serial_link_poll(Metrics& m, ClaudeUsage& cu) {
  while (Serial.available()) {
    LineType type = reader.feed(Serial.read(), m, cu);
    if (type != LineType::None) return type;
  }
  return LineType::None;
}
