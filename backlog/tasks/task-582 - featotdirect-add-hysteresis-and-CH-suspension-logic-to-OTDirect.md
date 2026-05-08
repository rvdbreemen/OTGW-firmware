---
id: TASK-582
title: 'feat(otdirect): add hysteresis and CH suspension logic to OTDirect'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-08 16:00'
updated_date: '2026-05-08 16:52'
labels:
  - otdirect
  - otgw32
  - control
  - sat
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OT-Thing-OTGW32 implements per-channel hysteresis in its CH control loop: when room temperature exceeds (setpoint + hysteresis), central heating is suspended; it resumes when room drops below (setpoint - hysteresis). This prevents short-cycling on mild days. The 2.0.0 OTDirect layer has no equivalent -- it relies on the SAT thermostat if enabled, but standalone OT-Direct mode (without SAT) has no protection. Add a configurable hysteresis deadband to the OTDirect CH control path.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OTDirect CH control respects a configurable hysteresis deadband (default 0.5°C, range 0.0-2.0°C) stored in settings.otd
- [x] #2 When room temperature >= (CH setpoint + hysteresis/2), CH is suspended (MsgID 0 master CH-enable bit cleared)
- [x] #3 When room temperature <= (CH setpoint - hysteresis/2), CH is resumed
- [x] #4 Hysteresis is only active when a valid room temperature source is available (OT MsgID 24 or external)
- [x] #5 Suspension state is published to MQTT (otgw32/ch_suspended) and visible in /api/v2/otdirect/status
- [x] #6 SAT mode bypasses this logic (SAT manages its own hysteresis)
- [x] #7 Setting bHysteresisEnable=false disables the feature entirely (backward-compat default)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add bCHSuspended to OTDirectSection (state.otd) in OTDirecttypes.h\n2. Add bHysteresisEnable + fHysteresis to OTDirectSettingsSection in OTDirecttypes.h\n3. Implement loopCHHysteresis() in OTDirect.ino with SAT bypass, cache validity guard\n4. Call loopCHHysteresis() every 10s via DECLARE_TIMER_SEC in doOTDirectLoop()\n5. Persist settings via settingStuff.ino (write/updateSetting handlers)\n6. Add ch_suspended to sendOTDirectStatus() in restAPI.ino (one-line targeted addition)\n7. Add hysteresis settings to GET/POST handlers in restAPI.ino
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added configurable CH hysteresis deadband to OTDirect control loop.

Changes:
- OTDirecttypes.h: added bCHSuspended to OTDirectSection (runtime state); added bHysteresisEnable+fHysteresis to OTDirectSettingsSection (persisted settings)
- OTDirect.ino: new loopCHHysteresis() function; called every 10s via DECLARE_TIMER_SEC; guards for SAT active, bHysteresisEnable, valid room temp cache
- OTDirect.ino: publishes 'otgw32/ch_suspended' MQTT topic (retained) on state transitions
- restAPI.ino: added ch_suspended field to sendOTDirectStatus(); added hysteresis settings to GET/POST handlers
- settingStuff.ino: write + updateSetting handlers for OTDhysteresisenable and OTDhysteresis

Behavior: when room temp >= setpoint+deadband/2, MsgID 0 CH-enable bit cleared; when room temp <= setpoint-deadband/2, CH-enable restored. Default bHysteresisEnable=false preserves existing behavior. SAT active bypasses the logic entirely.
<!-- SECTION:FINAL_SUMMARY:END -->
