#include <ArduinoJson.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite cardBuf(&tft);   // um sprite só, reaproveitado por todos os cards
TFT_eSprite sparkBuf(&tft);  // sprite fixo do gráfico de histórico

const int CARD_W = 96, CARD_H = 70;
const int SPARK_W = 304, SPARK_H = 52;
const int HISTORY_LEN = 60;

uint16_t COL_BG, COL_CARD, COL_GOOD, COL_WARN, COL_BAD, COL_TEXT, COL_MUTED;

struct Metrics {
  float cpu = 0, mem = 0, swap = 0, disk = 0;
  int press = 1, gpu = 0, temp = 0;
  long rx = 0, tx = 0, rd = 0, wr = 0;
};

// Apple Silicon não expõe temperatura em °C sem privilégios de root, então o
// agent manda o NSProcessInfo.thermalState (0=nominal .. 3=critical).
const char* THERMAL_LABEL[] = {"OK", "MED", "ALTA", "CRIT"};
const int THERMAL_PCT[] = {15, 45, 75, 100};

Metrics metrics;
unsigned long lastMetricsAt = 0;
bool waitingShown = false;
String lineBuffer;

int cpuHistory[HISTORY_LEN] = {0};
int historyIndex = 0, historyCount = 0;

void initPalette() {
  COL_BG    = tft.color565(10, 12, 16);
  COL_CARD  = tft.color565(24, 28, 36);
  COL_GOOD  = tft.color565(70, 200, 120);
  COL_WARN  = tft.color565(235, 180, 60);
  COL_BAD   = tft.color565(230, 80, 80);
  COL_TEXT  = tft.color565(230, 230, 235);
  COL_MUTED = tft.color565(120, 125, 135);
}

uint16_t barColor(int pct) {
  if (pct >= 85) return COL_BAD;
  if (pct >= 60) return COL_WARN;
  return COL_GOOD;
}

String formatRate(long bytesPerSec) {
  if (bytesPerSec >= 1024 * 1024) return String(bytesPerSec / (1024.0 * 1024.0), 1) + "mb";
  if (bytesPerSec >= 1024) return String(bytesPerSec / 1024.0, 1) + "kb";
  return String(bytesPerSec) + "b";
}

void drawStatusBar(bool online) {
  tft.fillRect(0, 0, 320, 20, COL_BG);
  tft.fillCircle(10, 10, 4, online ? COL_GOOD : COL_MUTED);
  tft.setTextColor(COL_MUTED, COL_BG);
  tft.setTextSize(1);
  tft.drawString(online ? "recebendo dados" : "aguardando", 20, 6);
  tft.drawRightString("heap " + String(ESP.getFreeHeap() / 1024) + "KB", 312, 6, 1);
}

// pct = -1 esconde a barra (usado no card de rede, que não tem um percentual único)
void drawCard(int x, int y, const char* label, const String& value, int pct) {
  cardBuf.fillSprite(COL_CARD);
  cardBuf.setTextDatum(TL_DATUM);

  cardBuf.setTextColor(COL_MUTED, COL_CARD);
  cardBuf.setTextSize(1);
  cardBuf.drawString(label, 8, 6);

  cardBuf.setTextColor(COL_TEXT, COL_CARD);
  cardBuf.setTextSize(2);
  cardBuf.drawString(value, 8, 22);

  if (pct >= 0) {
    int barX = 8, barY = CARD_H - 14, barW = CARD_W - 16, barH = 8;
    cardBuf.drawRoundRect(barX, barY, barW, barH, 3, COL_MUTED);
    int fillW = (barW - 2) * constrain(pct, 0, 100) / 100;
    if (fillW > 0) cardBuf.fillRoundRect(barX + 1, barY + 1, fillW, barH - 2, 2, barColor(pct));
  }

  cardBuf.pushSprite(x, y);
}

// desenha um triangulo indicando sentido do trafego: down=true para download, false para upload
void drawArrowIcon(int x, int y, bool down, uint16_t color) {
  const int w = 10, h = 8;
  if (down) {
    cardBuf.fillTriangle(x, y, x + w, y, x + w / 2, y + h, color);
  } else {
    cardBuf.fillTriangle(x, y + h, x + w, y + h, x + w / 2, y, color);
  }
}

void drawNetworkCard(int x, int y, long rx, long tx) {
  cardBuf.fillSprite(COL_CARD);
  cardBuf.setTextDatum(TL_DATUM);

  cardBuf.setTextColor(COL_MUTED, COL_CARD);
  cardBuf.setTextSize(1);
  cardBuf.drawString("REDE", 8, 6);

  cardBuf.setTextColor(COL_TEXT, COL_CARD);
  cardBuf.setTextSize(2);

  const int iconX = 8, textX = 24;
  drawArrowIcon(iconX, 25, true, COL_TEXT);
  cardBuf.drawString(formatRate(rx), textX, 22);

  drawArrowIcon(iconX, 47, false, COL_TEXT);
  cardBuf.drawString(formatRate(tx), textX, 44);

  cardBuf.pushSprite(x, y);
}

void pushHistory(int value) {
  cpuHistory[historyIndex] = value;
  historyIndex = (historyIndex + 1) % HISTORY_LEN;
  if (historyCount < HISTORY_LEN) historyCount++;
}

void drawSparkline() {
  sparkBuf.fillSprite(COL_CARD);
  sparkBuf.drawRoundRect(0, 0, SPARK_W, SPARK_H, 4, COL_MUTED);
  sparkBuf.setTextColor(COL_MUTED, COL_CARD);
  sparkBuf.setTextSize(1);
  sparkBuf.drawString("CPU (60s)", 6, 4);

  if (historyCount > 1) {
    int stepX = (SPARK_W - 8) / (HISTORY_LEN - 1);
    int prevX = -1, prevY = -1;
    for (int i = 0; i < historyCount; i++) {
      int idx = (historyIndex - historyCount + i + HISTORY_LEN) % HISTORY_LEN;
      int px = 4 + i * stepX;
      int py = SPARK_H - 4 - (cpuHistory[idx] * (SPARK_H - 18) / 100);
      if (prevX >= 0) sparkBuf.drawLine(prevX, prevY, px, py, COL_GOOD);
      prevX = px; prevY = py;
    }
  }
  sparkBuf.pushSprite(8, 178);
}

void drawWaiting() {
  tft.fillScreen(COL_BG);
  tft.setTextColor(COL_MUTED, COL_BG);
  tft.setTextSize(2);
  tft.drawString("Aguardando o Mac...", 20, 110);
  drawStatusBar(false);
  waitingShown = true;
}

void drawDashboard() {
  drawStatusBar(true);
  drawCard(8, 26, "CPU", String((int)metrics.cpu) + "%", (int)metrics.cpu);
  drawCard(112, 26, "GPU", String(metrics.gpu) + "%", metrics.gpu);
  drawCard(216, 26, "MEM", String((int)metrics.mem) + "%", (int)metrics.mem);
  drawNetworkCard(8, 102, metrics.rx, metrics.tx);
  drawCard(112, 102, "DISCO", String((int)metrics.disk) + "%", (int)metrics.disk);
  int thermalIdx = constrain(metrics.temp, 0, 3);
  drawCard(216, 102, "TEMP", THERMAL_LABEL[thermalIdx], THERMAL_PCT[thermalIdx]);
  drawSparkline();
  waitingShown = false;
}

void applyLine(const String& line) {
  JsonDocument doc;
  if (deserializeJson(doc, line)) return;

  metrics.cpu = doc["cpu"] | 0.0;
  metrics.mem = doc["mem"] | 0.0;
  metrics.gpu = doc["gpu"] | 0;
  metrics.temp = doc["temp"] | 0;
  metrics.disk = doc["disk"] | 0.0;
  metrics.rx = doc["rx"] | 0;
  metrics.tx = doc["tx"] | 0;

  pushHistory((int)metrics.cpu);
  lastMetricsAt = millis();
  if (waitingShown) tft.fillScreen(COL_BG);
  drawDashboard();
}

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  tft.invertDisplay(true);
  initPalette();

  cardBuf.createSprite(CARD_W, CARD_H);
  sparkBuf.createSprite(SPARK_W, SPARK_H);

  drawWaiting();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      applyLine(lineBuffer);
      lineBuffer = "";
    } else if (c != '\r') {
      lineBuffer += c;
      if (lineBuffer.length() > 600) lineBuffer = "";
    }
  }

  if (!waitingShown && millis() - lastMetricsAt > 3000) {
    drawWaiting();
  }
}