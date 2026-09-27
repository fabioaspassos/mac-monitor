#include "line_reader.h"

#include <ArduinoJson.h>

namespace {

// compartilhado: as linhas são parseadas uma de cada vez, sempre no loop()
JsonDocument doc;

bool parseLine(const char* line, size_t len, Metrics& m) {
  if (deserializeJson(doc, line, len)) return false;
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

bool LineReader::feed(char c, Metrics& m) {
  if (c == '\n') {
    bool ok = !overflow_ && len_ > 0 && parseLine(buf_, len_, m);
    len_ = 0;
    overflow_ = false;
    return ok;
  }
  if (c != '\r') {
    if (len_ < CAPACITY - 1) {
      buf_[len_++] = c;
    } else {
      overflow_ = true;  // descarta a linha inteira em vez de parsear pela metade
    }
  }
  return false;
}
