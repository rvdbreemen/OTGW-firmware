---
id: TASK-3011
title: >-
  SAT uses an unchosen BLE sensor (possibly a neighbour's) over the thermostat
  room temperature when no BLE MAC is pinned
status: To Do
assignee: []
created_date: '2026-11-21T15:38:00Z'
labels:
  - sat
  - ble
  - bug
  - needs-maintainer-ok
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1130000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by gruijter, #alpha-testing msg 1573723870945353759 (2026-11-21). Verified in code at origin/dev: BLE scans by default on PSRAM boards (SATtypes.h:637 bBleEnable = true). With no pin (sBleMAC empty) satBLEUpdateState() drives SAT from the first fresh roster slot (SATble.ino:866-867, 'no pin set: first fresh slot in roster order'), and the auto-select block pins the only fresh sensor permanently (SATble.ino:777-797). satGetRoomTemp() gives BLE the highest priority, above the MQTT external sensor and MsgID 24 (SATcontrol.ino:2774-2782). Also: bBleFailover defaults true (SATtypes.h:639), so even a pinned sensor going stale falls over to any fresh roster sensor. Effect: a neighbour's sensor in radio range can silently become SAT's room temperature. The name filter only gates ingestion when bBleNameFilterIngest is on (default false, SATtypes.h:650). Tester suggestion: only use BLE once a sensor is chosen. Behaviour change, so Robert decides (tester request).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Decision recorded (Robert): BLE feeds SAT only from an explicitly chosen sensor, or another rule; including what happens to auto-select and failover
- [ ] #2 Host test on the real satBLEUpdateState()/satGetRoomTemp(): with no pin and a fresh unknown sensor, SAT keeps the thermostat/MQTT room (red on old code, green on fix)
- [ ] #3 Web UI / release notes say how to choose the BLE sensor
<!-- AC:END -->
