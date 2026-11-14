---
id: TASK-1930
title: >-
  Pressing the reset button several times no longer forces the WiFi config
  portal on the ESP32-S3: the external-reset counter never counts
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T08:57:00Z'
updated_date: '2026-11-14T11:47:00Z'
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
- [x] #1 Host reproduction on the real gate and counter: three reset-button boots (S3 ESP_RST_POWERON) do not open the portal on the old code and do on the fix (test/host/test_wifi_portal_reset.py P1, P5; P2/P3/P4/P6 green on both)
- [ ] #2 Fix per Robert's decision A (DM 1571125618777522260; the S3 cannot tell a reset press from a power-on, reset_reasons.h:38-40): reset-button presses and power-ons count, software/USB/watchdog/system brown-out resets do not; ADR-238 Proposed supersedes ADR-043; OLED hint and WiFi recovery guide updated
- [ ] #3 FIELD/MAINTAINER (not bench-testable: a USB reset reports ESP_RST_USB and no USB route presses the button): on a build with the fix, three reset-button presses or three plug pulls, each within 10 s of boot, open the config portal; a single power cut does not
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Analysis (2026-11-14): shouldForceWifiConfigPortal() (OTGW-firmware.ino:95) counts a boot only when platformIsExternalReset() (esp_reset_reason()==ESP_RST_EXT, platform_esp32.h:827) is true; the S3 never reports ESP_RST_EXT. The pinned libs' soc/esp32s3 reset_reasons.h:38-40 give the EN pin (reset button), power-on, chip brown-out and the super watchdog the same raw code 0x01 (RESET_REASON_CHIP_POWER_ON/_BROWN_OUT/_SUPER_WDT), so the button cannot be told apart from a power-on in software. The counter lives in NVS (platformRtcWrite), so it survives a power-on. A forced portal WIPES the stored WiFi credentials (networkStuff.ino:364, ADR-043/TASK-760). Consequence: counting 0x01 means 3 power interruptions, each within 10 s of the previous boot (WIFI_PORTAL_RESET_WINDOW_MS), erase the WiFi. ADR-043 (Accepted) says 'external resets' and 'without boot-time button reads', so every fix changes it: needs a superseding ADR. Open decision put to Robert via the coordinator; no code before the answer.

Option C corrected (coordinator): GPIO0 is a strapping pin on the S3, holding BOOT at reset enters the ROM download mode, so C is a long press (>=5 s) while the firmware runs, not during boot. Button pin per board (activeButton(), OTGW-firmware.h:946): OTGW32 GPIO0 (boards.h:58), Classic+S3 Mini GPIO18 D3 (boards.h:269), Classic+S3 Mini Pro GPIO13 (boards.h:291). The OLED page switch reacts to the falling edge with debounce (OLED.ino:580), so a long press is distinguishable. The button pin is set up only when an OLED is found (oledStart(true), OLED.ino:521-523), so C needs its own setup. Unknown: whether every Classic PCB revision has a button fitted on D3. Suggested guard for C: only in the first 120 s after boot, LED fast-blink after 5 s, portal on release.

Host harness (prepared before the decision, coordinator): test/host/test_wifi_portal_reset.py + .cpp compile the real WIFI_PORTAL_RESET block of OTGW-firmware.ino and the real platformIsExternalReset() with the S3 reset mapping (button and power-on = ESP_RST_POWERON / raw 0x01, NVS survives boots, RAM and millis() restart). On the current code: P1 (three reset-button presses within the window force the portal) FAIL = the bug reproduced; P2 (software resets never count) pass; P3 (presses after the window) pass; P4 (counter fed ESP_RST_EXT, ESP8266 semantics) forces the portal on the third = the counting logic works, only the gate is dead. P1 is the expectation of options A/B; under C it is replaced by long-press cases. 09 (2026-11-14): the .63 USB reset is ESP_RST_USB (raw expected 0x15/0x16, not verified), not power-on, and no USB route holds GPIO0 low while the firmware runs; so neither A/B nor C can be bench-tested without a physical button press (field/maintainer AC).

Robert's decision A (DM 1571125618777522260, via the coordinator): count every power-on-type boot; risk accepted (three power interruptions in the windows also wipe the WiFi). D3 button on every Classic revision: 'Not sure' (DM 1571125777720672267), so pulling the plug is named as a route. ACs rewritten to the decision (old AC2 asked to tell a press from a power-on, which the S3 cannot do). Fix: platformIsResetButtonOrPowerOn() in platform_esp32.h (ESP_RST_EXT or ESP_RST_POWERON); isExternalSystemReset() renamed isPortalResetGesture() (its only caller is the counter) and calls it; OLED hint 'Triple-reset = config' -> '3x reset/power=config' (21 chars); docs/guides/WIFI_RECOVERY_TRIPLE_RESET.md rewritten for 2.x (the old text said a power cycle does not count); c4-code-sensors.md hint text. ADR-238 Proposed (number from the coordinator; 234-237 reserved) supersedes ADR-043; strict lint: only the two-sided supersede link open, which 'adr supersede' writes at acceptance. Host evidence (AC1): test_wifi_portal_reset.py --old-rev HEAD (pre-fix) -> FIX P1-P6 pass; OLD P1 and P5 FAIL, P2/P3/P4/P6 pass; RESULT PASS. Related: 9 test files PASS (incl. test_wifi_skip_portal 37, ADR-211), evaluate --quick 86/0.
<!-- SECTION:NOTES:END -->
