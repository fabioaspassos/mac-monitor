# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

A hardware system monitor for macOS: a Python agent on the Mac reads system metrics and streams them as JSON over USB serial to an ESP32 microcontroller, which renders them on a small TFT display. Two independent halves live side by side:

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
python agent.py
```

There is no test suite or linter configured for the agent.

**Architecture:**
- `agent.py` — entry point. Finds the ESP32 over USB by matching the CH340 USB-serial chip's VID/PID (`0x1A86, 0x7523`), opens a serial connection at 115200 baud, and every `INTERVAL` (1s) writes one JSON line (`Collector.collect()` + `"\n"`) to the port. Reconnects automatically (polling every 2s) if the board isn't found or the connection drops.
- `metrics.py` — `Collector.collect()` gathers all metrics into a single dict, keyed by short field names (`cpu`, `mem`, `swap`, `press`, `temp`, `gpu`, `rx`, `tx`, `disk`, `rd`, `wr`, plus `cores` and a `v` version tag) because they're sent over a slow serial link. Notable details:
  - CPU thermal state comes from `NSProcessInfo.thermalState()` (via PyObjC) — this is the only thermal signal Apple exposes without root on Apple Silicon (0=nominal..3=critical), not an actual °C reading.
  - Memory pressure comes from `sysctl kern.memorystatus_vm_pressure_level`.
  - GPU utilization is parsed by regex from `ioreg -r -d 1 -c IOAccelerator` output (no public API for this).
  - Network/disk throughput are computed as rates via the `RateTracker` helper, which diffs cumulative `psutil` counters against the previous reading.
- The JSON schema exchanged between agent and firmware is the contract between these two halves — when adding/renaming a field in `metrics.py`, update the corresponding parsing in `firmware/src/serial_link.cpp` (`parseLine()`) and the `Metrics` struct in `firmware/src/metrics.h`.
- `fake.py` — sends test lines (scenarios `onda`, `normal`, `alta`, `nucleos`) once per second, to exercise the display without real Mac load: `python fake.py alta`. Stop `agent.py` first; only one process can hold the port.

## Firmware (`firmware/`)

Standard PlatformIO project targeting `esp32dev` (Arduino framework) on the ESP32-2432S028R ("CYD", no PSRAM). Key config lives in `platformio.ini`: TFT pinout/driver via `build_flags` (ILI9341, 240x320, HSPI, `TFT_INVERSION_ON`), XPT2046 touch pins as `XPT_*` (separate VSPI bus — don't name them `TOUCH_*`, that enables TFT_eSPI's own touch code), and the `huge_app.csv` partition. Library deps: `TFT_eSPI`, `ArduinoJson`, `lvgl@9.2.2`, `XPT2046_Touchscreen`. LVGL is configured in `include/lv_conf.h` (built-in themes off — every widget is styled explicitly; custom fonts declared via `LV_FONT_CUSTOM_DECLARE`).

**Build/upload (requires PlatformIO, e.g. the `pio` CLI or the PlatformIO IDE extension):**
```bash
cd firmware
pio run                 # build
pio run -t upload       # flash to the ESP32
pio device monitor       # serial monitor (decodes crashes, timestamps — see monitor_filters)
```
If upload fails, try lowering `upload_speed` in `platformio.ini` (comment notes 460800 → 115200 as fallback).

**Architecture (`firmware/src/`):**
- `main.cpp` — LVGL setup (`lv_tft_espi_create` + `LV_DISPLAY_ROTATION_90`, partial draw buffer of 1/10 screen, XPT2046 input device with calibration constants `TOUCH_*_MIN/MAX`) and the update loop from section 7 of the phase 5 doc: `lv_timer_handler()`, `serial_link_poll()` → `ui_update()`, online/offline after 3s, `ui_tick_offline()` once per second while offline.
- `serial_link.cpp` — non-blocking line reader into a fixed buffer (RX buffer enlarged to 1024 because LVGL can stall the loop), parses JSON protocol v1 into `Metrics` (`metrics.h`, `cores[MAX_CORES=20]` + `coreCount`).
- `ui/` — LVGL interface: `ui.cpp` (screen, `lv_tabview` with bottom tab bar, "Aguardando o Mac..." overlay on `lv_layer_top()`), `status_bar.cpp`, one `tab_*.cpp` per screen, `chart_panel.cpp` (layout shared by Histórico and Rede), `theme.h/.cpp` (color tokens `col::*`, shared `lv_style_t`s, `value_color()` driven by `UI_COLOR_MODE`, small widget helpers), `format.cpp` (Brazilian number formatting), `fonts/` (Montserrat generated with `lv_font_conv` — bpp 4, range `0x20-0x7E,0xA0-0xFF,0x2212`; regenerate rather than hand-edit).
- No allocation on the update path beyond LVGL's own: fixed `char` buffers + `snprintf`, and `ui_set_text()` only touches labels whose text changed. Core bars are the one exception: recreated when the core count changes.

There are no firmware tests configured (`firmware/test/` only has PlatformIO's default placeholder README).

## Interface da fase 5
- Requisitos: docs/fase5/fase5-requisitos-interface.md
- Telas de referência (320x240, escala 1:1): docs/fase5/*.png
- Ao implementar uma tela, siga as medidas do documento e confira com a imagem correspondente.