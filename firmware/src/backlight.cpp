#include "backlight.h"

#include <Arduino.h>

namespace {

constexpr uint8_t PWM_CHANNEL = 0;
constexpr uint32_t PWM_FREQ = 5000;
constexpr uint8_t PWM_BITS = 8;

constexpr uint32_t DIM_AFTER_MS = 30000;
constexpr uint8_t DIM_LEVEL = 13;   // ~5%
constexpr uint8_t AUTO_MIN = 38;    // ~15%: mínimo do modo automático no escuro
constexpr uint32_t FADE_STEP_MS = 4;  // 1 passo de duty a cada 4 ms: 0→255 em ~1 s

// LDR do CYD: quanto mais escuro, MAIOR a leitura. Valores típicos com
// atenuação de 0 dB; se o automático não acompanhar a luz, compile com
// -D LDR_DEBUG, veja as leituras no monitor serial e ajuste estes limites.
constexpr int LDR_PIN = 34;
constexpr int LDR_BRIGHT = 0;
constexpr int LDR_DARK = 400;
constexpr uint32_t LDR_READ_MS = 100;
constexpr float LDR_SMOOTH = 0.05f;  // média móvel exponencial: ~2 s para acomodar

const uint8_t MODE_LEVEL[BL_MODE_COUNT] = {0, 255, 153, 77};  // BL_AUTO usa o LDR
const char* const MODE_NAME[BL_MODE_COUNT] = {"Auto", "100%", "60%", "30%"};

uint8_t mode = BL_AUTO;
bool dimmed = false;
uint32_t lastActivity = 0;
float ldr = 0;
uint32_t lastLdrRead = 0;
uint8_t duty = 0;
uint32_t lastFadeStep = 0;

uint8_t autoLevel() {
  long level = map((long)ldr, LDR_BRIGHT, LDR_DARK, 255, AUTO_MIN);
  return constrain(level, AUTO_MIN, 255);
}

uint8_t targetLevel() {
  if (dimmed) return DIM_LEVEL;
  return mode == BL_AUTO ? autoLevel() : MODE_LEVEL[mode];
}

void readLdr(uint32_t now) {
  if (now - lastLdrRead < LDR_READ_MS) return;
  lastLdrRead = now;
  int raw = analogRead(LDR_PIN);
  ldr += (raw - ldr) * LDR_SMOOTH;
#ifdef LDR_DEBUG
  static uint32_t lastLog = 0;
  if (now - lastLog >= 1000) {
    lastLog = now;
    Serial.printf("ldr raw=%d media=%.0f -> %u\n", raw, ldr, autoLevel());
  }
#endif
}

}  // namespace

void backlight_begin(uint8_t m) {
  mode = m < BL_MODE_COUNT ? m : BL_AUTO;

  analogSetPinAttenuation(LDR_PIN, ADC_0db);  // o LDR varia pouco: mais sensibilidade
  ldr = analogRead(LDR_PIN);

  lastActivity = millis();
  duty = targetLevel();
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_BITS);
  ledcAttachPin(TFT_BL, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, duty);
}

void backlight_poll() {
  uint32_t now = millis();
  readLdr(now);
  if (!dimmed && now - lastActivity > DIM_AFTER_MS) dimmed = true;

  if (now - lastFadeStep < FADE_STEP_MS) return;
  // o loop pode atrasar (LVGL): anda proporcional ao tempo passado, com limite
  uint32_t steps = min<uint32_t>((now - lastFadeStep) / FADE_STEP_MS, 16);
  lastFadeStep = now;
  uint8_t target = targetLevel();
  if (duty == target) return;
  if (duty < target) {
    duty = min<uint32_t>(target, duty + steps);
  } else {
    duty = duty > target + steps ? duty - steps : target;
  }
  ledcWrite(PWM_CHANNEL, duty);
}

uint8_t backlight_cycle_mode() {
  mode = (mode + 1) % BL_MODE_COUNT;
  return mode;
}

const char* backlight_mode_name() {
  return MODE_NAME[mode];
}

void backlight_activity() {
  lastActivity = millis();
  dimmed = false;
}

bool backlight_touch() {
  bool wasDimmed = dimmed;
  backlight_activity();
  return wasDimmed;
}
