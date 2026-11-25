---
id: "ADR-296"
title: "SAT keeps one PWM ON clock per flame: a switch to continuous freezes it, a PWM re-entry resumes it, and the minimum burn counts from the real flame-on"
status: "Proposed"
date: "2026-11-25"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "sat"
  - "pwm"
  - "control-mode"
  - "minimum-burn"
aliases:
  - "PWM ON freeze"
  - "frozen PWM ON clock"
  - "entry 475 freeze"
  - "TASK-3039"
components:
  - "SAT PWM ON clock across mode switches"
symbols:
  - "satSetControlMode"
  - "satPwmBeginOn"
  - "satPwmReset"
  - "_pwm_lastUpdateMs"
  - "_sat_chFlameOnMs"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-296 SAT keeps one PWM ON clock per flame: a switch to continuous freezes it, a PWM re-entry resumes it, and the minimum burn counts from the real flame-on

## Status

Proposed, 2026-11-25. Guideline-level per ADR-080: verified by a host test, no evaluate.py gate. Release 2.0.0 (Robert, 2026-11-24).

**Decision Maker:** Sergeant D, who decides SAT behaviour (ADR-292), in entries 475, 476, 478 and 479 (2026-11-24 and 2026-11-25). The points marked "interpretation (ours)" below are not his words. Robert said yes to all four on 2026-11-25 (Open Questions); Sergeant D may still correct any of them as a new entry.

**Acceptance condition (Robert, 2026-11-25):** this ADR stays Proposed until the host test F1-F10 (`tests/test_sat_pwm_on_freeze.py`) is green on the real code and the bench run of TASK-3039 confirms it; only then does it go to Accepted.

## Status History

```yaml
status_history:
  - date: 2026-11-25
    status: Proposed
    changed_by: "Agent: Claude (otgw-architect)"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context

SAT (Smart Autotune Thermostat) runs the boiler either continuously or in PWM (pulse width modulation: timed `ON` and `OFF` phases). Inside one flame, SAT can flip between the two modes several times.

**How a flip inside one flame happens today** (firmware at origin/dev 15d8e2b58):

1. PWM to continuous: an exit rule fires (underheat, saturation or at-max, `SATcycles.ino:1488-1516`) and sets `_cycle_burnLeftPwm`.
2. Continuous to PWM inside the same flame: only the fail-safe, flow at least 3.0 C above the request for 30 s (`SATcycles.ino:41-42`, `:1434-1466`; entry 429).
3. Every mode change goes through `satSetControlMode()` (`SATcontrol.ino:1975`), which calls `satPwmReset()` (`:1985`, body `:1936-1969`). That sets the ON clock `_pwm_lastUpdateMs` to now (`:1942`) and clears `_pwm_flameOnMs` (`:1944`).
4. The re-entry opens a fresh ON phase in `satPwmBeginOn()` (`:1435`): the clock starts at now (`:1440`) and, with the flame burning, so does the minimum-burn anchor (`:1451`).
5. The ON phase ends when `millis() - _pwm_lastUpdateMs >= onTimeMs` (`:1783`, `:1798-1800`), after the minimum burn of 180 s (600 s for a heat pump) from `_pwm_flameOnMs` (`:1796-1797`).

**The result.** Each fail-safe re-entry gives the flame a full new ON time, so three or four flips stretch the PWM ON time of one flame.

**Sergeant D's decisions** (`docs/sat/decisions/sergeant-d-corrections.md`, OTGW-sat-decisions origin/main 5b8c13d):

- **Entry 475 (the request):** "Whenever the flame is ON we count time only once. So when goes from PWM to continuous we freeze the PWM ON time instead of resetting."
- **Entry 476 (Q-475a, answer A):** only PWM time counts; continuous time is not added; the total resets when the flame goes out.
- **Entry 478 (Q-475a1, answer A):** the freeze is meant for the fail-safe flips of entry 429, the only flips left inside one flame.
- **Entry 479 (Q-475b, answer A):** the minimum burn counts from the real flame-on of the flame, across flips. A resume with little ON time left ends when the frozen clock ends.

**thermo-nova, a reference (rank 4 under ADR-292).** At 7beaaf5 (local copy `other-projects/SAT-releases-thermo-nova/`, whose `UPSTREAM_COMMIT` names 7beaaf5), `disable()` calls `reset()` (`custom_components/sat/pwm.py:106-108`). `reset()` sets `_last_update` to now and clears `_flame_on_at` (`:87-97`); the ON phase ends when `elapsed >= on_time_seconds` (`:134`, `:205`), after the hold from `_flame_on_at` (`:201`). thermo-nova therefore restarts the ON clock every time PWM is left. This decision differs from it by design.

**A precedent in the firmware.** A hot-water draw already freezes the ON clock: `satPwmOnDhwStart()` stores the used time (`SATcontrol.ino:1520`), and `satPwmOnDhwEnd()` sets the anchor back to `millis() - _pwm_dhwUsedMs` (`:1599`, `:1616`; entry 95).

## Decision

SAT keeps one PWM ON clock per flame.

1. **Freeze.** When SAT leaves PWM while the flame burns, the ON time already used in this flame is kept. It is not reset.
2. **Resume.** When SAT enters PWM again in the same flame, the new ON phase continues from the kept time: the anchor is set back by that time, as the hot-water freeze does. The existing check `elapsed >= onTimeMs` then ends the phase against the ON time the schedule asks for at that tick.
3. **Only PWM time counts.** Time in continuous is not added (entry 476).
4. **Minimum burn from the real flame-on.** The 180 s (600 s heat pump) minimum burn counts from the CH flame-on of the flame (`_sat_chFlameOnMs`, `SATcontrol.ino:8121-8125`), across flips (entry 479).
5. **Reset.** The kept time goes back to zero:
   - when the flame goes out (entry 476);
   - when an ON phase ends by its own timer (interpretation (ours): the next ON phase is a new one, otherwise it would end at once);
   - on heater off, SAT disable and restart (interpretation (ours), in line with entry 462: no PWM state is kept after a restart or a full heating-off).
6. **Hot water** keeps its own freeze; draw time still does not count (entry 95 unchanged).
7. **Which flips.** Every PWM to other-mode switch with the flame burning freezes, through the one code path `satSetControlMode()`. That covers the exit rules followed by the fail-safe re-entry (entry 478) and also a manual `control_mode` command (interpretation (ours): the same code path, no separate rule).

## Decision Contract

### Must

- Decide the freeze on the live flame, `satIsFlameOn()`, at the moment PWM is left, not on the ON-phase state (`bPwmFlameRequested`, `_pwm_flameOnMs`). Reason: at flame-off the end-of-cycle verdict can switch PWM to continuous (`SATcycles.ino:1235`, `:1045`) after the burn-end hook has run (`:1121`), while the ON-phase flag is still set.
- Clear the kept time after the mode switch inside the reset paths, not before: `satPwmDisableOnHeaterOff()` switches mode first (`SATcontrol.ino:2018`) and resets after (`:2020`); the same applies to `satSetControlMode(SAT_MODE_OFF)` (`:4395`) and the switch at `:8495`.
- Set the minimum-burn anchor explicitly from the real flame-on. Do not inherit it from the set-back clock (today `_pwm_flameOnMs` copies `_pwm_lastUpdateMs` at `:1451`).
- Fall back to the segment's own flame-on time when `_sat_chFlameOnMs` is 0 (a flame lit with the hot-water bit set, or before the first flame-off was seen, `:8121-8125`). Otherwise the `!= 0` guard at `:1797` would skip the minimum burn altogether (interpretation (ours) of entry 479 for that edge).
- Leave unchanged: the start gate (entries 385/387, ADR-252), the exit rules (ADR-236, entries 456 and 463), the per-flame start counting (entry 428) and the hourly start limit.

### Must Not

- Add continuous time to the ON clock (entry 476, option B not chosen).
- Count the minimum burn per PWM segment (entry 479, option B not chosen).
- Clear the kept time in `satPwmReset()`: it runs on every mode change, so the freeze would never survive a flip.

### Exceptions

- None.

### Verification

- **Host test `tests/test_sat_pwm_on_freeze.py`**, compiling the real PWM block of `SATcontrol.ino` as `tests/test_sat_pwm_anchors.py` does. Each case runs on the code before this ADR (red where marked) and on the fix (green):
  - F1 (red before): one flip in one flame. The ON phase ends once the PWM time of both segments adds up to the ON time, not a full ON time after the re-entry.
  - F2: three flips: the sum of the PWM segments equals the ON time.
  - F3: flame out between two segments: the clock starts at zero.
  - F4: an ON phase ended by its timer: the next starts at zero.
  - F5: a hot-water draw during a frozen or running clock: draw time not counted.
  - F6: frozen time already at or above the ON time at the resume: the phase ends on the first tick after the minimum burn from the real flame-on.
  - F7: PWM left while waiting for ignition: nothing kept.
  - F8: a PWM to continuous switch by the end-of-cycle verdict at flame-off: nothing kept for the next flame.
  - F9: a flame lit with the hot-water bit set (`_sat_chFlameOnMs` 0): the minimum burn still applies.
  - F10: a first PWM entry with nothing kept: the same ON phase as before this ADR.
- The existing tests of the start gate, the exit rules and the per-flame counting stay green.
- **Bench and field.** A flame with several flips on the bench (otgw-bench-tester) shows a shorter total PWM ON time per flame than before; Sergeant D's testers confirm it in the field.

## Alternatives Considered

- **Keep the thermo-nova reset** (`pwm.py:106-108`). Rejected by Sergeant D (entry 475): every flip gives the flame a full new ON time.
- **Count all flame time, continuous included** (Q-475a option B). Not chosen (entry 476): continuous time would eat the ON time, which is not a freeze.
- **Minimum burn per PWM segment** (Q-475b option B). Not chosen (entry 479): a resume with little ON time left would still burn 180 s more.

## Consequences

**Positive:**

- One ON clock per flame: the fail-safe flips of entry 429 no longer stretch a flame's PWM ON time.
- The mechanism reuses the hot-water freeze (set the anchor back), so the end check `elapsed >= onTimeMs` stays one line.

**Negative:**

- **Small expected effect.** In BlueFlame's open-loop replay of indigo's beta.410 logs (entry 476), the freeze pre-empts about 4 of 62 flips. Mitigation: the flips themselves are the subject of entries 464 and 466; this ADR does not claim to stop them.
- **State that survives a mode change** is a new place for stale-state bugs. Mitigation: the explicit reset list above and cases F3, F4, F7 and F8.
- **A deliberate difference from thermo-nova.** Mitigation: recorded here, and in `docs/sat/firmware-sat-spec.md` when built.

## Open Questions

- [x] Robert accepts the four interpretations marked "(ours)": reset at the end of an ON phase; reset on heater off, SAT disable and restart; a manual `control_mode` flip freezes too; the fallback when the real flame-on is not known. — **Answered 2026-11-25 by Agent: Claude (otgw-architect):** Robert, 2026-11-25 (interactive in the coordinator session, relayed): yes to all four (a reset at the end of an ON phase, b reset on heater off, SAT disable and restart, c a manual control_mode flip freezes too, d the segment fallback when the real flame-on is unknown), as our interpretations, not Sergeant D's words. Acceptance of this ADR waits until the host test F1-F10 is green on the real code and the bench confirms.

## Related Decisions

- **ADR-292** (SAT ground truth): the recorded entries decide; thermo-nova is a reference.
- **ADR-236** (PWM to continuous underheat exit): unchanged; it is one of the exits that freezes the clock.
- **ADR-252** (PWM-first start and 60 s start gate): unchanged.
- **ADR-250 / ADR-253** (tolerance band and its minimum CH-off): unchanged.

## References

- Sergeant D entries 475, 476, 478 and 479, `docs/sat/decisions/sergeant-d-corrections.md` (OTGW-sat-decisions origin/main 5b8c13d); also entries 95, 428, 429 and 462.
- thermo-nova 7beaaf5, `custom_components/sat/pwm.py:87-97`, `:106-108`, `:134`, `:201`, `:205`.
- Firmware `src/OTGW-firmware/SATcontrol.ino`: `:1435-1457` (`satPwmBeginOn`), `:1520`, `:1599`, `:1616` (hot-water freeze), `:1783-1800` (ON phase end and minimum burn), `:1936-1969` (`satPwmReset`), `:1975-1999` (`satSetControlMode`), `:2013-2025` (`satPwmDisableOnHeaterOff`), `:4395`, `:8121-8125` (CH flame-on).
- Firmware `src/OTGW-firmware/SATcycles.ino`: `:41-42`, `:1434-1466` (fail-safe), `:1488-1516` (exits), `:1121`, `:1235` (flame-off hooks).
- TASK-3039 (implementation plan and the architect's review notes).
