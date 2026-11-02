---
id: TASK-1558
title: >-
  SAT PWM: a CH flame after a hot-water draw gets the 180 s minimum burn hold
  also when the first-flame wait had timed out (Q-1502b A, entry 211)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-02T05:45:00Z'
updated_date: '2026-11-02T11:09:00Z'
labels:
  - sat
dependencies: []
priority: medium
ordinal: 710000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-11-02 answered Q-1502b with A (entry 211): an ON phase whose 180 s first-flame wait had already timed out, then a hot-water draw, then a CH flame: that flame gets the 180 s minimum burn hold, as without a draw. thermo-nova pwm.py:144-146 / :201-203 (hold starts at the first flame seen after a timed-out wait). Today the phase can end sooner (330 s example in TASK-1502). Related: TASK-1518 (entry 158).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on the real code: timed-out wait, draw, CH flame -> the ON phase holds 180 s from that flame (red on HEAD, green)
- [ ] #2 No change when the draw comes before the wait times out (TASK-1502 behaviour kept)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
One change in satPwmDhwResume() (SATcontrol.ino), shared with TASK-1518/1558 (same function, same question family Q-1502a/b; one commit, one tag). Resume after a draw: no CH flame -> _pwm_flameOnMs = 0 (entry 158: the hold lapses, also when it was running at the draw); CH flame and a hold running at the draw -> rest of the hold (entry 132 case 2, unchanged); CH flame and no hold anchor yet (wait still running, entry 132 case 1, or wait already timed out, entry 211) -> the hold starts at that flame. No flame at the resume and no anchor before -> _pwm_dhwAnchorLate, so a later CH flame still anchors the hold (entry 211, same as m2b). thermo-nova: pwm.py:144-146 anchors at the first flame of the ON phase, :200-203 hold, no hot-water branch (opened 2026-11-02). Not asked, kept as today: a burning start whose flame does not relight after the draw and relights only after the 180 s fallback gets no new hold (thermo-nova keeps its first anchor). Tests: tests/test_sat_pwm_anchors.py m5 (P4) -> 270 s (was 390), m4 (P3) -> 480 s (was 330), new m6 (timed-out wait, draw, CH flame after the fallback -> 180 s hold); red on HEAD, green on fix; g1/g2/m1/m2/m3 stay green. Then evaluate --quick, full tests folder, firmware-sat-spec.md, combo build, bench .63 (SAT sim on, DHW draw via boiler emulator if feasible, else host evidence + boot/health on bench).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-02 evidence (wt-1552, base origin/dev 3136f8d21 = beta.8): tests/test_sat_pwm_anchors.py on HEAD: 3 FAIL (m4 ended 330 exp 480; m5 ended 390 exp 270; m6 ended 510 exp 660), m5b green on both (control: CH flame back in the post-draw wait keeps the rest of the hold, 240 s). Fix in satPwmDhwResume(): 42/42 OK. Opus review: one MEDIUM held (a second draw after a lapsed hold read the cleared anchor as "no flame yet" and gave a relit flame a fresh hold, 540 s vs 420 s; conflicts entry 103 B). Fixed with _pwm_dhwHoldLapsed (cleared at satPwmBeginOn/EndOn/Reset); new test m5c red on the first fix (540), green now; 43/43 OK. LOW findings fixed: spec line citations refreshed, minimum burn named 180 s / 600 s heat pump in comments and spec. NEGLIGIBLE (anchor 0 = none, 1 ms per 49.7 days, pre-existing pattern) not changed. evaluate --quick: 0 failed, 100 %. Not verified: heat-pump timings (600 s) with the new branches (gas cases only); review read entries 95, 97, 101, 103, 105-108, 131-133, 156, 158, 195, 197, 211: no other conflict.

Heat-pump paths (peer suggestion): harness cmd "src 2" (satGetMinOnTimeSec 600 s). New m7 (no CH flame after the draw, 600 s hold lapses: 270 s), m7b (control, flame back: rest of the 600 s hold, 660 s), m7c (timed-out wait, draw, CH flame: 600 s hold, 900 s). Fix: 46/46 OK. HEAD copy (OTGW_SAT_INO=git show HEAD): 5 FAIL = m4 330, m5 390, m6 510, m7 810, m7c 330; m5b, m5c, m7b green on both.
<!-- SECTION:NOTES:END -->
