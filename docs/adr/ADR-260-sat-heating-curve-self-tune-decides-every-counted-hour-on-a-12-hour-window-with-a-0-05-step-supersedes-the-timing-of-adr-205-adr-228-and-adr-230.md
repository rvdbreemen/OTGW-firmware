---
id: "ADR-260"
title: "SAT heating curve self-tune decides every counted hour on a 12-hour window with a 0.05 step (supersedes the timing of ADR-205, ADR-228 and ADR-230)"
status: "Accepted"
date: "2026-11-19"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-205"
  - "ADR-228"
  - "ADR-230"
topics:
  - "sat"
  - "self-tune"
  - "heating-curve"
  - "mqtt"
  - "rest-api"
aliases:
  - "faster self-tune"
  - "hours not days"
  - "self-tune window hours"
components:
  - "SAT heating curve self-tune timing (SATcurveTune.h constants, satTuneCloseDay)"
  - "self-tune window file /sat_tune.json version 3"
  - "curve_tune telemetry fields window_hours and window_hours_needed"
symbols:
  - "SAT_CT_WINDOW_DAYS"
  - "SAT_CT_HOURS_PER_DAY"
  - "SAT_CT_MIN_CLEAN_HOURS"
  - "SAT_CT_STEP"
  - "SAT_CT_FILE_VERSION"
  - "satTuneCloseDay"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-260 SAT heating curve self-tune decides every counted hour on a 12-hour window with a 0.05 step (supersedes the timing of ADR-205, ADR-228 and ADR-230)

## Status

Accepted, 2026-11-19.

**Decision Maker:** scope by User: Robert van den Breemen (maintainer), relayed by the coordinator on 2026-11-18:
"Die snellere self tune (1889) moet wel mee" (TASK-1889 goes into 2.0.0). Behaviour: Sergeant D decided the direction
(corrections entry 329, 2026-11-13: "hours, not days"; entry 367, Q-TUNE1 A: keep the exclusions, only a faster
window and step). The numbers (12 counted hours, step 0.05) are option A of Q-1889a, first taken by directive and then
confirmed by Sergeant D on 2026-11-19 (entry 423, "my answer is A",
https://discord.com/channels/812969634638725140/1105556725714649128/1572825703299293265); acceptance is Robert's.

## Status History

```yaml
status_history:
  - date: 2026-11-18
    status: Proposed
    changed_by: Agent (ba)
    reason: "Written by otgw-webui (Claude) for TASK-1889 on Robert's scope decision and Sergeant D entries 329 and 367; numbers from Q-1889a option A by directive; acceptance is Robert's"
    changed_via: adr-kit
  - date: 2026-11-18
    status: Proposed
    changed_by: Agent (ba)
    reason: Related to ADR-205
    changed_via: adr-kit lifecycle
  - date: 2026-11-18
    status: Proposed
    changed_by: Agent (ba)
    reason: Related to ADR-228
    changed_via: adr-kit lifecycle
  - date: 2026-11-18
    status: Proposed
    changed_by: Agent (ba)
    reason: Related to ADR-230
    changed_via: adr-kit lifecycle
  - date: 2026-11-19
    status: Accepted
    changed_by: "User: Robert van den Breemen (maintainer)"
    reason: Accepted by Robert, Discord DM 1572842856148766751 (2026-11-19 05:15:58 UTC, 'Ja', a reply to the coordinator's question 1572834466131480677 naming ADR-260); Sergeant D entries 423 and 424 confirm the numbers
    changed_via: adr-kit lifecycle
```

## Context

SAT (Smart Autotune Thermostat, the firmware's own room controller) tunes the heating curve coefficient by itself
(ADR-205): every control tick adds a curve signal to the current tuner hour, an hour counts when at least 90 % of its
expected ticks are clean (ADR-234), and the hours are grouped in tuner days of 24 hours. A day counts with at least 6
counted hours, a counted day enters a window of 3 counted days, and with a full window the warm-up and floating phases
each vote on their mean against a band of +-1.0 C (ADR-228, ADR-230). When both agree the coefficient moves 0.1 and the
window empties (`SATcurveTune.h:10-15`, `satTuneCloseDay()` in `SATcontrol.ino`). The fastest possible step is
therefore 72 tuner hours, and in the field it is slower: on gruijter's capture of 13 November 0 of 17 tuner hours
counted (TASK-1889 notes).

Sergeant D accepted Robert's proposal to make the tune react in hours instead of days and keep the control as it is
(entry 329), and chose to keep every exclusion and change only the window and the step (entry 367). thermo-nova, the
reference SAT at commit 7beaaf5, has no coefficient tuner: the coefficient is read from the options once
(`entry_data.py:213`, default `const.py:182` = 2.0), stored in the curve (`heating_curve.py:15`) and only read
(`heating_curve.py:39`, `pid.py:71`). This is firmware-only behaviour (entries 72, 329, 358).

## Decision

The tuner keeps its hour, its exclusions, its two phases and its agreement rule, and changes three constants: a tuner
day becomes a decision block of one hour that counts when its hour counts, the window holds the last 12 counted blocks
(12 counted hours), and a decision moves the coefficient by 0.05.

In code: `SAT_CT_HOURS_PER_DAY` 24 -> 1, `SAT_CT_MIN_CLEAN_HOURS` 6 -> 1, `SAT_CT_WINDOW_DAYS` 3 -> 12, `SAT_CT_STEP`
0.1 -> 0.05. `satTuneCloseDay()` runs unchanged after every tuner hour: a not-counted hour leaves the window as it is,
a counted hour enters it (the oldest drops out when it is full), and from 12 counted hours on each counted hour is a
decision. A change still empties the window, so two steps are at least 12 counted hours apart and 0.1 takes at least 24
counted hours. The identifiers keep the word "day" for the decision block, so the host tests and the window logic stay
the code that was reviewed for ADR-228; the header and this ADR say what a "day" now is.

## Decision Contract

### Must

- Keep every per-tick exclusion of ADR-205, ADR-231 and ADR-234, the heating-failure rule of entry 358, the phase split
  and agreement rule of ADR-228 and the floating minimum of ADR-230 (one hour of floating data in the window).
- Decide on the window of the last 12 counted hours, at each counted hour, with a step of 0.05, and empty the window
  after a change.
- Write the window file `/sat_tune.json` as version 3, and only when the window changed (a counted hour, or a change);
  a version 1 or 2 file is dropped on restore.
- Publish the window size as REST `curve_tune.window_hours` and `curve_tune.window_hours_needed`, and MQTT
  `sat/curve_tune/window_hours`.

### Must Not

- Reuse the old field names `days`, `days_needed` or the topic `sat/curve_tune/days` with a different unit.
- Change any exclusion or the 90 % clean-hour rule.

### Exceptions

- None.

### Verification

- `tests/test_sat_curve_selftune.py` compiles the real tuner blocks of `SATcontrol.ino` with `SATcurveTune.h`: a clear
  signal steps 0.05 after 12 counted hours and not before, a second step needs 12 new counted hours, not-counted hours
  do not move the window, a version 2 file is dropped, version 3 round-trips at its widest values.
- `tests/test_sat_tuner_convergence.py`: the old constants take no step within 24 counted hours of a clear signal (red),
  the new constants do (green).
- `tests/webui/sat-tune-progress.test.mjs` drives the real `data/v2.js` against a status object with `window_hours`.

## Alternatives Considered

- **B: a 6-hour "day" with 2 counted hours, a 3-block window, step 0.1.** Moves 0.1 in at least 18 hours, but a
  block of 6 hours still waits for clean hours to line up inside one block, so a house with scattered clean hours
  rarely completes a block; rejected as the recommendation of Q-1889a.
- **C: a window of 24 counted hours, decided every counted hour, step 0.1.** Same top speed as the decision (0.1 in 24
  counted hours) with double-size steps, so a wrong step costs twice as much before the next decision can correct it.
- **Keep the 3-day window.** Contradicts entry 329 ("hours, not days"), and on the field data it takes no step at all.
- **Rewrite the tuner around an hour ring with new identifiers.** Clearer names, but it replaces reviewed window code
  and most of the 1293-line host test for no behavioural gain; rejected for KISS.

## Consequences

**Positive:**

- The coefficient can follow a mis-set curve within about a day of clean heating hours instead of at best three days.
- No exclusion changes, so the tune never counts an hour it counted as unsafe before (entry 367).
- The change is three constants, a file version and two field names; the decision code stays as reviewed.

**Negative:**

- Twelve hours of mixed warm-up and floating ticks is a noisier signal than three days; the two-phase agreement and the
  1.0 C band are the filter. Mitigation: the step is halved to 0.05, so one wrong step is small and the next decision
  can reverse it after 12 counted hours.
- A 12-hour window holds less floating data than three days. When the window has under one hour of floating ticks the
  floating phase does not vote (ADR-230) and the warm-up phase decides alone (entry 305). In the closed-loop test
  (`tests/test_sat_tuner_convergence.py`, 60 simulated days from 1.0 towards 1.8) that was 1 of 13 decisions; with the
  3-day window every decision had a floating vote. The test still converges without a direction change in every
  scenario (lower 2.6 to 1.4: final 1.55 within the 0.20 tolerance after 21 steps, the last on day 10).
- Every unit in the field drops its stored window once at the upgrade (file version 2 to 3), so any progress towards a
  step on the old 3-day window is lost.
- Contract change for MQTT and REST users: `sat/curve_tune/days` and REST `curve_tune.days`/`days_needed` are gone
  (replaced by `window_hours`/`window_hours_needed`); `clean_hours_today`, `clean_hours_needed`, `hours_today` and the
  topic `sat/curve_tune/clean_hours_today` are removed because a one-hour block has no "today". There is no Home
  Assistant discovery entity for these topics, so no entity id changes; a user automation that reads the old topic
  stops getting values. The beta note says so.
- The window file is written up to once per counted hour instead of once a day (at most 24 small writes a day on
  LittleFS, which wear-levels; a not-counted hour writes nothing).
- The tuner still gets no signal on a house whose hours are all excluded (gruijter, 13 November: 0 of 17). A step in the
  field before the 2.0.0 stable (about 21 November) is unlikely; the host tests are the evidence until then.

## Open Questions

- [x] Q-1889a: Sergeant D confirms option A (12 counted hours, step 0.05) or chooses B or C. Answered A on 2026-11-19
  (entry 423): the decision as built in beta.379, no code change.
- [x] Q-1889b: Sergeant D accepts decisions on the warm-up phase alone when the 12-hour window holds under one hour of
  floating data (entry 305 as is), or asks for a floating vote first. Answered A on 2026-11-19 (entry 424,
  https://discord.com/channels/812969634638725140/1105556725714649128/1572830405680300035): warm-up-only decisions
  stay, no code change.

## Related Decisions

- **ADR-205 (self-tune, hourly evaluation, daily decision):** its daily decision and 3-day window are superseded by this
  ADR; its signal, exclusions and step rule otherwise stay.
- **ADR-228 (warm-up and floating phases):** the phase split and agreement rule stay; the window they are summed over
  becomes 12 counted hours.
- **ADR-230 (floating phase needs one hour of data):** stays; it now applies to the 12-hour window.
- **ADR-231 (PWM ticks on a gas boiler count for warm-up)** and **ADR-234 (90 % clean ticks per hour):** unchanged.

## References

- TASK-1889; Sergeant D corrections entries 72, 305, 329, 340, 358, 367, 422, 423, 424; docs/sat/decisions/open-questions.md Q-1889a, Q-1889b.
- `src/OTGW-firmware/SATcurveTune.h` (constants), `src/OTGW-firmware/SATcontrol.ino` (`satTuneCloseDay`, `satTuneFormat`,
  `satTuneParse`, `satTuneRollHour`, the `curve_tune` REST object and MQTT publish).
- thermo-nova 7beaaf5 (`other-projects/SAT-releases-thermo-nova/custom_components/sat`): `entry_data.py:213`,
  `const.py:182`, `heating_curve.py:15`, `heating_curve.py:39`, `pid.py:71`.
