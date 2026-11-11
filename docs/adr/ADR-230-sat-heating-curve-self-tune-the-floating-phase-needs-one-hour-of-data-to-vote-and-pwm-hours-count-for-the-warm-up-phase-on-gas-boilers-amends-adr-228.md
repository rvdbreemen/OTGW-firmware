---
id: "ADR-230"
title: "SAT heating curve self-tune: the floating phase needs one hour of data to vote, and PWM hours count for the warm-up phase on gas boilers (amends ADR-228)"
status: "Proposed"
date: "2026-11-12"
binding: false
gate: "tests/test_sat_curve_selftune.py"
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-205"
  - "ADR-228"
topics:
  - "heating curve self-tune"
  - "phase split"
  - "floating phase minimum"
  - "pwm hours"
aliases:
  - "floating phase needs one hour"
  - "self-tune pwm hours warm-up"
components:
  - "self-tune floating vote minimum"
  - "self-tune pwm hour rule"
symbols:
  - "SAT_CT_FLOAT_MIN_MS"
  - "satTuneCloseDay"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-230 SAT heating curve self-tune: the floating phase needs one hour of data to vote, and PWM hours count for the warm-up phase on gas boilers (amends ADR-228)

## Status

Proposed, 2026-11-12. Amends ADR-228 decision 4 (the decision rule) and ADR-205 decision 2 (the clean hour) for
gas boilers; all other ADR-228 and ADR-205 decisions stay in force. Guideline-level per ADR-080 until accepted; the
gate named in the frontmatter is the host test that compiles the real tuner.

**Decision Maker:** Sergeant D (SAT design owner) for both rules, entries 313, 315 and 316 of
`docs/sat/sergeant-d-corrections.md`; Robert (maintainer) for the PWM rule as well (entry 316: his Discord DM answer
B revises his own entry 174), and he accepts or rejects this record. Written by an agent (otgw-firmware-d3, TASK-1816,
TASK-1817).

## Status History

```yaml
status_history:
  - date: 2026-11-12
    status: Proposed
    changed_by: "Agent: Claude"
    reason: Initial proposal for Sergeant D entries 313, 315 and 316 (TASK-1816, TASK-1817)
    changed_via: adr-kit
  - date: 2026-11-12
    status: Proposed
    changed_by: "Agent: Claude"
    reason: ADR-230 amends ADR-228 (Sergeant D entries 313, 315, 316)
    changed_via: adr-kit lifecycle
  - date: 2026-11-12
    status: Proposed
    changed_by: "Agent: Claude"
    reason: ADR-230 amends ADR-205 (Sergeant D entries 313, 315, 316)
    changed_via: adr-kit lifecycle
```

## Context

The SAT (Smart Autotune Thermostat) heating curve self-tune of ADR-228 splits every signal tick into a warm-up phase
(PID error outside the 0.1 C deadband) and a floating phase (inside it), gives each phase a tick-weighted mean over the
3 counted days, and changes the coefficient by 0.1 only when both phases say the same direction (ADR-228 decision 4).
A phase without any tick does not veto: the other decides alone (entry 305).

Two field observations on gruijter's gas boiler (10 and 11 Nov, beta.167 and beta.171, analysed in TASK-1796) showed
the rule could not act:

- **A tiny floating sample vetoed a clear warm-up verdict.** 12 floating minutes against 650 warm-up minutes gave
  `phases_disagree`. The floating signal is structurally small (P at most kp times 0.1, D frozen, I small), so a short
  sample says little.
- **No hour counted at all.** The boiler ran in SAT PWM (pulse-width modulation: on/off cycling below the minimum
  output) 88 % of the time; ADR-205 decision 2 excludes every PWM hour, so 0 of 20 hours counted overnight. Entry 174
  (maintainer) had accepted this low-load blind spot.

Sergeant D answered three questions on 2026-11-11: Q-1796a B (entry 313, a floating minimum), Q-1796a2 B (entry 315,
the minimum is 1 hour) and Q-1796b B (entry 316, PWM hours count for warm-up on gas boilers), and Robert answered
Q-1796b B as well. The tuner is ours: thermo-nova has no coefficient tuner (entry 298), so there is no thermo-nova
code to cite for these rules.

## Decision

1. **Floating vote minimum (entries 313, 315).** The floating phase votes only when its data in the window is at least
   1 hour: its signal ticks over the counted days times the control interval, `SAT_CT_FLOAT_MIN_MS 3600000` in
   `SATcurveTune.h`. With less it counts as a phase without data, so the warm-up phase decides alone (entry 305). The
   warm-up phase has no minimum: entry 313 names the floating phase only. `satTuneCloseDay()` gets the control tick
   length from its caller (`satTuneRollHour()`, the same interval its minimum-ticks check uses); `SATTUNED` logs
   `fvote` (1 the floating phase took part, 0 it did not).
2. **PWM hours on gas boilers (entry 316).** On a gas boiler a SAT PWM hour is no longer excluded from the self-tune:
   it counts, and its ticks feed the warm-up phase. The signal stays the ADR-228 decision 2 value (setpoint minus
   curve, before the PWM override). Heat pumps keep entry 205 (PWM hours logged, not used). How the PWM ticks are
   assigned and what a hybrid does are the open questions below; this part is built only after they are answered
   (TASK-1817).

## Decision Contract

### Must

- Treat a floating phase with less than `SAT_CT_FLOAT_MIN_MS` of data in the window as a phase without data.
- Compute the floating data as ticks times the control interval passed to `satTuneCloseDay()`.
- Log `fvote` in every `SATTUNED` record.

### Must Not

- Must not apply a minimum to the warm-up phase.
- Must not count PWM hours on a heat pump (entry 205).

### Exceptions

- None.

### Verification

- `tests/test_sat_curve_selftune.py`: `test_short_floating_sample_does_not_veto` (12 floating minutes against 72
  warm-up hours at a 60 s interval raises; on the code before TASK-1816 it holds with `phases_disagree`),
  `test_one_hour_of_floating_data_votes` (60 ticks of 60 s vote, 59 do not), `test_record_formats` (`fvote`).
- The PWM rule gets its own red/green test with TASK-1817.

## Alternatives Considered

- **Keep ADR-228 unchanged (any floating tick votes).** Rejected by Sergeant D (Q-1796a A); a few floating minutes
  block a raise the warm-up phase supports over days.
- **A 2-hour floating minimum.** Our advice; Sergeant D chose 1 hour (Q-1796a2 B).
- **A minimum for both phases.** Not asked for: entry 313 names the floating phase; the warm-up phase rarely lacks
  data when the curve is off.
- **Keep excluding PWM hours on gas boilers (entry 174).** Rejected by Sergeant D and Robert (Q-1796b B): a gas boiler
  at low load cycles most of the time, so the tuner never counts a day.

## Consequences

**Positive:**

- A clear warm-up verdict is no longer blocked by a few floating minutes (the 10 Nov case would raise).
- Gas boilers at low load give the tuner counted days at all (with decision 2).

**Negative:**

- A floating phase just under 1 hour cannot veto, also when its verdict is right. Mitigation: `fvote` and `fticks3`
  in `SATTUNED` show it, and the minimum is one named constant.
- PWM ticks carry the PWM setpoint behaviour; counting them can bias the warm-up mean. Mitigation: the signal is taken
  before the PWM override (ADR-228 decision 2), and the open questions settle the assignment before it is built.

## Open Questions

- [ ] Q-1817a (for Sergeant D): which phase does a PWM tick on a gas boiler go to? A) every PWM tick counts for the
  warm-up phase, whatever the PID error; B) PWM hours are no longer excluded and their ticks follow the normal phase
  split by the error. Advice A, the plain reading of entry 316 "count for the warm-up phase".
- [ ] Q-1817b (for Sergeant D): does a hybrid (`satsource` 3) in PWM follow the gas-boiler rule or the heat-pump rule
  (entry 205)? Advice: the gas-boiler rule, since PWM on a hybrid runs on the boiler side; entry 82 (a hybrid in PWM
  uses gas boiler timing) supports it, entry 21 ("hybrid systems should be treated as heat pumps", given for MM) points
  the other way, so both are named in the question.
- [ ] Acceptance by Robert (maintainer); this ADR is never self-accepted.

## Related Decisions

- **ADR-228 (Accepted)**: amended for decision 4 (a floating phase below the minimum counts as without data) and, for
  gas boilers, for the PWM exclusion it keeps from ADR-205; all its other decisions stay.
- **ADR-205 (Accepted)**: its decision 2 (clean hour) loses the PWM exclusion on gas boilers once decision 2 here is
  built.

## References

- `docs/sat/sergeant-d-corrections.md` entries 305, 313, 315, 316 (and 174, 205, 298).
- `src/OTGW-firmware/SATcurveTune.h` (`SAT_CT_FLOAT_MIN_MS`), `src/OTGW-firmware/SATcontrol.ino` (`satTuneCloseDay()`,
  `satTuneRollHour()`, `satTuneLogDay()`).
- `docs/sat/sat-log-format.md` (`SATTUNED fvote`).
- Tasks: TASK-1816 (decision 1), TASK-1817 (decision 2), TASK-1796 (field analysis).
