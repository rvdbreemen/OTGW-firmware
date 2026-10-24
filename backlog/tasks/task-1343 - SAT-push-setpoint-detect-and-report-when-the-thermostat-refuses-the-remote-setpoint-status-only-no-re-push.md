---
id: TASK-1343
title: >-
  SAT push setpoint: detect and report when the thermostat refuses the remote
  setpoint (status only, no re-push)
status: To Do
assignee: []
created_date: '2026-10-24T09:39:00Z'
labels:
  - sat
  - mqtt
  - webui
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 466000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter 2026-10-24 09:30 UTC (#alpha-testing msg 1563484727879540897): add to the push-setpoint hint that a thermostat can refuse the sync; show the user cleanly that the target IS refused; expose a state so an integration (Homey) can fall back to its own SAT->thermostat sync. Field evidence (his capture 20261024-110022, Honeywell Round): after TC=19.5 the PIC answers MsgID 9 = 19.5, the thermostat keeps TrSet 15.0, the PIC cancels after ~3 min (MsgID 9 = 0). Detection idea (PIC boards): after a push, if MsgID 9 goes from the pushed value to 0 while the thermostat's own MsgID 16 still differs from the pushed value -> state 'refused' (cleared on a new target, push off, or when the thermostat's TrSet matches the push). This is status only: no re-push (entry 16, the hybrid re-push was dropped for flapping). OT-Direct boards never cancel TC= by themselves: there 'refused' = thermostat MsgID 16 keeps differing from the pushed value for N minutes; decide in the plan. Not a SAT boiler-control change (display/state only).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A state push_setpoint_refused (REST sat/status + MQTT topic + HA binary_sensor or attribute) goes true when the thermostat refuses (host test with the gruijter frame sequence: red on HEAD = no such state, green on the fix) and clears on a new target / push off / adoption
- [ ] #2 The push-setpoint hint (settingsHelp) and the user docs say a thermostat can refuse the remote setpoint; the v2 SAT page shows a short note while refused (screenshot)
- [ ] #3 MQTT.md documents the new topic for integrations (Homey fallback use case)
<!-- AC:END -->
