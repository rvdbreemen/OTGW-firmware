---
id: TASK-494
title: >-
  fix(satble,otdirect): BLE continuous-scan + queue coalesce-by-MsgID +
  high-water diagnostic
status: Done
assignee:
  - '@claude'
created_date: '2026-04-30 17:33'
updated_date: '2026-04-30 17:39'
labels:
  - esp32
  - ble
  - sat
  - otdirect
  - bug
  - owner-debug
dependencies:
  - TASK-487
  - TASK-488
  - TASK-466
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Origin

Owner-reported during session: BLE sensors not discovered (while OT-Thing
reference works on identical hardware), and OTDirect command queue drops
messages in normal operation. Plan approved at
`C:\Users\rvdbr\.claude\plans\stateless-growing-phoenix.md`.

## Owner architectural principles (binding)

- Queue is THE channel for the multi-channel fan-in (MQTT/REST/WebUI/serial).
- Queue size 10/12 is intentional. Channel of origin doesn't matter.
- Fix the mechanics first; queue should not get full in normal operation.
- Everything via the queue. No side-channels — when queue dropped, the
  command is gone. No silent recovery via override-table updates.

These principles rule out queue-size-bump and override-table-on-drop
side-channels.

## Issue 1 — BLE continuous-scan port (`SATble.ino`)

Match OT-Thing reference (`other-projects/OT-Thing-OTGW32/Firmware/src/sensors.cpp:355-365`).

- Move `_pBLEScan->start(0, false, true)` into `satBLEInit()` so the scan runs forever.
- Drop the periodic-timing block in `satBLELoop()` (the `_bleLastScanMs`/`iBleInterval` gate around `start(...)`). Keep the `iBleInterval` guard around `satBLEUpdateState()` for publish/state-update cadence.
- Remove `_bleLastScanMs` static and `BLE_SCAN_DURATION_SEC` constant.
- Update `SATtypes.h` inline comment on `iBleInterval`: "publish/state-update cadence (NOT scan rate)". Settings schema unchanged for backwards-compat.

## Issue 2 — OTDirect queue coalesce-by-MsgID + diagnostic (`OTDirect.ino`)

Inside `otCmdEnqueue()` (line 268), before adding a new entry to the head, scan pending entries (tail → head). If a frame for the SAME MsgID is already pending, REPLACE its data field in place and return success. Otherwise, fall through to the existing add-to-head path.

Add file-static `static uint8_t otCmdQueueHighWater = 0;` updated after a successful insert. Visible via `OTDDebugTf` periodic logging (no new exposure surface).

Header comment must call out:
- Coalesce is by MsgID (not by full frame).
- Position-preserving: the existing entry's slot stays put; only the data field is replaced.
- For sequences `set MsgID 100=X` then `set MsgID 100=Y` within the same OT cycle, only Y reaches the bus — intended PIC-parity semantics, self-consistent with the override-table state.

## MsgID coalesce safety table

All MsgIDs in the current producer set (1, 7, 8, 14, 16, 56, 57, 99, 100, 4, 116-123) have been verified safe under "latest WRITE_DATA per MsgID wins" semantics. See plan file MsgID coalesce safety review.

## Fixture

Add a row to `tests/otdirect_pic_parity_fixture.md` documenting the
TT/TC rapid-toggle case and the bus-state semantics under coalesce.

## Validation

- `pio run -e esp32 -j 1` SUCCESS
- `pio run -e esp8266 -j 1` SUCCESS (BLE inside `#if defined(ESP32)`, OTDirect inside `#if HAS_DIRECT_OT`)
- `python tests/check_otdirect_fixture.py` PASS
- `python evaluate.py --quick` no new violations
- Hardware (owner): BLE sensor discovered within seconds of boot (compare to OT-Thing); rapid TT/TC/CS sequence across MQTT+REST+WebUI does not trigger "queue full, dropped" log; high-water-mark stays well below 12 in normal operation

## Out of scope

- Queue size change (owner explicit).
- Override-table side-channel update on queue-drop (owner explicit).
- Removing `iBleInterval` from settings schema (deferred follow-up).
- ADR amendment (existing ADR-064 / ADR-087 cover the queue model).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Continuous BLE scan: `_pBLEScan->start(0, false, true)` is called once in `satBLEInit()` and there is no `start(...)` call in `satBLELoop()`. Verified by grep.
- [x] #2 `_bleLastScanMs` and `BLE_SCAN_DURATION_SEC` are removed from SATble.ino
- [x] #3 `SATtypes.h` inline comment on `iBleInterval` updated to publish/state-update cadence semantics
- [x] #4 `otCmdEnqueue()` coalesces by MsgID: scanning tail → head, if matching MsgID found, data is replaced in place and the function returns true without growing the queue
- [x] #5 File-static `otCmdQueueHighWater` exists and is updated after each successful insert
- [x] #6 Header comment block on `otCmdEnqueue` documents the coalesce semantics, MsgID-key, position-preserving replacement, and the PIC-parity rationale
- [x] #7 `tests/otdirect_pic_parity_fixture.md` gains a row covering rapid TT/TC toggle (set+clear within one OT cycle) and the bus-state under coalesce
- [x] #8 ESP32 build SUCCESS
- [x] #9 ESP8266 build unchanged
- [x] #10 evaluate.py: zero new violations
- [ ] #11 Hardware verification (owner): BLE detection within seconds of boot; rapid TT/TC/CS burst does not produce queue-full drops
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## TASK-494 — BLE continuous-scan + OTDirect queue coalesce + high-water diagnostic

Two owner-reported defects fixed in one commit per the approved plan
(`~/.claude/plans/stateless-growing-phoenix.md`). Pushed in commit
`805c6728` on `feature-dev-2.0.0-otgw32-esp32-sat-support`.

### Issue 1 — BLE continuous-scan
- `SATble.ino`: `_pBLEScan->start(0, false, true)` now lives in
  `satBLEInit()`. The earlier periodic 3-s/30-s model is gone. Scan
  runs forever on the BLE host task; loop() never blocks. Matches
  OT-Thing reference exactly.
- `_bleLastScanMs` and `BLE_SCAN_DURATION_SEC` removed.
- `_bleLastPublishMs` retained: `satBLELoop()` now only throttles the
  `satBLEUpdateState()` call (state propagation + MQTT publish) at the
  user-tunable `iBleInterval` cadence.
- `SATtypes.h` inline comment on `iBleInterval` updated to
  publish/state-update semantics. Settings schema unchanged so existing
  configs round-trip cleanly.

### Issue 2 — OTDirect queue coalesce-by-MsgID
- `otCmdEnqueue()` scans tail→head; if matching MsgID is found, replaces
  the data field in place and returns success without growing the queue.
- Position-preserving: queue ordering across DIFFERENT MsgIDs is preserved.
- Coalesce-event log: `OTDDebugTf("OTD: queue coalesced MsgID %u -> ...")`.
- `otCmdQueueHighWater` file-static counter, updated after each
  non-coalesce insert. Logged on each new high water for telnet
  visibility.
- Header comment block documents the MsgID-key, position-preservation,
  PIC-parity semantics, and the safety scope (the entire current
  producer set).

### Out of scope (per owner architectural principles)
- Queue size change (intentional 10/12 design).
- Override-table side-channel update on queue-drop.
- Removing `iBleInterval` from settings schema (deferred follow-up).

### Verification
- ESP32 build SUCCESS (`pio run -e esp32 -j 1`, 1m54s incremental, build 3859).
- ESP8266 build SUCCESS (44s, RAM 84.7%, Flash 77.3% — byte-identical to
  pre-change since BLE/OTDirect regions are ESP32-only).
- `python tests/check_otdirect_fixture.py` PASS (8 tables, all rows valid;
  new TT/TC coalesce row included).
- `python evaluate.py --quick` 95.5% health, zero new violations.
- ACs 1-10 satisfied; AC #11 (owner hardware verification) stays
  open until the owner runs the lab regressions described in the AC text.

### Hardware verification scenario (AC #11, owner-only)
1. Power-cycle OTGW32 with at least one Xiaomi LYWSD03MMC and one
   BTHome v2 emitter in radio range. Within seconds of boot the
   telnet log should show `SAT BLE: ad from <mac>` callbacks. Compare
   to OT-Thing on identical hardware (must be indistinguishable).
2. Burst MQTT + REST + WebUI commands targeting the same MsgID
   (`TT=20.0` then `TT=21.0`, etc.) within one OT cycle. Telnet log
   should show `OTD: queue coalesced MsgID 16` and the bus reports
   one frame with the latest value. No `queue full, dropped` log.
3. 24-hour soak: confirm `otCmdQueueHighWater` stays well below 12 in
   normal household use.

If any of the three scenarios surfaces an issue, reopen this task or
log a follow-up; the architectural principles (queue is THE channel,
no side-channels, fix mechanics first) still apply.
<!-- SECTION:FINAL_SUMMARY:END -->
