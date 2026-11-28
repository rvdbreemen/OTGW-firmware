---
id: "ADR-308"
title: "SAT heating curve self-tune counts a very-low-power hour when the room is more than 0.1 C below target (amends ADR-205)"
status: "Proposed"
date: "2026-11-28"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "canonical"
topics:
  - "sat"
  - "heating-curve-self-tune"
  - "minimum-modulation"
aliases:
  - "count low-power hours"
  - "min_mod exclusion"
  - "Q-3106b"
  - "TASK-3106"
components:
  - "SAT heating curve self-tune"
symbols:
  - "satTuneTickReason"
  - "SATT_MIN_MOD"
  - "SAT_CT_PHASE_BAND_C"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-308 SAT heating curve self-tune counts a very-low-power hour when the room is more than 0.1 C below target (amends ADR-205)

## Status

Proposed, 2026-11-28. Guideline-level (ADR-080): checked by the host test of the pure tuner block, no evaluate.py gate.

## Status History

```yaml
status_history:
  - date: 2026-11-28
    status: Proposed
    changed_by: "Agent: Claude (otgw-architect)"
    reason: "Initial proposal. Robert, #dev-sat-mqtt 1576322428009975940 (19:42:33 UTC): 'ADD the auto tune counting at very low power now.' Implements Sergeant D's entry 513 (Q-3106b B) in the next 2.0.0 beta instead of 2.1.0. Coordinator order 2026-11-28 20:43 laptop time."
    changed_via: adr-kit
```

## Context

Terms: SAT (Smart Autotune Thermostat, the thermostat logic in the firmware); the self-tune (the part of SAT that changes the heating curve coefficient by itself); PID (proportional-integral-derivative controller); error (target - room, so positive means the room is below target); minimum modulation (the boiler's lowest power, MsgID 15 low byte, 5 % when MsgID 15 was never seen).

The self-tune leaves a tick out when the boiler runs at or below its minimum modulation, the `min_mod` exclusion (ADR-205 decision 2, entry 174; `SATcontrol.ino:6735-6736` at origin/dev `d20abbbe7`). PWM ticks on a gas boiler are already exempt (ADR-231, entry 322; `:6727`, `:6734-6736`). An hour counts when at least 90 % of its ticks are clean (ADR-234, entry 340; `:6836-6837`).

As a result, the tuner cannot learn on evenings like indigo_light's. There the room sat 0.12 to 0.26 C under target for hours with the boiler at its minimum, and the PID asked 1 to 7 C more flow than the curve. Those hours were all left out (entry 502; the replay below).

Decisions this rests on:
- **Entry 502** (Sergeant D, Q4 = C, 2026-11-27): count low-modulation hours when the room is under target. Release 2.1.0 as posted.
- **Entry 513** (Sergeant D, Q-3106b = B, 2026-11-28): such an hour counts only when the room is more than 0.1 C below target, the same limit the tune uses for its warm-up phase. Release 2.1.0 as posted.
- **Robert, 2026-11-28 19:42:33 UTC**, #dev-sat-mqtt message 1576322428009975940, read back by otgw-architect: "ADD the auto tune counting at very low power now." This moves the release from 2.1.0 to the next 2.0.0 beta. The release scope is Robert's call. The behaviour is Sergeant D's entry 513 and is unchanged.

Evidence: the replay `OTGW-field-data/_comms/selftune_replay/REPORT.md` (otgw-webui, 2026-11-27, read by otgw-architect). It compiles the real pure tuner block and feeds it the recorded ticks.
- indigo_light beta.428, 25 h: with "any shortfall" (error > 0), 14 hours count instead of 4. The 12-hour window fills at hour 16 with a mean of +3.06 C, both phases agree, and the coefficient goes 2.30 -> 2.35.
- With the threshold of entry 513 (error > 0.1 C), only 9 hours count. The window does not fill in 25 h, and nothing moves.
- marceld91d beta.428 (23 h) and will_85903 beta.428 (25 h): no change.

Thermo-nova (7beaaf5) and Alex's SAT 5.1.0 (9c2fad9) have no self-tune that changes the coefficient. 5.1.0 has a read-only advisor, compared in `OTGW-field-data/_comms/curve_advisor_compare_2026-11-28.md`. So no reference rule applies (entry 502: "firmware-only").

## Decision

The `min_mod` exclusion of the self-tune no longer applies to a tick whose error is above `SAT_CT_PHASE_BAND_C` (0.1 C, `SATcurveTune.h:44`). Such a tick counts as clean. Every other exclusion and its order stay as they are.

1. **The rule, per tick.** The `min_mod` exclusion (`SATcontrol.ino:6735-6736`) is skipped when `isfinite(error) && error > SAT_CT_PHASE_BAND_C`. At or below 0.1 C under target, at target, or above target, a tick at minimum modulation is still excluded.
2. **Per tick, not per hour.** Entry 513 says "an hour ... counts". The tuner judges ticks, and an hour counts at 90 % clean ticks (ADR-234). We apply the rule per tick, as the replay did. Reading (ours): an hour in which the room is more than 0.1 C below target for at least 90 % of the ticks then counts, which is what entry 513 describes.
3. **Same limit as the phase split.** The threshold is the existing warm-up band (ADR-228), not a new constant. Such a tick always falls in the warm-up phase.
4. **Unchanged:**
   - the 0.05 step and the 12-counted-hour window (ADR-260);
   - the 1.0 C decision band;
   - the two-phase agreement (ADR-228);
   - the PWM rule (ADR-231);
   - the other exclusions: SAT not active, hot water, diagnostic event, sun, PWM on a heat pump or hybrid, flame off with CH on, no room temperature (`:6728-6737`);
   - the coefficient range 0.1 to 5.0.
5. **The logged reason.** An hour that only counts because of this rule keeps reason `none`. A new reason name is not added.

## Decision Contract

### Must

- Skip `min_mod` only for a tick with a finite error above `SAT_CT_PHASE_BAND_C`.
- Keep the order of every other exclusion in `satTuneTickReason()`.
- Have a host test on the pure tuner block (`tests/test_sat_curve_selftune.py`) that is red on the code before this ADR and green after:
  - a minimum-modulation tick at error 0.2 counts;
  - one at error 0.1 and one at error 0.05 are excluded as `min_mod`;
  - one with a NaN error is excluded (`no_room` or `min_mod`, as today).

### Must Not

- Change the step, the window, the band, the phase split or the PWM rule.
- Count a minimum-modulation tick when the room is at or above target - 0.1 C.
- Add a new tuner setting for this rule.

### Exceptions

- None.

### Verification

- The host test above, red then green (owner otgw-poc-dev).
- The replay of `REPORT.md` rerun on the built code, with indigo_light beta.428 at the 0.1 C threshold: expected 9 counted hours and no decision in 25 h. A different result is a finding.
- The bench run by otgw-bench-tester: the self-tune hour line still logs, and a minimum-modulation hour below target - 0.1 counts (simulation).

## Alternatives Considered

- **Any shortfall counts (error > 0, entry 502 option as first replayed).** Rejected by Sergeant D in entry 513 (B over A). It moved the curve on indigo, but it also counts hours that are only a few hundredths under target.
- **Keep the exclusion until 2.1.0 (entry 513 as posted).** Overruled by Robert's order for the next beta.
- **Count the whole hour when its mean error is above 0.1 C.** Not chosen: the tuner has no hour-level error today, and a per-tick rule keeps the 90 % clean share of ADR-234 meaningful.
- **A separate threshold constant.** Not chosen: entry 513 names the same limit as the warm-up phase.

## Consequences

**Positive:**

- The tuner can learn on a house whose boiler sits at its minimum while the room stays clearly below target. That is the indigo_light case, the blind spot that entry 174 accepted.
- A small code change in one pure function, covered by an existing host test file.

**Negative:**

- On the only affected capture, the 0.1 C threshold changes nothing within 25 h. The benefit in the field is not shown yet. Mitigation: the hour and day lines show the counted hours, so a longer capture shows whether the window fills.
- A room that is below target for another reason (sensor placement, an open window, a cold room on the thermostat) with an oversized boiler at its minimum pushes the curve up. A higher curve raises the flow request and the short-cycling pressure that the `min_mod` exclusion was meant to avoid (entries 174, 322; REPORT.md "Main risk"). Mitigation: only ticks more than 0.1 C below target count, the step stays 0.05 per full 12-hour window, the range stays 0.1 to 5.0, and the user can switch the self-tune off.
- Entry 502's reading was "replay first, build only if the curve moves the right way". At the chosen threshold, the replay shows no movement either way. Robert's order brings it in now.

## Open Questions

- [ ] A register entry for Robert's order of 2026-11-28 19:42:33 UTC (message 1576322428009975940): it moves entry 513 from 2.1.0 to the next 2.0.0 beta. Owner: otgw-docs via the coordinator. It must exist before the code ships.
- [ ] Sergeant D is told that entries 502 and 513 now ship in the 2.0.0 beta instead of 2.1.0, with the replay result at his threshold. Owner: BlueFlame after the beta, through the coordinator.
- [ ] Entry 174 (Robert, 2026-10-31: accept the low-load blind spot) is not named as changed by entries 502 or 513. The register should say that 513 now limits it. Owner: otgw-docs.
- [ ] Replay on indigo_light's beta.451 run, or a longer modulating-boiler capture, at 0.1 C. Owner: otgw-poc-dev.

## Related Decisions

- **ADR-205**: amended in decision 2. The `min_mod` part of the clean-hour rule gains the exemption above; the rest stays.
- **ADR-228**: unchanged; its warm-up band is the threshold.
- **ADR-231**: unchanged; PWM ticks on a gas boiler were already exempt.
- **ADR-234**: unchanged; the 90 % clean share applies to the newly counted ticks.
- **ADR-260**: unchanged; step 0.05, window 12 counted hours.

Sergeant D entries checked:
- **Agree:** 502 and 513 (implemented); 161, 171, 205, 228, 234 and 317 (unchanged); 322 (PWM exemption, unchanged); 340 (90 % share, unchanged).
- **Changed in effect, not by name:** 174 (see Open Questions).
- **Entries 31 and 80:** these are PWM entries (duty ranges and the 180 s minimum flame time; the ON-phase timing and ignition wait). They do not touch the self-tune, so there is no conflict.

## References

- TASK-3106 (self-tune low-modulation hours).
- `src/OTGW-firmware/SATcontrol.ino:6725-6740` (`satTuneTickReason`), `:6797-6817` (`satTuneAddTick`), `:8567-8585` (tick input) at origin/dev `d20abbbe7`.
- `src/OTGW-firmware/SATcurveTune.h:44` (`SAT_CT_PHASE_BAND_C`), `:32` (`SAT_CT_DEFAULT_MIN_MOD`).
- `OTGW-field-data/_comms/selftune_replay/REPORT.md` (replay, otgw-webui, 2026-11-27).
- Register entries 174, 322, 340, 502, 513 (`docs/sat/decisions/sergeant-d-corrections.md`).
- Discord #dev-sat-mqtt 1576322428009975940 (Robert, 2026-11-28 19:42:33 UTC).
