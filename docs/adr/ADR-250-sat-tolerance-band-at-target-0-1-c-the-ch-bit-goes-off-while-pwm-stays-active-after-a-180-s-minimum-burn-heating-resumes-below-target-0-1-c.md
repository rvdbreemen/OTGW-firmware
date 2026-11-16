---
id: "ADR-250"
title: "SAT tolerance band: at target + 0.1 C the CH bit goes off while PWM stays active, after a 180 s minimum burn; heating resumes below target - 0.1 C"
status: "Proposed"
date: "2026-11-16"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "canonical"
topics:
  - "sat"
  - "tolerance band"
  - "hysteresis"
  - "heating demand"
  - "ch enable bit"
  - "minimum burn"
aliases:
  - "hot tolerance"
  - "cold tolerance"
  - "generic thermostat tolerance"
  - "entry 326"
  - "entry 384"
components:
  - "satToleranceDemand"
  - "satToleranceMaxError"
  - "satBandWaitsForMinBurn"
  - "_sat_tolBandOff"
symbols:
  - "satToleranceDemand"
  - "satToleranceMaxError"
  - "satBandWaitsForMinBurn"
  - "satChEnableDecide"
  - "SAT_TOL_COLD_C"
  - "SAT_TOL_HOT_C"
  - "_sat_tolErr"
  - "_sat_tolDemand"
  - "_sat_tolBandOff"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-250 SAT tolerance band: at target + 0.1 C the CH bit goes off while PWM stays active, after a 180 s minimum burn; heating resumes below target - 0.1 C

## Status

Proposed, 2026-11-16. Guideline-level per ADR-080: the host tests named under Verification check it, it is not wired into evaluate.py. Amends ADR-199 (the CH-enable rule gets one more input).

## Status History

```yaml
status_history:
  - date: 2026-11-16
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context

SAT (Smart Autotune Thermostat) decides whether the boiler heats from the requested setpoint: the cold cutoff of ADR-199 turns heating off when the requested setpoint is at or below the cold setpoint, and in PWM (pulse-width modulation: on/off cycles) the off phase does the same. The room temperature itself has no on/off threshold.

Sergeant D, who designed SAT, asked for "a hot tolerance 0.1 and cold tolerance -0.1" on the SAT climate entity (entry 326 in `docs/sat/sergeant-d-corrections.md`) and pointed to the Home Assistant generic thermostat for the meaning (entry 328): heating stops when the room is at or above target + tolerance and starts again below target - tolerance. He then settled how it works in the firmware, one question at a time:
- entry 361: the band only decides heating on/off, the proportional-integral-derivative (PID) controller keeps running, and the cold cutoff stays as it is;
- entry 384 (Q-377a, "yes, B", confirming entry 377): a band stop sends CH=0 and PWM stays active, not CS=10 with PWM ending, so the burner does not restart inside the 20 minute PWM cycle when the room swings around target + 0.1;
- entry 376 (Q-361c B): a continuous burn runs at least 180 s before a band stop, the same minimum as a PWM ON phase (entry 275);
- entry 378 (Q-361b): with zones, eco means only the main room decides and comfort means any usable room keeps heating on.

thermo-nova at the pinned commit 7beaaf5 has no tolerance band (entry 326, a recursive search of `custom_components/sat`). Its heater-off acts at once (`heating_control.py:229-234`) and its 180 s flame hold belongs to PWM (`pwm.py:201-203`); its zones only raise the requested setpoint (`climate.py:384-385`). So this is a firmware rule on top of the port.

## Decision

While the tolerance band says stop, the CH-enable bit SAT sends is 0, and nothing else changes: the control setpoint, the PWM schedule and its exits, the PID and the cold cutoff run as before.

- The band is a latch (`satToleranceDemand()`, `SATchEnable.h`): it stops at an error (target - room) at or below -`SAT_TOL_HOT_C`, starts again above `SAT_TOL_COLD_C`, and holds in between. Both are 0.1 C, fixed values of entry 326, no setting.
- The input `_sat_tolErr` (`satToleranceMaxError()`) uses the room the PID uses (filtered) and its effective target. In comfort (`iHeatingMode` 0) the largest error over the main room and every usable zone decides; in eco (`iHeatingMode` 1) only the main room (entry 378). With no usable room the band never blocks heating.
- A stop takes effect only after the running CH burn has lasted 180 s (`satBandWaitsForMinBurn()`): in PWM through the cold cutoff's own rule `satPwmColdCutWaitsForMinBurn()` (entry 275), in continuous mode for a lit CH flame younger than 180 s since its flame-on edge (entry 376). Applying the PWM rule to the band is our reading of entry 376's "the same minimum as a PWM ON phase".
- `satChEnableDecide()` gets the effective stop (`_sat_tolBandOff`) as one more input after the cold cutoff; it is read only by the main CH command of the control tick. The other CH senders keep their own value: the thermal safe fallback (CH=1, a safety path with no usable room data) and the summer and valves-closed paths (CH=0).
- The band starts with demand on at boot and when SAT comes back from `satDisable()` (entry 83: the first tick sends a real setpoint).
- A PWM ON phase that starts while the band stop is in force (`_sat_tolBandOff`) is not counted toward the hourly start limit (entry 398). If the band lets CH on again while that ON phase is still running, the phase is counted once at that moment. This second rule is our reading, recorded in the TASK-1887 notes: from then on the burner may start, and entries 59, 90 and 95 count every ON phase in which it may start, lit or not. The count is never on a flame edge. During the 180 s minimum-burn wait CH is still 1, so ON phases in that wait count as before.

## Decision Contract

### Must

- Send CH=0 while the band holds heating off, and leave the control setpoint, the PWM mode and its schedule, the PID and the cold cutoff untouched (entries 361, 384).
- Let a CH burn run 180 s before a band stop takes effect, in PWM and in continuous mode (entries 275, 376).
- Let only the main room decide in eco and any usable room in comfort (entry 378).
- Never block heating when no deciding room temperature is usable.
- Leave an ON phase that starts during a band stop out of the hourly start count, and count it once if CH is allowed again within that phase (entry 398, plus our reading above).

### Must Not

- Must not end PWM or send CS=10 because of the band.
- Must not add a setting or a REST (HTTP API) or MQTT (message broker) field for the band.

### Exceptions

- The thermal safe fallback keeps CH=1: it runs when the room data is too old to trust, so the band input is not trustworthy either.

### Verification

- `tests/test_sat_tolerance_band.py`. It compiles the real `SATchEnable.h` helpers and checks:
  - the exact boundaries at an error of 0.1 and -0.1, and that the state holds inside the band;
  - that a missing room temperature never blocks heating;
  - comfort versus eco with a cold zone;
  - that a band stop clears only the CH bit.
- It also compiles the real control-tick segment of `satControlLoopRun()` and checks:
  - a warm room sends CH=0 while the setpoint stays real and the PID request is unchanged;
  - heating stays off inside the band and resumes below target - 0.1;
  - in PWM the mode stays PWM and the on/off schedule keeps toggling during the stop;
  - a lit PWM phase and a continuous burn both run 180 s first;
  - the cold cutoff is unchanged.
  - the start count (entry 398): flameless ON phases during a band stop leave `cycles_this_hour` unchanged, a phase that starts during a stop and sees CH allowed again counts once, a phase that starts during the 180 s wait counts, and nothing changes outside a stop;
  - the narration names the value the band decided on.
- Source checks:
  - only the main CH command reads the band;
  - the cold-cutoff threshold has no band code;
  - SAT starts with demand on after `satDisable()`.
- Old-vs-fix through `OTGW_SAT_INO`: on origin/dev before this ADR, 10 of the 11 loop and source tests fail. The one that passes is the cold-cutoff control. On the fix, all 18 pass.

## Alternatives Considered

- **Band stop through the cold cutoff's heater-off path (CS=10, CH=0, PWM ends).** Our first proposal. Sergeant D chose against it (entries 377, 384): ending PWM lets the burner restart within a 20 minute cycle when the room swings around target + 0.1.
- **Stop at once in continuous mode.** Sergeant D chose the 180 s minimum burn (entry 376), against short cycling.
- **The main room always decides, or any room always decides.** Sergeant D tied it to the eco and comfort choice instead (entry 378).
- **A setting for each tolerance, like the generic thermostat options.** Entry 326 gives fixed values, and the Home Assistant MQTT climate entity has no such field; a setting would change the settings, REST, MQTT and web contract (YAGNI).

## Consequences

**Positive:**

- Heating pauses once the rooms are 0.1 C above target and resumes when they fall 0.1 C below it, like a Home Assistant generic thermostat.
- PWM keeps its rhythm through a pause, so a room that swings around the band edge does not restart the burner faster than the PWM cycle (entry 384).
- A burn always lasts at least 180 s before the band stops it (entries 275, 376).

**Negative:**

- During a pause the PWM schedule runs ON phases with CH=0, so with no flame. Per entry 398 they do not count toward the hourly start limit, so a long pause does not use up the limit; the cost is one more exception in the start count, which otherwise counts every ON phase (entries 59, 90, 95).
- The PID keeps running during a pause (entry 361), so its integral term moves down in a warm room and the first setpoint after a pause can be lower than before it.
- With 0.1 C on either side, a sensor that steps in 0.1 C increments can flip the band at about every sensor step near the edge. The 180 s minimum burn and the PWM rhythm limit how often the boiler actually switches.

## Open Questions

- [x] During a band pause, do flameless PWM ON phases count as starts in the hourly limit? Answered by Sergeant D on 2026-11-16 (entry 398, Q-384a): no.

## Related Decisions

- ADR-199 (SAT cold state and CH-enable rule): amended, the CH-enable rule gets the band as one more input; the cold cutoff itself is unchanged.
- ADR-196 (SAT ground truth order): the sources this decision is built from.

## References

- `docs/sat/sergeant-d-corrections.md` entries 28, 59, 83, 90, 95, 275, 326, 328, 361, 376, 377, 378, 384, 398.
- thermo-nova 7beaaf5: `custom_components/sat/heating_control.py:66-72`, `:229-234`, `pwm.py:201-203`, `climate.py:384-385`.
- Home Assistant generic thermostat: https://www.home-assistant.io/integrations/generic_thermostat/ (cold_tolerance, hot_tolerance).
- `src/OTGW-firmware/SATchEnable.h` (`satToleranceMaxError`, `satToleranceDemand`, `satChEnableDecide`), `src/OTGW-firmware/SATcontrol.ino` (`satBandWaitsForMinBurn`, the band latch in the control tick, the reset after `satDisable()`).
- TASK-1887.
