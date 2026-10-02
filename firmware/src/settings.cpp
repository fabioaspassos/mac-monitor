#include "settings.h"

#include <Arduino.h>
#include <Preferences.h>

namespace {

constexpr const char* NAMESPACE = "macmon";
constexpr uint8_t MODE_COUNT = 4;  // BrightnessMode em backlight.h
constexpr uint8_t TAB_COUNT = 5;   // Geral, Histórico, Núcleos, Rede, Claude
constexpr uint32_t TAB_SAVE_DELAY_MS = 5000;

Preferences prefs;
uint8_t mode = 0;
uint8_t savedTab = 0;
uint32_t pendingTab = 0;
uint32_t pendingSince = 0;

}  // namespace

void settings_begin() {
  prefs.begin(NAMESPACE, false);
  mode = prefs.getUChar("bl", 0);
  if (mode >= MODE_COUNT) mode = 0;
  savedTab = prefs.getUChar("tab", 0);
  if (savedTab >= TAB_COUNT) savedTab = 0;
  pendingTab = savedTab;
}

uint8_t settings_brightness_mode() {
  return mode;
}

void settings_set_brightness_mode(uint8_t m) {
  if (m == mode) return;
  mode = m;
  prefs.putUChar("bl", mode);
}

uint8_t settings_tab() {
  return savedTab;
}

void settings_poll(uint32_t activeTab) {
  uint32_t now = millis();
  if (activeTab != pendingTab) {
    pendingTab = activeTab;
    pendingSince = now;
    return;
  }
  if (pendingTab != savedTab && now - pendingSince >= TAB_SAVE_DELAY_MS) {
    savedTab = pendingTab;
    prefs.putUChar("tab", savedTab);
  }
}
