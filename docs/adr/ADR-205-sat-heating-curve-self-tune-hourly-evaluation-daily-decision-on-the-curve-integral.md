---
id: "ADR-205"
title: "SAT heating curve self-tune: hourly evaluation, daily decision on the curve integral"
status: "Accepted"
date: "2026-10-31"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-198"
  - "ADR-204"
topics:
  - "heating curve self-tune"
  - "curve coefficient auto-tune"
  - "curve integral"
  - "clean hour"
  - "low load"
aliases:
  - "autotune"
  - "self-tune"
  - "curve tuner"
  - "satcoefficient tuning"
components:
  - "SAT heating curve self-tune"
symbols:
  - "fHeatingCurveCoeff"
  - "SATcoefficient"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-205 SAT heating curve self-tune: hourly evaluation, daily decision on the curve integral

## Status

Accepted, 2026-10-31.

**Decision Makers:** Sergeant D (SAT design, entries 160-174) and Robert van den Breemen (maintainer, entries 173-176).
Drafted by Agent: Claude from the TASK-1519 study.

## Status History

```yaml
status_history:
  - date: 2026-10-31
    status: Proposed
    changed_by: "Agent: Claude"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-10-31
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-198
    changed_via: adr-kit lifecycle
  - date: 2026-10-31
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-204
    changed_via: adr-kit lifecycle
  - date: 2026-10-31
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: "Accepted by Robert in session 2026-10-31 (interactive question) and in #dev-sat-mqtt msg 1566083570903027713; design from Sergeant D entries 160-178"
    changed_via: adr-kit lifecycle
```

## Context

SAT (Smart Autotune Thermostat) computes the boiler flow temperature from a heating curve with one user coefficient
(default 2.0, clamped 0.1-5.0, ADR-198) and lets its PID (proportional-integral-derivative) controller add a
correction on top. A coefficient that is too low leaves the room cold in cold weather; one that is too high is
mostly absorbed by the PID. The firmware once had a self-tune that scaled the coefficient after mostly-overshoot
cycles and wrote settings.ini each time; Sergeant D asked for its removal (entry 72, Q22) and TASK-1413 removed it.
thermo-nova, the upstream SAT, has no tuner and never changes the coefficient (ADR-198, point 5).

On 2026-10-31 Robert asked for a new self-tune with three fixed requirements: an hourly evaluation, an on/off
setting, and a log line for every change with the old and new coefficient and the reason (TASK-1519). This record
revisits entry 72 on purpose. The design was settled with Sergeant D one question at a time (entries 160-175) and
tested in simulation: about 50,000 simulated winters in rounds 1-11 (`docs/sat/sim-1519/`), with real weather
(20 winters 2005-06 .. 2024-25 and 2025-26, Open-Meteo archive, 52.1N 6.2E), a two-node house model per Dutch
building era, gas boilers and two heat-pump types with a minimum output, and the thermo-nova PID and PWM rules.

Key facts the decision rests on:

- **The SAT PID integral cannot be the signal.** thermo-nova zeroes it whenever the room error exceeds 0.1 C
  (`pid.py:187-189`, `const.py:16`), so with a wrong curve it stays 0 (round 1). The **curve integral**, the mean of
  setpoint minus curve (everything the PID adds on top of the curve), works in both directions (entry 163).
- **Hourly decisions chase the weather.** The right coefficient moves with the outside temperature (about 1.3 at -5 C
  and 2.1 at +10 C in one house). Hourly rules made 110-180 changes and 50-90 wrong-way steps per winter; a daily
  decision on several days about 7 changes and none, at the same comfort (rounds 1-2, entries 162, 164, 172).
- **A derivative term adds nothing** to a multi-day mean (round 7: window mean <= 0.024 C against 0.74-2.25 C);
  Sergeant D chose the curve integral alone (entry 171).
- **Low-load hours mislead.** In SAT PWM (pulse width modulation, on/off capacity control) or with the heat source at
  its minimum output, a warm room says nothing about the curve. Counting those hours made the tuner worse (worst
  pair +100 min/day, round 5b); excluding them is safe (worst pair 0) but blinds the tuner in new, low-load houses
  (rounds 5c-5g, entry 174).
- **The rule is safe over 20 winters** (round 9, 11,100 winters): worst pair +33 min/day outside +-0.3 C, 1-4 % of
  pairs more than 5 min/day worse than no tuner, rescue from a curve of 1.0 up to -415 min/day in cold winters.
- **3 days is steadier than 2** (round 9: runs with >= 3 direction flips 88 -> 25 of 3,000), matching the 60-72 h
  response of a house (entry 168, entry 175).

## Decision

SAT gets an on/off self-tune, on by default, that evaluates every clean hour, logs it, and changes the heating curve
coefficient at most once per day by a fixed 0.1 when the mean curve integral of the last 3 days of clean hours lies
outside +-1.0 C of flow.

1. **Signal (entries 163, 171).** For each hour, the tuner averages `setpoint - curve` over the SAT control ticks of
   that hour, where `setpoint` is the PID output (curve + P + I + D, the SATTICK `pid` field,
   `docs/sat/sat-log-format.md:19`) clamped to the setpoint range (10 C to the system maximum), as in the
   simulation, and before the PWM override. No PID integral alone, no derivative term of the tuner's own.
2. **Clean hour (entries 160, 161, 174, rounds 4-5g).** An hour counts only when, for the whole hour: SAT is the
   active controller; no hot water was drawn; a valid sun elevation is known and below 12 degrees; SAT was not in
   PWM; the flame was not off while CH (central heating) was enabled; the relative modulation (OpenTherm MsgID 17)
   was above the boiler's minimum modulation (MsgID 15 low byte, 5 % when MsgID 15 is missing); and the coefficient
   did not change in that hour. A day counts when it has at least 6 clean hours.
3. **Decision (entries 164, 172, 173, 175).** Once a day, when the last 3 counted days are available, the tuner takes
   the mean of their clean-hour signals. Above +1.0 C it raises the coefficient by 0.1; below -1.0 C it lowers it by
   0.1; otherwise it holds. After a change the window starts empty, so the next decision needs 3 new counted days.
4. **Bounds (entries 165, 167, ADR-198).** A change is 0.1, inside entry 165's +-0.2 limit; the result is clamped
   to the coefficient setting range 0.1-5.0 (`settingStuff.ino:1060`).
5. **PID (ADR-204).** A tuner change leaves the PID integral and derivative as they are (ADR-204 Must Not: "Reset on a
   coefficient change"). Round 10 showed this changes the outcome by at most +-5 min/day.
6. **Persistence.** The new coefficient is written to settings.ini only when it changes: at most once per day, in
   simulation 2-3 times per winter on average and at most 14.
7. **Setting.** One boolean setting, on by default (entry 176), exposed like the other SAT settings. Switching it off
   stops evaluation and decisions and keeps the current coefficient. A manual change of the coefficient by the user
   empties the window.
8. **Logging (Robert's requirement, entries 172, 174).** Every hour one machine-readable record in the SAT log with
   the hour's mean signal, whether it counted and, if not, the first reason (not SAT, hot water, no sun elevation,
   sun, PWM, flame off, minimum modulation, coefficient changed). Every day one record with the window's clean-hour
   count, the 3-day mean, the decision (raise, lower, hold) and the reason for a hold (inside +-1.0 C, too few counted
   days). Every change also as a readable line on telnet and in the SAT log: old and new coefficient and the mean
   that caused it.

## Decision Contract

### Must

- Use the curve integral (setpoint before the PWM override, minus the curve) as the only signal.
- Count an hour only under every condition of decision 2.
- Decide at most once per day, on the last 3 counted days, with a symmetric +-1.0 C threshold and a fixed 0.1 step,
  clamped to 0.1-5.0.
- Leave the PID terms alone on a tuner change.
- Persist the coefficient only when it changes.
- Write the hourly record, the daily record and a change line with old value, new value and reason.

### Must Not

- Use the SAT PID integral or a derivative term as the signal (entries 163, 171).
- Count PWM hours, flame-off-with-CH hours or minimum-modulation hours, also not one-sided (rounds 5d-5g: selection
  bias drifts the curve up).
- Count hours without a valid sun elevation (entries 160, 178).
- Change the coefficient more than once per day or by more than 0.2 (entry 165).

### Exceptions

- None.

### Verification

- Host tests that call the tuner's hour and day functions (not a re-implementation): red on a missing exclusion,
  green after; a too-low curve raises by 0.1 after 3 counted days; a PWM-only winter makes no change.
- `python scripts/sat_curve_selftune_study9.py` reproduces the round 9 numbers (study evidence, not a firmware test).
- A bench run with SAT simulation that shows the hourly and daily records and one logged change.

## Alternatives Considered

- **Hourly decisions with guards (Robert's original wish).** Rejected by Sergeant D (Q-1519d B, entry 172): about 10
  changes and 2 flips per winter with the best guards, against 7 and 0 for daily; measuring and logging stay hourly.
- **The SAT PID integral as the signal (Q-1519a A, first proposal).** Rejected: it is reset outside 0.1 C and never
  moved from a curve that was too low (round 1).
- **Curve integral plus a derivative term (entry 170).** Rejected after round 7 (no effect); entry 171.
- **Asymmetric thresholds (raise at 0.5, lower at 1.0).** Rejected: no comfort gain, more changes and flips (round 5c,
  entry 173).
- **Proportional steps, a comfort fast lane, undo after a worse day.** Rejected: no gain, more churn; undo made comfort
  worse (rounds 1-2).
- **Reading low-load hours one-sided, or a cold-day safety net.** Rejected: selection bias or runaway in oscillating
  light houses (rounds 5d-5g); the low-load blind spot is accepted and logged instead (entry 174).
- **A 2-day window.** Rejected for 3 days (entry 175): faster rescue but twice the direction flips.
- **Off by default.** Rejected by Robert (entry 176); open with Sergeant D (Q-1519i).
- **Do nothing (no tuner, entry 72).** Rejected by Robert's request; the simulation shows the tuner recovers a curve
  that is far too low and costs little where the curve is right.

## Consequences

**Positive:**

- A curve set far too low recovers without user action: median -33 min/day outside +-0.3 C over 20 winters for gas
  from a coefficient of 1.0, up to -415 in cold winters; -384 for heat pumps that modulate to 30 % (round 9).
- The hourly log shows what the controller sees and why the tuner holds, which is also field data for SAT itself.
- Few flash writes: 2-3 per winter on average.

**Negative:**

- **Blind spot:** in houses whose boiler or heat pump cannot modulate below the demand (2015, BENG (the Dutch nearly-zero-energy building standard of 2021)), a curve far too
  low drives SAT into PWM and the tuner sees no clean hours. Mitigation: the log says so (entry 174); revisit with
  field data.
- **Slower in mild winters:** fewer clean hours, so gas and 60 % heat pumps barely move in the mildest winters.
- **A stored user setting changes without an explicit choice** when the default is on. Mitigation: small logged
  steps, the setting can be switched off, a manual change resets the window; Sergeant D asked (Q-1519i).
- **Needs a sun elevation source.** The firmware gets it only over MQTT (Message Queuing Telemetry Transport) or REST (Representational State Transfer) today (`SATcontrol.ino:2574-2588`);
  without it the tuner holds and logs why. Mitigation: TASK-1410 (local sun elevation from latitude / longitude and
  NTP (Network Time Protocol) time) is a prerequisite of the implementation (entry 178).
- **Model limits.** Absolute coefficient values and all start and gas numbers are model-dependent (the boiler's own
  cycling is not validated: starts per hour 1.5 in the model, 2.5 in the field capture). The simulation used solar
  radiation < 50 W/m2 instead of the sun elevation rule.

## Open Questions

- [x] Q-1519i: is "on by default" also right for existing units, given entry 24 ("Leave existing settings alone")? Asked to Sergeant D 2026-10-31 (msg 1566080794131439660). — **Answered 2026-10-31 by Agent: Claude:** Sergeant D 2026-10-31 13:28 UTC (msg 1566081440956026972): A, on by default also for existing units; entry 177 confirms entry 176.
- [x] Should the tuner run without a sun elevation source until TASK-1410 lands (entry 160 interpretation says no)? Needs a maintainer decision. — **Answered 2026-10-31 by User: Robert van den Breemen:** Robert (maintainer) 2026-10-31 in session (entry 178): no; without a valid sun elevation the tuner holds and logs it, and TASK-1410 (local sun elevation from lat/lon and NTP time) is a prerequisite of the implementation.

## Related Decisions

- **ADR-198 (SAT heating curve):** defines the coefficient, its default 2.0 and its 0.1-5.0 range, and records the
  removal of the old auto-tune (entry 72). This ADR adds a new tuner within that range; it does not change the curve.
- **ADR-204 (SAT PID reset triggers):** a coefficient change must not reset the PID; this tuner follows that.
- **ADR-197 (SAT PID v3):** the PID gains grow with the coefficient (kp = coefficient x curve / 3), which is why the
  oscillation in light houses matters (rounds 3, 5f).
- **ADR-196 (SAT ground truth):** a firmware addition needs Sergeant D's view; entries 160-176.

## References

- Task: TASK-1519. Decisions: `docs/sat/sergeant-d-corrections.md` entries 72, 160-176; questions
  `docs/sat/open-questions.md` Q-1519a .. Q-1519i.
- Study: `docs/sat/sim-1519/round5b-round7.md` (sections 1-9), `docs/sat/sim-1519/evaluation-2026-10-31.md`,
  scripts `scripts/sat_curve_selftune_study*.py`, data `docs/sat/sim-1519/data/`.
- Firmware anchors: coefficient `SATtypes.h:477`, clamp `settingStuff.ino:1060`, sun elevation
  `SATcontrol.ino:2574-2588`, SAT log records `docs/sat/sat-log-format.md:19`.
- thermo-nova at 7beaaf5: `pid.py:187-189` (integral reset), `pid.py:205-207` (derivative held), `pid.py:69-70`
  (automatic kp).
- OpenTherm Application Functional Specification v1.1f, section 2.1.1 (low-load detection signals used in decision 2).

## Enforcement

Not yet: the tuner is not implemented. The implementation task adds the declarative rules (for example that no
tuner code path writes the coefficient outside the daily decision) once the code and its file names exist.
