---
id: TASK-493
title: >-
  fix(satble,mqttstuff): five high-priority code-review follow-ups
  (TASK-493..497)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-30 16:55'
labels:
  - esp32
  - ble
  - sat
  - otdirect
  - mqtt
  - code-review
  - follow-up
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Origin

Five fix-before-release findings from the comprehensive review of session
work `ace21a48..fa5ef3c5`. Bundled into one task because the changes are
small, interrelated, and verified by the same single ESP32 build cycle.

## Findings being addressed

- **1A-H1** (TASK-493): `bleSensorPublishHaDiscovery` returns void; the
  caller flips `bDiscoveryPublished = true` regardless of whether the
  helper actually succeeded. A transient first-scan failure (MQTT not
  yet connected, low heap) permanently stops HA discovery for that
  sensor until reboot or slot recycle. Fix: change helper to return
  `bool`, gate the flag flip on success.
- **1B-H1** (TASK-494): `platformio.ini` was auto-reformatted in
  commit 59b1478d, erasing about 90 lines of build-rationale comments
  and silently bumping `tool-esptoolpy` from `~1.30000.0` to
  `^2.41100.0`. Fix: restore the original prose-rich layout (171
  lines), keep the original esptoolpy pin, re-apply only the
  intentional change (NimBLE-Arduino dependency add).
- **2A-H1** (TASK-495): `setRemoteOverride()` does
  `(int16_t)(celsius * 256.0f)` with no clamp. Out-of-range floats are
  C/C++ undefined behaviour. Fix: add a 3-line range guard
  (`-40..127`) mirroring `otdMqttSetRoomSetpoint`'s existing pattern.
- **2B-H1** (TASK-496): `feedWatchDog()` is called only on the success
  path of `bleSensorPublishOneDiscovery` and the four-publish loop in
  `bleSensorPublishHaDiscovery`. The 16-publish first-scan burst
  (4 sensors × 4 configs) over PubSubClient's 15-second timeouts can
  starve the watchdog. Fix: feed the watchdog before every
  `return false` that follows a network attempt.
- **Cross-phase concurrency** (TASK-497): NimBLE 2.x scan callback
  runs on a separate FreeRTOS task on ESP32-S3 and writes
  `_bleSensors[]` while the loop task reads it (in
  `satBLEPublishMQTT` and `satBLEUpdateState`) without
  synchronisation. Three independent agents flagged this from
  different angles (architecture / security / performance).
  Performance impact is sub-microsecond; security and correctness
  matter. Fix: add a `portMUX_TYPE` and wrap the slot-write and
  slot-iteration regions in `portENTER_CRITICAL` / `portEXIT_CRITICAL`.

## Files touched

- `src/OTGW-firmware/MQTTstuff.ino` (1A-H1, 2B-H1)
- `src/OTGW-firmware/OTGW-firmware.h` (1A-H1 declaration update)
- `src/OTGW-firmware/SATble.ino` (1A-H1 caller gate, cross-phase MUX)
- `src/OTGW-firmware/OTDirect.ino` (2A-H1 clamp)
- `platformio.ini` (1B-H1 revert + NimBLE re-apply)

## Validation

- ESP32 build clean (incremental).
- ESP8266 build unchanged (BLE region inside `#if defined(ESP32)`,
  OTDirect region inside `#if HAS_DIRECT_OT`, platformio.ini ESP8266
  section restored to original).
- `python tests/check_otdirect_fixture.py` passes.
- evaluate.py adds zero new violations.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 1A-H1: `bleSensorPublishHaDiscovery` returns bool; caller in `satBLEPublishMQTT` only sets `bDiscoveryPublished = true` when the helper returns true
- [ ] #2 1B-H1: `platformio.ini` is restored to its 171-line prose-rich layout matching `git show 59b1478d^:platformio.ini`, with the NimBLE-Arduino dependency added at the correct position in `[env:esp32]` lib_deps; original esptoolpy pin (`~1.30000.0`) preserved
- [ ] #3 2A-H1: `setRemoteOverride` clamps `celsius` to `-40.0f..127.0f` before the f8.8 cast, mirroring `otdMqttSetRoomSetpoint`'s existing range guard
- [ ] #4 2B-H1: every `return false` path inside `bleSensorPublishOneDiscovery` and the four-helper loop in `bleSensorPublishHaDiscovery` calls `feedWatchDog()` before returning when the failure follows a network attempt
- [ ] #5 Cross-phase: a `portMUX_TYPE` is added in `SATble.ino`, the scan-callback slot-write region is wrapped in `portENTER_CRITICAL`/`portEXIT_CRITICAL`, and the slot-iteration regions in `satBLEPublishMQTT` + `satBLEUpdateState` are wrapped consistently
- [ ] #6 ESP32 build SUCCESS, ESP8266 build unchanged
- [ ] #7 evaluate.py: zero new violations
<!-- AC:END -->
