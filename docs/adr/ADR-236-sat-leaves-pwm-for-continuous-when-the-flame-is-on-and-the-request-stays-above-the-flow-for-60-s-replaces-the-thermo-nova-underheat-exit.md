---
id: "ADR-236"
title: "SAT leaves PWM for continuous when the flame is on and the request stays above the flow for 60 s (replaces the thermo-nova underheat exit)"
status: "Proposed"
date: "2026-11-14"
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
  - "PWM underheat exit"
  - "entry 342 exit"
  - "PWM to continuous"
  - "Q-342a"
components:
  - "SAT PWM to continuous exit"
symbols:
  - "satCycleCheckPwmExits"
  - "SAT_UNDERHEAT_SUSTAIN_MS"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-236 SAT leaves PWM for continuous when the flame is on and the request stays above the flow for 60 s (replaces the thermo-nova underheat exit)

## Status

Proposed, 2026-11-14. Guideline-level per ADR-080: verified by host tests, no evaluate.py gate.

## Status History

```yaml
status_history:
  - date: 2026-11-14
    status: Proposed
    changed_by: "Agent (d3)"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context

SAT (Smart Autotune Thermostat) runs the boiler either continuously or in PWM (pulse width modulation: timed ON and OFF
phases for a low heat demand). Thermo-nova, the reference implementation pinned at 7beaaf5, leaves PWM for continuous on
a sustained underheat: while the PWM status is ON, the flow at or below the request minus 3.0 C for 180 s
(`heating_control.py:308-356`, `cycles/const.py:11` `UNDERSHOOT_MARGIN_CELSIUS -3.0`, `const.py:22`
`UNDERHEAT_SUSTAIN_SECONDS 180.0`). The firmware ported that rule (`SATcycles.ino`, `satCycleCheckPwmExits()`).

Sergeant D, who decides SAT behaviour, replaced it with his own rule: flame on and the request above the boiler
temperature for 60 s, then continuous (corrections entries 339 and 342). Before building it, TASK-1933 simulated the
rule against the old one (1600 runs, thresholds committed first, `docs/sat/sim-1933/`). By the pre-registered rule it
failed two house classes on toggling (up to 12.5 PWM re-entries per day, limit 4.0); a post-hoc trace pointed at the
single-node plant model (the return drops below room air after an ignition) and one tester's captures do not show the
dip (request above flow at 60-90 s after an ON start in 0-5 % of the ticks against 25-35 % in the model). Asked with
those results (Q-342a), Sergeant D chose to build it now and watch the field (entry 350).

## Decision

While SAT is in PWM and the PWM phase is ON, it switches to continuous when the flame is on and the requested setpoint
is above the flow temperature for 60 s, timed from the first qualifying control tick; this replaces the flow <= request
- 3.0 for 180 s rule. The gates stay those of thermo-nova: not during hot water or the 300 s after it, not in the PWM
OFF phase, a missing flow temperature clears the timer, and the saturation exit (300 s without off time) is unchanged.

## Decision Contract

### Must

- Count a tick only with the flame on and the request strictly above the flow (`satIsFlameOn() && requestedSetpoint >
  flowTemp`); any other tick clears the timer.
- Fire on the first control tick at least `SAT_UNDERHEAT_SUSTAIN_MS` (60 000 ms) after the first qualifying tick.
- Keep the PWM ON, hot-water and 300 s guards and the saturation exit as they are.

### Must Not

- Count a tick with the flame off, or with the flow equal to or above the request.
- Use the 3.0 C margin for this exit (`SAT_UNDERSHOOT_MARGIN_C` stays for the cycle classifier only).

### Exceptions

- None.

### Verification

- `tests/test_sat_overshoot_sustain.py` compiles the real `satCycleCheckPwmExits()`: the exit fires 60 s after the first
  tick at 30 s and 60 s intervals, a flow 0.5 C below the request counts, a flow equal to the request does not, the flame
  off never counts. On the code before TASK-1933 13 cases fail; a mutant without the flame condition fails the flame-off
  case.
- Bench (TASK-1933 AC6, bench-tester): the exit fires as specified in a simulated warm-up, no reboot.
- Field: the testers' captures are watched for PWM re-entries shortly after an ignition (hot water and slow ignition
  first), as promised to Sergeant D in entry 350.

## Alternatives Considered

- **Keep thermo-nova's exit (flow <= request - 3.0 for 180 s).** Rejected by Sergeant D (entry 342). It leaves PWM late:
  3 C below the request for 3 minutes.
- **Rerun the failing classes with a better plant model first (Q-342a option B).** Not chosen by Sergeant D (entry 350);
  the field data already shows no post-ignition dip on the one house measured.
- **A longer hold, for example 120 s (option C).** Not chosen; it would change his rule.

## Consequences

**Positive:**

- PWM ends as soon as the boiler cannot reach the request with the flame burning for a minute, instead of after 3
  minutes at 3 C below it.

**Negative:**

- In the simulation the rule toggles (PWM re-entry within 10 minutes) up to 12.5 times a day on plants like the
  gruijter house in the hot-water and slow-ignition classes. Mitigation: the sim result looks like a model artefact; the
  field captures are watched and toggling goes back to Sergeant D.
- A deviation from thermo-nova (`heating_control.py:331-356`).

## Related Decisions

- **ADR-235 (CS cadence)**: unrelated mechanism; both are part of the 2026-11-14 continuous/PWM work (TASK-1924, TASK-1942).

## References

- TASK-1933; Sergeant D corrections entries 339, 342, 350 (`docs/sat/sergeant-d-corrections.md`).
- `docs/sat/sim-1933/thresholds.md`, `docs/sat/sim-1933/report.md`; `tools/research/sat/sat_pwm_exit_sim_1933.py`,
  `tools/research/sat/sat_pwm_exit_trace_1933.py`.
- `src/OTGW-firmware/SATcycles.ino` `satCycleCheckPwmExits()`, `SAT_UNDERHEAT_SUSTAIN_MS`.
- thermo-nova 7beaaf5: `custom_components/sat/heating_control.py:308-356`, `const.py:22`, `cycles/const.py:11`.
