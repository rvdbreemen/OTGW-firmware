---
id: TASK-184
title: 'Fix: Add flame ratio tracking to OTGW32 OTDirect'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:32'
updated_date: '2026-04-08 23:08'
labels:
  - audit-fix
  - otgw32
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OT-Thing tracks flame on/off duty cycle (%) and flame cycle frequency (cycles/h) via a ring buffer (FlameRatio class). OTDirect has no equivalent. This metric is useful for HA dashboards and efficiency monitoring. Implementation should poll MsgID 0 slave status byte flame bit and maintain a rolling 60-minute window.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Flame duty cycle (%) is computed from MsgID 0 slave status flame bit over a rolling window
- [x] #2 Flame cycle frequency (cycles/h) is tracked
- [x] #3 Values are exposed via REST API and MQTT
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add FlameRatio static state (ring buffer, 180 slots x 60s = 3h window) in OTDirect.ino
2. Detect flame on/off from MsgID 0 slave status byte bit 3 in handleMasterResponse()
3. Call flameRatioLoop() every second, update duty and cycle buffers
4. Expose duty% and cycles/h via REST sendOTDirectStatus() and MQTT
5. Publish MQTT topics: otgw32/flame_duty_pct and otgw32/flame_cycles_per_hour
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added flame ratio tracking (duty cycle % and cycles/h) to OTGW32 OTDirect layer.

Changes:
- Added FlameRatioBuf ring buffer struct with 180-slot (3h) rolling window, matching OT-Thing FlameRatio class
- initFlameRatio() called from initOTDirect() to zero buffers
- flameRatioSet() detects flame on/off transitions; flameRatioAccum() accumulates on-time between calls
- loopFlameRatio() called every 1s from timerOTDStatus; advances ring buffer every 60s
- Flame state read from MsgID 0 boiler cache bit 3 (flame bit) in handleMasterResponse()
- getFlameRatioDuty() returns 0-100%; getFlameRatioFreq() returns cycles/h (float)
- Published to MQTT as otgw32/flame_duty_pct and otgw32/flame_cycles_per_hour every 60s
- Exposed via REST API sendOTDirectStatus() as flame_duty_pct and flame_cycles_per_hour
- Forward declarations added to OTGW-firmware.h
<!-- SECTION:FINAL_SUMMARY:END -->
