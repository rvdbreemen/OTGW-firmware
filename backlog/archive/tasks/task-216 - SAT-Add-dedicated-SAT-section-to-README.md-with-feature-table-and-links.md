---
id: TASK-216
title: 'SAT: Add dedicated SAT section to README.md with feature table and links'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:25'
updated_date: '2026-04-09 05:46'
labels:
  - audit-fix
  - sat
  - docs
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
README.md mentions SAT in the v1.4.0 highlights section (~line 13) but does not have a dedicated SAT section. A user reading the README cannot find:\n\n- A complete list of SAT features (weather curve, PID, PWM cycling, OPV, presets, BLE, multi-area, etc.)\n- Links to the detailed documentation (integration guide, MQTT topics, OPV calibration)\n- Any description of how SAT relates to the thermostat and boiler\n- Known limitations vs. the Python SAT component\n\nThe README needs a ## SAT - Smart Autotune Thermostat section added after the highlights, covering:\n1. One-paragraph description of what SAT does\n2. Feature list (all implemented features)\n3. Quick start summary (enable, set target temp, configure heating system)\n4. Links to: backlog/docs/sat-integration-guide.md, backlog/docs/sat-mqtt-topics.md, backlog/docs/sat-opv-calibration.md\n5. Note on supported hardware (ESP8266/ESP32, BLE on ESP32 only)\n\nSource: README.md (lines 11-18), backlog/docs/sat-integration-guide.md."
<!-- SECTION:DESCRIPTION:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added a dedicated 'SAT - Smart Autotune Thermostat' section to README.md, placed after the v1.4.0 highlights section. The section includes: a one-paragraph description of what SAT does (weather-compensated heating curve + PID v3, replaces external controller), a bullet list of all major features (auto-tune, two control modes, six safety layers, heating curve recommendation, OPV calibration, presets, multi-area, solar gain, summer simmer, pressure monitoring, BLE on ESP32), a hardware support table (ESP8266/ESP32 with BLE availability), a 5-step quick start guide, and an integration table with links to MQTT.md, openapi.yaml, OPV calibration doc, and preset configuration doc.
<!-- SECTION:FINAL_SUMMARY:END -->
