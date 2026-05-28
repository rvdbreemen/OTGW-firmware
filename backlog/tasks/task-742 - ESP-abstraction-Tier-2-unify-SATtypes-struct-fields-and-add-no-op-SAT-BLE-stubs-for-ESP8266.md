---
id: TASK-742
title: >-
  ESP abstraction Tier 2: unify SATtypes struct fields and add no-op SAT-BLE
  stubs for ESP8266
status: To Do
assignee: []
created_date: '2026-05-28 08:28'
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
- [ ] #1 SATtypes.h has no inner #if defined(ESP32) guards inside struct definitions
- [ ] #2 settings.json round-trips between ESP8266 and ESP32 binaries without loss of BLE fields (BLE fields are zero/empty on ESP8266 but present)
- [ ] #3 platform_esp8266.h declares inline no-op stubs for the full satBLE* API surface
- [ ] #4 All ESP32-feature-call sites listed in the task description no longer carry a raw ESP32 ifdef
- [ ] #5 BLE settings serialise/parse via platformSerializeBleSettings / platformParseBleSettings - settingStuff.ino has no inner BLE ifdef
- [ ] #6 python build.py --firmware exits 0 on both platforms
- [ ] #7 Guardrail violation count drops by at least another 15 from Tier 1 baseline
<!-- AC:END -->
