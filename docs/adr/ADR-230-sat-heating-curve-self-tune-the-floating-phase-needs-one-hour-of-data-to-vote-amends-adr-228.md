---
id: "ADR-230"
title: "SAT heating curve self-tune: the floating phase needs one hour of data to vote (amends ADR-228)"
status: "Accepted"
date: "2026-11-12"
binding: false
gate: "tests/test_sat_curve_selftune.py"
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-228"
topics:
  - "heating curve self-tune"
  - "phase split"
  - "floating phase minimum"
aliases:
  - "floating phase needs one hour"
  - "short floating sample veto"
components:
  - "self-tune floating vote minimum"
symbols:
  - "SAT_CT_FLOAT_MIN_MS"
  - "satTuneCloseDay"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-230 SAT heating curve self-tune: the floating phase needs one hour of data to vote (amends ADR-228)

## Status

Accepted, 2026-11-12. Amends ADR-228 decision 4 (the decision rule) only; all other ADR-228 decisions stay in force.
The gate named in the frontmatter is the host test that compiles the real tuner.

**Decision Maker:** Sergeant D (SAT design owner) for the rule, entries 313 and 315 of
`docs/sat/sergeant-d-corrections.md`; Robert (maintainer) accepts or rejects this record. Robert chose on 2026-11-12 to
keep this ADR to the floating minimum and to give the PWM-hours rule of entry 316 its own ADR once Sergeant D has
answered Q-1817a and Q-1817b (TASK-1817). Written by an agent (otgw-firmware-d3, TASK-1816).

## Status History

```yaml
status_history:
  - date: 2026-11-12
    status: Proposed
    changed_by: "Agent: Claude"
    reason: "Sergeant D entries 313 and 315 (TASK-1816); split from the PWM rule on Robert's choice"
    changed_via: adr-kit
  - date: 2026-11-12
    status: Proposed
    changed_by: "Agent: Claude"
    reason: ADR-230 amends ADR-228 decision 4 (Sergeant D entries 313, 315)
    changed_via: adr-kit lifecycle
  - date: 2026-11-12
    status: Accepted
    changed_by: Robert van den Breemen
    reason: Accepted by Robert van den Breemen, 2026-11-12, in session otgw-firmware-d3 (AskUserQuestion 'Ja, accepteer', after the split); also DM 'A' to the coordinator (msg 1570216668469133343); Sergeant D entries 313, 315; TASK-1816
    changed_via: adr-kit lifecycle
```

## Context

The SAT (Smart Autotune Thermostat) heating curve self-tune of ADR-228 splits every signal tick into a warm-up phase
(PID (proportional-integral-derivative) error outside the 0.1 C deadband) and a floating phase (inside it), gives each phase a tick-weighted mean over the
3 counted days, and changes the coefficient by 0.1 only when both phases say the same direction (ADR-228 decision 4).
A phase without any tick does not veto: the other decides alone (entry 305).

On gruijter's gas boiler (10 Nov, beta.167, analysed in TASK-1796) 12 floating minutes against 650 warm-up minutes gave
`phases_disagree`, so a raise the warm-up phase supported over days was blocked. The floating signal is structurally
small (P at most kp times 0.1, D frozen, I small), so a short floating sample says little. Sergeant D answered Q-1796a B
(entry 313: the floating phase votes only above a minimum amount of data) and Q-1796a2 B (entry 315: the minimum is
1 hour over the counted days). The tuner is ours: thermo-nova has no coefficient tuner (entry 298), so there is no
thermo-nova code to cite for this rule.

## Decision

The floating phase votes only when its data in the window is at least 1 hour: its signal ticks over the counted days
times the control interval, `SAT_CT_FLOAT_MIN_MS 3600000` in `SATcurveTune.h`. With less it counts as a phase without
data, so the warm-up phase decides alone (entry 305). The warm-up phase has no minimum: entry 313 names the floating
phase only. `satTuneCloseDay()` gets the control tick length from its caller (`satTuneRollHour()`, the same interval
its minimum-ticks check uses); `SATTUNED` logs `fvote` (1 the floating phase took part, 0 it did not).

## Decision Contract

### Must

- Treat a floating phase with less than `SAT_CT_FLOAT_MIN_MS` of data in the window as a phase without data.
- Compute the floating data as ticks times the control interval passed to `satTuneCloseDay()`.
- Log `fvote` in every `SATTUNED` record.

### Must Not

- Must not apply a minimum to the warm-up phase.

### Exceptions

- None.

### Verification

- `tests/test_sat_curve_selftune.py`: `test_short_floating_sample_does_not_veto` (12 floating minutes against 72
  warm-up hours at a 60 s interval raises; on the code before TASK-1816 it holds with `phases_disagree`),
  `test_one_hour_of_floating_data_votes` (60 ticks of 60 s vote, 59 do not), `test_record_formats` (`fvote`).

## Alternatives Considered

- **Keep ADR-228 unchanged (any floating tick votes).** Rejected by Sergeant D (Q-1796a A); a few floating minutes
  block a raise the warm-up phase supports over days.
- **A 2-hour floating minimum.** Our advice; Sergeant D chose 1 hour (Q-1796a2 B).
- **A minimum for both phases.** Not asked for: entry 313 names the floating phase; the warm-up phase rarely lacks
  data when the curve is off.
- **One ADR with the PWM-hours rule as well.** Robert chose to split: the PWM (pulse-width modulation) rule still has open questions for
  Sergeant D (Q-1817a, Q-1817b), and one ADR would hold this built, tested rule back until they are answered.

## Consequences

**Positive:**

- A clear warm-up verdict is no longer blocked by a few floating minutes (the 10 Nov case would raise).

**Negative:**

- A floating phase just under 1 hour cannot veto, also when its verdict is right. Mitigation: `fvote` and `fticks3`
  in `SATTUNED` show it, and the minimum is one named constant.
- The control interval can change inside the window; ticks times the current interval is then approximate.
  Mitigation: the interval rarely changes, and a manual change of the coefficient already empties the window.

## Open Questions

- [x] Acceptance by Robert (maintainer); this ADR is never self-accepted. — **Answered 2026-11-12 by User: Robert van den Breemen:** Accepted by Robert van den Breemen, 2026-11-12, in session otgw-firmware-d3 (AskUserQuestion 'Ja, accepteer', after the split); also DM 'A' to the coordinator (msg 1570216668469133343)

## Related Decisions

- **ADR-228 (Accepted)**: amended for decision 4 (a floating phase below the minimum counts as without data); all its
  other decisions stay.

## References

- `docs/sat/sergeant-d-corrections.md` entries 305, 313, 315 (and 298).
- `src/OTGW-firmware/SATcurveTune.h` (`SAT_CT_FLOAT_MIN_MS`), `src/OTGW-firmware/SATcontrol.ino` (`satTuneCloseDay()`,
  `satTuneRollHour()`, `satTuneLogDay()`).
- `docs/sat/sat-log-format.md` (`SATTUNED fvote`).
- Tasks: TASK-1816 (this decision), TASK-1817 (the PWM-hours rule, its own ADR later), TASK-1796 (field analysis).
