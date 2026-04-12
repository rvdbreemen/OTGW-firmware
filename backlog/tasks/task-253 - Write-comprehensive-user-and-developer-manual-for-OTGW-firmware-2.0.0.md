---
id: TASK-253
title: Write comprehensive user and developer manual for OTGW-firmware 2.0.0
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-11 15:50'
updated_date: '2026-04-12 08:17'
labels:
  - documentation
  - 2.0.0
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Write a single cohesive documentation manual covering the full OTGW-firmware v2.0.0 product. Target audience: end users (chapters 1-6) and developers (chapters 7-10). Must include introduction, chapter structure, and conclusion. Output: docs/MANUAL.md
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Document has a clear introduction (what is OTGW-firmware, who is it for)
- [x] #2 User-facing chapters: Getting Started, Web Interface, Home Assistant, SAT Thermostat, Troubleshooting
- [x] #3 Developer chapters: Architecture, MQTT/REST API reference, Build system, Contributing
- [x] #4 Consistent style throughout, no em dashes, English
- [ ] #5 Saved to docs/MANUAL.md, committed and pushed
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Written docs/MANUAL.md as a 10-chapter comprehensive manual for OTGW-firmware v2.0.0.

User chapters (1-6): Introduction (what the firmware is, who it is for, hardware support, v2.0.0 changes), Getting Started (flash script, first boot, Wi-Fi setup, port reference), Web Interface (dashboard, graphs, settings, FSexplorer, OTA, OLED, Telnet debug), Home Assistant Integration (MQTT setup, auto-discovery entities, commands, climate entity, webhook quick start, HA built-in integration), SAT Smart Autotune Thermostat (control modes, presets, multi-zone, solar gain, summer suppression, pressure monitoring, OPV calibration, Open-Meteo), Troubleshooting (Wi-Fi, MQTT, no OT data, missing HA entities, unexpected reboots, AP fallback, error pattern table).

Developer chapters (7-10): Architecture (component table, platform abstraction, data flow diagram, settings/state ADR-051, memory constraints, timer system, command queue), MQTT and REST API Reference (topic structure, key published topics, command topics, webhook full reference, REST API overview with key endpoints and curl examples), Build System (PlatformIO environments, build and flash commands, build.py, evaluate.py, Arduino IDE, release builds), Contributing (coding rules: PROGMEM, no ArduinoJson, no String in hot paths, no Serial after init, command queue, buffer validation, watchdog; naming conventions; ADR list; testing steps; community links).

Sources used: README.md, docs/api/MQTT.md, docs/api/README.md, docs/features/webhook.md, docs/c4/c4-context.md, backlog/docs/doc-3 (SAT integration guide).
<!-- SECTION:FINAL_SUMMARY:END -->
