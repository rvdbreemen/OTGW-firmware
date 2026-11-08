---
id: TASK-1662
title: Dallas sensor pin after a Wemos-to-S3 swap points at a different hole
status: Done
assignee:
  - '@claude'
created_date: '2026-11-05T19:30:00Z'
updated_date: '2026-11-08T06:59:00Z'
labels:
  - docs
  - sensors
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 786000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
On the Classic the S3 Mini is footprint-compatible, but settings.sensors.iPin (gpiosensorspin) is a raw GPIO number: a 1.x value (e.g. 13 = D7 on the D1 mini) lands on another hole on the S3 (GPIO13 = D6; D7 = GPIO11, PINOUT.md 3a). A user who restores 1.x settings or keeps the old number gets no sensor or the wrong pin. Raised by a field-tester question in #nederlandse-ondersteuning 2026-11-05 (msg 1567961775649853561). Options: document the mapping in the S3 migration guide and web UI help; or translate D1-mini GPIO numbers on settings import when the board is Classic-on-S3.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Decision by Robert: document only, or translate on import
- [x] #2 The S3 migration docs list the D1-mini hole to S3 GPIO mapping for the sensor pin
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-08 decision (Robert, AskUserQuestion): document only. Translating on import cannot tell an ESP8266 GPIO number from an S3 one (13 is valid on both).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: Robert chose 'document only' (2026-11-08, AskUserQuestion; import translation cannot tell an ESP8266 GPIO from an S3 one). AC2: docs/MANUAL.md 'Migrating from ESP8266 (1.x)' now explains that pin settings are GPIO numbers and lists the free Classic holes D6/D7/D8 -> Wemos D1 mini 12/13/15, S3 Mini 13/11/10, S3 Mini Pro 39/38/37 (sources: Arduino core variants lolin_s3_mini/lolin_s3_mini_pro pins_arduino.h, docs/hardware/combo-esp32-s3-pinout.md table 1), notes that D0-D5/TX/RX are board pins the firmware will not use (boardPinReserved(), TASK-1552; sensors_ext.ino logs 'sensors not started'), and that the Pro D7 (38) is the display data line. The Dallas section's stale 'setting range 0 to 16' is corrected to the TASK-1686 range and points Pro users to D6/D8. tests/test_manual_sat_commands.py 6/6, test_sat_mqtt_state.py 30/30. Not done (by decision): no firmware translation, no web UI help change.
<!-- SECTION:FINAL_SUMMARY:END -->
