---
id: TASK-945
title: >-
  Fix 0x26 external-WD I2C log storm on ESP32-S3 when the watchdog does not
  respond
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-29 05:08'
updated_date: '2026-06-29 05:12'
labels:
  - async-esp32s3
dependencies: []
ordinal: 158000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
On a classic-S3 (LOLIN S3 Mini in OTGW Classic socket, MAC AC:27:6E:CE:45:D8) the firmware spams [E][esp32-hal-i2c-ng.c:275] i2cWrite(): i2c_master_transmit failed: [259] ESP_ERR_INVALID_STATE every ~100ms. Root cause: feedWatchDog() (OTGW-Core.ino) writes 0xA5 to the external 0x26 watchdog every 100ms UNCONDITIONALLY; the code comment assumes 'absent 0x26 simply NACKs (no-op)', but on the ESP32-S3 esp32-hal-i2c-ng driver a non-responding 0x26 logs an [E] error per write instead of a silent NACK. Same applies to WatchDogEnabled() (arm/disarm) and the boot-time 0x26 status read. Found on-device 2026-06-29 (alpha.286, both pre-existing firmware and a fresh esp32-classic flash spam identically; OLED at 0x3C also absent on this board, probed-and-disabled cleanly — only the WD path spams because it is periodic and ungated). Fix: probe 0x26 once at init (mirror probeOLED at 0x3C), store a runtime state.hw.bExtWdPresent flag, and gate the 0x26 feed/arm/read on it. Safe: an absent chip cannot bite, and the ESP32 TWDT is the PRIMARY watchdog (ADR-135); the 0x26 is SECONDARY. Verifiable via serial (i2c error count per 10s: before vs after).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 0x26 is runtime-probed once at init (beginTransmission/endTransmission==0) and the result stored in a runtime flag
- [ ] #2 feedWatchDog(), WatchDogEnabled() arm/disarm, and the boot 0x26 status read are gated on the runtime flag (still compile-gated by HAS_PIC_WATCHDOG)
- [ ] #3 On a board with no responding 0x26, serial shows ZERO i2c_master_transmit errors over a 10s window (was ~97)
- [ ] #4 On a board WITH a responding 0x26 the watchdog still feeds normally (probe true -> feed continues)
- [ ] #5 Build esp32-classic + esp32 green; evaluate.py --quick no new failures
<!-- AC:END -->
