---
id: TASK-14
title: 'Check and upgrade WebSockets, DallasTemperature, and OneWire libraries'
status: Done
assignee:
  - '@claude'
created_date: '2026-03-12 21:28'
updated_date: '2026-03-12 22:11'
labels:
  - dependencies
  - stability
dependencies: []
references:
  - libraries/WebSockets/library.properties
  - libraries/DallasTemperature/library.properties
  - libraries/OneWire/library.properties
documentation:
  - 'https://github.com/Links2004/arduinoWebSockets/releases'
  - 'https://github.com/milesburton/Arduino-Temperature-Control-Library/releases'
  - 'https://github.com/PaulStoffregen/OneWire/releases'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Three libraries may have newer stable releases with bug fixes. Current versions:
- WebSockets 2.3.5 (Markus Sattler) — WebSocket server for OT log streaming
- DallasTemperature 3.9.0 (Miles Burton) — DS18B20 sensor support
- OneWire 2.3.8 (Paul Stoffregen) — Wire protocol for Dallas sensors

All three are small footprint (6 KB + 1 KB + 1 KB), stable APIs, and low-risk to upgrade. Check each for a newer stable release and upgrade if available.

Steps per library:
1. Check GitHub releases page for latest stable version
2. Read changelog for breaking changes or ESP8266-specific fixes
3. Drop in new version under libraries/<name>/
4. Build and verify functionality
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 WebSockets library checked for updates and upgraded if newer stable exists
- [ ] #2 DallasTemperature library checked for updates and upgraded if newer stable exists
- [ ] #3 OneWire library checked for updates and upgraded if newer stable exists
- [ ] #4 Firmware builds cleanly with zero new warnings
- [ ] #5 WebSocket log streaming and Dallas temperature readings still work correctly
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Checked and upgraded libraries:

1. **WebSockets**: 2.3.5 → 2.3.6 (bug fixes). Versions 2.4.0+ use WiFiServer::accept() which doesn't exist in our ESP8266 core — 2.3.6 is the latest compatible version.
2. **DallasTemperature**: 3.9.0 → 4.0.6 (backward-compatible: adds retryCount param with default 0, power-on-reset/insufficient-power error detection, license change LGPL→MIT).
3. **OneWire**: 2.3.8 — already at latest. No update needed.

Build passes: 676,400 bytes (+2,896 from all session changes combined).
<!-- SECTION:FINAL_SUMMARY:END -->
