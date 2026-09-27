#include "ota_link.h"

#include <ArduinoOTA.h>
#include <WiFi.h>
#include <lvgl.h>

#include "ui/ui.h"

namespace {

constexpr const char* HOSTNAME = "macmonitor";  // macmonitor.local, igual ao upload_port
constexpr uint32_t ERROR_SHOW_MS = 4000;

bool otaStarted = false;
int lastPct = -1;
uint32_t errorAt = 0;

void startOta() {
  Serial.printf("Wi-Fi conectado: %s (OTA em %s.local)\n",
                WiFi.localIP().toString().c_str(), HOSTNAME);

  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    lastPct = -1;
    errorAt = 0;
    ui_ota_progress(0);
    lv_timer_handler();
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    int pct = total ? done * 100 / total : 0;
    if (pct == lastPct) return;
    lastPct = pct;
    // handle() segura o loop durante a gravação inteira: redesenha daqui
    ui_ota_progress(pct);
    lv_timer_handler();
  });
  ArduinoOTA.onEnd([]() {
    ui_ota_message("Reiniciando...");
    lv_timer_handler();
  });
  ArduinoOTA.onError([](ota_error_t err) {
    Serial.printf("OTA falhou: erro %u\n", err);
    ui_ota_message("Falha na atualização");
    lv_timer_handler();
    errorAt = millis();
  });

  ArduinoOTA.begin();
  otaStarted = true;
}

}  // namespace

void ota_link_begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void ota_link_poll() {
  if (!otaStarted) {
    if (WiFi.status() == WL_CONNECTED) startOta();
    return;
  }
  ArduinoOTA.handle();

  if (errorAt && millis() - errorAt > ERROR_SHOW_MS) {
    errorAt = 0;
    ui_ota_hide();
  }
}
