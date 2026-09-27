#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>

#include "ble_link.h"
#include "metrics.h"
#include "serial_link.h"
#include "ui/ui.h"

constexpr int SCREEN_W = 320, SCREEN_H = 240;
constexpr uint32_t OFFLINE_AFTER_MS = 3000;

// Leituras cruas do XPT2046 nos cantos da tela (calibração típica do CYD).
// Se o toque sair deslocado, ajuste estes limites.
constexpr int TOUCH_X_MIN = 200, TOUCH_X_MAX = 3700;
constexpr int TOUCH_Y_MIN = 240, TOUCH_Y_MAX = 3800;

// Buffer parcial de 1/10 da tela (~15 KB); uint32_t garante o alinhamento de 4 bytes
constexpr size_t DRAW_BUF_BYTES = SCREEN_W * SCREEN_H / 10 * sizeof(uint16_t);
static uint32_t drawBuf[DRAW_BUF_BYTES / sizeof(uint32_t)];

// No CYD o toque fica num barramento SPI separado do da tela
SPIClass touchSpi(VSPI);
XPT2046_Touchscreen touch(XPT_CS, XPT_IRQ);

Metrics metrics;
bool online = false;
uint32_t lastPacketAt = 0;
uint32_t lastOfflineTick = 0;

uint32_t tickMillis() {
  return millis();
}

void readTouch(lv_indev_t* indev, lv_indev_data_t* data) {
  if (touch.tirqTouched() && touch.touched()) {
    TS_Point p = touch.getPoint();
    // posição em paisagem, como aparece na tela
    int x = constrain(map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W - 1), 0, SCREEN_W - 1);
    int y = constrain(map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H - 1), 0, SCREEN_H - 1);
    // Com LV_DISPLAY_ROTATION_90 o LVGL espera o ponto na orientação nativa
    // (retrato) e gira ele mesmo; entregar em paisagem faria girar duas vezes.
    data->point.x = y;
    data->point.y = SCREEN_W - 1 - x;
    data->state = LV_INDEV_STATE_PRESSED;
#ifdef TOUCH_DEBUG
    Serial.printf("touch raw=%d,%d z=%d -> %d,%d\n", p.x, p.y, p.z, x, y);
#endif
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void setup() {
  serial_link_begin(115200);
  ble_link_begin();

  touchSpi.begin(XPT_SCLK, XPT_MISO, XPT_MOSI, XPT_CS);
  touch.begin(touchSpi);
  touch.setRotation(1);

  lv_init();
  lv_tick_set_cb(tickMillis);

  // o driver cria a TFT_eSPI em retrato; a rotação de 90° deixa em paisagem (setRotation(1))
  lv_display_t* disp = lv_tft_espi_create(TFT_WIDTH, TFT_HEIGHT, drawBuf, DRAW_BUF_BYTES);
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);

  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, readTouch);

  ui_init(disp);
}

void loop() {
  lv_timer_handler();

  while (serial_link_poll(metrics) || ble_link_poll(metrics)) {
    ui_update(metrics);
    lastPacketAt = millis();
    if (!online) {
      online = true;
      ui_set_online(true);
    }
  }

  uint32_t now = millis();
  if (online && now - lastPacketAt > OFFLINE_AFTER_MS) {
    online = false;
    ui_set_online(false);
    lastOfflineTick = now;
  }
  if (!online && now - lastOfflineTick >= 1000) {
    lastOfflineTick = now;
    ui_tick_offline();
  }

  delay(5);
}
