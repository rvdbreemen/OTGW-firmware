---
id: TASK-1762
title: >-
  SAT continuous -> PWM fail-safe: flow - CS >= 3.5 for 30 s (extra beside the
  60 s rule)
status: To Do
assignee: []
created_date: '2026-11-09T19:17:00Z'
labels:
  - sat
  - pwm
dependencies: []
priority: high
ordinal: 861000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D, 2026-11-09, #dev-sat-mqtt: entry 288 (18:20:47 UTC, msg 1569416482025119805: "as a fail safe condition when boiler temperature - cs >= 3.5 in continuous mode fall back to PWM") and entry 289 (18:58:46 UTC, msg 1569426039703216169, answer A to Q-288a: "we need to stay 30 seconds at flow - CS >= 3.5 just not to trigger unnecessary PWM cycles. This will be an extra fail safe mechanism for flame out"). Unblocked by entry 289.

Rule: in continuous mode, when flow - CS >= 3.5 holds for 30 s, SAT switches to PWM. CS = the requested (PID) setpoint. Extra beside the existing 60 s sustained-overshoot rule (thermo-nova heating_control.py:262-305, const.py:21, cycles/const.py:10; firmware SATcycles.ino satCycleCheckOvershootSwitch(), margin settings.sat.fOvershootMargin default 2.0), which stays unchanged. Entry 213 (MsgID 3 PWM-not-allowed bit blocks every PWM entry path) applies. Our reading in entry 289, not asked: the 30 s timer runs like the 60 s one (reset on a non-qualifying tick, missing flow, outside continuous mode or within the hot-water guard; entry 109: ticks never catch up); equality counts. Deliberate addition on top of thermo-nova (record as such). Related TASK-1412; entries 88, 109, 213, 288, 289.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Firmware: in continuous mode, flow - CS >= 3.5 for 30 s switches to PWM, beside the unchanged 60 s rule; CS is the requested (PID) setpoint; entry 213's MsgID 3 block applies
- [ ] #2 Timer behaviour as in entry 289 (start, reset conditions, no catch-up, >=) covered by native tests, including the boundary 3.5 and 29/30 s
- [ ] #3 Recorded as a deliberate deviation from thermo-nova in the SAT ground-truth docs, citing entries 288/289
- [ ] #4 Shown on a capture or simulation replay of tjfs's 2026-11-09 case: the switch comes 30 s after flow - CS reaches 3.5 instead of via the 60 s rule
<!-- AC:END -->
