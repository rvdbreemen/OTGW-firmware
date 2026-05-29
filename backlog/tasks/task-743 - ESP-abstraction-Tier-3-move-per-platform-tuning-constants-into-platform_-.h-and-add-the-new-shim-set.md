---
id: TASK-743
title: >-
  ESP abstraction Tier 3: move per-platform tuning constants into platform_*.h
  and add the new shim set
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-28 08:28'
updated_date: '2026-05-29 22:32'
labels:
  - esp-abstraction-audit
  - refactor
dependencies: []
ordinal: 70000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
(a) Move per-platform sizing/timing constants out of application .ino files into platform_esp8266.h / platform_esp32.h: SAT_WIN4H_SIZE (SATtypes.h:100), SAT_FLOW_SAMPLE_SIZE (SATcycles.ino:62), flow index types (SATcycles.ino:68/164), SAT_TAIL_SAMPLE_SIZE (SATcycles.ino:79), HCR_DAYS (SATcycles.ino:139), HCR_INTRADAY_SIZE (SATcycles.ino:146), SAT_CYCLES_FILE_BUF_SIZE (SATcycles.ino:1115), MQTT_DISCOVERY_HEAP_MIN (MQTTstuff.ino:57), STATUS_BURST_COOLDOWN_MS (MQTTstuff.ino:136). (b) Add new platform shims and replace their callers: platformSetLed/platformBlinkLed (OTGW-firmware.ino:247-307 LED block), platformRestTxAppend/Reset/Flush/AppendProgmem (jsonStuff.ino:191-263), platformHeapHasPressure/platformHeapIsHealthyForRestore (MQTTstuff.ino:1898-1924), platformWiFiIsEncrypted (restAPI.ino:1942), platformWiFiClientConfigureSync (MQTTstuff.ino:588), platformStartNTP (networkStuff.ino:610-624 hostname workaround), platformGetResetReasonChar (OTDirect.ino:3076), platformPicSerialBegin (OTGWSerial.cpp:835-847), platformRandom (SATmqttPublish.h:46/63), platformGetMacU64 (Ethernet.ino:42, optional symmetry). Depends on Tier 2.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 All tuning constants listed are defined in platform_*.h, not in application .ino files
- [ ] #2 All new shims in the (b) list are declared in both platform_esp8266.h and platform_esp32.h with appropriate per-platform bodies (stub or real)
- [ ] #3 All caller sites listed are rewritten to call the shim and contain no ESP-platform ifdef
- [ ] #4 OTGWSerial library constructor uses platformPicSerialBegin and has no #if defined(ESP8266)/(ESP32)
- [ ] #5 python build.py --firmware exits 0 on both platforms
- [ ] #6 Guardrail violation count drops to under 5 (board-macro references in boards.h dispatch logic only)
<!-- AC:END -->
