---
id: TASK-1581
title: >-
  SAT: send MM=T at the maximum PWM duty (range 5) as the threshold to
  continuous mode (entry 242)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-03T06:08:00Z'
updated_date: '2026-11-03T08:17:00Z'
labels:
  - sat
dependencies: []
priority: medium
ordinal: 727000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D entry 242 (2026-11-03, msg 1567055574548353047): when PWM is at the maximum cycle length (duty range 5) SAT sends MM=T instead of its own MM= value, as the step into continuous mode. Part 1 of the same answer (no dip every 2 periods in range 5) matches the current firmware. Our reading: MM=T follows the entry 237/239 sending rules (on change, once a minute). Waits on Q-1412.9c for the way back below range 5.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Q-1412.9c answer recorded in sergeant-d-corrections.md
- [x] #2 Host test on the real SATcontrol.ino: entering range 5 sends MM=T once (on change), unchanged it repeats once a minute, leaving range 5 sends what Q-1412.9c decided; red on the old code
- [x] #3 SATTICK/log shows the MM value sent; docs/sat/firmware-sat-spec.md updated
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-03: Q-1412.9c answered A (entry 243): below the maximum duty SAT sends its own MM= again (on change, entry 237). Unblocked.

2026-11-03: agent stopped before editing on an uncovered interaction: Immergas TP=11:12=min(MM,80) precedes every MM= (entry 233, SATcontrol.ino:678-694) but MM=T has no number. Queued Q-1581a for Sergeant D (A: TP=11:12=80 before MM=T, advice). HP/hybrid: MM=T applies to every source at range 5, independent of the PWM phase (plain reading of entry 242). Design ready: range-5 flag in satPwmSchedule (duty >= dutyMax branch), satSendMM target T|number, SATTICK shows mm=T.

2026-11-03: Q-1581a answered A (entry 246): on an Immergas TP=11:12=80 right before MM=T at the maximum duty; below it TP and MM back to the low-load values. Unblocked.

2026-11-03 (agent, verified by lead in wt-next): range-5 flag _pwm_atMaxDuty set in satPwmSchedule (duty >= dutyMax), cleared each pass and in satPwmReset; satSendMM takes SAT_MM_T=255 and sends MM=T (on change, 60 s repeat, cache reset, commit-on-success unchanged); Immergas gets TP=11:12=80 before MM=T via the existing min(mm,80) (entry 246); below range 5 the numeric MM and TP=min(MM,80) again (entry 243). DHW draw keeps its numeric maximum (overrides on change), hand-back MM=T unchanged, cold cutoff and non-PWM unchanged; HP/hybrid same rule. SATTICK prints mm=T. Tests: test_sat_cmd_cache modes 16/17 + test_n11 (gas/HP/hybrid): old 2 FAIL + 3 subtests, fix cmd_cache 27 OK, anchors 64 OK, hp_pwm 35, pic_pm_chain 32, log_to_csv 8, lowload contract OK, evaluate 0 failed. Not verified: the control-tick line itself on a host; bench.

2026-11-03 lead: full sweep found tests/test_sat_immergas_tp.py failing to compile (harness cuts satSendMM without the new SAT_MM_T constant); the harness now reads the real SAT_MM_T declaration from the source. 5 OK on the fix and on the pre-1581 code; full tests 107/107 on the rebased integration HEAD.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
At the maximum PWM duty SAT sends MM=T (Immergas: TP=11:12=80 first) and its own MM= again as soon as the duty drops below it, with the existing on-change / once-a-minute / commit-on-success rules (entries 242, 243, 246). Evidence: host tests on the real satSendMM and satPwmSchedule, red on the old code, green on the fix. Not bench-tested.
<!-- SECTION:FINAL_SUMMARY:END -->
