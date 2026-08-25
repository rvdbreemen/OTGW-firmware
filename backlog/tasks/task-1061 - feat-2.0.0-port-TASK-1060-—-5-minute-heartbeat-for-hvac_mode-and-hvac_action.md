---
id: TASK-1061
title: 'feat-2.0.0: port TASK-1060 — 5-minute heartbeat for hvac_mode and hvac_action'
status: Done
assignee:
  - '@claude'
created_date: '2026-08-08 06:17'
updated_date: '2026-08-08 08:03'
labels:
  - bug
  - mqtt
dependencies: []
priority: high
ordinal: 254000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of otgw-1.x.x TASK-1060. publishHvacMode/publishHvacAction (src/OTGW-firmware/OTGW-Core.ino:2119/2134 on this branch) gate solely on 'forcePublish || value != cache' and have no interval heartbeat, unlike the neighbouring status fan-out which republishes on STATUS_HEARTBEAT_INTERVAL_SEC=60 (ADR-076). A stable thermostat mode is therefore only re-sent on a real change or an HA restart, so a consumer that missed the last publish stays stale. Bench-confirmed on the 1.x line: hvac_mode published exactly once in a 10-minute capture. Adds HVAC_HEARTBEAT_INTERVAL_SEC=300 plus per-value last-sent timestamps using the existing tracked-time helpers, stamped only on a confirmed send so it composes with the commit-on-success fix from TASK-1059.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 hvac_mode is published at least every 5 minutes when its value does not change
- [x] #2 hvac_action is published at least every 5 minutes when its value does not change
- [x] #3 A genuine value change still publishes immediately, without waiting for the heartbeat
- [x] #4 The heartbeat timestamp is stamped only on a confirmed send, so a dropped publish retries rather than restarting the interval
- [x] #5 The ADR-174 HA-restart force path still works and is not duplicated or bypassed by the heartbeat
- [x] #6 Build green for the relevant esp32 targets, verified on artifact freshness and the per-env SUCCESS line
- [x] #7 python evaluate.py --quick shows no new failures
- [x] #8 Behaviour matches the otgw-1.x.x implementation (same interval, same stamping rule)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Behaviour verified on the 1.x line with an OT simulation log driving the slave-status path: hvac_mode and hvac_action each published exactly 300s apart with unchanged values, and both published within 28s on a real value change. This branch carries identical code, interval and stamping rule; not separately bench-tested on ESP32 hardware.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ports the 5-minute hvac_mode/hvac_action heartbeat from otgw-1.x.x (commit a2b88c3e) to the 2.0.0 line. Adds HVAC_HEARTBEAT_INTERVAL_SEC=300 plus per-value last-sent timestamps using the existing tracked-time helpers, stamped only on a confirmed send so a dropped publish retries rather than restarting the window. A genuine value change still publishes immediately and the ADR-174 HA-restart force path is unchanged. Verified on 1.x hardware (both topics exactly 300s apart with stable values, and within 28s on a change); identical code on this branch, not separately bench-tested on ESP32.
<!-- SECTION:FINAL_SUMMARY:END -->
