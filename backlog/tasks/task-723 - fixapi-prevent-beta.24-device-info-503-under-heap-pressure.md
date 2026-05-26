---
id: TASK-723
title: 'fix(api): prevent beta.24 device-info 503 under heap pressure'
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-26 20:18'
updated_date: '2026-05-26 20:36'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Hardware capture log-issues/OTGW_1.6_beta_24.txt from firmware 1.6.0-beta.24+a16606a records six REST GET /api/v2/device/info => 503 responses plus cumulative WS_drops=18 and MQTT_drops=72 during the capture. TASK-701 is Done but did not define acceptance criteria for eliminating device-info failures. Diagnose and apply the smallest justified fix for the remaining endpoint failure, and assess whether the concurrent drop growth is causal or diagnostic load.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Document beta.24 reproduction evidence and root cause for /api/v2/device/info returning 503.
- [ ] #2 Apply a minimal fix so the identified device-info failure path no longer returns 503 under the reproduced heap-pressure condition.
- [x] #3 Assess the observed MQTT/WebSocket drop growth and record whether additional remediation is required.
- [x] #4 Run required validation including firmware plus filesystem artifact build in one validation pass and relevant evaluator/tests.
- [ ] #5 Record branch, coding agent, implementation notes, and final summary before completion.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Analyseer beta.24 capture tegenover de beta.24 heap- en REST-guards.
2. Herijk alleen de device/info contiguous-block precheck naar de werkelijk benodigde streaming-headroom.
3. Bouw firmware en LittleFS samen en voer de snelle evaluator uit.
4. Laat hardwarevalidering op beta.25 bevestigen dat device/info onder vergelijkbare druk geen onterechte 503 meer teruggeeft.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-26 @codex on branch dev: analysed `log-issues/OTGW_1.6_beta_24.txt` from `1.6.0-beta.24+a16606a`. Evidence: six `REST GET /api/v2/device/info => 503` responses; lifetime counters reach `WS_drops=18` and `MQTT_drops=72`. `beta.20` comparison had throttle events but no device/info 503 in its capture.

Root cause: `sendDeviceInfoV2()` added `DEVICE_INFO_MIN_HEAP_BLOCK 8192` as an endpoint-only preflight guard. That guard is disproportionate after TASK-701: `processAPI()` already rejects any REST request below 4096 bytes free heap; JSON responses use a static 512-byte coalescing buffer; existing fragmentation risk guard uses 1536 bytes as one pbuf-sized contiguous floor. Thus transient maxBlock below 8192 returns 503 even without evidence the streamed response cannot run.

Implementation: lower only `DEVICE_INFO_MIN_HEAP_BLOCK` from 8192 to 1536 and document the alignment in `restAPI.ino`. Leave `HEAP_LOW/WARNING/CRITICAL` MQTT/WebSocket backpressure thresholds unchanged: lowering those would postpone protection and increase allocation risk. Bumped prerelease metadata once from beta.24 to beta.25; default build synchronized generated version headers/assets.

Throttle assessment: drops are a real observation but not proof that the MQTT/WebSocket thresholds are too aggressive. The beta.24 capture enables REST, MQTT and MQTTGate debug near the start, and drop events precede the first device/info 503. A post-flash beta.25 capture is still required to decide whether independent throttle remediation is justified.

Validation: `.\.venv\Scripts\python.exe build.py` passed and built both firmware and LittleFS (`OTGW-firmware-1.6.0-beta.25+7cfe4a0.ino.bin`, `OTGW-firmware.1.6.0-beta.25+7cfe4a0.littlefs.bin`); firmware reported 742224 bytes flash and 23092 bytes available dynamic memory. `.\.venv\Scripts\python.exe evaluate.py --quick --no-color` passed: 34 passed, 0 warnings, 0 failed, 2 info. Build completion emitted a non-blocking Windows cleanup warning for a locked `.tmp\echarts\.git\objects\pack` file after artifacts were created.

Blocker for Done: hardware validation on beta.25 under comparable UI/debug load must show the `device/info` 503 symptom is resolved and reveal whether drops remain actionable.
<!-- SECTION:NOTES:END -->
