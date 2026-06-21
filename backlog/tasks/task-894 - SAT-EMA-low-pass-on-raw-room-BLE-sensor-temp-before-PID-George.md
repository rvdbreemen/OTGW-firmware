---
id: TASK-894
title: 'SAT: EMA low-pass on raw room/BLE sensor temp before PID (George)'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-21 08:49'
updated_date: '2026-06-21 08:56'
labels: []
milestone: 2.0.0
dependencies: []
references:
  - src/OTGW-firmware/SATcontrol.ino
priority: high
ordinal: 118000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
George (#dev-sat-mqtt 2026-06-21 08:35): 'Yes implement it. Leave the pid untouched and add a filter to the raw sensor data.' Tames the D-term jitter (0->7 on stable temp) at the SOURCE so P, I and D all see a smooth signal; keeps the PID byte-identical to pid.py (pid.py is fed an already-smoothed HA sensor entity, our firmware feeds the raw BLE reading). Light EMA (~30-60s time constant) on the raw room temperature before it reaches the control loop / PID.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Add an EMA low-pass on the raw room/BLE sensor temperature applied at the single ingestion chokepoint before satPidUpdate; PID core untouched (George: leave pid untouched).
- [x] #2 EMA time constant ~30-60s (room temp is slow; negligible real lag); cold-start seeds from first raw reading (no ramp-from-zero).
- [x] #3 build esp32 exit 0; evaluate.py --quick 0-fail; prerelease bumped; pushed.
- [ ] #4 Report to George on #dev-sat-mqtt.
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added a time-based EMA low-pass (tau ~45s) on the raw room/BLE sensor temperature at the satControlLoop chokepoint (SATcontrol.ino ~4257, right after the NaN/range/skip validation), so the controller sees a smooth signal and the PID D-term stops swinging on a stable temperature (George's 0->7 report). PID core untouched (George: 'leave the pid untouched, add a filter to the raw sensor') -> stays byte-identical to pid.py, which is fed an already-smoothed HA sensor entity. Time-based EMA is robust to variable control cadence and self-heals after a source gap (large dt -> alpha ~1 -> snaps to raw). Seeds from the first valid reading (no ramp-from-zero). sat/room_temp MQTT still publishes the raw satGetRoomTemp() reading. esp32 SUCCESS (fw+fs), evaluate.py --quick 0-fail. AC#4 (report to George) on commit.
<!-- SECTION:FINAL_SUMMARY:END -->
