---
id: "ADR-252"
title: "Every SAT heating start runs in PWM first and no PWM to continuous switch fires within 60 s after the flame lights"
status: "Accepted"
date: "2026-11-17"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-254"
topics:
  - "sat"
  - "pwm"
  - "control mode"
  - "heating start"
aliases:
  - "pwm first"
  - "pwm-first start"
  - "start gate"
  - "start sequence"
  - "entry 385"
components:
  - "satPwmFirstStart"
  - "satStartGateActive"
symbols:
  - "satPwmFirstStart"
  - "satStartGateActive"
  - "satStartGateOpen"
  - "SAT_START_GATE_MS"
  - "satRestoreDeferredStep"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-252 Every SAT heating start runs in PWM first and no PWM to continuous switch fires within 60 s after the flame lights

## Status

Accepted, 2026-11-17.

## Status History

```yaml
status_history:
  - date: 2026-11-16
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-17
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-254
    changed_via: adr-kit lifecycle
  - date: 2026-11-17
    status: Accepted
    changed_by: "Robert van den Breemen (decision, in session 2026-11-16 23:03, relayed by the coordinator session), recorded by Agent (d3)"
    reason: "Accepted: decision Robert van den Breemen (2026-11-16 23:03, coordinator session), recorded by Agent (d3) after the four verification gates passed"
    changed_via: adr-kit lifecycle
```

## Context

SAT (Smart Autotune Thermostat) chooses between two control modes: PWM (pulse width modulation: the boiler is switched on and
off in phases) and continuous (the boiler modulates on a setpoint). Thermo-nova, the SAT code this firmware ports, starts every
heating start in continuous: after a heater off it disables PWM (`heating_control.py:229-231` at commit 7beaaf5), after a boot
PWM is off unless a saved state restores it (`pwm.py:63`, `:89`, `:112-114`), and PWM is enabled only on a sustained overshoot
(`heating_control.py:262-305`). The firmware mirrored that until TASK-1942.

Sergeant D, who decides SAT behaviour, decided otherwise (`docs/sat/sergeant-d-corrections.md`):

- entry 385: every heating start runs in PWM first; PWM or continuous is chosen after the start sequence;
- entry 387: the reasons: (1) after a restart SAT continues in PWM, (2) a start gets a short warm-up of the boiler components,
  which means less thermal stress;
- entry 389 (Q-385a A): the start phase ends 60 s after the flame lights;
- entry 391 (Q-385b A): also with a manual continuous control mode;
- entry 394 (Q-385c A): inside those 60 s SAT stays in PWM; the existing rules decide after them.

A cold start usually has the request at the maximum setpoint on its first tick. Without a gate the existing max-setpoint exit
(the copy of `heating_control.py:203-204`, `SATcontrol.ino` near the cold cutoff) would leave PWM on that tick, so most cold
starts would get no PWM warm-up at all.

## Decision

A heating start enters PWM through `satSetControlMode()`, and a start gate keeps every PWM to continuous exit quiet until 60 s
after the first CH flame-on of that start.

- A heating start is the heater going from off to on in the control tick, SAT being switched on while no flame burns, and a boot.
  `satPwmFirstStart()` enters PWM for every requested control mode (auto, continuous, pwm).
- PWM is entered only on a tick with a known flow temperature; until then the continuous start sequence of entry 348 runs. Right
  after a boot the flow is unknown, PWM would idle and the plain request would light the boiler with no flame-off hold and no
  no-return flood (found by `tests/test_sat_loop_integration.py`). The first PWM phase is ON (entry 43, `pwm.py:154-155`).
- `satStartGateActive()` is true from that entry until `SAT_START_GATE_MS` (60 000 ms) after the first CH flame-on of the start.
  While it is true, the max-setpoint exit, the underheat exit (ADR-236), the saturation exit and the underheat verdict at flame off
  do not fire; their timers keep counting, so a condition that lasted through the window fires on the first tick after it.
- A flame already burning when SAT takes over is no heating start (the mode stays as it is), also after an OpenTherm loss, a trip or
  a fallback lift: `satDisable()` resets the flame-off flag.
- A saved PWM flag is restored after a restart through `satRestoreDeferredStep()`, once the heater runs and the flow is known; never at
  `initSAT()` while the heater may be off. It does not wait for MsgID 3 (ADR-251, entries 392/393).

The firmware differs from thermo-nova here on purpose.

## Decision Contract

### Must

- Enter PWM at every heating start through `satSetControlMode()`, for every requested control mode.
- Enter it only on a tick with a known flow temperature, with the first PWM phase ON in that tick.
- Keep the max-setpoint, underheat, saturation and end-of-cycle underheat exits quiet until 60 s after the first CH flame-on of the start.
- Leave the mode as it is when SAT takes over while a flame burns.
- Leave PWM on a heater off as before (ADR-199).

### Must Not

- Put PWM on while the heater is off (no PWM at `initSAT()`).
- Reset the exit timers when the gate ends.
- Wait for a MsgID 3 reply before the PWM entry or the restore (ADR-251).

### Exceptions

- The 60 s boot silence of entry 388 comes first: nothing, mode logic included, runs before it ends.

### Verification

- `tests/test_sat_pwm_first_start.py` (real SAT sources in the loop harness): 14 tests green on the fix, 8 red on origin/dev
  2268516db (cases a, a2, b, c, d, e, f, i), with mutant tests for the flame-off guard, the resume reset and the flow check.
- `tests/test_sat_loop_integration.py` (21 tests, incl. the no-return flood at a PWM-first start and the boot silence).
- Bench (simulator on the device, not hardware): an OTGW (OpenTherm Gateway) Classic bench unit with SAT's built-in boiler simulation on showed
  `SATMODE from=1 to=2`, `Control mode: PWM (heating start)` with duty 1.00 at the heater off to on edge after the boot silence,
  then the simulated flame; the first PWM to continuous switch came 90 s after that flame-on. The behaviour with a real boiler is
  not verified.

## Alternatives Considered

- **Keep thermo-nova's continuous start.** Rejected: Sergeant D decided PWM first (entries 385, 387), and he ranks above the
  thermo-nova code (ADR-196).
- **End the start phase with the first PWM ON phase (Q-385a option B).** Rejected by entry 389: an ON phase lasts at least 180 s
  (`pwm.py:53`) and up to 3600 / cycles per hour, not the "small warm-up" of entry 387.
- **Let the normal exits act inside the 60 s (Q-385c option B).** Rejected by entry 394: a cold start at the maximum setpoint would
  leave PWM on its first tick.

## Consequences

**Positive:**

- Every start gets the PWM warm-up Sergeant D asked for, also a cold start at the maximum setpoint.
- A restart continues in PWM through the deferred restore.
- The no-return start-up handling (flame-off hold, 20 s flow flood) runs at the ignition of a PWM start.

**Negative:**

- A start that clearly needs continuous stays in PWM for at least 60 s after the flame lights. Mitigation: the exit timers keep
  counting, so the switch follows on the first tick after the window.
- Right after a boot the start waits for the first flow reading; until then the entry 348 continuous start runs.
- The PWM-first start was seen only with SAT's boiler simulation on a bench unit, not with a real boiler; the timing rules are proven
  by host tests. Field captures of the first starts after the release are the hardware check.

## Open Questions

- [x] The three open points of entry 385 (when the start phase ends, whether it also applies with manual continuous, which
  exits may fire inside it). — **Answered 2026-11-16 by Sergeant D:** entries 389, 391 and 394.

## Related Decisions

- **ADR-196** (SAT ground truth): Sergeant D's entries rank above the thermo-nova code.
- **ADR-199** (SAT cold state and CH rule): a heater off still leaves PWM.
- **ADR-236** (PWM to continuous after 60 s of underheat, Proposed): that exit is held by the start gate.
- **ADR-251** (MsgID 3 plays no part in PWM): the PWM-first entry and the restore do not wait for MsgID 3.

## References

- TASK-1942; `docs/sat/sergeant-d-corrections.md` entries 348, 385, 387, 389, 391, 394.
- `src/OTGW-firmware/SATcontrol.ino`: `satPwmFirstStart()`, `satStartGateActive()`, `satStartGateOpen()`, `satRestoreDeferredStep()`;
  `src/OTGW-firmware/SATcycles.ino`: `satCycleCheckPwmExits()`, `satCycleApplyEndOfCycle()`.
- thermo-nova `custom_components/sat` at 7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb: `heating_control.py:203-204`, `:229-231`,
  `:262-305`; `pwm.py:53`, `:63`, `:89`, `:112-114`, `:154-155`.
