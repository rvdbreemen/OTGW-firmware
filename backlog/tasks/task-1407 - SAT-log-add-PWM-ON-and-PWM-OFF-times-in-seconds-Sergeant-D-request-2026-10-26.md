---
id: TASK-1407
title: >-
  SAT log: add PWM ON and PWM OFF times in seconds (Sergeant D request
  2026-10-26)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-26T18:24:00Z'
updated_date: '2026-10-28T08:44:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1412.3
references:
  - TASK-1412
ordinal: 545000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-26 13:35 (#alpha-testing msg 1564271354390257867): 'Also in the SAT log add PWM ON and PWM OFF times in seconds'. Logging only, no control change. Add the computed PWM on-time and off-time (seconds) for the current period to the SAT debug/narration log line where the PWM schedule is computed (SATcontrol.ino satPwmSchedule / PWM state publish), and to the v2 SAT log view if that view is fed from the same narration ring.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Each PWM schedule decision logs the ON and OFF time in seconds (telnet SAT debug and the SAT event log)
- [x] #2 Host or bench evidence of the new line; no change to what is sent to the boiler
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Logging only (entries 61, 93). At each PWM phase transition log the mapped ON and OFF time in seconds of the current duty: OFF->ON start (satPwmBeginOn) and ON->OFF (satPwmEndOn) via satNarratef_P into the SAT event log (v2 SAT log view, WS event ring) with fields pwm_on=<s> pwm_off=<s> duty=<0.00>, plus the per-tick SAT debug line (telnet) of satPwmSchedule. No change to what is sent to the boiler. Proof: host test that captures the narration text on an OFF->ON and ON->OFF with known duty (range 3, 1200 s period, duty 0.22 -> on 264 off 936), and bench telnet/WS line.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.

2026-10-28: request repeated with field names pwm_on (seconds) and pwm_off (seconds) (entry 93, msg 1564861520494461061).

2026-10-28: satPwmNarrateEdge logs 'PWM ON: pwm_on=264 s pwm_off=936 s duty=0.22' / 'PWM OFF: ...' at every edge (SAT event log = telnet + WS ring, not MQTT/REST) and a per-tick telnet line 'SAT PWM: pwm_on=.. s pwm_off=.. s duty=.. ON|OFF'. Host test test_edges_are_narrated_with_pwm_on_and_pwm_off_seconds: FAILED (failures=1) on the alpha.515 source, OK on the fix; all tests/test_sat_*.py OK; evaluate --quick 0 failed. Control path unchanged (onTime/offTime passed into satPwmBeginOn/EndOn only for the log).
<!-- SECTION:NOTES:END -->
