---
id: TASK-3143
title: >-
  SAT: send MM=T only in continuous mode; in PWM keep MM=0 (Sergeant D entry
  505, scope 2.0.0 or 2.1.0 not decided)
status: To Do
assignee: []
created_date: '2026-11-27T21:27:00Z'
updated_date: '2026-11-27T21:31:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1190000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D, 2026-11-27 (entry 505): MM=T only when SAT is in continuous mode; in PWM MM stays 0. Today the firmware sends MM=T inside the PWM ON phase at the maximum duty range (TASK-1581, entries 242, 243, 246; SATcontrol.ino satPwmAtMaxDuty, satSendMM). Interpretation (ours): PW' = PWM. Release scope not decided, to be asked of Sergeant D. SAT control behaviour: the plan quotes thermo-nova and waits for his OK before code.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The plan quotes thermo-nova heating_control.py (file:line at 7beaaf5) and every earlier MM entry (21, 44, 113, 237, 239, 242, 243, 246, 269) and states which are superseded by entry 505
- [ ] #2 Release scope (2.0.0 or 2.1.0) decided by Sergeant D
- [ ] #3 In PWM mode (ON and OFF phase) SAT never sends MM=T: host test red on the old code, green on the fix
- [ ] #4 In continuous mode MM behaviour is unchanged except as entry 505 says; the hand-back MM=T at SAT disable (entry 47) is unchanged
- [ ] #5 Bench with the SAT simulator shows no MM=T during a PWM ON phase and the sattick log mm column matches
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Entry 505 (register OTGW-sat-decisions). Thermo-nova citations heating_control.py:510-523 and :133-139 at 7beaaf5 are cited, not yet confirmed by a second reader (otgw-architect to open both locations and try to refute; his verdict goes here). Release scope 2.0.0 is provisional until Sergeant D answers the scope question. Firmware send site: SATcontrol.ino:8914 (mmTarget) and :8921 (satSendMM); SAT_MM_T in satSendMM :758-771.
<!-- SECTION:NOTES:END -->
