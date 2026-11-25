---
id: "ADR-236"
title: "SAT leaves PWM for continuous only when the flow stays at least 3.0 C under the request for 180 s, thermo-nova's underheat exit"
status: "Accepted"
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
aliases:
  - "PWM underheat exit"
  - "entry 448 exit"
  - "PWM to continuous"
  - "Q-342b"
  - "thermo-nova underheat exit"
components:
  - "SAT PWM to continuous exit"
symbols:
  - "satCycleCheckPwmExits"
  - "SAT_UNDERHEAT_SUSTAIN_MS"
  - "SAT_UNDERSHOOT_MARGIN_C"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-236 SAT leaves PWM for continuous only when the flow stays at least 3.0 C under the request for 180 s, thermo-nova's underheat exit

## Status

Accepted, 2026-11-25, by Robert van den Breemen. Proposed 2026-11-14 and rewritten 2026-11-21 after Sergeant D's answer D to Q-342b (entry 448), replacing the entry-342 rule in place. Guideline-level per ADR-080: verified by host tests, no evaluate.py gate.

**Decision Maker:** Sergeant D, who decides SAT behaviour (ADR-196). Entry 448, 2026-11-21: "My answer is D".

## Status History

```yaml
status_history:
  - date: 2026-11-14
    status: Proposed
    changed_by: "Agent (d3)"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-25
    status: Accepted
    changed_by: "Agent: Claude (otgw-architect)"
    reason: Accepted by Robert van den Breemen, 2026-11-25, interactive in the coordinator session (relayed to otgw-architect)
    changed_via: adr-kit lifecycle
```

## Context

SAT (Smart Autotune Thermostat) runs the boiler either continuously or in PWM (pulse width modulation: timed `ON` and `OFF` phases for a low heat demand). This ADR covers the PWM to continuous exit while the boiler underheats.

**The thermo-nova reference, pinned at 7beaaf5.** It leaves PWM for continuous on a sustained underheat (`custom_components/sat/heating_control.py:331-354`). The conditions:

- the PWM status is ON, and there is heating demand (`demand.heater_state == HeaterState.ON`, `heating_control.py:317`). `HeaterState.ON` is a property of the demand, not of the flame: `HEAT` mode, valves open and the request above `COLD_SETPOINT` (`heating_control.py:65-72`). There is no flame check;
- the flow is at or below the request plus `UNDERSHOOT_MARGIN_CELSIUS` (-3.0, `cycles/const.py:11`), compared at `heating_control.py:337`;
- that holds for `UNDERHEAT_SUSTAIN_SECONDS` (180, `const.py:22`), checked at `heating_control.py:345`;
- not during hot water, and not within 300 s after it.

**Sergeant D's 60 s rule.** In 2026-11 he replaced this exit with his own rule: flame on and the request above the flow for 60 s (entries 339 and 342, TASK-1933). Option A of Q-342a already named the risk: "it may leave PWM about 60 s after each ignition" and could toggle. He chose to build it and watch the testers' captures (entry 350).

**The field data showed the toggling.**

- **indigo_light**, Vitodens 100-W, beta.392, 21 Nov: the burns at 01:40, 03:03, 04:25 and 11:38 looped 3-4 times each.
- **The mechanism**, from the raw frames:
  1. PWM entry, then the minimum-power command MM=0 lands in the thermostat's MsgID 14 slot 1-55 s later.
  2. The modulation reads 0 % within 4-6 s and the flow falls 8-13 C.
  3. "Request above the flow" holds for 60 s, and SAT goes to continuous.
  4. Continuous runs at MM=100 and modulation 60-98 %. The flow reaches 39-41 C, the 3.0 C fail-safe fires, and SAT is in PWM again.
- **The replay.** It runs on the real `SATcycles.ino` switch code, closed loop on the mode, and reproduces 34 of 34 recorded switches. Per night burn:
  - the device rule: 3-4 exits;
  - a 1.5 C margin: 2-4 exits;
  - a latch after the fail-safe: 1;
  - thermo-nova's 3 C / 180 s: 0.
- **Why thermo-nova's rule gives 0.** "flow <= request - 3.0" held for at most 60 s in any loop, never the 180 s it needs.
- **marceld's capture (beta.392)** gives the same ordering: A and C 2 switches, D none.
- **Evidence, under `OTGW-field-data`:**
  - `indigo_light/2026-11-21_2.0.0-beta.392/_replay/REPORT.md` with SHA256SUMS;
  - `marceld91d/2026-11-21_2.0.0-beta.392/_replay/`;
  - the tools in `_replay_tools/sat_switch/`.

With those results Sergeant D chose option D of Q-342b (entry 448).

## Decision

While SAT is in PWM and the PWM phase is ON, and there is heating demand, it switches to continuous only when the flow is at or below the request minus 3.0 C for 180 s: thermo-nova's own underheat exit (`heating_control.py:331-354`).

- **What it replaces.** The entry-342 rule (request above the flow for 60 s) is gone, and so is its "flame on" condition. Entry 448 settles the rule as identical to `heating_control.py:337-347`, and that code checks heating demand, not the flame (`heating_control.py:65-72`, `:317`).
- **The gates of thermo-nova stay:**
  - not during hot water or the 300 s after it;
  - not in the PWM `OFF` phase;
  - not without heating demand, including a request at or below the cold setpoint (`heating_control.py:317`; the firmware's counterpart is the per-heating-system cold setpoint of ADR-199);
  - a missing flow temperature clears the timer.
- **Saturation exit.** It is unchanged: 300 s without off time, with the live-flow duty since beta.410, entries 437 and 444.
- **The start gate of ADR-252 stays in the code.** It keeps every PWM to continuous exit quiet until 60 s after the first CH flame-on. With a 180 s hold it can no longer change the outcome of this exit (entry 448, our interpretation), but it still guards the other exits.
- **Hysteresis.** Continuous to PWM needs flow - request >= 2.0 C for 60 s (ADR-254, entry 407). PWM to continuous now needs request - flow >= 3.0 C for 180 s. The band between them is 5 C wide, against 0 C to 2.0 C before.

## Decision Contract

### Must

- **What counts.** A tick counts only in the PWM ON phase, with heating demand (request above the cold setpoint, ADR-199) and `flowTemp <= requestedSetpoint - SAT_UNDERSHOOT_MARGIN_C` (3.0 C, `SATcycles.ino:43`). The flame state is not part of the condition. Any other tick clears the timer.
- **When it fires.** On the first control tick at least 180 s (`SAT_UNDERHEAT_SUSTAIN_MS` = 180,000 ms) after the first qualifying tick.
- **What stays as it is:** the PWM ON, hot-water and 300 s guards, the saturation exit and the ADR-252 start gate.

### Must Not

- Count a tick without heating demand, in the PWM `OFF` phase, or with the flow above request - 3.0 C.
- Add a flame condition that thermo-nova does not have. If one is wanted, it goes to Sergeant D first.
- Bring back the entry-342 60 s rule, or a margin or latch variant of it (Q-342b options B and C, TASK-2987 and TASK-2989 stay 2.1.0 per entry 448).

### Exceptions

- None.

### Verification

- **Host test.** `tests/test_sat_overshoot_sustain.py` compiles the real `satCycleCheckPwmExits()`:
  - the exit fires 180 s after the first tick at a flow 3.0 C below the request (equality counts, as `heating_control.py:338` returns only when flow > threshold);
  - a flow 2.9 C below never fires;
  - no heating demand (request at or below the cold setpoint) never counts;
  - the flame bit does not change the outcome.

  The cases fail on the entry-342 code; a mutant with a 60 s hold fails the 180 s case.
- **Replay.** `switch5.py` on the indigo_light and marceld captures gives 0 PWM to continuous exits in the loop windows (`_replay/REPORT.md`, column "thermo-nova 3 C / 180 s").
- **Field.** The next captures from indigo_light and marceld are checked for PWM to continuous exits inside one burn.

## Alternatives Considered

- **A. Keep the entry-342 rule (request above the flow for 60 s).** Rejected by Sergeant D (entry 448). The field and the replay show 3-4 loops per night burn.
- **B. Entry 342 with a 1.5 C margin (TASK-2989).** Not chosen (entry 448): 2-4 exits remain in the replay, and it stays 2.1.0.
- **C. A latch after the fail-safe (TASK-2987, entry 433).** Not chosen (entry 448): 1 exit plus 1 fail-safe per burn remain, and it stays 2.1.0.
- **Do nothing.** Rejected: the loop costs cycles and keeps the boiler swinging between 0 % and 98 % modulation.

## Consequences

**Positive:**

- The PWM to continuous loop stops in the replayed burns: 0 exits instead of 3-4 per burn.
- **Back on the reference.** The exit is identical to thermo-nova at the pin, and SAT 5.0.0 keeps the same exit (entry 449: `heating_control.py:418-430` upstream, same constants).
- **A wide hysteresis:** 2.0 C in, 3.0 C out.

**Negative:**

- **SAT leaves PWM later** when the boiler really cannot reach the request: after 3 minutes at 3 C below, instead of 1 minute at any shortfall. Mitigation: the saturation exit (300 s without off time, live-flow duty) still catches a boiler that keeps running at full duty.
- **ADR-252 has a stale pointer.** It names "the underheat exit (ADR-236)" in its gate list (`ADR-252`:99, :114). The pointer stays correct as a name, but the 60 s exit it guarded is gone. No supersede of ADR-252 is needed: the gate mechanism is unchanged.

## Related Decisions

- **ADR-196** (SAT ground truth): Sergeant D's entry 448 decides; thermo-nova at the pin confirms the rule.
- **ADR-252** (PWM-first start and 60 s start gate, Accepted): unchanged; its gate keeps applying to this exit.
- **ADR-254** (continuous to PWM at 2.0 C for 60 s; it partly replaces ADR-249): the opposite transition, unchanged; together they form the 2.0 C / -3.0 C hysteresis.
- **ADR-249** (Accepted, partly superseded by ADR-254): unchanged.
- **ADR-250 / ADR-253** (tolerance band): unchanged ("the PWM schedule and its exits run as before").

## References

- Sergeant D corrections, `docs/sat/decisions/sergeant-d-corrections.md`:
  - entries 339, 342 (the replaced rule), 350 (build and watch), 433 and 435 (latch and margin, not chosen), 437 and 444 (live-flow duty);
  - entry 448 (answer D, the decision);
  - entry 449 (upstream at 5.0.0, the pin stays 7beaaf5).
- thermo-nova 7beaaf5, `custom_components/sat/`:
  - `heating_control.py:307-376` (the exit dispatcher and the underheat and saturation exits; underheat at `:331-354`, comparison `:337-338`, hold `:345`);
  - `cycles/const.py:11`; `const.py:22`, `:23`.
- Firmware `src/OTGW-firmware/SATcycles.ino`:
  - `:43` `SAT_UNDERSHOOT_MARGIN_C`;
  - `:47` `SAT_UNDERHEAT_SUSTAIN_MS`, today 60,000 ms;
  - `:1494-1500`, the exit, today the entry-342 rule.
- Replay evidence, under `OTGW-field-data`:
  - `indigo_light/2026-11-21_2.0.0-beta.392/_replay/REPORT.md`, `loops_out.txt`, `switch5_out.txt`, SHA256SUMS;
  - `marceld91d/2026-11-21_2.0.0-beta.392/_replay/`.
- Tasks: TASK-1933 (the entry-342 implementation, replaced); the implementation task for this rewrite is otgw-dev's (number added when filed).
