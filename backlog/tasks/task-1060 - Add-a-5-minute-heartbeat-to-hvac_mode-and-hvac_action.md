---
id: TASK-1060
title: Add a 5-minute heartbeat to hvac_mode and hvac_action
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-08 06:09'
updated_date: '2026-08-08 08:02'
labels:
  - bug
  - mqtt
dependencies: []
priority: high
ordinal: 171000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench-confirmed gap found while validating TASK-1058 on otgw1.local (1.7.3-beta.2): hvac_mode published exactly ONCE in a 10-minute capture, and only because the ADR-088 HA-restart force fired. publishHvacMode/publishHvacAction (OTGW-Core.ino:1654/1671) gate solely on 'forcePublish || value != cache' and have no interval heartbeat, unlike every neighbouring status topic which republishes on STATUS_HEARTBEAT_INTERVAL_SEC=60 via shouldPublishTrackedStatusByte/Bit (ADR-076 per-slot heartbeat). Consequence: a stable thermostat mode is re-sent only on a real change or an HA restart, so any consumer that missed the last publish stays stale indefinitely. Maintainer requirement: publish both topics at least every 5 minutes when the value does not change. Implementation follows the existing tracked-time pattern (currentTrackedSeconds/elapsedTrackedSeconds, TRACKED_TIME_UNSEEN sentinel, TRACKED_TIME_MODULUS=65535 giving a ~18.2h window), adding a per-value last-sent timestamp stamped only on a confirmed send so it composes with the commit-on-success fix from TASK-1058.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 hvac_mode is published at least every 5 minutes when its value does not change
- [x] #2 hvac_action is published at least every 5 minutes when its value does not change
- [x] #3 A genuine value change still publishes immediately, without waiting for the heartbeat
- [x] #4 The heartbeat timestamp is stamped only on a confirmed send, so a dropped publish retries rather than resetting the interval
- [x] #5 The ADR-088 HA-restart force path still works and is not duplicated or bypassed by the heartbeat
- [x] #6 python build.py --firmware exits 0, verified on artifact freshness and the success line
- [x] #7 python evaluate.py --quick shows no new failures
- [x] #8 Verified on device: two consecutive heartbeat publishes observed roughly 5 minutes apart with a stable value and no reboot
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Bench-verified on otgw1.local (192.168.88.16, ESP8266, fw 1.7.3-beta.2+3d66b20):
- hvac_mode published 08:21:07 and 08:26:07, exactly 300s apart, value unchanged ("off"), no reboot. Heartbeat lands on interval.
- Before the change, the same device published hvac_mode exactly ONCE in a 10-minute capture, and only because the ADR-088 HA-restart force fired.
- AC#2 (hvac_action) NOT verified on hardware: publishSlaveStatusState was called 0 times because the boiler does not answer MsgID 0 on this bench, despite 622 boiler frames. Same code path and same helper as hvac_mode, but untested on device. Needs a bench with a responding boiler or the simulator.

Simulation run on otgw1.local closes the remaining gap:
- Uploaded a 2-line steady-state /otgw_simulation.log (T00000200 master ch_enable + B40000202 slave centralheating), replayed at 750ms. publishSlaveStatusState went from 0 to 174 calls, so the previously-dead slave path now runs.
- hvac_mode  09:56:03 -> 10:01:03 = exactly 300s, value "heat" unchanged.
- hvac_action 09:56:06 -> 10:01:06 = exactly 300s, value "heating" unchanged.
- Change-driven path also proven: simulation stopped 10:01:41, thermostat 30s timeout fired, both topics published "off" at 10:02:09 (28s later), well before the 300s window would have elapsed at 10:06. A real change still preempts the heartbeat.
AC#4 (stamp only on confirmed send) remains inspection-only: forcing a sendMQTTData failure on demand is not practical on the bench.
<!-- SECTION:NOTES:END -->
