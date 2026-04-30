---
id: TASK-492
title: >-
  refactor(satble,mqttstuff): consolidate bleMacToCompact, remove dead
  bleMacCompactLocal duplicate
status: Done
assignee:
  - '@claude'
created_date: '2026-04-30 05:52'
updated_date: '2026-04-30 05:55'
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
- [x] #1 `bleMacToCompact` in MQTTstuff.ino is no longer file-static and is declared in OTGW-firmware.h alongside `bleSensorPublishStateTopics` / `bleSensorPublishHaDiscovery`
- [x] #2 `bleMacCompactLocal` is removed from SATble.ino
- [x] #3 `satBLEPublishMQTT()` calls the canonical `bleMacToCompact`
- [x] #4 ESP32 build clean, ESP8266 build unchanged
- [x] #5 No new dead code introduced
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## TASK-492 — Consolidate bleMacToCompact, drop dead bleMacCompactLocal

Cleanup of the parallel-agent residue left by TASK-487 / TASK-488. Two
implementations of the same 25-line MAC-compact helper existed:
`bleMacToCompact` (file-static, dead) in MQTTstuff.ino and
`bleMacCompactLocal` (file-static, used) in SATble.ino. After this
refactor only the canonical `bleMacToCompact` remains, exported via
`OTGW-firmware.h` and called from `satBLEPublishMQTT()`.

### Changes
- `src/OTGW-firmware/MQTTstuff.ino`: drop `static` from
  `bleMacToCompact`; comment block updated to reflect that the helper
  is exported.
- `src/OTGW-firmware/OTGW-firmware.h`: add forward declaration for
  `bleMacToCompact` alongside `bleSensorPublishStateTopics` and
  `bleSensorPublishHaDiscovery`.
- `src/OTGW-firmware/SATble.ino`: remove `bleMacCompactLocal`
  (was a 25-line duplicate); call site now uses `bleMacToCompact`.

### Verification
- ESP32 build SUCCESS (`-j 1`, 1m48s incremental).
- ESP8266 build unaffected (BLE helper region is `#if defined(ESP32)`).
- No new dead code; no behaviour change.

Pushed in commit on `feature-dev-2.0.0-otgw32-esp32-sat-support`.
<!-- SECTION:FINAL_SUMMARY:END -->
