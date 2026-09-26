#include <ArduinoJson.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

struct Metrics {
  float cpu = 0, mem = 0, swap = 0, disk = 0;
  int press = 1, gpu = 0;
  long rx = 0, tx = 0, rd = 0, wr = 0;
};

Metrics metrics;
unsigned long lastMetricsAt = 0;
bool waitingShown = false;
String lineBuffer;

String formatRate(long bytesPerSec) {
  if (bytesPerSec >= 1024 * 1024) return String(bytesPerSec / (1024.0 * 1024.0), 1) + " MB/s";
  if (bytesPerSec >= 1024) return String(bytesPerSec / 1024.0, 1) + " KB/s";
  return String(bytesPerSec) + " B/s";
}

void drawWaiting() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("Aguardando o Mac...", 20, 110);
  waitingShown = true;
}

void drawMetrics() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Monitor ESP32", 20, 10);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("CPU:   " + String(metrics.cpu, 0) + "%", 20, 45);
  tft.drawString("Mem:   " + String(metrics.mem, 0) + "%", 20, 70);
  tft.drawString("GPU:   " + String(metrics.gpu) + "%", 20, 95);
  tft.drawString("Disco: " + String(metrics.disk, 0) + "%", 20, 120);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Down: " + formatRate(metrics.rx), 20, 155);
  tft.drawString("Up:   " + formatRate(metrics.tx), 20, 180);

  waitingShown = false;
}

void applyLine(const String& line) {
  JsonDocument doc; // ArduinoJson 7: cresce sozinho, sem tamanho fixo
  if (deserializeJson(doc, line)) return; // linha corrompida, ignora

  metrics.cpu = doc["cpu"] | 0.0;
  metrics.mem = doc["mem"] | 0.0;
  metrics.swap = doc["swap"] | 0.0;
  metrics.press = doc["press"] | 1;
  metrics.gpu = doc["gpu"] | 0;
  metrics.rx = doc["rx"] | 0;
  metrics.tx = doc["tx"] | 0;
  metrics.disk = doc["disk"] | 0.0;
  metrics.rd = doc["rd"] | 0;
  metrics.wr = doc["wr"] | 0;

  lastMetricsAt = millis();
  drawMetrics();
}

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
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
      if (lineBuffer.length() > 600) lineBuffer = ""; // linha absurda, descarta
    }
  }

  if (!waitingShown && millis() - lastMetricsAt > 3000) {
    drawWaiting();
  }
}