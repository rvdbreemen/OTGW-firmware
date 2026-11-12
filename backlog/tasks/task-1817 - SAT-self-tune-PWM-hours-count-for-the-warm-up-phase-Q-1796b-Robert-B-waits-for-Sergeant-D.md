---
id: TASK-1817
title: >-
  SAT self-tune: PWM hours count for the warm-up phase (Q-1796b, Robert B; waits
  for Sergeant D)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T20:20:00Z'
updated_date: '2026-11-12T07:12:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 910000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert decided Q-1796b = B (DM msg 1570171160111620147, relayed by the coordinator 2026-11-11): PWM hours count for the warm-up phase of the self-tune; the both-phases-agree rule stays, the floating minimum stays 60 min (entry 315, TASK-1816). Today PWM-controlled ticks/hours are excluded from the tuner's clean hours (see the clean-hour rule, ADR-205 decision 2, and the satCurveTune block in SATcontrol.ino / SATcurveTune.h). Sergeant D's answer to the same question is still open: implement only after his answer or explicit silence is recorded in docs/sat/sergeant-d-corrections.md (sat-ground-truth rule; changes SAT control behaviour). Check whether ADR-205 decision 2 / ADR-228 need a superseding ADR.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Sergeant D's answer (or recorded silence) on Q-1796b is in docs/sat/sergeant-d-corrections.md before the code commit
- [ ] #2 Host test: an hour with PWM ticks counts for the warm-up phase on the fix and is excluded on the old code; floating unchanged
- [ ] #3 ADR impact settled (ADR-205 d2 / ADR-228) via adr-kit, recorded in the notes
- [ ] #4 Build, evaluate --quick, OTA bench on .61 with PWM active (satboilercapacity low) shows the PWM hour counted in SATTUNEH
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Sergeant D answered Q-1796b B as well, entry 316 (20:04:33 UTC, msg 1570167372474617888); with Robert's DM B this revises entry 174. Needs an ADR-228 update (ADR-228 is Accepted: supersede or amend via adr-kit). Unblocked once entry 316 is on origin/dev.

ADR-230 (Proposed) carries this rule as decision 2. Built only after Sergeant D answers Q-1817a (A: every PWM tick counts for warm-up, advice; B: PWM hours stay in the normal phase split) and Q-1817b (hybrid: gas-boiler rule, advice, or heat-pump rule), asked via comms 92.

2026-11-12 Robert chose to split (session d3): ADR-230 now holds only the floating minimum (Accepted); the PWM-hours rule gets its own new ADR once Sergeant D answers Q-1817a/b (posted by 92, msgs 1570214599238877266 and 1570214605882662923).

2026-11-12 otgw-firmware-d3: AC1 met: Sergeant D entries 316 (Q-1796b B: PWM hours count for warm-up on gas boilers), 318 (Q-1817a A: every PWM tick goes to warm-up, whatever the PID error) and 319 (Q-1817b B: a hybrid in PWM follows entry 205, PWM hours logged and not used) are on origin (e6f9be70c) before any code. Gap found before implementing: satTuneTickReason() (SATcontrol.ino:6018-6031, order of ADR-205 decision 2) checks pwm before flame_off (chOn && !flameOn) and min_mod (relMod <= minMod); lifting only the pwm gate leaves those two, which a PWM hour hits by construction (the flame pulses, the boiler runs low). Measured on gruijter's capture 10-11 Nov (beta.171, capture-gruijter-1111, 20 tuner hours; SATTICK mode=2 = SAT_MODE_PWM, relMod from boiler MsgID 17 Read-Ack in otframes.log, minMod 5 = SAT_CT_DEFAULT_MIN_MOD because the boiler answers MsgID 15 Unknown-DataID): B (only the pwm gate goes): 0 hours gain; PWM hours 18/19 (120/120 PWM ticks) are then excluded by min_mod (61 and 56 of 120 PWM ticks at or below 5 %, plus 3-4 flame-off ticks). A (on a gas boiler a PWM tick also skips flame_off and min_mod; sun, DHW, no_room, coefficient change and tick count stay): 1 hour gains (hour 19 counts; hour 18 still excluded by 2 DHW ticks; sun hours 13-17 by sun; mixed hours 8, 9, 11 by min_mod on their non-PWM ticks). Scripts: scratchpad pwmhours.py / pwmhours_a.py, outputs pwmhours-gruijter1111*.txt. Not covered by entries 316/318/319: asked Sergeant D via comms 92 (Q-1817c). Signal source checked: ti.signal = clamp(pidRaw) - curve (SATcontrol.ino:7593-7596), the PID request before the PWM duty and ignition override, as entry 316 says. AUTO satsource resolves to gas (satGetEffectiveHeatingSource, SATcontrol.ino:309-314), so an unconfigured hybrid gets the gas rule: to be named in the ADR.

ADR-231 Proposed (docs/adr, local commit on lane-1817): the PWM tick rule for the self-tune; amends ADR-205 decision 2 and ADR-228 decision 1 (relate written on both sides via adr-kit); Q-1817c listed as the open question, decision 2's flame_off/min_mod skip written as our interpretation. tests/test_adr_governance.py passes (README category entry added). Acceptance by Robert after Sergeant D answers Q-1817c; no code before.
<!-- SECTION:NOTES:END -->
