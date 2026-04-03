# OTGW-firmware v1.4.0 Release Notes
**Release date:** 2026-04-02
**Branch:** main (from dev)
**Compare:** [v1.3.4...v1.4.0](https://github.com/rvdbreemen/OTGW-firmware/compare/v1.3.4...v1.4.0)

## Overview

v1.4.0 is a major feature release. It adds an embedded smart thermostat (SAT) that turns the OTGW into a standalone heating controller, and introduces ESP32 support alongside the existing ESP8266 target through a unified platform abstraction layer.

## New features

- **SAT (Smart Autotune Thermostat):** An embedded heating controller that runs entirely on the ESP, independent of Home Assistant or any external automation. SAT calculates optimal boiler flow temperature using a weather-compensated heating curve and PID v3 controller, supporting both continuous modulation and PWM flame cycling. Six independent safety layers ensure the boiler is always released when something goes wrong. Includes Web UI dashboard, REST API (`/api/v2/sat/*`), MQTT publish/subscribe, and Home Assistant auto-discovery (climate entity + 7 sensors). Supports radiator and underfloor heating. See [feature documentation](docs/features/smart-autotune-thermostat.md) and [ADR-062](docs/adr/ADR-062-sat-smart-autotune-thermostat-integration.md).
- **Unified ESP8266/ESP32 platform abstraction:** 30+ inline shim functions (heap, chip ID, filesystem, serial, reset reason, RTC, hostname, etc.) let the firmware compile for ESP8266 or ESP32 from one source tree. The abstraction is compile-time only, so the ESP8266 binary is unaffected. Documented in [ADR-061](docs/adr/ADR-061-unified-esp8266-esp32-platform-abstraction.md).
- **PlatformIO build support:** New `platformio.ini` with dual environments (`esp8266`, `esp32`). Build with `pio run -e esp8266` or `pio run -e esp32`.
- **ESP32 OTA update server:** Dedicated `OTGW-ModUpdateServer-esp32.h` providing web-based firmware update for ESP32.
- **Board-level GPIO definitions:** New `boards.h` auto-detects the target platform and provides unified pin names (`PIN_LED1`, `PIN_BUTTON`, `PIN_I2C_SCL`, etc.).
- **Unified directory iteration:** `PlatformDir` class wraps both ESP8266 and ESP32 filesystem directory APIs behind a single interface.

## Improvements

- **OpenTherm binary literal modernization:** `OpenThermMessageType` enum values changed from `B000`..`B111` to standard `0b000`..`0b111` (C++14), improving compiler compatibility.
- **Serial error detection abstraction:** New `platformSerialHasOverrun()` and `platformSerialHasRxError()` functions. Available on ESP8266; returns `false` on ESP32 (hardware limitation).
- **Unified filesystem handling:** `LittleFS` is used on both platforms with abstracted `FSInfo` retrieval.

## ESP32 status

ESP32 support is functional but **experimental** in this release:

- Pin definitions are provisional until NodoShop ESP32 hardware is finalized.
- RTC user memory is emulated via NVS Preferences (flash-backed, limited write endurance).
- Serial overrun/error detection is not available on ESP32.
- ESP32 provides significantly more resources: ~320KB usable RAM, dual-core 240 MHz, larger flash partitions.

## Breaking changes

No breaking changes vs v1.3.4. The ESP8266 build output is functionally identical.

## Upgrade notes

- Flash both firmware and filesystem.
- Hard-refresh the browser after flashing (Ctrl+F5).
- Existing ESP8266 users: no configuration changes needed.
- ESP32 users: this is a first release; expect pin assignments and partition layout to evolve.
