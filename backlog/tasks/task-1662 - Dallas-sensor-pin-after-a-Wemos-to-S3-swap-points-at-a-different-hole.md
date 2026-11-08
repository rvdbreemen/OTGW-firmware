---
id: TASK-1662
title: Dallas sensor pin after a Wemos-to-S3 swap points at a different hole
status: Done
assignee:
  - '@claude'
created_date: '2026-11-05T19:30:00Z'
updated_date: '2026-11-08T09:03:00Z'
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
2026-11-07 Robert's decision (AskUserQuestion in session 74b8fc81): document only. A table 'D1 mini hole to S3 GPIO' in the S3 migration docs plus a short hint in the help text of the setting; no translation on import (a 13 from a 2.x export is a valid GPIO13, which cannot be told from the number).

2026-11-07: docs in docs/manuals/en/ch02-hardware-setup.md and docs/manuals/nl/h02-hardware-installatie.md (Dallas section): why the pin must be set again after the Wemos to S3 swap, plus a hole table D0-D8 with Wemos, S3 Mini and S3 Mini Pro GPIO and whether the hole is free on the Classic. Sources: docs/hardware/PINOUT.md 3a/4a, boards.h:270-271, OTGW-firmware.h:937 boardPinReserved, and the Arduino variants lolin_s3_mini and lolin_s3_mini_pro pins_arduino.h (Pro D6-D8 = MISO/MOSI/SS 39/38/37). Help text gpiosensorspin rewritten: 'GPIO number of the DS18B20 data line, not the hole: D7 is GPIO 11 on the S3 Mini.' (settings-help.test.mjs 28/28 pass, 18395 of 18432 bytes). Found: on the Pro the free holes are GPIO 37-39, outside the 0-16 setting range; filed as TASK-1686.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1 Robert: document only. AC2: docs/manuals/en/ch02-hardware-setup.md and nl/h02-hardware-installatie.md carry the hole table (Wemos / S3 Mini / S3 Mini Pro GPIO per hole, which are free on the Classic) and the GPIO-not-hole rule; sensor help text says the same (settings-help test 29/29). Pushed f984eb36e (beta.137, origin/dev 963b68d2a), bench .61 /settings-help.json serves it, #alpha-testing 1568913428758798336 (zip beta.141+963b68d covers 137-141).
<!-- SECTION:FINAL_SUMMARY:END -->
