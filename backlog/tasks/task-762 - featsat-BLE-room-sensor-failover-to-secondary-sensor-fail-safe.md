---
id: TASK-762
title: 'feat(sat): BLE room-sensor failover to secondary sensor (fail-safe)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-29 17:17'
updated_date: '2026-05-29 17:52'
labels:
  - sat
  - ble
  - esp32
  - feature
  - field-report
milestone: 2.0.0
dependencies: []
references:
  - 'src/OTGW-firmware/SATcontrol.ino:969'
  - 'src/OTGW-firmware/SATcontrol.ino:978'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Feature request from @sergeantd (2026-05-29): "use another temperature sensor as a fallback. So if the first one fails the firmware will use the second one. Like a fail-safe mechanism."

Current behaviour: satGetRoomTemp() (SATcontrol.ino:978-986) uses only the single active BLE source (state.sat.bBleTempValid / fBleTemp). After SAT_STALE_TEMP_BLE_MS (5 min) staleness it abandons the BLE tier entirely and falls through to external MQTT temp, then MultiArea, then OT-bus MsgID 24, then NAN. With multiple healthy roster sensors present (George runs 5), it does NOT hop to a second BLE sensor before leaving the BLE tier.

Goal: when the active BLE room sensor goes stale/unhealthy and another roster sensor is fresh, automatically promote the best alternate BLE sensor as the room-temp source (fail-safe), and recover to the primary when it returns. Keep MultiArea weighting priority intact when enabled.

2.0.0-only (SAT BLE is ESP32/OTGW32). No cross-worktree port.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 When the active BLE sensor exceeds the staleness/health threshold and another roster sensor is fresh, satGetRoomTemp() returns the alternate BLE sensor temp instead of skipping straight to external/OT/NAN
- [ ] #2 Alternate-sensor selection is deterministic and documented (e.g. freshest healthy sensor, or explicit roster priority order)
- [ ] #3 Failover and recovery (primary sensor returns) are logged via SATDebug
- [ ] #4 No behaviour change when MultiArea weighting is enabled (MultiArea path keeps its existing priority over single-sensor logic)
- [ ] #5 python build.py green (ESP32 + ESP8266 no-op); python evaluate.py --quick no new failures
- [ ] #6 Field-validated by @sergeantd: powering down the primary BLE sensor causes SAT to continue with a secondary sensor rather than losing room input
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Mechanism (SATble.ino:483-538 satBLEUpdateState): the active room sensor is chosen here. If settings.sat.sBleMAC is EMPTY it already uses the first fresh roster slot (implicit failover works). If a sensor is PINNED (sBleMAC set, via the UI "Select" button) only that MAC feeds state.sat.fBleTemp; when it goes stale (>BLE_STALE_MS 5min) bBleTempValid=false and SAT loses room input -- no failover. George pinned "Living Room", so he hit this.

Proposed change (in satBLEUpdateState, two-pass):
1. Prefer the pinned sensor when it is fresh (current behaviour; gives auto-recovery for free).
2. If pinned is stale/absent, fall back to the best OTHER fresh roster slot (per selection metric) instead of going invalid. Log the failover (and the recovery).
3. No pinned sensor set -> unchanged.
Gate with a setting so strict-pin users (multi-room) can opt out.

OPEN DESIGN DECISIONS (asked of user before coding):
A. Selection metric for the fallback slot.
B. Whether to add a bBleFailover toggle and its default.
No MultiArea change (that path keeps priority).
<!-- SECTION:PLAN:END -->
