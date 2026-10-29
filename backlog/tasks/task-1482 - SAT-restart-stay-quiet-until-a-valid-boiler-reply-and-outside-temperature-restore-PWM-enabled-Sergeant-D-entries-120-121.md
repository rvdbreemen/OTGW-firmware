---
id: TASK-1482
title: >-
  SAT restart: stay quiet until a valid boiler reply and outside temperature,
  restore PWM enabled (Sergeant D entries 120/121)
status: To Do
assignee: []
created_date: '2026-10-29T19:01:00Z'
labels:
  - sat
  - bug
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 642000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D Q59 A (entry 121, settles entry 120 'this is SAT's bug'): after a gateway restart SAT sends nothing of its own until it has a valid boiler reply AND a valid outside temperature (entries 39/47/56/57/79), then takes over; it restores whether PWM was enabled (thermo-nova pwm.py:111-114 via heating_control.py:181-184, climate.py:169-174). Found in gruijter's alpha.527 capture (TASK-1474): at 12:57:24 SAT sent CS=34.1/CH=1/MM=100 with outside 0.0 C (MsgID 27 fallback) and came back in continuous mode after being in PWM OFF. Firmware today: boot CS=0 SATcontrol.ino:4317-4325, continuous at :4338. Relates to TASK-1390 (outside temperature missing).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on the real boot/control path: after boot with no boiler reply or no valid outside temperature SAT queues no CS/CH/MM of its own (red on HEAD: CS=34.1-like setpoint with outside 0.0); once both are valid SAT takes over at the next tick
- [ ] #2 PWM enabled state persisted and restored across a reboot (red on HEAD: continuous after a restart in PWM); phase and timers start fresh
- [ ] #3 firmware-sat-spec.md and sat-comparison.md describe the restart sequence with entry 121 and the thermo-nova citations
<!-- AC:END -->
