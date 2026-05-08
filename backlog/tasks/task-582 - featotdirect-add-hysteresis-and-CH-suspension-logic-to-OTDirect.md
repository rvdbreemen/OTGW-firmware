---
id: TASK-582
title: 'feat(otdirect): add hysteresis and CH suspension logic to OTDirect'
status: To Do
assignee: []
created_date: '2026-05-08 16:00'
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
- [ ] #1 OTDirect CH control respects a configurable hysteresis deadband (default 0.5°C, range 0.0-2.0°C) stored in settings.otd
- [ ] #2 When room temperature >= (CH setpoint + hysteresis/2), CH is suspended (MsgID 0 master CH-enable bit cleared)
- [ ] #3 When room temperature <= (CH setpoint - hysteresis/2), CH is resumed
- [ ] #4 Hysteresis is only active when a valid room temperature source is available (OT MsgID 24 or external)
- [ ] #5 Suspension state is published to MQTT (otgw32/ch_suspended) and visible in /api/v2/otdirect/status
- [ ] #6 SAT mode bypasses this logic (SAT manages its own hysteresis)
- [ ] #7 Setting bHysteresisEnable=false disables the feature entirely (backward-compat default)
<!-- AC:END -->
