---
id: TASK-1930
title: >-
  Pressing the reset button several times no longer forces the WiFi config
  portal on the ESP32-S3: the external-reset counter never counts
status: To Do
assignee: []
created_date: '2026-11-14T08:57:00Z'
labels:
  - bug
  - wifi
  - platform
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 988000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during TASK-1904 (code reading, not seen on a device). shouldForceWifiConfigPortal() (src/OTGW-firmware/OTGW-firmware.ino:91-120) counts consecutive external resets through isExternalSystemReset() -> platformIsExternalReset() (platform_esp32.h, esp_reset_reason() == ESP_RST_EXT) and forces the WiFi config portal after WIFI_PORTAL_RESET_TRIGGER_COUNT of them. The ESP32-S3 reset mapping get_reset_reason() (framework-espidf esp_system/port/soc/esp32s3/reset_reason.c, local copy; the pinned libs are precompiled, not proven identical) never returns ESP_RST_EXT: a press of the reset button pulls EN/CHIP_PU and comes in as RESET_REASON_CHIP_POWER_ON (0x01) -> ESP_RST_POWERON. So the counter is reset to 0 on every boot and the portal is never forced this way; the feature from the 1.x line (where an external reset is its own cause) is dead on 2.0.0. Note: 0x01 is shared by power-on, brown-out and the chip-level super watchdog, so 'button pressed' cannot be told from 'power applied' by the reset code alone; a fix needs another signal (e.g. a RTC_NOINIT boot marker that survives a reset but not a power cut, as TASK-1904 and ADR-233 use RTC memory). The TASK-1904 raw reset code shim (platformResetRawCode) is a likely building block; scope kept separate.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce on a bench unit: N quick reset-button presses (or EN pulses) do not open the WiFi config portal on the old build; record the reset reason codes
- [ ] #2 A fix that tells a reset press from a power-on on the S3 (decision on the signal recorded in the task), with old red / fix green on the same reproduction
<!-- AC:END -->
