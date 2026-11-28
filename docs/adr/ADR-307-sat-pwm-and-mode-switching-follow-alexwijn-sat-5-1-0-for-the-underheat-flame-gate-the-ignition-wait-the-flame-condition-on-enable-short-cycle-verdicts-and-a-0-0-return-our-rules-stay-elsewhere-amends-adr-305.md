---
id: "ADR-307"
title: "SAT PWM and mode switching follow Alexwijn SAT 5.1.0 for the underheat flame gate, the ignition wait, the flame condition on enable, short-cycle verdicts and a 0.0 return; our rules stay elsewhere (amends ADR-305)"
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
  - "ignition wait 600 s"
  - "decision table 5.1.0"
components:
  - "SAT control mode switching"
symbols:
  - "satCycleCheckPwmExits"
  - "satCycleCheckOvershootSwitch"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-307 SAT PWM and mode switching follow Alexwijn SAT 5.1.0 for the underheat flame gate, the ignition wait, the flame condition on enable, short-cycle verdicts and a 0.0 return; our rules stay elsewhere (amends ADR-305)

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

ADR-305 (Accepted 2026-11-28) makes the switching between continuous and PWM follow Alex Wijn's SAT at 7beaaf5 (March 2026). ADR-306 (Proposed) keeps the margin as the setting `satpwmswitchmargin`, default 3.0. Since then, Robert has given four orders. All are recorded in the register; the DM ids were read back by otgw-architect.
- **Entries 536 and 537:** compare with Alex's latest 5.x release. The snapshot `other-projects/SAT-develop/` is now tag 5.1.0, `9c2fad9`, 2026-11-25.
- **Entry 540:** DM 1576235189657669763, "C". The benched build 313c4ba ships as the next beta, and 5.1.0 follows after it.
- **Entry 541:** DM 1576247783747231919, "A". Point (c) is not adopted: the live-flow duty stays.
- **Entry 542:** coordinator chat, 16:11 laptop time. This quote was relayed by the coordinator and not seen by otgw-architect: "Just update and align to 5.1.0. Only where you think we have a solution better in this firmware we use ours. Analysis 5.1.0 and our and you do what thinks best."

The differences between 7beaaf5 and 5.1.0 are listed in `docs/sat/alex-sat-5.1.0-delta.md` section 1. otgw-architect and otgw-poc-dev analysed them in `OTGW-field-data/_comms/sat_5_1_0_decision_table_2026-11-28.md`. The rule was: adopt 5.1.0 unless there is evidence that ours is better. The coordinator decided the open rows at 16:40.

Entry 542 outranks the per-point questions of entry 540. A Sergeant D entry that a decision below overrides is named in that decision, and Sergeant D is informed afterwards, not asked.

All 5.1.0 citations are `custom_components/sat/<file>:<line>` at `9c2fad9`, opened by otgw-architect on 2026-11-28. Firmware lines are otgw-poc-dev's, read at lane tip 313c4ba.

## Decision

SAT takes over five 5.1.0 rules for PWM and mode switching, and keeps its own rule on the other points. One point is decided by a replay.

**Adopted (step 1, in this order):**

1. **(a) The underheat exit waits for an established flame.** The PWM-to-continuous underheat count (flow at or below request - 3.0 C, held 180 s) only runs once the flame has burned at least 180 s since the current flame-on edge. A flame-off resets the count.
   - Source: 5.1.0 `heating_control.py:446`, `:481-486`.
   - Reason: at the start of each PWM burn the water is still warming up, so a low flow is not evidence of a weak boiler.
   - It brings back entry 456 (Sergeant D, 2026-11-21), which ADR-305 removed under entry 517.
   - The cold-start order of entry 492 holds again: saturation at about 300 s, then underheat at about 360 s.
2. **(h) Wait up to 600 s for ignition.** The one 180 s constant is split in two:
   - an ignition wait of 600 s (5.1.0 `pwm.py:262`, `const.py:18`);
   - a minimum burn and band wait of 180 s, unchanged (entry 376; `SATcontrol.ino:1411`, `:1421`, `:1614`).

   Reason: our 180 s wait starts the ON timer without a flame, so a slow or second ignition gets its burn cut short.

   This only works together with (a). While a phase waits for its flame, the underheat count already runs and would end PWM after 180 s. So (a) and (h) ship as one pair.

   Overrides entries 496 (Q-456d A) and 80 (Q35 A).
3. **(d1) Enable PWM only while the flame burns.** The overshoot count resets on every tick with the flame off.
   - Source: 5.1.0 `heating_control.py:380-382`.
   - Reason: an overshoot with the flame off is a cooling boiler, not one that cannot modulate low enough.
4. **(k) A PWM cycle shorter than 180 s that looked too cold is classified "uncertain".**
   - Source: 5.1.0 `cycles/classifier.py:39-49`, PWM branch only.
   - Reason: our firmware has no duration test here, so such a cycle ends PWM at flame-off (`SATcycles.ino:933-937`, `:1034-1039`).
5. **(i) A return temperature of exactly 0.0 counts as missing**, so the flame-off hold uses the flow. This implements entry 532.

**Kept as ours** (evidence or a recorded reason that ours is better or safer):

6. **(i, rest) The 5 min wait after boot keeps the previous setpoint** until the boiler is confirmed to have no return (entry 263). 5.1.0 uses the flow at once (`heating_control.py:605-613`). Reason: until the first return answer, "flow + 18" is a guess, and the previous setpoint is not.
7. **(g) The hourly start limit:** no change. It already works as 5.1.0 (`pwm.py:226`, `:102-112`). Three recorded choices stay:
   - heat pumps have no limit (entry 81);
   - a burn already going at PWM entry is never cut (entry 89);
   - the count is kept over a PWM reset (TASK-1401).
8. **(b) A stop of the heating demand ends PWM.** 5.1.0 pauses it instead (`heating_control.py:302`, `pwm.py:132-143`).
   - Reason: pausing would make the first heating after a pause continue in PWM, and would keep the PWM flag over a restart in the pause. That reverses ADR-305's "a heating start runs in continuous", which Robert accepted this morning.
   - It is also the highest-risk change: three code paths.
   - Revisit with a capture count (below).
9. **(f) After a restart, plain restore** (ADR-305 Decision 7 and the ADR-306 clarification). 5.1.0 resumes in the off phase from the last switch (`pwm.py:167-190`).
   - Reason: it needs a wall-clock switch time saved in flash, at about 3 to 4 writes per hour (otgw-poc-dev's estimate, not measured), and a valid time at boot. 5.1.0 keeps the time in Home Assistant state, without flash wear.
   - The gain is one burn per restart, and restarts are rare (entry 462).
   - Revisit together with (b).
10. **(c) The live-flow duty, and the plain 300 s saturation (e):** Robert, entry 541.
11. **Earlier deviations:**
    - the margin setting (entries 522 and 525, ADR-306);
    - the per-system cold setpoint (ADR-199);
    - the tolerance band (ADR-250, ADR-253, entry 402);
    - the OFF state (entry 519, rule 4b);
    - the maximum-modulation rules (entries 505 and 514).

**Decided by a replay (step 2):**

12. **(d2) The overshoot reference: the highest request of the last 300 s (5.1.0, `heating_control.py:388`, `:407-415`) or this tick's request (ours, entry 497).**
    - otgw-poc-dev replays the indigo and gruijter recordings with both rules, and counts the PWM entries that start within 5 min after a request drop.
    - The look-back is adopted only if every entry it removes later ended through underheat or saturation. Otherwise ours stays.
    - If it is adopted, entry 497 is overridden.

**What this changes in ADR-305:**
- Decision 2: enable only with a burning flame, and (d2) if the replay adopts it.
- Decision 3: the underheat flame gate.

Decisions 7 and 8 stay. Nothing in ADR-306 changes.

## Decision Contract

### Must

- Start the underheat count only when the flame has burned at least 180 s since the current flame-on edge; reset it on flame-off.
- Ship (a) and (h) together: an ignition wait of 600 s, and a minimum burn and band wait of 180 s.
- Reset the continuous-to-PWM overshoot count on every tick with the flame off.
- Classify a PWM underheat cycle shorter than 180 s as uncertain.
- Treat a return temperature of exactly 0.0 as missing.
- Keep: the 5 min boot wait with the previous setpoint, the hourly-limit choices, leaving PWM on a demand stop, the plain restore, and the live-flow duty.

### Must Not

- Pause PWM on a demand stop, or restore the PWM phase after a restart, before the coordinator decides on the capture count.
- Change the overshoot reference before the replay decides (d2).
- Raise the 180 s minimum burn or band wait together with the ignition wait.
- Change the margin setting, its default or its migration (ADR-306).

### Exceptions

- None.

### Verification

- **Host tests, red on the code before this ADR and green after:**
  - underheat from ignition does not exit before 360 s, and does exit at 360 s;
  - an ON phase that waits 400 s for its flame stays in PWM and then times its burn from the real ignition;
  - an overshoot with the flame off does not enable PWM;
  - a 120 s PWM underheat cycle is uncertain;
  - a 0.0 return gives the flow-based hold.
- **Unchanged:** the band still waits 180 s (entry 376 test unchanged).
- **Replays** of the indigo and gruijter recordings for (a), (d1) and the (d2) count.
- **Bench run** by otgw-bench-tester after the build slot, not before 18:55. Nothing from this ADR is in the 313c4ba beta.

## Alternatives Considered

- **Adopt every 5.1.0 point.** Rejected per point above:
  - (b) and (f) would reverse ADR-305's continuous start and add flash wear;
  - the rest of (i) would drop a safer default;
  - (c) was rejected by Robert.
- **Ask Sergeant D first on every point (entry 540).** Replaced by entry 542: the team decides, and Sergeant D is informed.
- **Adopt (d2) at once.** Rejected for now. Neither side has evidence; the replay decides.
- **Raise the shared 180 s constant to 600 s.** Rejected: it would also make the band wait 600 s (entry 376).

## Consequences

**Positive:**

- A burn in PWM is no longer cut short during warm-up or during a slow ignition.
- An overshoot can only enable PWM while the boiler really burns.
- A very short cycle no longer ends PWM on noise.
- Boilers that report a return of 0.0 get a sensible flame-off hold.
- Five rules match the latest reference. Each kept rule states its reason, and the open ones have a measurement attached.

**Negative:**

- A boiler that really cannot keep up stays in PWM up to about 3 min longer after ignition. Mitigation: saturation (300 s) still ends it first (entry 492).
- A boiler that does not ignite is timed out after 600 s instead of 180 s. Mitigation: the `no_ignition` alarm counts on its own and does not use this constant (otgw-poc-dev; its own threshold is not checked).
- The rules are partly aligned. Mitigation: (b), (f) and (d2) each have a named measurement and an owner.
- Sergeant D's entries 80, 263 (partly), 456 (revived), 496 and, if (d2) is adopted, 497 are overridden or revived without asking him first. Mitigation: BlueFlame informs him per step ("what we do and why, tell us if you see a reason"), and a reason he gives goes to the coordinator.

## Open Questions

- [ ] (d2) The replay on the indigo and gruijter recordings decides: the 300 s look-back is adopted only if it removes only entries that later end through underheat or saturation. Owner: otgw-poc-dev; the coordinator decides on the result.
- [ ] (b) and (f) are revisited after steps 1 and 2. The input is a capture count of heating restarts after a pause that overshoot into PWM within 10 min. Owner: the coordinator.
- [ ] (h) The `no_ignition` alarm threshold is checked, so that a 600 s wait and the alarm do not contradict each other. Owner: otgw-poc-dev.
- [x] (c) PWM duty: the held boiler floor as 5.1.0, or the live flow as now. **Answered 2026-11-28 by Robert van den Breemen (decision, DM 1576247783747231919), recorded by Agent: Claude (otgw-architect):** "A" (14:45:56 UTC, to BlueFlame's question 1576215743375351899): "(c) niet overnemen: onze live-aanvoerduty blijft; de rest van 5.1.0 wel." Register entry 541.
- [x] (e) The saturation exit with a learned floor. **Answered 2026-11-28 by Robert van den Breemen (decision, DM 1576247783747231919), recorded by Agent: Claude (otgw-architect):** not applicable after (c). The plain 300 s exit stays (entry 492).

## Related Decisions

- **ADR-305**: amended in Decision 2 (flame condition; (d2) if the replay adopts it) and Decision 3 (underheat flame gate). Decisions 7 and 8 stay.
- **ADR-306**: unchanged; the margin setting is a kept deviation.
- **ADR-292**: governs the ranking; the reference now follows the latest 5.x release (entries 536 and 537).
- **ADR-236**, **ADR-297**: their 180 s underheat flame gate returns in this form (entry 456).
- **ADR-199**, **ADR-250**, **ADR-253**: kept deviations.

## References

- Register `docs/sat/decisions` at main 8e3d57e: entries 80, 81, 89, 263, 376, 402, 456, 462, 492, 496, 497, 505, 514, 519, 522, 525, 532, 536, 537, 540, 541, 542.
- Robert's DMs 1576235189657669763 ("C") and 1576247783747231919 ("A"), both read back by otgw-architect. The 16:11 chat order is quoted as relayed (entry 542).
- Alex Wijn's SAT tag 5.1.0, `9c2fad9a7cdb0bf03b58fd0de4b873599e00b541` (https://github.com/Alexwijn/SAT/releases/tag/5.1.0), local `other-projects/SAT-develop/`:
  - `heating_control.py:302`, `:380-382`, `:388`, `:407-415`, `:446`, `:481-486`, `:605-613`;
  - `pwm.py:102-112`, `:132-143`, `:167-190`, `:226`, `:262`;
  - `cycles/classifier.py:39-49`; `const.py:18`.
- Firmware at lane tip 313c4ba (otgw-poc-dev's reading): `SATcontrol.ino:1411`, `:1421`, `:1609`, `:1614`; `SATcycles.ino:933-937`, `:1034-1039`.
- `docs/sat/alex-sat-5.1.0-delta.md` (origin/dev 2a337e39f); `OTGW-field-data/_comms/sat_5_1_0_decision_table_2026-11-28.md`.
