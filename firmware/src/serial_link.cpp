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

bool serial_link_poll(Metrics& m) {
  while (Serial.available()) {
    if (reader.feed(Serial.read(), m)) return true;
  }
  return false;
}
