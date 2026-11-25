---
id: "ADR-297"
title: "SAT's underheat exit counts only once the flame has burned 180 s; the at-max exit keeps only the 60 s start gate (amends ADR-236)"
status: "Proposed"
date: "2026-11-25"
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
aliases:
  - "underheat flame gate"
  - "entry 456 flame gate"
  - "amends ADR-236"
  - "TASK-3008"
components:
  - "SAT underheat exit flame gate"
symbols:
  - "SAT_UNDERHEAT_FLAME_MIN_MS"
  - "satCycleCheckPwmExits"
  - "satCycleGetFlameOnStartMs"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-297 SAT's underheat exit counts only once the flame has burned 180 s; the at-max exit keeps only the 60 s start gate (amends ADR-236)

## Status

Proposed, 2026-11-25. Guideline-level per ADR-080: verified by a host test, no evaluate.py gate. Amends ADR-236 (Accepted 2026-11-25) in three points; the rest of ADR-236 stands. Release 2.0.0 (entries 456, 463, 484).

**Decision Maker:** Sergeant D, who decides SAT behaviour (ADR-292), in entries 456 and 463 (2026-11-21). Robert accepts this record.

## Status History

```yaml
status_history:
  - date: 2026-11-25
    status: Proposed
    changed_by: "Agent: Claude (otgw-architect)"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context

ADR-236 (Accepted 2026-11-25) records Sergeant D's entry 448: SAT (Smart Autotune Thermostat) leaves PWM (pulse width modulation) for continuous only when the flow stays at or below the request minus 3.0 C for 180 s, with heating demand. Two later entries were not in its text. That was the architect's omission: the record was written from entry 448 without searching for the entries that amend it.

- **Entry 456 (Q-448a, answer A, 2026-11-21):** "the 180 s only starts counting once the flame has burned at least 180 s". The reason as posted: "In the first minutes after ignition the flow is always low, so this prevents a false exit right after the start. The exit can come at the earliest about 6 min after ignition." It amends entry 448. It also says the 60 s start gate of entries 385/387 becomes redundant for this exit, and that keeping it in the code is the implementer's call.
- **Entry 463 (Q-456g, answer A, 2026-11-21):** the at-max rule (entry 256) keeps its current gate. Only the 60 s start sequence (ADR-252) holds it back; the 180 s flame gate of entry 456 applies to the underheat exit only.
- **Entry 484 (Q-466a, answer D, 2026-11-25):** keep these PWM/continuous rules for 2.0.0 and add nothing.

**ADR-236 contradicts entry 456 in two places.** Its Decision says the exit checks "heating demand, not the flame", and its Must Not forbids "a flame condition that thermo-nova does not have. If one is wanted, it goes to Sergeant D first." Sergeant D decided exactly that condition in entry 456. ADR-236 is Accepted and therefore immutable, so this record amends it.

**References** (rank 4 under ADR-292, compared, not leading):

- thermo-nova 7beaaf5 has no flame gate on this exit (`custom_components/sat/heating_control.py:331-354`).
- Alex Wijn's SAT develop ba515c8 adds it (`heating_control.py:418-422`, `:450-455`; entry 456).

**The implementation in progress.** otgw-dev's TASK-3008 lane (working tree `wt-d3-docs1`, not yet committed) has `SAT_UNDERHEAT_FLAME_MIN_MS = 180000` (`SATcycles.ino:51`). It clears the underheat start while the flame has burned less than that since `satCycleGetFlameOnStartMs()` (`:1497-1505`).

## Decision

This ADR amends ADR-236 in three points. Everything else in ADR-236 stands: the margin of 3.0 C, the hold of 180 s, the PWM `ON` phase only, heating demand, the hot-water guards and the missing-flow rule.

1. **Flame gate (entry 456).** A tick counts toward the 180 s underheat hold only when the flame burns and has burned at least 180 s since its current flame-on edge. Before that, and on every flame-off, the hold is cleared. The earliest underheat exit is therefore about 360 s after ignition. "The current flame-on edge" is an interpretation (ours) recorded in entry 456: a flame-off resets the gate.
2. **This replaces ADR-236's Must Not bullet on a flame condition.** The flame gate is that condition, decided by Sergeant D. ADR-236's statement that the exit has "no flame check" no longer holds for this gate. Heating demand stays the demand condition, as ADR-236 says.
3. **The at-max exit is unchanged (entry 463).** It keeps only the 60 s start gate of ADR-252; the 180 s flame gate does not apply to it, nor to the saturation exit or the end-of-cycle verdict.
4. **The ADR-252 start gate stays in the code.** Entry 456 leaves this to the implementer. For the underheat exit it is redundant, because the 180 s flame gate is stricter than the 60 s start gate. It still guards the other exits, so removing it would change them. Keeping it is the minimal change.

## Decision Contract

### Must

- Clear the underheat hold on every tick where the flame is off, or has burned less than 180 s since its current flame-on edge.
- Take the flame-on time from the edge of the flame burning now (the cycle tracker's flame-on time), so a flame-off resets the gate.
- Keep the at-max exit, the saturation exit and the end-of-cycle verdict gated by the ADR-252 start gate only.
- Keep the heating-demand condition of ADR-236 in the underheat count.

### Must Not

- Apply the 180 s flame gate to the at-max exit (entry 463).
- Count the 180 s flame gate from the start of the PWM `ON` phase or from the heating start instead of the flame-on edge.

### Exceptions

- None.

### Verification

- **Host test** `tests/test_sat_overshoot_sustain.py` (TASK-3008), compiling the real `satCycleCheckPwmExits()`:
  - a flow 3.0 C under the request from the moment of ignition gives no exit before about 360 s, and an exit at the first tick at least 180 s after the flame has burned 180 s;
  - a flame-off inside the first 180 s resets the gate;
  - at the clamped maximum the at-max exit fires after the 60 s start gate, before the flame gate would allow the underheat exit (entry 463);
  - the cases fail on the code before TASK-3008.
- **Bench:** a cold start shows the underheat exit no earlier than about 6 min after ignition.

## Alternatives Considered

- **Supersede ADR-236 with a full rewrite.** Rejected: only three points change. A full copy would repeat ADR-236's evidence and invite drift. The project amends in place the same way elsewhere (ADR-253 for ADR-250, ADR-254 for ADR-249).
- **Leave ADR-236 as it is and rely on the register.** Rejected: ADR-236's Must Not would then forbid the code that entry 456 asks for, and a reviewer reading the ADR would flag a correct implementation.
- **Drop the ADR-252 start gate for the underheat exit.** Not chosen: allowed by entry 456, but it saves nothing and touches a shared gate.

## Consequences

**Positive:**

- The ADR record matches the register again: entries 448, 456 and 463 together.
- No false underheat exit in the warm-up minutes after ignition.

**Negative:**

- **A real underheat is caught later:** at the earliest about 6 min after ignition instead of 3 min. Mitigation: the saturation exit and the at-max exit are not delayed by the flame gate.
- **The meaning of ADR-236 now spans two records.** Mitigation: the README entry of ADR-236 points here.

## Open Questions

- [ ] Robert accepts this amendment, which records entries 456 and 463 against the Accepted ADR-236.

## Related Decisions

- **ADR-236** (Accepted): amended by this ADR in the three points above; the rest stands.
- **ADR-252** (PWM-first start and 60 s start gate): unchanged; still gates the at-max, saturation and end-of-cycle exits.
- **ADR-292** (SAT ground truth): the recorded entries decide; Alex Wijn's SAT and thermo-nova are references.
- **ADR-296** (Proposed, one PWM ON clock per flame): independent; it does not change when an exit fires.

## References

- Sergeant D entries 448, 456, 463 and 484, `docs/sat/decisions/sergeant-d-corrections.md` (OTGW-sat-decisions origin/main 67f489c).
- thermo-nova 7beaaf5, `custom_components/sat/heating_control.py:331-354`; Alex Wijn's SAT ba515c8, `heating_control.py:418-422`, `:450-455`, as cited in entry 456.
- otgw-dev's TASK-3008 working tree (`wt-d3-docs1`, uncommitted): `src/OTGW-firmware/SATcycles.ino:47-51`, `:1494-1512`.
- TASK-3008.
