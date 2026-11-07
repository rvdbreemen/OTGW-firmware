---
id: TASK-1686
title: >-
  S3 Mini Pro: the free sensor holes D6-D8 (GPIO 37-39) lie outside the 0-16 pin
  range; GPIO output default 16 is LED1 on the S3 Mini
status: To Do
assignee: []
created_date: '2026-11-07T09:40:00Z'
updated_date: '2026-11-07T11:07:00Z'
labels:
  - sensors
dependencies: []
priority: low
ordinal: 806000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found under TASK-1662 (2026-11-07). The pin settings gpiosensorspin, s0counterpin and gpiooutputspin accept 0-16 (settingStuff.ino GPIOSENSORSpin/S0COUNTERpin/GPIOOUTPUTSpin, restAPI.ino:4225), an ESP8266 limit. On the S3 Mini Pro in the Classic socket the free holes D6/D7/D8 are GPIO 39/38/37 (Arduino variant lolin_s3_mini_pro SPI pins), so a Pro user cannot select them. Also: the GPIO output default 16 (Outputstypes.h:22) is LED1 on the S3 Mini Classic map and is refused by isBoardReservedPin (TASK-1552); the S0 default 12 is the PIC reset hole on the S3 Mini and is refused the same way. Options: widen the range to the S3 GPIO set (0-48 minus reserved), and/or per-board defaults.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Decision by Robert: widen the pin range and/or per-board defaults
- [ ] #2 A Pro user can select a free hole for the 1-Wire bus, proven on a host test of the setting validation
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-07 Robert: '1686 is akkoord. Je mag hoger instellen op een s3' - widen the pin range on the S3 (beyond 0-16), board pins stay refused by isBoardReservedPin.
<!-- SECTION:NOTES:END -->
