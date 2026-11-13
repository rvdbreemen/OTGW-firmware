---
id: TASK-1882
title: 'SAT diagnostics: heating-failure and no-ignition event (no control action)'
status: To Do
assignee: []
created_date: '2026-11-13T06:13:00Z'
updated_date: '2026-11-13T15:47:00Z'
labels:
  - sat
  - diagnostics
  - needs-maintainer-ok
dependencies:
  - TASK-1879
priority: low
ordinal: 955000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Lesson 3 of docs/sat/vtherm-comparison-2026-11-13.md (TASK-1879). VTherm raises a heating-failure event when on >= 90% for 15 min and the room rises < 0.5 C, without a control action (const.py:540-544, feature_heating_failure_detection_manager.py:224-264). SAT counterparts: (a) high requested setpoint for N min while the room temperature does not rise; (b) PWM ON phase or requested CS above the cold cutoff with no flame within X. Event + telemetry + mark the tuner hour as not counted. Thresholds N and X are a proposal for Robert; marking tuner hours changes tuner input, so that part needs Sergeant D's OK before implementation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Thresholds and the tuner-marking part decided (Robert; Sergeant D for the tuner part) and recorded
- [ ] #2 Host test reproduces both failure patterns: event absent on the old code, raised on the fix; no change in CS/MM/CH output
- [ ] #3 Event visible in REST, MQTT (flat topic) and the telnet log
- [ ] #4 Web UI (U11): an active heating-failure or no-ignition event shows as a problem badge and an entry in the SAT messages panel (TASK-1884); webui test on a fixture payload
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13: UI follow-up U11 from the VTherm UI review added as an AC.

2026-11-13 AC1 part 1, thresholds decided by Robert (Discord DM 1570699094622470167, his reply 'A' to the coordinator's thresholds question; relayed by the coordinator, the DM itself not read by comms): heating failure = SAT asks for high heat for 15 min and the room rises < 0.5 C; no ignition = PWM ON or CS above the cold cutoff without flame within 180 s. AC1 part 2 (whether such hours are excluded from the self-tune) still waits for Sergeant D: Q-1882a, queued in docs/sat/open-questions.md behind Q-PWM1.

2026-11-13 material from gruijter beta.218 capture (analysis 1570826381443469447), relevant to entry 324 and the heating-failure / no-ignition detection: night 12->13 Nov target 17.5, room drifted 19.06->17.47 with no heating at all (no evening heating period, so no overshoot material for entry 324). Morning target 18.0 (06:46) and 19.0 (07:47), room -1.12 below at 07:47, settled ~09:50. 10:00-12:43 room 18.96-19.09 (mean +0.01), PWM, flame 31%. Afternoon: PWM run 13:01-13:10 with control setpoint up to 48.0 (max) at room 19.06->18.94; heating off from 13:22 ("requested 25.4 <= cold setpoint 28.2"), zero flame until 15:56; room 18.93 (13:14) -> 19.44 peak (14:30) -> 19.0 by 16:05. Open-Meteo cloud value (model, not measured) 100% at 13:04 and 13:34, then 27/7/80%; the steepest rise came before the cloud drop: sun vs residual radiator heat cannot be separated. SATTUNEH reason=sun at 13:59/14:59/15:59 means sun elevation >= 12 deg only (SATcontrol.ino:6027). 15 mode switches; beta.218 still 2.0 C/60 s (entry 327 not in this build). No heating-failure or no-ignition event in this run.
<!-- SECTION:NOTES:END -->
