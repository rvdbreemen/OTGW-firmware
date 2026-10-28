---
id: TASK-1422
title: >-
  SAT PWM: pause the ON timer while the boiler DHW-active bit is set, then
  restart the start-up sequence (entry 95, Q48b)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-28T07:36:00Z'
updated_date: '2026-10-28T09:38:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies:
  - TASK-1421
references:
  - docs/sat/sergeant-d-corrections.md
priority: medium
ordinal: 586000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Review finding on the TASK-1382/1380/1383 candidate (correctness + SAT-decision lenses, 2026-10-28). The SAT control loop returns early while the boiler DHW bit is set (SATcontrol.ino ~5096-5113), so satPwmSchedule() is frozen during a draw. An ON phase whose ON time runs out during a draw only switches to OFF at the first tick after the draw, so _pwm_lastUpdateMs gets the end of the draw and the next ON comes up to a full off time late (example: duty 0.22 at 1200 s, ON from T, 20-min draw from T+10: thermo-nova ON again ~T+1210, firmware ~T+2146). thermo-nova calls pwm.update() whenever heater_state is ON (heating_control.py:202-215); heater_state has no DHW term (heating_control.py:64-72). The draws in gruijter's 2026-10-27 capture all fell in OFF phases, which the candidate fixes. Running the schedule during DHW interacts with start counting: an ON start during a draw with the DHW flame lit (entry 89, Q48 'do not count flame when DHW enable is triggered', Q48b open), so plan it with TASK-1421.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test: an ON phase interrupted by a hot-water draw keeps its used ON time and continues with the remaining ON time after the CH flame relights (freeze-clock: lastUpdate shifted by the draw length, no BT reseed, no new EMA window or hold); red on alpha.516, green on the fix
- [ ] #2 No CS=10/CH=0 sent during the draw; OFF phase during a draw unchanged (timed from the ON->OFF switch)
- [ ] #3 Sergeant D's OK on the open points (post-draw 180 s wait then resume anyway; no new hold/BT reseed; flame already burning after the draw resumes at once) recorded in docs/sat/sergeant-d-corrections.md before implementation
- [ ] #4 build + evaluate green
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Decision: entry 95 (Q48b), his deviation from thermo-nova (pwm.update runs during DHW, heating_control.py:202-215). During the DHW early return (SATcontrol.ino ~5096-5113) with PWM active and an ON phase requested: on the DHW rising edge store the ON time already used (now - _pwm_lastUpdateMs, clamp >= 0) and mark the phase paused; keep sending nothing new (no CS=10/CH=0; the PIC CS keepalive keeps the last ON CS). On the DHW falling edge: the ON phase resumes as a waiting phase: _pwm_waitingForFlame = true, _pwm_flameOnMs = 0, a 'resume' anchor so that at the CH ignition _pwm_lastUpdateMs = now - usedOnMs (remaining ON time continues) and _pwm_flameOnMs = now (180 s hold, entry 31); if no CH flame within 180 s the timer resumes anyway at that point (entry 80 rule, our interpretation in entry 95); if the flame is already burning at the first tick after DHW (no DHW bit), resume at once. The start-up sequence (flame-off hold setpoint / entry 45 flood) runs through satApplyPWM's existing flame-off path. OFF phase during DHW: unchanged (wall-clock from the ON->OFF switch, entry 97). Not counted as a new start.
Proof: host test: ON 264 s (duty 0.22, 1200 s), draw from +100 s to +400 s, CH ignition at +430 s: ON ends at 430 + (264 - 100) = 594 s (+-1 tick), not at the first tick after the draw (red on HEAD: ends ~+420); no CS=10 during the draw; draw in OFF still gives ON at switch + off (D1 stays green).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 scope rewritten by Sergeant D (entry 95, msg 1564902129896792156): not thermo-nova parity. During a PWM ON phase with boiler DHW active: pause the ON-seconds counter, keep sending the PWM ON values (no CS=10/CH=0); when DHW active drops, run the start-up sequence (ignition wait, entry 45 flood without return temp) and resume the remaining ON time once the CH flame is on. OFF phase during DHW: unchanged, timed from the ON->OFF switch (entry 97). AC#1 now: ON phase paused by a draw resumes with its remaining ON time after the CH ignition.

2026-10-28 advisor review: design defects in the first plan (flame-sync would give a full ON time and reseed BT with hot DHW flow; wait timed from lastUpdate would end the phase at once; end-time contradiction 594 vs 610). Freeze-clock model chosen: on DHW rising edge capture used ON ms; on falling edge (before _sat_prevDhwActive clears, before satPwmSchedule) shift _pwm_lastUpdateMs by the draw length and enter a post-draw wait timed from the DHW falling edge, bypassing flame-sync; clear in satPwmReset. Waiting for Sergeant D's OK on the three open points (asked after Q42).
<!-- SECTION:NOTES:END -->
