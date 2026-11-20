---
id: "ADR-249"
title: "SAT switches from continuous to PWM when the flow stays 1.5 C above the request for 30 s, with its own margin setting (supersedes the entry-22 rule)"
status: "Accepted"
date: "2026-11-16"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-251"
  - "ADR-254"
topics:
  - "sat"
  - "pwm"
  - "continuous mode"
  - "overshoot"
  - "mode switch"
aliases:
  - "continuous to PWM threshold"
  - "sustained overshoot switch"
  - "pwm switch margin"
  - "entry 327"
components:
  - "satCycleCheckOvershootSwitch"
  - "fPwmSwitchMargin"
  - "satpwmswitchmargin"
symbols:
  - "satCycleCheckOvershootSwitch"
  - "SAT_OVERSHOOT_SUSTAIN_MS"
  - "SAT_OVERSHOOT_FAILSAFE_DELTA_C"
  - "fPwmSwitchMargin"
  - "fOvershootMargin"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-249 SAT switches from continuous to PWM when the flow stays 1.5 C above the request for 30 s, with its own margin setting (supersedes the entry-22 rule)

## Status

Accepted, 2026-11-16. Guideline-level per ADR-080: the host test named under Verification checks it, it is not wired into evaluate.py.

## Status History

```yaml
status_history:
  - date: 2026-11-16
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-16
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: "Maintainer accepted: 'A' (Discord direct message 1571779788178194473, reply to 1571670760131534909, 2026-11-16), incl. the setting PWM switch margin default 1.5, range 0.5-5.0"
    changed_via: adr-kit lifecycle
  - date: 2026-11-16
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-251
    changed_via: adr-kit lifecycle
  - date: 2026-11-17
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-254
    changed_via: adr-kit lifecycle
```

## Context

SAT (Smart Autotune Thermostat) runs the boiler either continuously, following a flow setpoint, or in PWM (pulse-width modulation: on/off cycles). It leaves continuous for PWM when the boiler keeps delivering hotter water than requested, because the boiler cannot modulate low enough.

thermo-nova at the pinned commit 7beaaf5 (the SAT ground truth after Sergeant D's decisions, ADR-196) switches when flow >= requested setpoint + `OVERSHOOT_MARGIN_CELSIUS` (3.0, `custom_components/sat/cycles/const.py:10`) holds for `OVERSHOOT_SUSTAIN_SECONDS` (60, `const.py:21`), in `_maybe_enable_pwm_on_sustained_overshoot()` (`heating_control.py:262-300`). Sergeant D first changed that to 2.0 C / 60 s (entry 22 in `docs/sat/sergeant-d-corrections.md`); the firmware took the margin from the setting `settings.sat.fOvershootMargin` (default 2.0, `SATtypes.h`), which also sets the margin of the cycle classifier (entry 37), and the 60 s from `SAT_OVERSHOOT_SUSTAIN_MS` (`SATcycles.ino`). Beside it sits a flame-out fail-safe: flow - requested >= 3.5 C for 30 s (entries 288 and 289, TASK-1762).

On 2026-11-13 Sergeant D decided a tighter switch: flow - requested >= 1.5 C for 30 s (entry 327). On 2026-11-15 he answered the two follow-up questions: keep the 3.5 C / 30 s fail-safe beside it (Q-327a A, entry 359), and give the switch a separate value while the overshoot margin keeps 2.0 for the classifier, with no migration (Q-327b A, entry 360).

## Decision

SAT switches from continuous to PWM when flow - requested setpoint (the output of the proportional-integral-derivative (PID) controller) >= `settings.sat.fPwmSwitchMargin` holds for 30 s; the new setting defaults to 1.5 C, allows 0.5 to 5.0 C, and is separate from `fOvershootMargin`, which keeps 2.0 and now only feeds the cycle classifier.

"A separate value" (entry 360) is read as a user setting, not a constant. Reason: Q-327a option A was argued as "it still catches a big overshoot when a user raises the main margin above 3.5 C"; with a constant there is no user margin, and the fail-safe that entry 359 keeps would never act. The coordinator confirmed this reading on 2026-11-15 under Robert's directive; the confirmation question to Sergeant D is Q-327c (non-blocking, see Open Questions).

Everything else of the switch stays: equality counts (entry 86, Q43 A), the timer counts from the first qualifying tick with no catch-up (entry 109), a tick outside continuous, within the 300 s hot-water guard or with a missing flow clears the start, and the boiler's MsgID 3 permission comes first (entry 213).

## Decision Contract

### Must

- Compare the flow against requested + `settings.sat.fPwmSwitchMargin` and require 30 s (`SAT_OVERSHOOT_SUSTAIN_MS` = 30000) from the first qualifying tick.
- Keep the 3.5 C / 30 s fail-safe as a separate timer with the same gates (entry 359).
- Keep `fOvershootMargin` (2.0) for the cycle classifier only; no migration of stored values (entry 360).
- Expose the setting as `SATpwmswitchmargin` in settings.ini, `satpwmswitchmargin` in the REST (HTTP API) settings, `pwm_switch_margin` on MQTT (the message-broker protocol) `sat/` and in the SAT status JSON (JavaScript Object Notation), range 0.5 to 5.0.

### Must Not

- Must not move the switch with `fOvershootMargin`.
- Must not drop the fail-safe while the switch margin is a setting.

### Exceptions

- None.

### Verification

- `tests/test_sat_overshoot_sustain.py` compiles the real `satCycleCheckOvershootSwitch()`: a flow 1.6 C over the request switches 30 s after the first qualifying tick (the code before this ADR never switches), equality at exactly +1.5, the 29 999 / 30 000 ms boundary, `fOvershootMargin` = 5 does not move the switch, the switch margin raised to 2.0 moves the threshold, and the fail-safe cases run with the switch margin at 5.0. Old-vs-fix through `OTGW_SATCYCLES_INO`: 30 cases fail on the code before this ADR, 83 pass on it.

## Alternatives Considered

- **A constant 1.5 C for the switch.** Smaller (no settings or REST change), but the fail-safe of entry 359 would then be unreachable code, which contradicts the reason given for keeping it.
- **One setting for switch and classifier, default 1.5, with a migration of a stored 2.0 (Q-327b B).** Moves the classifier to 1.5 as well; Sergeant D chose against it (entry 360).
- **One setting, default 1.5, no migration (Q-327b C).** Existing units would keep 2.0 for both and never get entry 327; rejected with entry 360.
- **Keep 2.0 C / 60 s (entry 22).** Superseded by Sergeant D's entry 327.

## Consequences

**Positive:**

- SAT leaves continuous earlier on a boiler that cannot modulate low enough: at 1.5 C instead of 2.0 C, after 30 s instead of 60 s.
- The classifier keeps its 2.0 C, so cycle statistics stay comparable with earlier captures.

**Negative:**

- With the default switch margin the fail-safe never fires first: both rules have the same gates and the same 30 s, and the switch threshold is lower. It only acts when a user raises the switch margin above 3.5 C. This is what entry 359 keeps it for; it is named here so nobody reads the fail-safe as active at the defaults.
- One more SAT setting in the settings contract (settings.ini, REST, MQTT, web UI (user interface), help, openapi).
- Earlier switching can mean more PWM cycles on boilers that hover around +1.5 C; testers' captures will show it.

## Open Questions

- [x] Q-327c: Sergeant D confirms that "a separate value for the switch" (entry 360) means a user setting with default 1.5 (asked non-blocking by 92 on 2026-11-15, #dev-sat-mqtt message 1571655324086050819, `docs/sat/open-questions.md` Q-327c; the implementation goes ahead under Robert's directive). — **Answered 2026-11-16 by User: Robert van den Breemen:** Robert (maintainer) chose A on 2026-11-16, Discord direct message (DM) 1571779788178194473 in reply to 1571670760131534909: accept with the user setting 'PWM switch margin' fPwmSwitchMargin, default 1.5 C, range 0.5-5.0. That settles the setting-versus-constant reading for the implementation; Sergeant D's own confirmation of Q-327c stays welcome and becomes a follow-up if he differs.

## Related Decisions

- ADR-196 (SAT ground truth order): the sources this decision is built from.
- ADR-236 (PWM to continuous exit): the opposite transition, unchanged.
- ADR-051 (settings and state architecture): naming of the new setting.

## References

- `docs/sat/sergeant-d-corrections.md` entries 22, 37, 86, 109, 213, 288, 289, 327, 359, 360.
- thermo-nova 7beaaf5: `custom_components/sat/heating_control.py:262-300`, `cycles/const.py:10`, `const.py:21`.
- `src/OTGW-firmware/SATcycles.ino` (`satCycleCheckOvershootSwitch()`, `SAT_OVERSHOOT_SUSTAIN_MS`, `SAT_OVERSHOOT_FAILSAFE_DELTA_C`), `src/OTGW-firmware/SATtypes.h` (`fPwmSwitchMargin`).
- TASK-1888, TASK-1762.
