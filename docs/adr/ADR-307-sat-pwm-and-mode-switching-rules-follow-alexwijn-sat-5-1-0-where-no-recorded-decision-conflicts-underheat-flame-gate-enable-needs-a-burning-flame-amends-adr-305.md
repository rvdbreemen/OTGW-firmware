---
id: "ADR-307"
title: "SAT PWM and mode-switching rules follow Alexwijn SAT 5.1.0 where no recorded decision conflicts: underheat flame gate, enable needs a burning flame (amends ADR-305)"
status: "Proposed"
date: "2026-11-28"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "canonical"
topics:
  - "sat"
  - "pwm"
  - "control-mode"
  - "alexwijn-sat-5.1.0"
aliases:
  - "follow SAT 5.1.0"
  - "underheat flame gate"
  - "enable needs flame"
components:
  - "SAT control mode switching"
symbols:
  - "satCycleCheckPwmExits"
  - "satCycleCheckOvershootSwitch"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-307 SAT PWM and mode-switching rules follow Alexwijn SAT 5.1.0 where no recorded decision conflicts: underheat flame gate, enable needs a burning flame (amends ADR-305)

## Status

Proposed, 2026-11-28.

## Status History

```yaml
status_history:
  - date: 2026-11-28
    status: Proposed
    changed_by: "Agent: Claude (otgw-architect)"
    reason: "Initial proposal. Part 1 only (items a, d1, g); the items with a register conflict are Open Questions. Coordinator order 2026-11-28 14:58 after Robert's option B (DM 1576215050258219349, register entry 539)."
    changed_via: adr-kit
```

## Context

Terms used below: SAT (Smart Autotune Thermostat, the thermostat logic in the firmware); PWM (pulse-width modulation, SAT's on/off burn cycle mode); continuous (SAT's modulating mode); request (the flow setpoint the PID (proportional-integral-derivative) controller asks for); flame-on edge (the moment the boiler reports the flame on).

ADR-305 (Accepted 2026-11-28) makes the switching between continuous and PWM follow Alex Wijn's SAT at 7beaaf5 (March 2026). ADR-306 (Proposed) keeps the margin as the setting `satpwmswitchmargin` with default 3.0.

Since then:
- Robert ordered that Alex's SAT is compared at its latest 5.x release: register entries 536 and 537; snapshot `other-projects/SAT-develop/` = tag 5.1.0, `9c2fad9`, 2026-11-25.
- Robert answered "B" (DM 1576215050258219349, 12:35:52 UTC, read back by otgw-architect; register entry 539): "first align the PWM and mode-switching rules with 5.1.0, then a beta".

Robert then chose "C" (DM 1576235189657669763, 13:55:54 UTC, read back by otgw-architect): "C, en sergeant d vragen om advies op elk punt van 5.1.0 en bespreken met hem per punt wat we doen." The benched build 313c4ba ships as the next beta, and every 5.1.0 point is discussed with Sergeant D one at a time. The question series is `OTGW-field-data/_comms/sat_5_1_0_question_series_2026-11-28.md`.

Under ADR-292 our implementation and the recorded decisions lead, and Alex's code is a reference. "B" therefore gives the direction; a rule of 5.1.0 that contradicts a recorded decision is not adopted until that decision is answered again.

The differences between 7beaaf5 and 5.1.0 are listed with citations in `docs/sat/alex-sat-5.1.0-delta.md` section 1. otgw-architect checked them against the register at main 557d82e (`OTGW-field-data/_comms/sat_5_1_0_register_conflicts_2026-11-28.md`):
- **No conflict:** (a) the underheat flame gate; (d1) enable only with a burning flame; (g) the hourly start limit.
- **Conflict:**
  - (b) heater off pauses PWM, and (f) restore in the off phase: entry 462 (Sergeant D).
  - (c) the held boiler floor for the duty: entries 528 (Robert) and 437, 438, 441, 443, 444 (Sergeant D).
  - (d2) a 300 s look-back on the request: entry 497 (Sergeant D).
  - (e) saturation needs a learned floor: entry 492, and it is tied to (c).
  - (h) a 600 s ignition wait: entry 496 (Sergeant D).
- **Kept deviation:** (i) a return temperature of 0.0 counts as missing: entry 532.

All 5.1.0 citations below are `custom_components/sat/<file>:<line>` at `9c2fad9`, opened by otgw-architect on 2026-11-28.

## Decision

Proposed Part 1: the three 5.1.0 rules that conflict with no recorded decision. Under Robert's "C", each of them is still put to Sergeant D first: (a) as question Q-51a, (d1) and (g) as notices. Part 1 is accepted only after those answers, and only if they agree. All other 5.1.0 differences wait as Open Questions; this ADR decides nothing about them.

1. **Underheat exit waits for an established flame (item a).**
   - The PWM-to-continuous underheat count (flow at or below request - 3.0 C, sustained 180 s; ADR-305 Decision 3) only runs once the flame has burned at least 180 s since the current flame-on edge.
   - A flame-off resets it. The earliest underheat exit is then about 360 s after ignition.
   - Source: 5.1.0 `heating_control.py:446`, `:481-486`, `const.py:17`.
   - It brings back register entry 456 (Sergeant D, 2026-11-21), which ADR-305 removed under entry 517.
   - With the saturation exit unchanged (300 s, no flame gate), the cold-start order of entry 492 holds again: saturation at about 300 s, before underheat at about 360 s.
2. **Continuous-to-PWM enable only while the flame burns (item d1).**
   - The overshoot count resets on every tick with the flame off. Source: 5.1.0 `heating_control.py:380-382`.
   - The rest of the enable rule stays as ADR-305 and ADR-306 have it: flow at or above this tick's request plus `satpwmswitchmargin` (default 3.0; entries 522, 525), for 60 s.
   - The reference stays this tick's request (entry 497). 5.1.0's 300 s look-back is Open Question (d2).
3. **The hourly PWM start limit (item g)** uses the configured cycles-per-hour setting. It counts the PWM ON starts of the last 3600 s, and that count survives a PWM exit.
   - Source: 5.1.0 `pwm.py:226`, `:102-112`. 7beaaf5 compared with `duty_cycle_seconds` (`pwm.py`, 7beaaf5), an upstream bug fixed in 5.1.0.
   - If the firmware already does this, it needs no change; otgw-poc-dev verifies.

**Kept deviations** (decided earlier by Robert or Sergeant D; 5.1.0 is not followed on these):
- the margin as the setting `satpwmswitchmargin`, default 3.0 (entries 522, 525; ADR-306);
- the per-system cold setpoint (ADR-199);
- the tolerance band (ADR-250, ADR-253, entry 402);
- the OFF state (entry 519, rule 4b);
- the maximum-modulation rules (entries 505, 514);
- a return temperature of exactly 0.0 counts as missing (entry 532).

The explicit guards (heater on, request above the cold setpoint, 300 s after hot water) are the same in 5.1.0 (`heating_control.py:364-378`, `:426-432`) and stay.

**What this changes in ADR-305:**
- Decision 2: enable only with a burning flame.
- Decision 3: the underheat exit gets the 180 s flame gate.

Decision 7 (plain restore) and Decision 8 (reset on every exit) stay until Open Questions (b), (c) and (f) are answered. Nothing in ADR-306 changes.

## Decision Contract

### Must

- Start the underheat count only when the flame has burned at least 180 s since the current flame-on edge; reset it on flame-off.
- Reset the continuous-to-PWM overshoot count on every tick with the flame off.
- Keep comparing the flow with this tick's request plus `satpwmswitchmargin` for the enable rule.
- Count PWM starts per rolling 3600 s against the cycles-per-hour setting, and keep the count across a PWM exit.

### Must Not

- Implement any item listed under Open Questions before it is answered and recorded in the register.
- Add a flame gate to the saturation exit (entry 492).
- Change the margin setting, its default or its migration (ADR-306).

### Exceptions

- None.

### Verification

- **Host test, underheat:** a PWM ON phase with flow 4 C under the request, starting at ignition, does not leave PWM before 360 s and does leave at 360 s. It must be red on the code before this ADR, which exits at 180 s.
- **Host test, enable:** flow 4 C over the request for 60 s with the flame off does not enable PWM; the same with the flame on does. It must be red on the code before this ADR.
- **Hourly limit:** a test or a cited code line shows the limit counts the starts of the last 3600 s and survives an exit.
- **Bench run** by otgw-bench-tester, with the log lines of both exits.

## Alternatives Considered

- **Adopt all of 5.1.0 at once (a literal reading of "B").** Rejected for now. Five items contradict recorded decisions (entries 462, 492, 496, 497, 528 and the live-flow entries). The ground-truth rule forbids resolving those silently; each gets its own question.
- **Keep ADR-305 as it is until every question is answered.** This is now the state for the next beta: Robert chose "C" (313c4ba ships). This ADR collects the answers for the release after it.
- **Take (d) whole, flame plus look-back.** Rejected: the look-back conflicts with entry 497.

## Consequences

**Positive:**

- A cold boiler right after ignition no longer counts as underheat: the burn is not cut short in its first minutes (entry 456).
- An overshoot can only enable PWM while the boiler really burns, so a hot-water tail or a flame-off cooling period cannot do it.
- Three rules now match the latest reference, and the rest is visible as open questions instead of silent drift.

**Negative:**

- A boiler that really cannot reach the request stays in PWM up to about 3 minutes longer on a cold start. Mitigation: the saturation exit (300 s) still ends it first in that case (entry 492).
- The rules are now half aligned: 5.1.0 for (a), (d1) and (g), ours for the rest until the questions are answered. Mitigation: each open item has a drafted question and an owner.

## Open Questions

- [ ] (a) Sergeant D confirms the underheat flame gate (Q-51a). Advice: adopt; it is his own entry 456.
- [ ] (d1) and (g): Sergeant D raises no objection to the notices (enable only with a burning flame; the hourly limit already matches 5.1.0, with entry 81, entry 89 and the count kept over a PWM reset (TASK-1401) kept).
- [ ] (k) Short underheat cycles count as "uncertain" in the end-of-cycle verdict, as 5.1.0 (`cycles/classifier.py:48-49`). otgw-poc-dev checked: our firmware has no duration test on a PWM underheat cycle (`SATcycles.ino:933-937`), and that verdict leaves PWM at flame-off (`:1034-1039`). Question Q-51k. Advice: adopt.
- [ ] (b) Heating demand stops: pause PWM as 5.1.0 (`heating_control.py:302`, `pwm.py:132-143`), or leave PWM as now (entries 12, 190, 462). Advice: pause after a short stop, leave PWM after a long one (more than 1 h). The 1 h limit is our idea, not his and not Alex's. Question Q-51b.
- [x] (c) PWM duty: the held boiler floor as 5.1.0 (`pwm.py:102-131`, `:155-165`, `:201-212`, `:288-296`), or the live flow as now (entry 528, Robert; entries 437, 438, 441, 443, 444, Sergeant D). **Answered 2026-11-28 by Robert van den Breemen (decision, DM 1576247783747231919), recorded by Agent: Claude (otgw-architect):** "A" (14:45:56 UTC), to BlueFlame's question 1576215743375351899, option A: "(c) niet overnemen: onze live-aanvoerduty blijft; de rest van 5.1.0 wel." He had the replay facts (BlueFlame's DM 1576215877102076052, 12:39 UTC) before he answered. The live-flow duty stays. Sergeant D gets a short notice with Robert's decision (Q-51c, FYI), because Robert asked for his advice on every point (DM 1576235189657669763).
- [ ] (d2) Enable reference: the highest request of the last 300 s as 5.1.0 (`heating_control.py:388`, `:407-415`), or this tick's request as now (entry 497, Sergeant D). Advice: this tick's request, until a field case shows false overshoots after a request drop. Question Q-51d2, Sergeant D.
- [x] (e) Saturation exit: only with a learned floor as 5.1.0 (`heating_control.py:495-505`, `:524-534`), or plain 300 s as now (entry 492). **Answered 2026-11-28 by Robert van den Breemen (decision, DM 1576247783747231919), recorded by Agent: Claude (otgw-architect):** not applicable. With (c) not adopted there is no learned floor, so the 5.1.0 evidence rule has nothing to use. The plain 300 s saturation exit stays (entry 492), and Q-51e is dropped.
- [ ] (f) Restore after a restart: resume in the off phase as 5.1.0 (`pwm.py:167-190`), or a plain restore as now (entry 462; ADR-305 Decision 7, ADR-306 clarification). Advice: resume in the off phase, no extra burn. Question Q-51f.
- [ ] (h) Ignition wait: 600 s as 5.1.0 (`pwm.py:262`, `const.py:18`), or 180 s as now (entry 496, Sergeant D). Advice: 180 s. Question Q-51h, Sergeant D.
- [ ] (i) Flame-off hold: confirm that the kept deviation of entry 532 (a return of exactly 0.0 counts as missing) stays next to 5.1.0's return-else-flow (`heating_control.py:605-613`). Entry 532 stays. The open part is the timing: use the flow as soon as the return is missing, as 5.1.0, or wait until the boiler is confirmed to have none, as now (entry 263). Advice: at once. Question Q-51i.

## Related Decisions

- **ADR-305**: amended in Decision 2 (enable only with a burning flame) and Decision 3 (underheat flame gate). Decisions 7 and 8 stay until (b), (c) and (f) are answered.
- **ADR-306**: unchanged; its margin setting is a kept deviation.
- **ADR-292**: governs the ranking; 5.1.0 is the reference, the register leads.
- **ADR-236**, **ADR-297**: their 180 s flame gate on the underheat exit returns in this form (entry 456).
- **ADR-199**, **ADR-250**, **ADR-253**: kept deviations.

## References

- Robert's DM 1576215050258219349 (2026-11-28 12:35:52 UTC, "B"), read back by otgw-architect; register entry 539.
- Register `docs/sat/decisions` at main 557d82e: entries 402, 437, 438, 441, 443, 444, 456, 462, 492, 496, 497, 505, 514, 519, 522, 525, 528, 532, 536, 537, 539.
- Alex Wijn's SAT tag 5.1.0, `9c2fad9a7cdb0bf03b58fd0de4b873599e00b541` (https://github.com/Alexwijn/SAT/releases/tag/5.1.0), local copy `other-projects/SAT-develop/`: `heating_control.py:364-382`, `:426-432`, `:446`, `:481-486`, `:495-505`, `:524-534`, `:605-613`; `pwm.py:102-112`, `:226`, `:262`; `const.py:17-18`.
- `docs/sat/alex-sat-5.1.0-delta.md` (origin/dev 2a337e39f), section 1.
- `OTGW-field-data/_comms/sat_5_1_0_register_conflicts_2026-11-28.md`.
