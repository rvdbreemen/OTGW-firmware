---
id: TASK-16
title: Fix MQTT status first publish and reconnect republish
status: Done
assignee: []
created_date: '2026-03-17 17:36'
updated_date: '2026-03-17 17:42'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Fix the OT status MQTT regression where status bits such as flame can stay stale because first/all-zero status frames and reconnects are suppressed by throttle state. Preserve current topic contracts while forcing a clean first publish on boot and after WiFi/MQTT reconnect.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Master and slave status publishing compares against a stable previous-status snapshot instead of repeatedly reading mutable global status during the publish loop.
- [x] #2 The first observed OT status frame after startup publishes status_master, status_slave, and all status-bit topics even when the values are all OFF/zero and the MQTT interval is non-zero.
- [x] #3 After WiFi or MQTT reconnect, the next observed OT status frame republishes status_master, status_slave, and all status-bit topics immediately without waiting for the normal interval gate.
- [x] #4 Existing MQTT topic names and payload contracts remain unchanged, including flame as ON/OFF.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add a dedicated one-shot status republish request path for boot and MQTT reconnect.
2. Fix master/slave status publishing to compare against stable previous-status snapshots.
3. Make the next observed status request/response bypass both the outer msgid throttle and the inner per-bit throttle, then clear the force flags.
4. Validate with error checks and a firmware build, then record the outcome in the task.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- Added one-shot master/slave status republish flags and a requestMQTTStatusRepublish() hook.
- Wired the hook into successful MQTT connect so the next status request and next status response republish immediately after startup or reconnect.
- Switched status-bit comparisons to stable previous-status snapshots to avoid stale reads across publish and yield windows.
- Verified the change with python build.py --firmware.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed stale MQTT status-bit publishing for OT status topics, including flame.

Changes:
- Added a dedicated status republish hook that forces the next master and slave status frames through both the outer OT message throttle and the inner per-bit throttle.
- Triggered that hook on successful MQTT connect so startup and WiFi or MQTT reconnects immediately refresh status_master, status_slave, and all derived ON/OFF topics.
- Changed master/slave status publishing to compare against stable status snapshots instead of repeatedly reading mutable global state during the publish loop.

Validation:
- python build.py --firmware

Compatibility:
- Existing MQTT topic names and ON/OFF payload contracts are unchanged.
<!-- SECTION:FINAL_SUMMARY:END -->
