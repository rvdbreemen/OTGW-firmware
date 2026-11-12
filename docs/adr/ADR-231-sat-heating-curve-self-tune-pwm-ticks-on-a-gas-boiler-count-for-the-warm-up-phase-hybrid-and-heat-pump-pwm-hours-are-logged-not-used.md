---
id: "ADR-231"
title: "SAT heating curve self-tune: PWM ticks on a gas boiler count for the warm-up phase; hybrid and heat pump PWM hours are logged, not used"
status: "Accepted"
date: "2026-11-12"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-205"
  - "ADR-228"
topics:
  - "heating curve self-tune"
  - "PWM hours"
  - "clean hour exclusions"
  - "heat source"
aliases:
  - "PWM hours count for warm-up"
  - "self-tune in PWM"
components:
  - "self-tune PWM tick rule"
symbols:
  - "satTuneTickReason"
  - "satTuneTickPhase"
  - "satTuneAddTick"
  - "satGetEffectiveHeatingSource"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-231 SAT heating curve self-tune: PWM ticks on a gas boiler count for the warm-up phase; hybrid and heat pump PWM hours are logged, not used

## Status

Accepted, 2026-11-12.

Amends ADR-205 decision 2 (the clean-hour exclusions) and ADR-228 decision 1 (the phase per tick)
for ticks in SAT (Smart Autotune Thermostat) PWM (pulse-width modulation) only; every other decision of both records stays in force. Both open questions are answered by
Sergeant D (entries 322, 323). Robert accepted this record on 2026-11-12 (DM msg 1570402646186139730, "A", a reply to
the accept question 1570337513208414273).

**Decision Maker:** Sergeant D (SAT design owner) for the rule: entries 316, 318 and 319 of
`docs/sat/sergeant-d-corrections.md`, and entries 322 (Q-1817c A) and 323 (Q-1817d A), 2026-11-12. Robert (maintainer) accepts or rejects this record;
he chose Q-1796b B as well (DM msg 1570171160111620147, relayed by the coordinator, TASK-1817 description).

## Status History

```yaml
status_history:
  - date: 2026-11-12
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-12
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-205
    changed_via: adr-kit lifecycle
  - date: 2026-11-12
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-228
    changed_via: adr-kit lifecycle
  - date: 2026-11-12
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: "Accepted by Robert: DM msg 1570402646186139730 (2026-11-12 11:39:27 UTC), \"A\", reply to accept question 1570337513208414273"
    changed_via: adr-kit lifecycle
```

## Context

The heating curve self-tune (ADR-205, ADR-228) learns from clean hours only. ADR-205 decision 2 drops a whole hour on the
first excluded tick, in a fixed order (`satTuneTickReason()`, `SATcontrol.ino:6018-6031`): SAT not active, hot water, sun
unknown or at least 12 degrees, **SAT in PWM**, flame off while CH is on, relative modulation (MsgID 17) at or below the
boiler minimum (MsgID 15 low byte, 5 % when missing), lost room temperature. A gas boiler at low load runs in SAT PWM for
hours; such a house never gives the tuner a clean hour. Sergeant D's field basis for entry 316: gruijter 10 Nov (PWM
88 %, no clean hour) and 11 Nov overnight (0 of 20 hours counted).

Sergeant D decided:

- Entry 316 (Q-1796b, 2026-11-11, msg 1570167372474617888): "Q-1796b: B": on gas boilers, PWM hours count for the warm-up
  phase; the signal is the same (setpoint minus curve, before the PWM override).
- Entry 318 (Q-1817a, 2026-11-12, msg 1570327834336165920): "Q-1817a: A": on a gas boiler every PWM tick goes to the
  warm-up phase, whatever the PID (proportional-integral-derivative controller) error.
- Entry 319 (Q-1817b, 2026-11-12, msg 1570328090490699818): "A-1817b: B" (read as Q-1817b: B, a reply to that post): a
  hybrid (satsource 3) in SAT PWM follows the heat-pump rule of entry 205: PWM hours are logged, marked as such, and not
  used by the tuner. Entry 82 (a hybrid in PWM uses gas boiler PWM timing) is about the PWM timing and stays.

The self-tune is our own design, not a thermo-nova port (entry 298); there is no thermo-nova code to cite for it.

**Gap found while preparing this record (measured, 2026-11-12).** Removing only the PWM exclusion does not let a PWM hour
count, because a PWM hour hits two later exclusions by its nature: flame off between the pulses, and the modulation at or
below the minimum. On gruijter's capture of 10-11 Nov (beta.171, 20 tuner hours, `capture-gruijter-1111`; relative
modulation from the boiler's MsgID 17 Read-Ack in `otframes.log`, minimum 5 % because the boiler answers MsgID 15
Unknown-DataID; scripts and output in the TASK-1817 notes):

| Rule for a PWM tick on a gas boiler | Hours that count, of 20 | What still excludes the two full PWM hours |
|---|---|---|
| A: skip `pwm`, `flame_off` and `min_mod` | 1 | hour 18: 2 hot water ticks; hour 19 counts |
| B: skip `pwm` only | 0 | `min_mod` (61 and 56 of 120 PWM ticks at or below 5 %) |

This choice is not covered by entries 316, 318 or 319. It was asked as Q-1817c (#dev-sat-mqtt msg 1570334185896411147,
2026-11-12 07:07 UTC, Coordinated Universal Time), with our advice A.

## Decision

On a gas boiler, a tick in SAT PWM is a warm-up tick of the self-tune, and the PWM-related exclusions do not drop its
hour; for a heat pump and a hybrid, a PWM tick keeps excluding the hour (`pwm`) and stays logged.

1. **Heat source.** The rule follows `satGetEffectiveHeatingSource()` (`SATcontrol.ino:309-314`): `SAT_SRC_GAS_BOILER`
   gets the PWM rule below; a heat pump and a hybrid keep `pwm` as an exclusion (entries 205 and 319). AUTO (heat source left on automatic) resolves
   to gas boiler here as it does for control (TASK-943), so an unconfigured heat pump or hybrid gets the gas rule
   (entry 323, Q-1817d A).
2. **Exclusions for a PWM tick on a gas boiler** (amends ADR-205 decision 2). The tick is not excluded for being in PWM.
   It is also not excluded for flame off while CH is on, nor for the modulation at or below the minimum (entry 322,
   Q-1817c A). SAT not active, hot water, the sun rule, lost room temperature, a coefficient
   change and the minimum tick count keep excluding as before.
3. **Phase** (amends ADR-228 decision 1). A PWM tick on a gas boiler goes to the warm-up phase whatever the PID error
   (entry 318). A tick outside PWM keeps the deadband rule.
4. **Signal.** Unchanged (ADR-205 decision 1, ADR-228 decision 2): `clamp(pidRaw, 10 C, effective maximum) - curve`,
   the PID request before the PWM duty and the ignition override (`SATcontrol.ino:7593-7596`), as entry 316 says.
5. **Logging.** `SATTUNEH` keeps `pwmticks=` (TASK-1551); an hour excluded for PWM on a heat pump or hybrid still logs
   `reason=pwm`.

## Decision Contract

### Must

- Decide the PWM rule from the configured heat source (`satGetEffectiveHeatingSource()`, AUTO = gas boiler, entry 323),
  not from a detected source.
- Put every PWM tick on a gas boiler in the warm-up phase, whatever its PID error.
- Keep `reason=pwm` for PWM ticks of a heat pump or a hybrid, and keep counting their `pwmTicks`.
- Keep the signal of a PWM tick as the PID request before the PWM override.

### Must Not

- Let a PWM tick of a heat pump or a hybrid count for the tuner.
- Skip the hot water, sun, room-lost, coefficient-change or tick-count exclusions for a PWM tick.
- Change the phase rule for ticks outside PWM.

### Exceptions

- None.

### Verification

- A host test on the real tuner (`tests/test_sat_curve_selftune.py`): a PWM hour on a gas boiler with flame-off and
  low-modulation ticks counts and adds its ticks to the warm-up phase on the fix, and is excluded with `reason=pwm` on
  the old code; the same hour with satsource heat pump or hybrid stays excluded with `reason=pwm`; a non-PWM tick near
  the target still goes to the floating phase.

## Alternatives Considered

- **B: drop only the `pwm` exclusion.** Rejected by Sergeant D (entry 322): on the measured field data it gains 0
  hours, so entry 316 would change nothing in practice.
- **AUTO as unknown, PWM hours excluded until a heat source is set** (Q-1817d B). Rejected by Sergeant D (entry 323).
- **Split PWM ticks by the PID error** (Q-1817a option B). Rejected by Sergeant D (entry 318).
- **Use the gas rule for hybrids** (Q-1817b option A, our advice then). Rejected by Sergeant D (entry 319).
- **Do nothing.** Rejected: Robert and Sergeant D both chose B on Q-1796b; low-load gas houses never tune.

## Consequences

**Positive:**

- A gas boiler that runs in PWM at low load can tune its heating curve: 1 of 20 hours on the measured night, where today
  0 of 20 count.
- Heat pumps and hybrids keep the conservative rule, so a tuner change never comes from PWM data they are not designed
  for (entry 205).

**Negative:**

- A PWM hour mixes pulses and pauses; its signal is the PID request, not the flow the house got. Sergeant D chose this
  (entry 316); the 3-day mean and the 0.1 step limit the effect of one such hour.
- Hours with a mix of PWM and continuous ticks can still drop on `min_mod` from their continuous ticks (hours 8, 9 and 11
  of the measured night). That is the existing low-load rule, unchanged here.
- An unconfigured heat pump or hybrid (`AUTO`) gets the gas rule and can learn from PWM hours it is not designed for
  (entry 323, accepted consequence). Mitigation: the setup asks for the heat source.

## Open Questions

- [x] Q-1817c (Sergeant D): for a PWM tick on a gas boiler, do `flame_off` and `min_mod` also not exclude the hour (A,
  our advice), or does only the `pwm` exclusion go (B)? Asked in #dev-sat-mqtt msg 1570334185896411147.
  — **Answered 2026-11-12 by Sergeant D (relayed by otgw-firmware-d3):** A. Sergeant D, #dev-sat-mqtt msg 1570335940474310677 (2026-11-12 07:14:23 UTC): "The answer is A" (entry 322). On a gas boiler a PWM tick also skips flame_off and min_mod; sun, hot water, lost room temperature, coefficient change and tick count still exclude.
- [x] Q-1817d (Sergeant D, then Robert): with the heat source on `AUTO`, does the tuner follow the control default and
  treat the system as a gas boiler (A, our advice: one rule everywhere; `satGetEffectiveHeatingSource()` already gives
  AUTO the gas control path, TASK-943, because OpenTherm carries no reliable source signal), or does it treat AUTO as
  unknown and keep excluding PWM hours until the user picks a source (B: never learns from PWM data of an unconfigured
  heat pump, but an unconfigured gas boiler, the common case, gains nothing until it is set)? Not from Sergeant D's
  entries; our design choice so far.
  — **Answered 2026-11-12 by Sergeant D (relayed by otgw-firmware-d3):** A. Sergeant D, #dev-sat-mqtt msg 1570336083177242756 (2026-11-12 07:14:57 UTC): "The answer is A" (entry 323). AUTO counts as a gas boiler for the self-tune as for control; an unconfigured heat pump or hybrid learns from PWM hours (accepted consequence).

## Related Decisions

- **ADR-205 (heating curve self-tune):** decision 2 (clean hour) is amended for PWM ticks on a gas boiler.
- **ADR-228 (warm-up and floating phases):** decision 1 (phase per tick) is amended for PWM ticks on a gas boiler.
- **ADR-230 (floating phase needs one hour):** unchanged; PWM ticks on a gas boiler never reach the floating phase.

## References

- `docs/sat/sergeant-d-corrections.md` entries 205, 298, 316, 318, 319; `docs/sat/open-questions.md` Q-1817c.
- `src/OTGW-firmware/SATcontrol.ino:6018-6031` (`satTuneTickReason`), `:6035-6039` (`satTuneTickPhase`),
  `:6077-6088` (`satTuneAddTick`), `:7580-7598` (the tick input), `:309-314` (`satGetEffectiveHeatingSource`).
- `src/OTGW-firmware/SATcurveTune.h:18` (`SAT_CT_DEFAULT_MIN_MOD 5.0f`).
- TASK-1817 notes: the measurement on `capture-gruijter-1111` (beta.171, 10-11 Nov 2026).
