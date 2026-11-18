---
id: "ADR-259"
title: "SAT halves the PID integral once when the error leaves the deadband and holds it, for the main and the zone PIDs (amends ADR-197)"
status: "Proposed"
date: "2026-11-18"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-197"
  - "ADR-204"
topics:
  - "sat"
  - "pid"
  - "integral"
  - "deadband"
aliases:
  - "integral halving"
  - "halve the integral"
  - "entry 329"
  - "entry 333"
  - "Q-329a"
  - "Q-329b"
components:
  - "_pidUpdateIntegral"
  - "satZonePidAdvance"
symbols:
  - "_pidUpdateIntegral"
  - "_pid_inBand"
  - "satZonePidAdvance"
  - "bPidInBand"
  - "SAT_PID_DEADBAND"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-259 SAT halves the PID integral once when the error leaves the deadband and holds it, for the main and the zone PIDs (amends ADR-197)

## Status

Proposed, 2026-11-18. Guideline-level (ADR-080): verified by the host tests named below, no evaluate.py gate. Amends ADR-197: it replaces decision 3 ("Integral, deadband 0.1") and its contract line "Set the integral to 0 outside the
deadband"; the rest of ADR-197 and all of ADR-204 stay.

## Status History

```yaml
status_history:
  - date: 2026-11-18
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-18
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-197
    changed_via: adr-kit lifecycle
  - date: 2026-11-18
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-204
    changed_via: adr-kit lifecycle
```

## Context

SAT (Smart Autotune Thermostat) computes a PID (proportional-integral-derivative) output on top of the heating curve.
thermo-nova, the SAT code this firmware ports, sets the integral to 0 whenever the room error is outside the fixed
0.1 C deadband (`pid.py:185-189`, `DEADBAND = 0.1` at `const.py:16`, commit 7beaaf5), and the zone PIDs use the same PID
class (`area.py:50`). The firmware followed that: `_pidUpdateIntegral()` (`SATpid.ino`) for the main PID and
`satZonePidAdvance()` (`SATcontrol.ino`) for the zones; ADR-197 decision 3 records it.

Decisions recorded in `docs/sat/sergeant-d-corrections.md`:

- entry 329 (Sergeant D, 2026-11-13, msg 1570710410427109397, on Robert's proposal): "We can follow this approach":
  outside the deadband, halve the integral instead of resetting it to 0;
- entry 333 (Q-329a, Robert's answer "B", 2026-11-13, msg 1570785451613225121; Sergeant D was invited to object and has
  not): halve once when the error leaves the deadband, then hold the value; back inside it accumulates from the held
  value; a target change still resets the integral fully;
- Q-329b (2026-11-18, open): whether the zone PIDs follow the same rule. Decided by directive: Robert's standing
  instruction of 2026-11-15 (do not wait for Sergeant D, take the recommended option, record it, he may overrule),
  applied by the coordinator; the recommended option was B (main PID and zones), because thermo-nova runs the zones on
  the same PID class.

## Decision

When the error leaves the 0.1 C deadband the integral is multiplied by 0.5 once and then held while the error stays
outside; when the error is back inside, the integral accumulates from the held value as before. This applies to the main
PID and to every zone PID.

- Main PID: `_pid_inBand` (SATpid.ino) records whether the previous integral update was inside the band. Outside:
  `if (_pid_inBand) _pid_integral *= 0.5f; _pid_inBand = false;`. Inside: `_pid_inBand = true`, then the existing
  accumulation and clamp.
- `_pid_inBand` starts as inside, and every reset or restore sets it to inside (`satPidReset()`, `satPidResetTerms()`,
  `satPidRestoreState()`, the lost-reading reset in `satPidCurveOnly()`): the first update outside after a reset halves
  0 (no change); the first update outside after a restore halves the restored value once.
- Zones: `SATZoneState::bPidInBand` with the same rule in `satZonePidAdvance()`; the existing zone resets (to 0) stay.
- Unchanged: the reset triggers of ADR-204 (a target change, a preset change, window open and close, SAT off to on and
  the others) still set the integral to 0; the solar freeze (ADR-197 decision 4) still skips the whole update; the
  clamp to +-curve stays; the persisted file format stays (the held value is what the 5-min save writes).

## Decision Contract

### Must

- Halve the integral exactly once per transition from inside to outside the deadband, for the main and every zone PID.
- Hold the integral unchanged while the error stays outside the deadband.

### Must Not

- Reset the integral to 0 only because the error is outside the deadband.
- Change the reset triggers of ADR-204.

### Exceptions

- None.

### Verification

- `tests/test_sat_pid_semantics.py` (the REAL SATpid.ino against the thermo-nova oracle with this deviation as a
  parameter): `test_outside_the_band_the_halved_integral_is_held_and_inside_it_accumulates_again` and the boundary,
  restore and saturation cases; 5 FAIL on the code before this ADR.
- `tests/test_sat_pid_cadence.py::test_g3_zone_integral_is_halved_once_outside_the_band_and_held` (the REAL zone PID);
  FAIL on the code before this ADR (the zone integral went to 0).

## Alternatives Considered

- **Reset to 0, as thermo-nova.** Rejected by Sergeant D (entry 329): it throws away the learned correction every time
  the room leaves the band.
- **Halve at every update while outside (a decay to 0).** Rejected by Robert (Q-329a, option B chosen): it ends in the
  same reset after a few minutes outside.
- **Halving for the main PID only (Q-329b option A).** Not chosen: the zones are the same controller in thermo-nova, and
  one rule is simpler to reason about; Sergeant D may still overrule (Q-329b open).

## Consequences

**Positive:**

- Half of the learned correction stays in the setpoint while the room is briefly outside the band, so the control does
  not start from zero after every small excursion.

**Negative:**

- A deliberate deviation from thermo-nova; the SAT simulation model and `docs/sat/firmware-sat-spec.md` /
  `docs/sat/sat-comparison.md` must describe it. Mitigation: the oracle in test_sat_pid_semantics.py carries it as a
  named deviation.
- After a long time outside the band the held half may no longer fit the new situation; it is still clamped to
  +-curve and any ADR-204 trigger resets it.
- The zone part rests on a directive, not yet on Sergeant D's answer (Q-329b); if he chooses A, the zone part is
  reverted.

## Open Questions

- [x] Halve once or at every update outside the band? — **Answered 2026-11-13 by Robert van den Breemen:** once, then hold (entry 333, Q-329a B).
- [x] Do the zone PIDs follow the rule? — **Answered 2026-11-18 by directive (Robert's 2026-11-15 instruction, applied by the coordinator):** yes (Q-329b B); Sergeant D may overrule.

## Related Decisions

- **ADR-197 (SAT PID v3 controller)**: decision 3 and the contract line "Set the integral to 0 outside the deadband" are
  replaced by this ADR; the rest stays.
- **ADR-204 (SAT PID reset triggers)**: unchanged; its resets still set the integral to 0 and also clear the
  in-band state.

## References

- `docs/sat/sergeant-d-corrections.md` entries 329 and 333; Q-329b (queued 2026-11-18).
- thermo-nova 7beaaf5: `custom_components/sat/pid.py:185-189`, `const.py:16`, `area.py:50`.
- `src/OTGW-firmware/SATpid.ino` `_pidUpdateIntegral()`, `src/OTGW-firmware/SATcontrol.ino` `satZonePidAdvance()`,
  `src/OTGW-firmware/SATtypes.h` `SATZoneState::bPidInBand`.
- TASK-1890.
