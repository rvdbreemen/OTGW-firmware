---
id: TASK-3011
title: >-
  SAT uses an unchosen BLE sensor (possibly a neighbour's) over the thermostat
  room temperature when no BLE MAC is pinned
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-21T15:38:00Z'
updated_date: '2026-11-21T16:39:00Z'
labels:
  - sat
  - ble
  - bug
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Step 1 (plan only, no code). Entry 451: BLE feeds SAT only from a user-chosen sensor; a primary room-temperature source with fallbacks to be considered.

Today (origin/dev): satGetRoomTemp() SATcontrol.ino:2774-2782 order = simulation > BLE (when bBleEnable and a fresh BLE value) > multi-area > MQTT external (satexternaltemp) > MsgID 24. BLE is on by default on PSRAM boards (SATtypes.h:637). With no pin, satBLEUpdateState() drives SAT from the first fresh roster slot (SATble.ino:866-867), and any nearby advertiser is admitted to the roster (SATble.ino:~578). The auto-select block pins the only fresh sensor for good (SATble.ino:777-797, updateSetting SATblemac). With a stale pin, bBleFailover (default true, SATtypes.h:639) falls back to another fresh roster sensor, also an unchosen one.

Change (all options): BLE drives SAT only from the pinned MAC (sBleMAC). Remove the unpinned 'first fresh slot' path and the auto-select pin. No pin = BLE is display/HA only.

Open (a) default source order. A: one setting satroomsource = thermostat | external | ble | areas (primary), fallback chain fixed: primary -> thermostat (MsgID 24) -> heating curve only (room lost, entries 38/46). B: an ordered list setting (e.g. ble,areas,external,thermostat; gruijter's idea), editable over MQTT. C: keep today's fixed order, only BLE needs a pin. Recommendation A: one choice the user understands, one fallback to the device the user owns, no list parsing/validation or UI for ordering (B is YAGNI until someone needs two fallbacks); default derived per install at migration (see c), fresh install = thermostat.

Open (b) failover default. Two levels. Source level (primary stale -> thermostat): recommend ON by default (thermostat is the user's own device; off means SAT runs on the curve only). BLE sensor-to-sensor failover (bBleFailover): recommend default OFF and only to sensors the user also chose; failing over to 'another fresh roster sensor' is exactly the neighbour problem of entry 451. Consequence of OFF: a pinned BLE sensor whose battery dies -> SAT uses the thermostat until it is back.

Open (c) migration. An auto-pinned MAC (SATble.ino:777-797) and a user-picked MAC are stored identically in sBleMAC: the firmware cannot tell them apart. A: keep every existing pin as a user choice (satroomsource=ble when sBleMAC set), BLE without a pin stops feeding SAT. B: clear every pin on upgrade, user re-picks. C: A plus a one-time notice (narration + web UI banner on the SAT page: 'SAT uses BLE sensor <name/MAC>, confirm or change'). Recommendation C: no silent change for the many single-sensor users whose pin is right, and a visible prompt for the few with a wrong (neighbour) pin; B silently drops users back to the thermostat. Persisted bBleFailover=true on existing installs: reset to the new default only if Robert picks OFF in (b) (one-time migration flag).

TASK-3015 (sat/room_temp_source): recommend the same change. satGetRoomTemp() then decides the source in one place and returning it with the value is a few lines; publishing it is what makes the failover of (b) visible to MQTT/HA. It is labelled needs-maintainer-ok: Robert must release it.

Scope after the answers: SATcontrol.ino (satGetRoomTemp + source enum), SATble.ino (remove unpinned path + auto-select), settings (satroomsource + migration), web UI SAT page (setting + banner, otgw-webui), MQTT/HA (setting command + state, 3015), tests red/green on the real functions, ADR (new setting + behaviour change of the room source).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21: released by Robert in #alpha-testing (msg 1573738563596591167), recorded as entry 451 in the private SAT decision register: BLE feeds SAT only from a user-chosen sensor; a primary-source setting with multiple fallbacks is to be considered. Open per entry 451: default source order, failover default, migration of existing installs.

2026-11-21 tester input (gruijter, msg 1573738963787714691, not a decision): make the source order MQTT-accessible, a command plus a retained state (e.g. sat/room_temp_sources = areas,external,thermostat), and the active source as its own topic (sat/room_temp_source, see TASK-3015). Design input for the open part of entry 451.
<!-- SECTION:NOTES:END -->
