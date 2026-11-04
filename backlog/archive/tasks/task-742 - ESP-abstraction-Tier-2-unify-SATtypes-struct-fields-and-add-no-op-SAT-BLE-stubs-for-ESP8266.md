---
id: TASK-742
title: >-
  ESP abstraction Tier 2: unify SATtypes struct fields and add no-op SAT-BLE
  stubs for ESP8266
status: Done
assignee: []
created_date: '2026-05-28 08:28'
updated_date: '2026-05-29 22:30'
labels:
  - esp-abstraction-audit
  - refactor
dependencies: []
ordinal: 69000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Eliminate the struct-field divergence that cascades into ~10 downstream debug/REST/MQTT sites. (a) In SATtypes.h, remove the #if defined(ESP32) guards at lines 335-344 (state.sat BLE fields) and 461-475 (settings.sat BLE fields). Either keep the fields unconditionally and zero-init on ESP8266 (preferred for binary settings.json compatibility), or hoist the whole header under #if HAS_SAT. (b) Provide empty inline stubs in platform_esp8266.h for satBLEInit, satBLELoop, satBLEPublishMQTT, satBLESendStatusJSON, satBLEGetTemp, etc., so every caller can drop its #if defined(ESP32). Then strip those guards from: OTGW-firmware.ino:644, OTGW-firmware.h:284-313, SATcontrol.ino:975/1948/2559/3042, networkStuff.ino:514/530, handleDebug.ino:184/236/257/298/401, settingStuff.ino:384/1002 (factor BLE settings I/O into platformSerializeBleSettings/platformParseBleSettings). Depends on Tier 1.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SATtypes.h has no inner #if defined(ESP32) guards inside struct definitions
- [x] #2 settings.json round-trips between ESP8266 and ESP32 binaries without loss of BLE fields (BLE fields are zero/empty on ESP8266 but present)
- [x] #3 platform_esp8266.h declares inline no-op stubs for the full satBLE* API surface
- [x] #4 All ESP32-feature-call sites listed in the task description no longer carry a raw ESP32 ifdef
- [x] #5 BLE settings serialise/parse via platformSerializeBleSettings / platformParseBleSettings - settingStuff.ino has no inner BLE ifdef
- [x] #6 python build.py --firmware exits 0 on both platforms
- [x] #7 Guardrail violation count drops by at least another 15 from Tier 1 baseline
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Tier 2 complete (commit 1f4be978, alpha.104), Option A. SATtypes.h BLE runtime+settings fields made unconditional (zero/empty on ESP8266). platform_esp8266.h gained inline no-op stubs for the full satBLE* surface. settingStuff.ino serialise/parse unconditional => settings.json round-trips ESP8266<->ESP32 (AC#2 verified by inspection: symmetric key sets). All BLE/weather raw defined(ESP32) sites across OTGW-firmware.{h,ino}/SATcontrol/handleDebug/networkStuff/restAPI/MQTTstuff removed (functional, stub-covered) or moved to HAS_SAT_BLE / HAS_WEATHER_FORECAST. ESP_ABSTRACTION_BASELINE 58->35 (-23, well past the -15 floor). Build clean both targets; ESP8266 +~1.5KB as expected; evaluate 66 passed 0 failed. NOTE: settings round-trip and BLE inert-on-ESP8266 verified by code inspection only; on-hardware confirmation not performed.
<!-- SECTION:FINAL_SUMMARY:END -->
