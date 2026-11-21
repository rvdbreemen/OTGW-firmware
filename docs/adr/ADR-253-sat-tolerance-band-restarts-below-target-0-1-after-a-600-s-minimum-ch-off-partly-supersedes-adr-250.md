---
id: "ADR-253"
title: "SAT tolerance band restarts below target + 0.1 after a 600 s minimum CH-off (partly supersedes ADR-250)"
status: "Accepted"
date: "2026-11-16"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-250"
  - "ADR-199"
topics:
  - "sat"
  - "tolerance band"
  - "minimum off time"
  - "ch enable bit"
  - "restart threshold"
aliases:
  - "entry 401"
  - "entry 402"
  - "entry 403"
  - "band restart"
  - "10 minute pause"
components:
  - "SAT_TOL_MIN_OFF_MS"
  - "_sat_tolOffSinceMs"
symbols:
  - "satToleranceDemand"
  - "SAT_TOL_MIN_OFF_MS"
  - "_sat_tolOffSinceMs"
  - "_sat_tolBandOff"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-253 SAT tolerance band restarts below target + 0.1 after a 600 s minimum CH-off (partly supersedes ADR-250)

## Status

Accepted, 2026-11-16 (Robert, 22:58 in the coordinator session). Guideline-level per ADR-080: the host tests named under Verification check it, it is not wired into evaluate.py. This ADR replaces ADR-250 (the SAT, Smart Autotune Thermostat, tolerance band) for its restart threshold (target - 0.1) and its hold zone between the two thresholds only; adr-kit has no partial-supersession form, so the front-matter `supersedes` list stays empty and ADR-250 stays Accepted for everything else (the stop at target + 0.1, CH (central heating) bit only, pulse-width modulation (PWM) stays active, the 180 s minimum burn, eco versus comfort, the entry-398 start count) stays as decided there.

## Status History

```yaml
status_history:
  - date: 2026-11-16
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-16
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-250
    changed_via: adr-kit lifecycle
  - date: 2026-11-16
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-199
    changed_via: adr-kit lifecycle
  - date: 2026-11-16
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Accepted by Robert, 2026-11-16 22:58 in the coordinator session ('Accept ADR 253'), relayed by the coordinator
    changed_via: adr-kit lifecycle
```

## Context

ADR-250 (Accepted) built the SAT tolerance band on our reading of the Home Assistant generic thermostat (entry 328 in `docs/sat/decisions/sergeant-d-corrections.md`): heating stops when the deciding rooms are at or above target + 0.1 C and starts again below target - 0.1 C, with a hold zone in between.

On a real boiler (indigo's Vitodens capture, 2026-11-16) Sergeant D, who designed SAT, saw heating start too late and settled the meaning in three steps:
- entry 401 (Q-399a A): both tolerances mean 0.1 above the target, so heating starts again as soon as a room is below target + 0.1; the target - 0.1 reading is withdrawn;
- entry 402 (Q-401a C): the stop stays at target + 0.1, and after a band stop CH (central heating) stays off for a minimum time before it may restart;
- entry 403 (Q-402a A, with 10 minutes): that minimum is 600 s, counted from the moment CH actually goes off (after any 180 s minimum burn of entry 376). Robert put it in release 2.0.0 with high priority.

thermo-nova at the pinned commit has no tolerance band (entry 326), so this stays a firmware rule.

## Decision

The band stops at target + 0.1 and may restart below target + 0.1, and once CH is off for the band it stays off for at least 600 s.

- `satToleranceDemand()` (`SATchEnable.h`): demand at an error (target - room) above -0.1, stop at -0.1 or below. Both thresholds are fixed 0.1 values (no setting); the hold zone is gone.
- `SAT_TOL_MIN_OFF_MS` = 600000 (`SATchEnable.h`). The control tick stores the tick-clock time when `_sat_tolBandOff` turns on (CH really off) in `_sat_tolOffSinceMs`, and keeps `_sat_tolBandOff` on until 600 s have passed, also when a room falls far below the target in the meantime (entry 403).
- The clock starts at the CH-off, not at the stop decision: a demand that returns during the 180 s minimum burn never turned CH off and needs no minimum (entry 403).
- Without a usable room temperature the band has no input and does not hold CH off, also inside the 600 s (entries 38 and 46: a lost room temperature never blocks heating). This is our reading; entries 401-403 do not mention it.
- The narration follows CH itself, once each per pause: "Heating paused ...", "Heating pause minimum 600 s: a room is below target + 0.1, CH stays off N s more", "Heating resumes: target - room X.XX, below target + 0.1; CH was off N s".
- PWM (pulse-width modulation) ON phases during the whole pause, the 600 s included, are not counted toward the hourly start limit (entries 398, 400), unchanged.

## Decision Contract

### Must

- Stop at target + 0.1 and allow a restart below target + 0.1 (entries 401, 402).
- Keep CH off for 600 s after it went off for the band, counted from the CH-off (entry 403).
- Never hold CH off for the band when no deciding room temperature is usable.

### Must Not

- Must not start the 600 s at the stop decision while the 180 s minimum burn still holds CH on.
- Must not add a setting or a REST (HTTP API) or MQTT (message broker) field for the band times.

### Exceptions

- The thermal safe fallback keeps CH=1, as in ADR-250.

### Verification

- `tests/test_sat_tolerance_band.py` on the real `SATchEnable.h` and the real control-tick segment of `satControlLoopRun()`:
  - demand above -0.1 and no hold zone;
  - a room back at the target restarts only 600 s after the CH-off;
  - a room far below the target still waits for the 600 s;
  - the 600 s starts at the CH-off after a 180 s burn, not at the stop decision;
  - a room hovering at target + 0.1 restarts at most once per 600 s;
  - no room temperature never blocks heating;
  - the narration texts and their once-per-pause count;
  - the entry-398 start-count tests still pass.
- Old-vs-fix: on the code before this ADR the 7 new-behaviour tests fail and 19 pass; on the fix all 26 pass.

## Alternatives Considered

- **Keep the hold zone (restart below target - 0.1).** Withdrawn by entry 401: it started heating later than Sergeant D intends.
- **No hysteresis and no minimum time.** With stop and restart on the same line, a room that hovers at target + 0.1 flips CH every tick; Sergeant D chose a minimum time instead of a gap (entry 402).
- **Move the stop line up to make a gap.** Option B of Q-401a; not chosen (entry 402).
- **A 300 s minimum.** Our first reading of entry 402; Sergeant D set 10 minutes (entry 403).

## Consequences

**Positive:**

- Heating restarts as soon as a room is below target + 0.1, not 0.2 C later.
- A pause lasts at least 10 minutes, so the boiler does not short-cycle on a room that hovers at the band edge.

**Negative:**

- After a pause the room can fall further below the target during the 600 s than with the old hold zone (a cold room has to wait up to 10 minutes).
- Two time rules now meet at a stop: the 180 s minimum burn before, the 600 s minimum CH-off after.

## Open Questions

- [x] Should the band hold CH off for the rest of the 600 s when the room temperature is lost during a pause? Decided by acceptance: released (entries 38/46; recorded as interpretation in entry 405). Robert accepted this ADR on 2026-11-16 at 22:58 in the coordinator session, with this reading named in the accept question. Asked to Sergeant D as Q-403a; if he chooses otherwise, a new ADR follows.

## Related Decisions

- ADR-250 (SAT tolerance band): partly superseded, the restart threshold and the hold zone; the rest stands.
- ADR-199 (SAT cold state and CH-enable rule): unchanged, the band remains one input of the CH-enable rule.

## References

- `docs/sat/decisions/sergeant-d-corrections.md` entries 38, 46, 326, 328, 376, 398, 400, 401, 402, 403, 405.
- `src/OTGW-firmware/SATchEnable.h` (`satToleranceDemand`, `SAT_TOL_MIN_OFF_MS`), `src/OTGW-firmware/SATcontrol.ino` (the band block of the control tick).
- TASK-1887.
