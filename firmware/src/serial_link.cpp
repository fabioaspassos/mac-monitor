#include "serial_link.h"

#include <Arduino.h>
#include <ArduinoJson.h>

namespace {

constexpr size_t LINE_CAPACITY = 768;

char lineBuf[LINE_CAPACITY];
size_t lineLen = 0;
bool lineOverflow = false;
JsonDocument doc;

bool parseLine(Metrics& m) {
  if (deserializeJson(doc, lineBuf, lineLen)) return false;
  if ((doc["v"] | 0) != 1) return false;

  m = Metrics{};
  m.cpu = doc["cpu"] | 0.0f;
  m.mem = doc["mem"] | 0.0f;
  m.swap = doc["swap"] | 0.0f;
  m.disk = doc["disk"] | 0.0f;
  m.gpu = doc["gpu"] | 0.0f;
  m.press = doc["press"] | 0;
  m.temp = doc["temp"] | 0;
  m.rx = doc["rx"] | 0L;
  m.tx = doc["tx"] | 0L;
  m.rd = doc["rd"] | 0L;
  m.wr = doc["wr"] | 0L;

  // núcleos além de MAX_CORES são ignorados
  for (JsonVariantConst core : doc["cores"].as<JsonArrayConst>()) {
    if (m.coreCount >= MAX_CORES) break;
    m.cores[m.coreCount++] = core | 0.0f;
  }
  return true;
}

}  // namespace

void serial_link_begin(unsigned long baud) {
  // O LVGL pode segurar o loop por dezenas de ms durante uma animação; com o
  // buffer padrão de 256 bytes uma linha de ~300 bytes chegaria truncada.
  Serial.setRxBufferSize(1024);
  Serial.begin(baud);
}

bool serial_link_poll(Metrics& m) {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      bool ok = !lineOverflow && lineLen > 0 && parseLine(m);
      lineLen = 0;
      lineOverflow = false;
      if (ok) return true;
    } else if (c != '\r') {
      if (lineLen < LINE_CAPACITY - 1) {
        lineBuf[lineLen++] = c;
      } else {
        lineOverflow = true;  // descarta a linha inteira em vez de parsear pela metade
      }
    }
  }
  return false;
}
