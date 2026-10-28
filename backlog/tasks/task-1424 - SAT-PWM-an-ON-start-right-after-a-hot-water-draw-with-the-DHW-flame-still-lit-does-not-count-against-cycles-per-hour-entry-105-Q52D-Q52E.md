---
id: TASK-1424
title: >-
  SAT PWM: an ON start right after a hot-water draw with the DHW flame still lit
  does not count against cycles per hour (entry 105, Q52D/Q52E)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-28T11:19:00Z'
updated_date: '2026-10-28T12:06:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies: []
references:
  - docs/sat/sergeant-d-corrections.md
priority: high
ordinal: 588000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-28 Q52D 'No' (entry 105, msgs 1564954645489782858, 1564956502849093675): corrects our entry 101 reading that TASK-1421 shipped in alpha.517/518 (counts such a start, as pwm.py:160). B (start, not counted; remember a flame lit while the boiler DHW-active bit was set) or C (no ON start while that flame burns; start and count after it goes out) asked as Q52E. Implementation waits for that answer.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Sergeant D's choice B or C (Q52E) recorded in docs/sat/sergeant-d-corrections.md before implementation
- [x] #2 Host test: ON start at the first tick after a draw with the flame still lit does not add to cycles_this_hour (red on alpha.518, green on fix)
- [x] #3 A normal unlit or CH-lit ON start still counts (TASK-1421 tests stay green)
- [ ] #4 build + evaluate green
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Entries 105, 106 (+ 90, 95, 97, 103). Rule: when the OFF wait ends (or range 5 wants ON) while the flame burns and that flame was lit while the boiler DHW-active bit was set, do not start the ON phase; start (and count) after that flame goes out. Implementation (SATcontrol.ino): a flag _pwm_dhwFlameLit set in the control loop's DHW early-return branch whenever the flame burns with the DHW bit set; cleared when the flame is off (any tick) and in satPwmReset. In satPwmSchedule's OFF branch and the range-5 start: if flame && _pwm_dhwFlameLit, hold (no start, no count, entry state unchanged; narrate once 'PWM ON held: hot-water flame still burning'). A CH flame (not lit during DHW) is never held (PWM entry ON, entry 43). ON phases are unaffected (TASK-1422 pause/resume). Known limit: a draw entirely between two control ticks is not seen (same as today). Proof: host tests in tests/test_sat_pwm_anchors.py: (h1) OFF wait ends at the first tick after a draw with the flame still lit -> no ON until flame off, then ON + count (red on alpha.518 source: ON starts and counts); (h2) CH-lit flame at OFF end (no DHW) still starts at once (entry ON path); (h3) range 5 with DHW-lit flame holds the same. Bench: covered by the bundled field build; bench replay if time allows.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 Q52E C (entry 106, msg 1564962902509752442): no ON start while a flame lit during boiler DHW-active still burns; start after it goes out, counted.

2026-10-28 implemented: _pwm_dhwFlameLit marked at the flame-on edge with the live DHW bit (satPwmNoteDhwFlameEdge) and per DHW tick; cleared at flame-off, resume (103 C), satPwmBeginOn, satPwmReset (kept across the no-flow reset); satPwmHoldForDhwFlame holds the OFF-branch and range-5 starts while the flame burns, only when satPwmDrivesSetpoint (entry 107); range-5 hold restores idle/entry. Host: anchors 32 OK; on alpha.518 source 5 failures (e4, h1, h3, k1, k2); k3/k4 red against a fix without the entry-107 gate/resume clear. Review (opus correctness + sonnet tests/decisions): 5 edge findings fixed. All tests/test_sat_*.py OK (test_sat_ot_loss stubs extended); evaluate --quick 0 failed; compile esp32-combo SUCCESS (pre-review version).
<!-- SECTION:NOTES:END -->
