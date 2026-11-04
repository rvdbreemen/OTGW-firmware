---
id: TASK-1518
title: >-
  SAT: no CH flame after a hot-water draw lets the PWM minimum-burn hold lapse
  (Sergeant D entry 158, Q-1502a B)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-31T05:12:00Z'
updated_date: '2026-11-02T12:00:00Z'
labels:
  - sat
  - pwm
dependencies: []
priority: high
ordinal: 674000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Entry 158 (Q-1502a B): a PWM ON phase that started with the flame burning, is interrupted by a draw and gets no CH flame after it ends on its normal ON time; the 180 s hold does not apply without a flame. Today (alpha.564, TASK-1502) the firmware freezes the hold clock through the draw and the 180 s post-draw wait and then applies the rest of the hold (ON phase ends at 390 s in the posted example instead of 270 s). thermo-nova measures the hold from flame-on by the clock (pwm.py:144-146, :200-202). Probe P4 of the TASK-1502 review reproduces it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test on the real SATcontrol.ino: burning start, draw, no CH flame after it: the ON phase ends on its ON time (red on alpha.564, green on fix)
- [x] #2 TASK-1502 cases 1 and 2 (tests/test_sat_pwm_anchors.py) stay green
- [x] #3 build + evaluate green; firmware-sat-spec.md updated
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
One change in satPwmDhwResume() (SATcontrol.ino), shared with TASK-1518/1558 (same function, same question family Q-1502a/b; one commit, one tag). Resume after a draw: no CH flame -> _pwm_flameOnMs = 0 (entry 158: the hold lapses, also when it was running at the draw); CH flame and a hold running at the draw -> rest of the hold (entry 132 case 2, unchanged); CH flame and no hold anchor yet (wait still running, entry 132 case 1, or wait already timed out, entry 211) -> the hold starts at that flame. No flame at the resume and no anchor before -> _pwm_dhwAnchorLate, so a later CH flame still anchors the hold (entry 211, same as m2b). thermo-nova: pwm.py:144-146 anchors at the first flame of the ON phase, :200-203 hold, no hot-water branch (opened 2026-11-02). Not asked, kept as today: a burning start whose flame does not relight after the draw and relights only after the 180 s fallback gets no new hold (thermo-nova keeps its first anchor). Tests: tests/test_sat_pwm_anchors.py m5 (P4) -> 270 s (was 390), m4 (P3) -> 480 s (was 330), new m6 (timed-out wait, draw, CH flame after the fallback -> 180 s hold); red on HEAD, green on fix; g1/g2/m1/m2/m3 stay green. Then evaluate --quick, full tests folder, firmware-sat-spec.md, combo build, bench .63 (SAT sim on, DHW draw via boiler emulator if feasible, else host evidence + boot/health on bench).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: hold clock still frozen through the draw (_pwm_dhwHoldValid, SATcontrol.ino:973-976); entry 158 decided. Agent-doable now.

2026-11-02 evidence (wt-1552, base origin/dev 3136f8d21 = beta.8): tests/test_sat_pwm_anchors.py on HEAD: 3 FAIL (m4 ended 330 exp 480; m5 ended 390 exp 270; m6 ended 510 exp 660), m5b green on both (control: CH flame back in the post-draw wait keeps the rest of the hold, 240 s). Fix in satPwmDhwResume(): 42/42 OK. Opus review: one MEDIUM held (a second draw after a lapsed hold read the cleared anchor as "no flame yet" and gave a relit flame a fresh hold, 540 s vs 420 s; conflicts entry 103 B). Fixed with _pwm_dhwHoldLapsed (cleared at satPwmBeginOn/EndOn/Reset); new test m5c red on the first fix (540), green now; 43/43 OK. LOW findings fixed: spec line citations refreshed, minimum burn named 180 s / 600 s heat pump in comments and spec. NEGLIGIBLE (anchor 0 = none, 1 ms per 49.7 days, pre-existing pattern) not changed. evaluate --quick: 0 failed, 100 %. Not verified: heat-pump timings (600 s) with the new branches (gas cases only); review read entries 95, 97, 101, 103, 105-108, 131-133, 156, 158, 195, 197, 211: no other conflict.

Heat-pump paths (peer suggestion): harness cmd "src 2" (satGetMinOnTimeSec 600 s). New m7 (no CH flame after the draw, 600 s hold lapses: 270 s), m7b (control, flame back: rest of the 600 s hold, 660 s), m7c (timed-out wait, draw, CH flame: 600 s hold, 900 s). Fix: 46/46 OK. HEAD copy (OTGW_SAT_INO=git show HEAD): 5 FAIL = m4 330, m5 390, m6 510, m7 810, m7c 330; m5b, m5c, m7b green on both.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Shipped in 2.0.0-beta.9+8c9c1f1 (commit 8c9c1f181, tag v2.0.0-beta.9, GitHub prerelease, Latest stays v1.7.8; download sha256 ba6895fe... matches SHA256SUMS). Together with TASK-1518/TASK-1558 (one function, satPwmDhwResume). AC evidence: tests/test_sat_pwm_anchors.py 46/46 OK on the fix; HEAD copy (beta.8) FAILED 5: m4 330 vs 480, m5 390 vs 270, m6 510 vs 660, m7 (heat pump) 810 vs 270, m7c (heat pump) 330 vs 900; controls m5b, m5c, m7b and the TASK-1502 cases (g1, g2, m1, m1b, m1c, m2, m2b, m3) green on both. Full tests/ 93/93. evaluate --quick 0 failed, 100 %. esp32-combo build: 2x Successfully created ESP32S3 image, Build completed successfully, fresh artifacts 12:25. firmware-sat-spec.md updated (decisions replace the open-with-Sergeant-D sentence; citations refreshed). Review (opus): MEDIUM fixed (_pwm_dhwHoldLapsed, test m5c), 2 LOW fixed, 1 NEGLIGIBLE left (pre-existing anchor-0 sentinel). Bench .63 (PIC 6.8): OTA app+fs, 175 settings unchanged, bootcount 60 unchanged after 11 min, SAT simulation in PWM with an ON phase (duty 0.96-0.65, flame request on), restored to auto. Not verified: the draw itself on hardware (no boiler on the bench); Robert approved publishing with that stated (AskUserQuestion 2026-11-02), as for TASK-1502. Posts: #alpha-testing zip 1566783406363119751, report 1566783786652405762; #beta-testing 1566783713428242564.
<!-- SECTION:FINAL_SUMMARY:END -->
