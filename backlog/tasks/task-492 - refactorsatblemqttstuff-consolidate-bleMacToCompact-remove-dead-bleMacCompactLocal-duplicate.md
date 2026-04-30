---
id: TASK-492
title: >-
  refactor(satble,mqttstuff): consolidate bleMacToCompact, remove dead
  bleMacCompactLocal duplicate
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-30 05:52'
labels:
  - esp32
  - ble
  - sat
  - mqtt
  - refactor
  - cleanup
dependencies:
  - TASK-487
  - TASK-488
  - TASK-489
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Problem

The TASK-487 + TASK-488 parallel-agent execution left two implementations
of the same 25-line MAC-compact helper:

- `bleMacToCompact()` in `MQTTstuff.ino` — file-static, originally
  intended as the canonical helper for callers. **Currently dead**:
  the only references are the comment block at the top of the BLE
  helpers region and the function definition itself; no callsite
  invokes it.
- `bleMacCompactLocal()` in `SATble.ino` — file-static, a near-verbatim
  copy that I added during TASK-488 caller wiring because the original
  was file-static in the off-limits MQTTstuff.ino territory at agent
  dispatch time.

Now that the parallel boundary is gone, the duplication is pure
cruft. Same algorithm, two definitions, one of them never called.

## Fix

Promote `bleMacToCompact()` to non-static (exported), declare in
`OTGW-firmware.h` next to the other BLE helpers, and remove
`bleMacCompactLocal()` from `SATble.ino`. The caller in
`satBLEPublishMQTT()` switches to `bleMacToCompact()`. One canonical
implementation, header-declared, no death code.

## Validation

- ESP32 build clean (incremental).
- ESP8266 build unchanged (entire BLE helper region is
  `#if defined(ESP32)`).
- The comment block in MQTTstuff.ino describing caller wiring stays
  accurate — it already named `bleMacToCompact`.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 `bleMacToCompact` in MQTTstuff.ino is no longer file-static and is declared in OTGW-firmware.h alongside `bleSensorPublishStateTopics` / `bleSensorPublishHaDiscovery`
- [ ] #2 `bleMacCompactLocal` is removed from SATble.ino
- [ ] #3 `satBLEPublishMQTT()` calls the canonical `bleMacToCompact`
- [ ] #4 ESP32 build clean, ESP8266 build unchanged
- [ ] #5 No new dead code introduced
<!-- AC:END -->
