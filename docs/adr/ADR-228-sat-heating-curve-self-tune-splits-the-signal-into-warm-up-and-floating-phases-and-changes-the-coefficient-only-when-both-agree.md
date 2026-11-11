---
id: "ADR-228"
title: "SAT heating curve self-tune splits the signal into warm-up and floating phases and changes the coefficient only when both agree"
status: "Accepted"
date: "2026-11-11"
binding: false
gate: "tests/test_sat_curve_selftune.py"
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-205"
  - "ADR-230"
topics:
  - "heating curve self-tune"
  - "phase split"
  - "warm-up phase"
  - "floating phase"
  - "deadband"
  - "phase agreement"
aliases:
  - "autotune"
  - "self-tune"
  - "curve tuner"
  - "phase-based self-tune"
components:
  - "SAT heating curve self-tune decision rule"
symbols:
  - "SAT_CT_PHASE_BAND_C"
  - "satTuneTickPhase"
  - "satTunePhaseSay"
  - "satTuneCloseDay"
  - "fHeatingCurveCoeff"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-228 SAT heating curve self-tune splits the signal into warm-up and floating phases and changes the coefficient only when both agree

## Status

Accepted, 2026-11-11. This ADR replaces ADR-205 decision 1 (signal) and decision 3 (decision rule) only, and its Must "Use the curve
integral (setpoint before the PWM (pulse-width modulation) override, minus the curve) as the only signal". All other ADR-205 decisions
(clean hour, bounds, PID (proportional-integral-derivative controller), persistence, setting, logging) stay in force. The gate named in
the frontmatter is the host test that compiles the real tuner (ADR-080).

**Decision Maker:** Sergeant D (SAT design owner) for the rule, entries 303-307 of `docs/sat/sergeant-d-corrections.md`;
Robert (maintainer) accepts or rejects this record. Written by an agent (otgw-firmware-d3, TASK-1796).

## Status History

```yaml
status_history:
  - date: 2026-11-11
    status: Proposed
    changed_by: Agent (otgw-firmware-d3)
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-11
    status: Accepted
    changed_by: Robert van den Breemen
    reason: Accepted by Robert van den Breemen, 2026-11-11, via the coordinator session ('Accept both'); TASK-1796, Sergeant D entries 303-307
    changed_via: adr-kit lifecycle
  - date: 2026-11-11
    status: Accepted
    changed_by: Robert van den Breemen
    reason: "ADR-228 replaces ADR-205 decision 1 (signal) and decision 3 (decision rule) only, and its Must 'curve integral as the only signal'; the other ADR-205 decisions stay in force. Accepted by Robert van den Breemen, 2026-11-11, via the coordinator session ('Accept both')"
    changed_via: adr-kit lifecycle
  - date: 2026-11-12
    status: Accepted
    changed_by: "Agent: Claude"
    reason: ADR-230 amends ADR-228 (Sergeant D entries 313, 315, 316)
    changed_via: adr-kit lifecycle
```

## Context

The SAT (Smart Autotune Thermostat) heating curve self-tune of ADR-205 changes the heating curve coefficient
(`settings.sat.fHeatingCurveCoeff`) by 0.1 at most once a day. Its signal is the curve integral: per control tick the PID
output before the PWM override (clamped to the setpoint range) minus the curve value, averaged over the clean hours of the
last 3 counted days; above +1.0 C it raises, below -1.0 C it lowers (ADR-205 decisions 1 and 3, entries 163-175).

On 2026-11-11 Sergeant D proposed to use the warm-up overdamping for the tuner (entry 302): outside the deadband the curve
value does the work an integral would do, so a too-low coefficient shows as a slow warm-up. He decided:

- entry 303 (msg 1569961995187261481): keep the 3-counted-day rule "to make sure that the overdamping is real"; raise only
  when the warm-up phase and the floating phase both say higher;
- entry 304 (msg 1569963753720651867, "Yes mirrored. So the answer is A"): lowering is the mirror. Our reading of the same
  message for Q-1787d (the phase split), confirmed to him in msg 1569964436796739676: floating = room within the deadband,
  warm-up = outside, per phase the ADR-205 mean and +-1.0 C rule, change only when both agree.

The deadband is thermo-nova's `DEADBAND = 0.1` (`custom_components/sat/const.py:16`, pin 7beaaf5), used by the PID at
`pid.py:187` (`if abs(state.error) > DEADBAND:` resets the integral) and `pid.py:205` (`if abs(state.error) <= DEADBAND:`
holds the derivative). The firmware has the same constant as `SAT_PID_DEADBAND` (`SATtypes.h:450`). thermo-nova has no
coefficient tuner (entry 298), so this rule is our own design under Sergeant D's direction (ADR-196 order of sources).

## Decision

Each signal tick of the self-tune is assigned to the warm-up phase (|PID error| > 0.1 C) or the floating phase
(|PID error| <= 0.1 C); each phase keeps its own tick-weighted mean of the ADR-205 signal over the 3 counted days and says
higher (above +1.0 C), lower (below -1.0 C) or hold; the coefficient moves 0.1 only when both phases say the same direction,
and a phase without data in the window does not veto: the phase with data then decides alone.

1. **Phase per tick.** The error is the held PID error `state.sat.fError` (target minus room, set by the 60 s PID step,
   `SATpid.ino:380`), the same value the PID deadband acts on. A tick without a room temperature has no phase (error NaN);
   a hot-water tick has no signal and no phase. "Outside" means both sides of the target: an overshoot tick counts as
   warm-up (entry 306, Q-1787f A, msg 1569976134198628413).
2. **Signal per phase.** Unchanged from ADR-205 decision 1: `clamp(pidRaw, 10 C, system maximum) - curve`. The phase sums
   only come from clean hours (ADR-205 decision 2, unchanged).
3. **Mean per phase.** Tick-weighted: the phase's signal sum over the counted days of the window divided by its signal
   ticks. ADR-205 used an hour-weighted mean of hour means; tick weighting stops an hour with one floating tick from
   weighing as much as an hour with sixty.
4. **Decision.** Once a day on a counted day with 3 counted days in the window: raise when both phases say higher, lower
   when both say lower; hold otherwise, with reason `inside_band` or `phases_disagree`. A phase without a tick in the
   window takes the verdict of the other phase, so the phase with data decides alone (entry 305, Q-1787e B, msg
   1569975937657999373; `SAT_CT_MISSING_PHASE_VETO 0`); with no phase data at all the reason is `no_phases`. Step 0.1, clamp 0.1-5.0, at most once a
   day, window empty after a change, no PID reset (ADR-204): all unchanged.
5. **Constants.** `SAT_CT_PHASE_BAND_C 0.1f`, `SAT_CT_BAND_C 1.0f`, `SAT_CT_STEP 0.1f` in `SATcurveTune.h`, so a correction
   by Sergeant D is a one-line change.
6. **Window file.** `/sat_tune.json` gets version 2 (`"v":2`) with per day the warm-up and floating tick counts and sums.
   A version-1 file has mixed-phase sums that cannot be split and is dropped on restore (`SATTUNER restored=0`).
7. **Logging.** ADR-205 decision 8 plus: `SATTUNEH` `wmean wticks fmean fticks`; `SATTUNED` `wmean3 wticks3 wsay fmean3
   fticks3 fsay` and the reasons above; the change line and narration name both phase means. `mean3` (ADR-205 value) is
   still logged for comparison and no longer decides.

## Decision Contract

### Must

- Classify each signal tick of a clean hour as warm-up (|error| > `SAT_CT_PHASE_BAND_C`) or floating (|error| <= it), on
  the held PID error; a tick without a room temperature or with hot water has no phase.
- Decide on per-phase tick-weighted means over the last 3 counted days, each with the symmetric +-1.0 C threshold.
- Change the coefficient only when both phases say the same direction, by 0.1, clamped to 0.1-5.0, at most once a day;
  a phase without data in the window does not veto (the phase with data decides alone).
- Keep every other ADR-205 decision (clean hour, PID untouched, persistence, setting, logging).
- Log the per-phase means, tick counts and verdicts in the hourly and daily records.
- Drop a window file without `"v":2`.

### Must Not

- Change the coefficient on one phase alone while the other phase has data and does not say the same direction.
- Use the overall mean (`mean3`) for the decision.
- Mix ticks of excluded hours, hot-water ticks or room-lost ticks into a phase.

### Exceptions

- None.

### Verification

- `python tests/test_sat_curve_selftune.py`: compiles the real `satCurveTune` blocks of `SATcontrol.ino` with
  `SATcurveTune.h`. Class `PhaseSplit`: raise on +/+, lower on -/-, hold on +/- and on +/0 (warm-up +2.4, floating 0.0:
  the ADR-205 tuner raises on the overall +1.8), a lone phase deciding when the other has no data, hold without any phase data, tick weighting, deadband boundary 0.1 / 0.1001 /
  both signs / NaN, excluded and hot-water ticks kept out, manual change clearing the phase day counters, version-1 file
  dropped, file round trip at 3600 ticks an hour. Red on the code before this ADR (`OTGW_SAT_INO` and `OTGW_SAT_HDR` from
  origin/dev f64aba399: 8 failures, 14 errors), green after (73 tests).
- `python tools/research/sat/sat_curve_selftune_study12.py`: study evidence from the research model (not a firmware test),
  rule P (this ADR) against ADR-205 and no tuner per house type and heat source.
- A bench run with SAT simulation that shows the per-phase hourly and daily records.

## Alternatives Considered

- **Keep ADR-205 (one signal over all clean hours).** Rejected by Sergeant D (entries 302, 303): it does not separate the
  warm-up overdamping from the floating behaviour, and it raises on a large warm-up signal while the floating phase is fine
  (the `test_warmup_high_floating_inside_holds` case).
- **Decide on the difference between the phases (our first option A of Q-1787a).** Not chosen: Sergeant D asked for
  agreement of both phases, not their difference (entry 303).
- **Warm-up phase only (Q-1787a option B).** Not chosen (entry 303): the floating phase must confirm.
- **Hour-weighted mean per phase (as ADR-205).** Rejected: an hour with one floating tick would weigh as much as an hour
  with sixty; tick weighting is the plain reading of "mean of setpoint minus curve over the 3 counted days".
- **Keep the version-1 window file and treat its days as phase-less.** Rejected: those days would dilute the phase means
  with nothing; dropping costs at most 3 days of evaluation once, after the update.

## Consequences

**Positive:**

- A change needs evidence from both phases, so a large warm-up signal alone (or a floating offset alone) no longer moves
  the curve.
- Per-phase records make the field logs show which phase drives or blocks a change.

**Negative:**

- (Resolved by Q-1787e B.) With too low a curve the room may never come within 0.1 C of the target during clean hours; then the floating phase has
  no data and the tuner holds (`no_floating`) where it should raise. Research-model smoke run (round 12): house 1992-2012
  heavy, gas, start 1.2: ADR-205 raises 4 times to 1.6, this rule holds 63 days with `no_floating`. Mitigation: asked to
  Sergeant D as Q-1787e (msg 1569970979948601356; recommendation: the phase with data decides alone). Full round 12
  (`docs/sat/sim-1519/data/round12_runs.json`, 5 houses x gas/hp30/hp60 x 5 starts x 2 seeds, paired; research model):
  minutes per day outside +-0.3 C, 2025-26 / 2010-11: no tuner 146 / 210, ADR-205 100 / 124, this rule 114 / 168,
  the variant where a phase without data does not veto 100 / 125 (changes per winter 2.2 / 4.0, 1.3 / 1.9, 2.1 / 3.0).
  Low starts (1.0, 1.2) never corrected: ADR-205 20/60 and 18/60, this rule 38/60 and 37/60, the variant 20/60 and 18/60.
- An update drops a window that was being collected (version-1 file): up to 3 days without a decision, once.
- The window file grows from about 60 to about 300 bytes; `SatTune` grows by 48 bytes of RAM (3 days x 2 phases x 8 B).

## Open Questions

- [x] Q-1787e: when one phase has no tick in the 3 counted days, hold or let the phase with data decide alone? — **Answered
  2026-11-11 by Sergeant D (msg 1569975937657999373, entry 305):** B, the phase with data decides alone; built.
- [x] Q-1787f: does an overshoot tick count as warm-up or only ticks below the target? — **Answered 2026-11-11 by Sergeant D
  (msg 1569976134198628413, entry 306):** A, both sides of the deadband; built.
- [x] Q-1787d: the phase split at the 0.1 C deadband — **Answered 2026-11-11 by our reading of entry 304** ("So the answer
  is A"), confirmed to Sergeant D in msg 1569964436796739676 and not corrected by him; his answers 305 and 306 build on this split.
  Robert weighs whether this reading suffices for acceptance.
- [x] Q-1787b (the tuner until this lands) — **Answered 2026-11-11 by Sergeant D (msg 1569976299315920906, entry 307):** A, the
  ADR-205 tuner stays on unchanged until this one ships; not part of this ADR.

## Related Decisions

- **ADR-205 (heating curve self-tune)**: partially superseded, decisions 1 and 3 and the "only signal" Must. Its other
  decisions stay in force.
- **ADR-204 (PID reset triggers)**: complements; a tuner change still leaves the PID terms alone.
- **ADR-196 (SAT ground truth)**: the rule comes from Sergeant D's recorded decisions (entries 302-304); thermo-nova has no
  tuner.
- **ADR-198 (coefficient range)**: the clamp 0.1-5.0 is unchanged.
- **ADR-080 (binding ADRs need a gate)**: the host test named in the frontmatter is the gate.

## References

- TASK-1796.
- `docs/sat/sergeant-d-corrections.md` entries 163, 164, 171-176 (ADR-205 design, unchanged except signal and decision),
  298 (no tuner in thermo-nova), 302-307. Each agrees with this record: 302-306 are its source, 307 keeps the ADR-205 tuner
  on until this one ships; 163/171 (signal is
  setpoint minus curve) still hold per phase; 164/172-175 (3-day window, +-1.0 C, step 0.1) still hold per phase; 176
  (on by default) unchanged; entry 166 (1.6-2.4 band) untouched.
- thermo-nova pin 7beaaf5: `custom_components/sat/const.py:16`, `custom_components/sat/pid.py:187`, `pid.py:205`.
- Firmware: `src/OTGW-firmware/SATcurveTune.h` (constants and types), `src/OTGW-firmware/SATcontrol.ino` (satCurveTune
  blocks: `satTuneTickPhase`, `satTuneAddTick`, `satTuneCloseHour`, `satTunePhaseSay`, `satTuneCloseDay`,
  `satTuneFormat`, `satTuneParse`; call site `ti.error = roomValid ? state.sat.fError : NAN;`).
- `docs/sat/sat-log-format.md` (`SATTUNEH`, `SATTUNED`, `SATTUNER`, `SATTUNE` rows).
- `tests/test_sat_curve_selftune.py`, `tools/research/sat/sat_curve_selftune_study12.py`.

## Enforcement

```json
{
  "forbid_pattern": [],
  "forbid_import": [],
  "require_pattern": [
    {
      "pattern": "#define SAT_CT_PHASE_BAND_C",
      "path_glob": "src/OTGW-firmware/SATcurveTune.h",
      "message": "ADR-228: the phase split band is a named constant in SATcurveTune.h."
    }
  ],
  "llm_judge": true
}
```
