---
id: TASK-1343
title: >-
  SAT push setpoint: detect and report when the thermostat refuses the remote
  setpoint (status only, no re-push)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-24T09:39:00Z'
updated_date: '2026-10-24T15:01:00Z'
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
- [x] #1 A state push_setpoint_refused (REST sat/status + MQTT topic + HA binary_sensor or attribute) goes true when the thermostat refuses (host test with the gruijter frame sequence: red on HEAD = no such state, green on the fix) and clears on a new target / push off / adoption
- [x] #2 The push-setpoint hint (settingsHelp) and the user docs say a thermostat can refuse the remote setpoint; the v2 SAT page shows a short note while refused (screenshot)
- [x] #3 MQTT.md documents the new topic for integrations (Homey fallback use case)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
One board-independent rule (KISS), reusing the MsgID 16 hook satOnThermostatSetpoint() (SATcontrol.ino ~1253): refused = push setpoint on AND a TC= push was sent (record its millis in satEnqueueIfChangedTC) AND >= 300 s after that push the thermostat's own MsgID 16 (T frame) still differs from the pushed value by > 0.25. Cleared when: a new push goes out, push setpoint is switched off (satTCPushOff), the thermostat setpoint matches the pushed value (<= 0.25), a value is adopted (satTCNoteAdopted), SAT disabled. Matches gruijter's capture (TC=19.5 at 11:01:17, thermostat keeps 15.0 -> refused at 11:06:17); works the same on PIC and OT-Direct (no MsgID 9 tracking). Status only: no re-push (entry 16). Expose state.sat.bPushRefused: REST /api/v2/sat/status push_refused, MQTT sat/push_refused via the core-state publisher (mirror fallback_active), HA entity only if fallback_active has one (mirror it), v2 SAT page short note 'Thermostat refuses the pushed setpoint' when true, settingsHelp hint for satpushsetpoint ('a thermostat can refuse a remote setpoint; the SAT page then says so'), docs/api/MQTT.md. Not a change in how SAT controls the boiler. Test: host test cutting the real decision function (make it a pure function satPushRefusedDecide(...) with plain-type args) with gruijter's frame timeline: red on HEAD (no state), green on fix; plus accept/clear cases.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24 evidence: tests/test_sat_push_refused.py (real satPushRefusedDecide cut from SATcontrol.ino) 6/6 OK incl. gruijter's timeline (TC=19.5 at t0, thermostat 15.0 every 40 s: false <300 s, true >=300 s); HEAD: function missing -> FAILED. Full python test sweep green (fixed test_sat_cmd_cache stub). tests/webui/sat-push-refused-hint.test.mjs (real page): 2/2 on fix, 2 failures on HEAD; screenshot scratchpad push-refused.png shows the hint under 'Heating to 19.5°'. evaluate 0 failed; build SUCCESS; bench OTA: /api/v2/sat/status has push_refused:false, broker retained 'sat/push_refused false'. MQTT.md row added. Not verified: a real refusing thermostat (bench has none); no HA entity (fallback_active has none either).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
New status push_refused: with push setpoint on, if the thermostat still reports its own, different setpoint (MsgID 16, > 0.25) 5 minutes after SAT pushed a new target, SAT flags it. Cleared on a new push, push off, the thermostat following, adoption or SAT disable. Same rule on PIC and OT-Direct; status only, no re-push (entry 16). Exposed on REST sat/status, MQTT sat/push_refused (retained, for Homey to fall back to its own sync), a hint on the SAT page and in the push-setpoint help.
Evidence: decision test 6/6 with gruijter's timeline, red on HEAD; browser test 2/2 with screenshot, red on HEAD; bench REST field and retained topic present. Not verified with a real refusing thermostat.
<!-- SECTION:FINAL_SUMMARY:END -->
