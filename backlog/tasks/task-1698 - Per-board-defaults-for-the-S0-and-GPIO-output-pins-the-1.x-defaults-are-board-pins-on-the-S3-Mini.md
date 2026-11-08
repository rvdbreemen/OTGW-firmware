---
id: TASK-1698
title: >-
  Per-board defaults for the S0 and GPIO output pins (the 1.x defaults are board
  pins on the S3 Mini)
status: Done
assignee: []
created_date: '2026-11-07T16:55:00Z'
updated_date: '2026-11-08T08:04:00Z'
labels:
  - firmware
  - settings
dependencies: []
priority: medium
ordinal: 811000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-07 (follow-up of TASK-1686): the S0 default GPIO 12 (PIC reset on the S3 Mini, I2C SDA on the Pro) and the output default GPIO 16 (LED1 on the S3 Mini) are board pins; initS0Count()/validateGPIOOutputsConfig() refuse them (TASK-1552), so enabling S0 or the output without changing the pin does nothing. Choose a free hole per board map (Classic S3 Mini, Classic S3 Mini Pro, OT-Direct) and use it only while the stored pin is still the old default, so a user's own choice is never overwritten.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Per board map a free default for S0 and the output, documented with the hole it lands on
- [x] #2 Applied only when the stored pin equals the old 1.x default; a user's own pin stays; host test red on the old code, green on the fix
- [x] #3 Bench: a unit with the old default gets the new pin, S0/output start without the board-pin refusal
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented (host-verified, not yet built/benched): OTGW-firmware.h [legacy-pin-defaults] legacyPinMigrated() + pin constants; settingStuff.ino [apply-legacy-pin-defaults] applyLegacyPinDefaults() (migrate S0 then output, skip a target another feature holds, writeSettings once); setup() calls it before initOutputs()/initS0Count(); settingsHelp texts. Final holes: S0 D6 (Mini 13, Pro 39); output Mini D7 (11), Pro D8 (37) because the Pro D7 hole (38) is TFT MOSI per the lolin_s3_mini_pro variant (TFT_BL 33, RST 34, CS 35, DC 36, MOSI 38, SCK 40). Test tests/test_legacy_pin_defaults.py compiles the real blocks: fix OK (2/2), old origin/dev 2/2 FAIL (no blocks, no call in setup). Mutants caught: without the busy check (cases 5-7 fail), migrating on OTGW32 (case 2 fails). tests/test_setting_gpio_range.py 5/5 OK; settings-help webui 28/28; evaluate --quick 0 FAIL.

Review (opus, refute lens) 2026-11-08: (1) BUG fixed: outputs_ext.ino validateGPIOOutputsConfig() still capped the pin at 16 (ESP8266), so the Pro default 37 and every free Pro hole 37-39 was refused 'invalid pin' - a TASK-1686 gap already on dev since beta.125; now settingGpioInRange(); new test compiles the real validator (old 37/38 refused, fix accepted; board pins 16/14, 23, -1 still refused). (6) a failed writeSettings() now marks settings dirty (TASK-1495). (9) help text names other boards. (10) source check: call is after readSettings(true) and applyResolvedComboPins(). (11) cases -1 and user-pin collisions added. (12) S0types/Outputstypes comments point at the migration. Accepted, not changed: (5) a Pro misdetected as S3 Mini gets 13/11 written (Pro button / I2C SCL); in that state the whole map is wrong already (watchdog I2C, LEDs, PIC reset on Pro SDA) and pin 11 is only driven when the user enables the output. (8) Ethernet SPI 11/13 overlap in degraded PIC mode: a Classic has no W5500.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: OTGW-firmware.h [legacy-pin-defaults] documents per map: S3 Mini S0 13 (D6) / output 11 (D7); S3 Mini Pro S0 39 (D6) / output 37 (D8, its D7 38 is TFT MOSI); OTGW32 keeps 12/16 (Robert 2026-11-07). Sources: LOLIN variants lolin_s3_mini/lolin_s3_mini_pro pins_arduino.h, docs/hardware/combo-esp32-s3-pinout.md. Help texts name the holes. AC2: applied only when the stored pin equals the 1.x default and the target is not held by another feature; tests/test_legacy_pin_defaults.py compiles the real blocks + real validateGPIOOutputsConfig(): old origin/dev 3/3 FAIL, fix 3/3 OK (12 migration cases incl. -1 and user-pin collisions; second boot writes nothing; call order after readSettings/applyResolvedComboPins). Mutants caught (no busy check; migrating on OTGW32). Review (opus) found the output validator still capped at 16 (TASK-1686 gap on dev since beta.125: Pro holes 37-39 refused) - fixed here. evaluate --quick 0 FAIL; test_setting_gpio_range 5/5. AC3: bench .63 (Classic + S3 Mini, boardmode 1, stored 1.x defaults 12/16): OTA beta.135+042cf92 -> settings diff exactly s0counterpin 12->13, gpiooutputspin 16->11 (164 keys, nothing else). Enabling the output: telnet shows 'Output GPIO11 set to 0' per frame, no 'used by the board'/'invalid pin'; S0 enabled + reboot on GPIO13: boots normally. Restored: beta.130 + original settings, diff vs pre-bench none. Not verified: S0 pulse counting with a meter on the D6 hole (no meter on the bench); an S3 Mini Pro unit (host-tested only).
<!-- SECTION:FINAL_SUMMARY:END -->
