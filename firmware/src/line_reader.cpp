#include "line_reader.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>

namespace {

// compartilhado: as linhas são parseadas uma de cada vez, sempre no loop()
JsonDocument doc;

void parseMetrics(Metrics& m) {
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
}

void parseClaude(ClaudeUsage& c) {
  c = ClaudeUsage{};
  c.ok = (doc["ok"] | 0) == 1;

  const char* src = doc["src"] | "none";
  c.src = strcmp(src, "api") == 0   ? ClaudeUsage::SRC_API
          : strcmp(src, "sl") == 0  ? ClaudeUsage::SRC_SL
          : strcmp(src, "est") == 0 ? ClaudeUsage::SRC_EST
                                    : ClaudeUsage::SRC_NONE;

  JsonObjectConst b5 = doc["b5"].as<JsonObjectConst>();
  if (!b5.isNull()) {
    c.hasBlock = true;
    c.blockPct = b5["pct"].is<int>() ? b5["pct"].as<int>() : -1;
    c.blockRst = b5["rst"] | 0L;
  }
  JsonObjectConst w7 = doc["w7"].as<JsonObjectConst>();
  if (!w7.isNull()) {
    c.hasWeek = true;
    c.weekPct = w7["pct"] | 0;
    c.weekDay = w7["wd"] | -1;
    c.weekHour = w7["wh"] | -1;
  }

  c.tokBlock = doc["tb"] | 0L;
  c.tokDay = doc["td"] | 0L;
  c.rate = doc["rate"] | 0L;
  c.sessions = doc["ses"] | 0;
  c.hour = constrain(doc["hr"] | 0, 0, 23);  // índice de hours[]
  int i = 0;
  for (JsonVariantConst v : doc["h"].as<JsonArrayConst>()) {
    if (i >= 24) break;
    c.hours[i++] = v | 0L;
  }
}

LineType parseLine(const char* line, size_t len, Metrics& m, ClaudeUsage& cu) {
  if (deserializeJson(doc, line, len)) return LineType::None;
  if ((doc["v"] | 0) != 1) return LineType::None;

  // sem "t" é a linha de métricas; tipos desconhecidos ficam para versões futuras
  const char* type = doc["t"] | "";
  if (type[0] == '\0') {
    parseMetrics(m);
    return LineType::Metrics;
  }
  if (strcmp(type, "cc") == 0) {
    parseClaude(cu);
    return LineType::Claude;
  }
  return LineType::None;
}

}  // namespace

LineType LineReader::feed(char c, Metrics& m, ClaudeUsage& cu) {
  if (c == '\n') {
    LineType type = !overflow_ && len_ > 0 ? parseLine(buf_, len_, m, cu) : LineType::None;
    len_ = 0;
    overflow_ = false;
    return type;
  }
  if (c != '\r') {
    if (len_ < CAPACITY - 1) {
      buf_[len_++] = c;
    } else {
      overflow_ = true;  // descarta a linha inteira em vez de parsear pela metade
    }
  }
  return LineType::None;
}
