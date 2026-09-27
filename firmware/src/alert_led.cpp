#include "alert_led.h"

#include <Arduino.h>

namespace {

// Pinos do LED no CYD; o LED é ânodo comum, acende com o pino em LOW
constexpr int PIN_R = 4, PIN_G = 16, PIN_B = 17;
constexpr uint8_t CH_R = 1, CH_G = 2, CH_B = 3;  // canal 0 é o backlight
constexpr uint32_t PWM_FREQ = 5000;
constexpr uint8_t PWM_BITS = 8;

constexpr float CPU_ALERT = 90;
constexpr float CPU_CLEAR = 85;  // histerese: só sai do alerta abaixo disto
constexpr uint32_t CPU_ALERT_AFTER_MS = 10000;

// O LED é muito forte: intensidades baixas, vermelho um pouco mais visível
struct Rgb {
  uint8_t r, g, b;
};
constexpr Rgb OFF = {0, 0, 0};
constexpr Rgb GREEN = {0, 12, 0};
constexpr Rgb YELLOW = {40, 18, 0};
constexpr Rgb RED = {90, 0, 0};

uint32_t cpuHighSince = 0;  // 0 = CPU abaixo do limite
bool cpuAlert = false;

void write(const Rgb& c) {
  ledcWrite(CH_R, 255 - c.r);
  ledcWrite(CH_G, 255 - c.g);
  ledcWrite(CH_B, 255 - c.b);
}

}  // namespace

void alert_led_begin() {
  ledcSetup(CH_R, PWM_FREQ, PWM_BITS);
  ledcSetup(CH_G, PWM_FREQ, PWM_BITS);
  ledcSetup(CH_B, PWM_FREQ, PWM_BITS);
  ledcAttachPin(PIN_R, CH_R);
  ledcAttachPin(PIN_G, CH_G);
  ledcAttachPin(PIN_B, CH_B);
  write(OFF);
}

void alert_led_update(const Metrics& m) {
  uint32_t now = millis();
  if (m.cpu > CPU_ALERT) {
    if (!cpuHighSince) cpuHighSince = now ? now : 1;
    if (now - cpuHighSince >= CPU_ALERT_AFTER_MS) cpuAlert = true;
  } else {
    cpuHighSince = 0;
    if (m.cpu < CPU_CLEAR) cpuAlert = false;
  }

  if (cpuAlert || m.press == 4) {
    write(RED);
  } else if (m.press == 2) {
    write(YELLOW);
  } else {
    write(GREEN);
  }
}

void alert_led_off() {
  cpuHighSince = 0;
  cpuAlert = false;
  write(OFF);
}
