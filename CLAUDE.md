# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

A hardware system monitor for macOS: a Python agent on the Mac reads system metrics and streams them as JSON over USB serial (or, optionally, Bluetooth LE) to an ESP32 microcontroller, which renders them on a small TFT display. Two independent halves live side by side:

- `agent/` — Python daemon running on the Mac
- `firmware/` — PlatformIO/Arduino C++ firmware running on the ESP32

Code comments and commit messages in this repo are written in Portuguese; match that when adding comments.

## Agent (`agent/`)

**Setup and run:**
```bash
cd agent
python3 -m venv .venv          # already present as agent/.venv
source .venv/bin/activate
pip install -r requirements.txt
python agent.py         # USB serial
python agent.py --ble   # Bluetooth LE
```

There is no test suite or linter configured for the agent.

**Architecture:**
- `agent.py` — entry point. Opens a transport (`--ble` picks Bluetooth, default USB) and every `INTERVAL` (1s) writes one JSON line (`Collector.collect()` + `"\n"`). On `TransportError` it closes and reopens the same transport.
- `transport.py` — `SerialTransport` finds the ESP32 by the CH340 USB-serial chip's VID/PID (`0x1A86, 0x7523`) at 115200 baud; `BleTransport` (via `bleak`, own asyncio loop so the agent stays synchronous) scans for the Nordic UART Service UUID / name `"Mac Monitor"` and writes each line to the NUS RX characteristic in chunks of `max_write_without_response_size`. Both retry every 2s until connected. The first `--ble` run triggers macOS's Bluetooth permission prompt for the terminal app.
- `metrics.py` — `Collector.collect()` gathers all metrics into a single dict, keyed by short field names (`cpu`, `mem`, `swap`, `press`, `temp`, `gpu`, `rx`, `tx`, `disk`, `rd`, `wr`, plus `cores` and a `v` version tag) because they're sent over a slow serial link. Notable details:
  - CPU thermal state comes from `NSProcessInfo.thermalState()` (via PyObjC) — this is the only thermal signal Apple exposes without root on Apple Silicon (0=nominal..3=critical), not an actual °C reading.
  - Memory pressure comes from `sysctl kern.memorystatus_vm_pressure_level`.
  - GPU utilization is parsed by regex from `ioreg -r -d 1 -c IOAccelerator` output (no public API for this).
  - Network/disk throughput are computed as rates via the `RateTracker` helper, which diffs cumulative `psutil` counters against the previous reading.
- The JSON schema exchanged between agent and firmware is the contract between these two halves — when adding/renaming a field in `metrics.py`, update the corresponding parsing in `firmware/src/line_reader.cpp` (`parseLine()`) and the `Metrics` struct in `firmware/src/metrics.h`.
- `fake.py` — sends test lines (scenarios `onda`, `normal`, `alta`, `nucleos`) once per second, to exercise the display without real Mac load: `python fake.py alta` (add `--ble` for Bluetooth). Stop `agent.py` first; only one process can hold the port / BLE connection.

## Firmware (`firmware/`)

Standard PlatformIO project targeting `esp32dev` (Arduino framework) on the ESP32-2432S028R ("CYD", no PSRAM). Key config lives in `platformio.ini`: TFT pinout/driver via `build_flags` (ILI9341, 240x320, HSPI, `TFT_INVERSION_ON`), XPT2046 touch pins as `XPT_*` (separate VSPI bus — don't name them `TOUCH_*`, that enables TFT_eSPI's own touch code), and the `min_spiffs.csv` partition (two 1.9 MB app slots, needed for OTA). Wi-Fi SSID/password and the OTA password come from `firmware/secrets.ini` (gitignored; copy `secrets.ini.example`) via `extra_configs` and are passed as `WIFI_SSID`/`WIFI_PASS`/`OTA_PASSWORD` build flags. Library deps: `TFT_eSPI`, `ArduinoJson`, `lvgl@9.2.2`, `XPT2046_Touchscreen`, `NimBLE-Arduino@^2` (peripheral-only, 1 connection, via `CONFIG_BT_NIMBLE_*` build flags). LVGL is configured in `include/lv_conf.h` (built-in themes off — every widget is styled explicitly; custom fonts declared via `LV_FONT_CUSTOM_DECLARE`).

**Build/upload (requires PlatformIO, e.g. the `pio` CLI or the PlatformIO IDE extension):**
```bash
cd firmware
pio run                 # build
pio run -t upload       # flash to the ESP32
pio device monitor       # serial monitor (decodes crashes, timestamps — see monitor_filters)
pio run -e esp32dev_ota -t upload   # flash over Wi-Fi (ArduinoOTA, macmonitor.local)
```
The first flash after the partition-table change must be over USB; after that OTA works.
If upload fails, try lowering `upload_speed` in `platformio.ini` (comment notes 460800 → 115200 as fallback).

**Architecture (`firmware/src/`):**
- `main.cpp` — LVGL setup (`lv_tft_espi_create` + `LV_DISPLAY_ROTATION_90`, partial draw buffer of 1/10 screen, XPT2046 input device with calibration constants `TOUCH_*_MIN/MAX`) and the update loop from section 7 of the phase 5 doc: `lv_timer_handler()`, `serial_link_poll()` / `ble_link_poll()` → `ui_update()`, online/offline after 3s, `ui_tick_offline()` once per second while offline.
- `line_reader.cpp` — `LineReader::feed(char, Metrics&)`: assembles bytes into a fixed buffer up to `\n` and parses JSON protocol v1 into `Metrics` (`metrics.h`, `cores[MAX_CORES=20]` + `coreCount`). Each source has its own reader so lines never interleave.
- `serial_link.cpp` — feeds USB serial bytes to a `LineReader` (RX buffer enlarged to 1024 because LVGL can stall the loop).
- `ble_link.cpp` — NimBLE peripheral advertising the Nordic UART Service (UUIDs must match `agent/transport.py`). `onWrite` runs on the NimBLE task and only pushes bytes into a static FreeRTOS stream buffer; `ble_link_poll()` drains it from `loop()` into its own `LineReader`.
- `ota_link.cpp` — non-blocking Wi-Fi STA connect; once connected starts `ArduinoOTA` (hostname `macmonitor`, password-protected). During an upload `ArduinoOTA.handle()` blocks the loop, so the progress callbacks call `ui_ota_progress()` + `lv_timer_handler()` themselves.
- `backlight.cpp` — backlight PWM (LEDC channel 0 on `TFT_BL`, attached after the TFT init). Modes Auto/100/60/30% (cycled by tapping the status bar); Auto maps the CYD's LDR on GPIO 34 (`ADC_0db`, higher reading = darker; calibrate `LDR_BRIGHT/LDR_DARK` with `-D LDR_DEBUG`). Dims to ~5% after 30 s without packets or touches; the touch that wakes it is swallowed in `readTouch()`.
- `alert_led.cpp` — CYD RGB LED (R=4, G=16, B=17, common anode → inverted PWM on LEDC channels 1–3): green normal, yellow at `press==2`, red at `press==4` or CPU > 90% for 10 s (clears below 85%); off while offline.
- `settings.cpp` — `Preferences` namespace `macmon`: brightness mode (`bl`, written on tap) and last tab (`tab`, written after 5 s on a tab, restored by `ui_init`).
- `ui/` — LVGL interface: `ui.cpp` (screen, `lv_tabview` with bottom tab bar, "Aguardando o Mac..." overlay on `lv_layer_top()`), `status_bar.cpp`, one `tab_*.cpp` per screen, `chart_panel.cpp` (layout shared by Histórico and Rede), `theme.h/.cpp` (color tokens `col::*`, shared `lv_style_t`s, `value_color()` driven by `UI_COLOR_MODE`, small widget helpers), `format.cpp` (Brazilian number formatting), `fonts/` (Montserrat generated with `lv_font_conv` — bpp 4, range `0x20-0x7E,0xA0-0xFF,0x2212`; regenerate rather than hand-edit).
- No allocation on the update path beyond LVGL's own: fixed `char` buffers + `snprintf`, and `ui_set_text()` only touches labels whose text changed. Core bars are the one exception: recreated when the core count changes.

There are no firmware tests configured (`firmware/test/` only has PlatformIO's default placeholder README).

## Interface da fase 5
- Requisitos: docs/fase5/fase5-requisitos-interface.md
- Telas de referência (320x240, escala 1:1): docs/fase5/*.png
- Ao implementar uma tela, siga as medidas do documento e confira com a imagem correspondente.