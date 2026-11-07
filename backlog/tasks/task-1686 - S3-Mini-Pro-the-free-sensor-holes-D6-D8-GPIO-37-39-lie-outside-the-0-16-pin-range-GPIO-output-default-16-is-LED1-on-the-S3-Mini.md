---
id: TASK-1686
title: >-
  S3 Mini Pro: the free sensor holes D6-D8 (GPIO 37-39) lie outside the 0-16 pin
  range; GPIO output default 16 is LED1 on the S3 Mini
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-07T09:40:00Z'
updated_date: '2026-11-07T13:26:00Z'
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
- [x] #2 A Pro user can select a free hole for the 1-Wire bus, proven on a host test of the setting validation
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-07 Robert: '1686 is akkoord. Je mag hoger instellen op een s3' - widen the pin range on the S3 (beyond 0-16), board pins stay refused by isBoardReservedPin.

Robert 2026-11-07 via 54: 'akkoord, je mag hoger instellen op een s3'. Defaults unchanged (sensor 10, S0 12, output 16) as instructed: changing them needs Robert. Implemented: settingGpioInRange() in OTGW-firmware.h ([setting-gpio-range] markers, SETTING_GPIO_MAX 48): GPIO 0-21 and 26-48 (22-25 do not exist on the ESP32-S3). The three setters (GPIOSENSORSpin, S0COUNTERpin, GPIOOUTPUTSpin) use it instead of 0-16; restAPI announces max SETTING_GPIO_MAX for gpiosensorspin/s0counterpin/gpiooutputspin, which the v2 settings page uses as the input max. Board pins inside the range stay refused where they are used (isBoardReservedPin in initSensors/initS0Count/validateGPIOOutputsConfig, TASK-1552).

Evidence: tests/test_setting_gpio_range.py compiles the REAL setter branches, settingGpioInRange() and boardPinReserved() with the real boards.h (MSVC) and calls the setters like a REST POST. Fix: 5/5 OK (Pro free holes GPIO 39/38/37 stored for the 1-Wire bus and not reserved on COMBO_MAP_CLASSIC_PRO; 22, 25, 49, -1 refused; 10, 0, 16 still accepted; REST max is SETTING_GPIO_MAX; the three isBoardReservedPin guards are in place). On origin/dev settingStuff.ino + OTGW-firmware.h: the Pro case fails (GPIOSENSORSpin=38/37, S0COUNTERpin=38, GPIOOUTPUTSpin=37 refused, pin stays -1). test_board_pin_reserved, test_settings_whitelist, test_settings_float_roundtrip pass; evaluate --quick 0 FAIL. Build and OTA bench pending (one build with TASK-1692).
<!-- SECTION:NOTES:END -->
