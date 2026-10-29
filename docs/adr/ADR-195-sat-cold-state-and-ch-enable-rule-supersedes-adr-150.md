---
id: "ADR-195"
title: "SAT cold state and CH-enable rule (proposed successor to ADR-150)"
status: "Superseded"
date: "2026-10-29"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes:
  - "ADR-150"
superseded_by: "ADR-199"
topics:
  - "sat"
  - "cold-setpoint"
  - "ch-enable"
  - "pwm"
  - "opentherm"
aliases:
  - "cold cutoff"
  - "COLD_SETPOINT"
  - "CH enable bit"
  - "CH rule during PWM"
  - "per-system cold setpoint"
components:
  - "SAT cold cutoff"
  - "SAT CH enable rule"
symbols:
  - "satGetColdSetpoint"
  - "satColdCutoff"
  - "satChEnableDecide"
  - "satPwmFlameOffHold"
  - "satEnqueueIfChangedCH"
  - "satPwmDisableOnHeaterOff"
  - "satCycleResetSustainGuards"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-195 SAT cold state and CH-enable rule (proposed successor to ADR-150)

## Status

Superseded by ADR-199, 2026-10-29.

This ADR is the proposed successor to ADR-150. ADR-150 stays Accepted and its front matter is untouched while this ADR is Proposed, so the `supersedes` list here is empty: adr-kit writes both sides of a supersession at once (`adr supersede`), and running it now would retire ADR-150 before this decision is accepted.

The maintainer accepted this decision on 2026-10-24. The `<=` boundary is settled by entry 12; the remaining confirmations (CH=1 during the PWM (pulse-width modulation) ON phase on Sergeant D's own setup, TASK-1316; the OT-Direct gateway path, TASK-1238) are recorded under Open Questions as deferred.

## Status History

```yaml
status_history:
  - date: 2026-10-23
    status: Proposed
    changed_by: "Agent: Claude"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-10-24
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Supersedes ADR-150
    changed_via: adr-kit lifecycle
  - date: 2026-10-24
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Accepted decision after all four verification gates passed
    changed_via: adr-kit lifecycle
  - date: 2026-10-29
    status: Superseded
    changed_by: Robert van den Breemen
    reason: Maintainer OK in session 2026-10-29 (TASK-1355); open questions answered by Sergeant D
    changed_via: adr-kit lifecycle
```

## Context

SAT (Smart Autotune Thermostat) is the firmware's port of the thermo-nova Home Assistant component. Ground truth for SAT behaviour is, in order: Sergeant D's recorded corrections in `docs/sat/sergeant-d-corrections.md`, then the thermo-nova code at commit `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb` (local copy `other-projects/SAT-releases-thermo-nova/`, paths below relative to `custom_components/sat`). The firmware is never a source of truth for SAT behaviour (CLAUDE.md, "SAT ground truth").

thermo-nova has one global cold threshold, `COLD_SETPOINT = 22` (`const.py:34`), next to `MINIMUM_SETPOINT = 10.0` (`const.py:35`). It uses it at six places:

- heater state: ON only when `requested_setpoint > COLD_SETPOINT` (`heating_control.py:72`);
- PWM enable guard (sustained overshoot): returns and clears its timer when `requested_setpoint <= COLD_SETPOINT` (`heating_control.py:280-282`);
- PWM disable guards: reset when the heater is not ON or `requested_setpoint <= COLD_SETPOINT` (`heating_control.py:317-319`);
- PWM gate of the setpoint override: returns when `requested_setpoint <= COLD_SETPOINT` (`heating_control.py:385`);
- flame-off hold floor: `clamp(return + offset, COLD_SETPOINT, maximum_setpoint)` (`heating_control.py:398`);
- and the CH (central heating) enable bit after every control step: ON only when `_control_setpoint > COLD_SETPOINT` (`heating_control.py:238`), which the serial OTGW (OpenTherm Gateway) coordinator turns into the CH enable bit (`coordinator/serial/__init__.py:212-214`).

When the heater state is off (OFF), `update()` takes the else branch: `self._pwm.disable()` (`heating_control.py:230`), which is `reset()` plus `_enabled = False` (`pwm.py:106-109`), then `_reset_pwm_disable_guards()` (`:231`), `_control_setpoint = MINIMUM_SETPOINT` (`:233`) and the maximum relative modulation (`:234`).

ADR-150 (Accepted 2026-06-24) brought a per-heating-system cold setpoint into the firmware. Its Decision text no longer describes HEAD (`c3aa767a0`):

1. ADR-150 effect 1 states the cutoff as `pidOutput < satGetColdSetpoint()` ("falls below"). Since TASK-1323 (commit `9ec1b87e2`) the cutoff is `requestedSetpoint <= coldSetpoint` (`SATchEnable.h:26-29`). At exactly 28.2 C or 21.0 C the firmware now cuts off, where ADR-150's text would not. `tests/test_sat_ch_enable.cpp` prints the strict rule as "old".
2. ADR-150 derives CH from `finalSetpoint > SAT_MIN_SETPOINT`. HEAD decides CH in `satChEnableDecide` (`SATchEnable.h:46-52`) from the requested setpoint and the PWM (pulse width modulation) phase, called at `SATcontrol.ino:5069` (TASK-1316, commit `812cca580`; simplified by TASK-1323).
3. ADR-150 lists three effects. Sergeant D's entry 11 (answer 2) adds the PWM gate and the CH bit as cold-setpoint sites, and entry 12 adds switching PWM off with a state and guard reset at the cutoff. HEAD implements that reset since TASK-1327 (commit `c3aa767a0`): `satPwmDisableOnHeaterOff()` in `SATcontrol.ino`.
4. ADR-150's Enforcement says "Manual review only". A host test now compiles the real `SATchEnable.h`.

ADR-150 names the SAT designer George; his Discord name is Sergeant D (handle sergeantd), the name the corrections file and this ADR use. Sergeant D (named George in ADR-150) is the same person.

The decisions this ADR records, all from 2026-10-23, in `docs/sat/sergeant-d-corrections.md`:

- **Entry 5** (PWM off: CH=0 together with CS=10), #alpha-testing 10:21 UTC (Coordinated Universal Time), https://discord.com/channels/812969634638725140/1514720723980259460/1563135315991072789: "When the PMW goes to OFF, correctly you send the CS=10 command, but you should also send the command CH=0." (His PMW means PWM.)
- **Entry 7** (CH threshold and cold-start hold), 12:49 UTC, https://discord.com/channels/812969634638725140/1514720723980259460/1563172561162862714 ("below 28.2 not 22. Other than that follow the thermo-nova exactly") and 13:31 UTC, https://discord.com/channels/812969634638725140/1514720723980259460/1563182969345282071 ("You are correct"; "We can limit the cold start value to 28.2 and 21 for underfloor").
- **Entry 9** (CH rule during PWM), 15:08 UTC, https://discord.com/channels/812969634638725140/1514720723980259460/1563207530275152037: "great understanding!" in reply to our four-point proposal. **Interpretation (ours):** not an explicit yes; read as yes to all four points.
- **Entry 11** (28.2/21 at every thermo-nova `COLD_SETPOINT` site), 16:47 UTC, https://discord.com/channels/812969634638725140/1514720723980259460/1563232315969249376: "The switch thresholds are 28.2/ 21. If the requested setpoint is lower than the thresholds then we send CH=0 CS=10" and "Yes we use 28.2/21".
- **Entry 12** (cold cutoff: switch PWM off and reset the setpoint), 18:13 UTC, https://discord.com/channels/812969634638725140/1514720723980259460/1563254104346730600: "yes keep the thermo-nova solution". **Interpretation (ours):** an OK to mirror thermo-nova on the cutoff.

## Decision

While SAT is the active controller, one per-heating-system cold setpoint stands in for thermo-nova's `COLD_SETPOINT` 22 at every place thermo-nova uses it: the heater state, the PWM enable and disable guards, the PWM gate, the flame-off hold floor and the CH bit. `satGetColdSetpoint()` (`SATcontrol.ino:266-270`) returns 28.2 C for radiators (`SAT_COLD_SETPOINT_RAD`) or 21.0 C for underfloor (`SAT_COLD_SETPOINT_FLOOR`, `SATcontrol.ino:57-58`). These are fixed constants, not settings (entry 7; entry 11 answer 2).

The terms used: CS (Control Setpoint) is the gateway command for the CH water target, CH (central heating) the enable bit command, MM (Maximum relative Modulation) the modulation cap command, OT (OpenTherm) the boiler protocol. The requested setpoint is the PID (proportional-integral-derivative) output after the multi-zone block and the solar offset, clamped and stored as `_sat_requestedSetpoint` (TASK-1323, `SATcontrol.ino:4978-4993`).

**1. Cutoff.** When the requested setpoint is at or below the cold setpoint (`satColdCutoff`, `<=` as thermo-nova `heating_control.py:72` and `:385`), SAT sends CS=10, CH=0 and MM at the configured maximum relative modulation (`settings.sat.iMaxRelModulation`, default 100), switches PWM off, and resets the PWM state and the PWM disable guards, as `heating_control.py:229-234` and `pwm.py:106-109` do (entry 12). HEAD (`c3aa767a0`, TASK-1327) does this in `satControlLoopRun()` (`SATcontrol.ino:5052-5060`): on `satColdCutoff` it sets the final setpoint to `SAT_MIN_SETPOINT` (10) and calls `satPwmDisableOnHeaterOff()` (`SATcontrol.ino:997-1006`), which:

- always leaves PWM mode for continuous (`satSetControlMode(SAT_MODE_CONTINUOUS)`, which also resets the PWM state). The Force PWM option that used to keep PWM here was removed (TASK-1332, Sergeant D entry 13, 2026-10-23);
- calls `satPwmReset()` (`SATcontrol.ino:966-975`) in every case: PWM phase, ignition wait, hold and effective boiler temperature;
- calls `satCycleResetSustainGuards()` (`SATcycles.ino:147-152`), which clears the sustained overshoot, underheat and saturation timers, as thermo-nova `_reset_pwm_disable_guards()` (`heating_control.py:525-528`);
- sets the held CS `_sat_lastControlCS` to `SAT_MIN_SETPOINT`, so a later PWM tick cannot return a stale pre-cutoff CS;
- sets `state.sat.iCurrentModulation` to `settings.sat.iMaxRelModulation` (`heating_control.py:234`).

The CS, MM and CH commands then go out through the write-on-change helpers (`SATcontrol.ino:5063-5069`), CH=0 from point 2. The all-valves-closed path calls the same `satPwmDisableOnHeaterOff()` (`SATcontrol.ino:4850-4858`), as thermo-nova treats closed valves as heater state OFF (`heating_control.py:69-70`). `satCycleInit()` (`SATcycles.ino:197-209`) still clears the guards at start-up through the same helper. PWM comes back through `satCycleCheckAutoSwitch()` (`SATcycles.ino:951`), which the loop skips while cold (`SATcontrol.ino:5034`). thermo-nova's second re-enable path, `on_cycle_end()` after a continuous cycle classified OVERSHOOT (`pwm.py:219-223`), is not ported (TASK-1331, open).

**2. CH enable.** `satChEnableDecide` (`SATchEnable.h:46-52`) decides the bit:

- requested setpoint at or below the cold setpoint gives CH=0 (with CS=10 from point 1);
- PWM OFF phase, while PWM drives the CS (`satPwmDrivesSetpoint`, `SATcontrol.ino:906-910`), gives CS=10 and CH=0 (entries 5 and 9; `satApplyPWM` returns `SAT_MIN_SETPOINT` at `SATcontrol.ino:922-925`);
- everything else gives CH=1. That includes the PWM ON phase, whatever low CS keeps the flame on, continuous mode, and a PWM tick that keeps the previous CS.

This departs deliberately from thermo-nova `heating_control.py:238` in two ways:

- **PWM ON.** thermo-nova would clear CH whenever its suppression CS (flow minus the offset, `heating_control.py:441`) is at or below the threshold during a burn. The firmware keeps CH=1. This rests on entry 9, which is our reading of "great understanding!".
- **Requested, not sent.** thermo-nova compares the sent `_control_setpoint`; the firmware compares the requested setpoint. Example: overshoot protection off, held CS 24, requested setpoint 30 gives CH=1 in the firmware, where thermo-nova with 28.2 substituted would give CH=0. That this follows from entry 11 answer 1 is our reading (TASK-1323 notes).

**3. Flame-off hold.** The flame-off PWM hold stays clamped to [cold setpoint, maximum setpoint] (`satPwmFlameOffHold`, `SATcontrol.ino:704-710`, floor at `:707`), as `heating_control.py:398` with 28.2/21 for 22 (entry 7).

**4. CH cadence.** The CH bit is sent on change plus every 5 minutes (`satEnqueueIfChangedCH`, `SATcontrol.ino:432-441`, `SAT_CMD_REFRESH_MS` at `:353`), not after every control step as `heating_control.py:238`. Entry 7 records that we named this to Sergeant D and he did not answer; it is kept without an explicit OK. TASK-1324 tracks a related gap: the write-on-change cache can skip a CH/MM resend after an OT online edge early after boot.

**Scope.** This fires only while SAT is the active controller. The SAT-disabled path still releases the boiler to the thermostat with CS=0 through `satEnqueueOwnCS` (`SATcontrol.ino:2250`), unchanged. The cutoff's CS=10 goes out through `satEnqueueIfChangedCS`, which calls `satEnqueueOwnCS` (`SATcontrol.ino:385-393`), so ADR-179's MsgID 1 ownership is unchanged. The CH=0 goes out through `satEnqueueIfChangedCH` and `addCommandToQueue` (`SATcontrol.ino:432-441`); how that bit reaches the boiler in OT-Direct gateway mode is not settled (see Consequences and ADR-179).

## Decision Contract

### Must

- Use `satGetColdSetpoint()` (28.2 radiators, 21.0 underfloor) at every site where thermo-nova uses `COLD_SETPOINT`: the cutoff, the PWM enable and disable guard gates, the PWM gate, the flame-off hold floor and the CH bit.
- Compare with `<=` for the cutoff (`satColdCutoff`), and `>` for "warm", as thermo-nova does.
- On the cutoff: send CS=10, CH=0 and MM at the configured maximum relative modulation; switch PWM off; reset the PWM state, the held CS and the sustained overshoot, underheat and saturation guard timers (`satPwmDisableOnHeaterOff()`).
- Decide the CH bit only in `satChEnableDecide`, from the requested setpoint and the PWM phase.
- Send SAT's CS through `satEnqueueOwnCS` (ADR-179).

### Must Not

- Turn the cold setpoints into user settings, or reintroduce the global 22.
- Clear CH during the PWM ON phase because the flame-keeping CS is low.
- Derive CH from the sent CS.
- Change the SAT-disabled release path (CS=0 to the thermostat).

### Exceptions

- None. The heater-off branch always leaves PWM mode, as thermo-nova's `pwm.disable()` does unconditionally (`heating_control.py:230`). The earlier exception, keeping PWM on the cutoff when the firmware's own `bForcePWM` option was on (TASK-1327, `c3aa767a0`), is gone: Sergeant D entry 13 (2026-10-23) removed the Force PWM option completely (TASK-1332). thermo-nova stored that option at `entry_data.py:58` and `:223` but never read it in `custom_components/sat`.

### Verification

- `tests/test_sat_ch_enable.cpp` compiles the real `SATchEnable.h` and checks the boundary at exactly 28.2 and 21.0, the PWM OFF and ON phases, and continuous mode, printing the strict TASK-1316 rule as "old".
- `python tests/test_sat_pwm_override.py` compiles the real PWM block of `SATcontrol.ino` (including `satPwmReset()` and `satPwmDisableOnHeaterOff()`), `satCycleCheckAutoSwitch()` of `SATcycles.ino` and the requested-setpoint segment of `satControlLoopRun()`. Since TASK-1327 its cutoff scenario calls the real `satPwmDisableOnHeaterOff()` under `satColdCutoff` and checks the PWM phase and held-CS reset, leaving PWM mode when not forced, the sustain-guard reset, the maximum relative modulation and the closed-valves path (`test_cutoff_*`, `test_closed_valves_reset_pwm_like_the_cold_cutoff`); `ColdCutoffSource` checks at source level that the loop calls it inside the cutoff block and the valves-closed path. It does not reach the CH queueing (stated in its docstring). The TASK-1327 commit message records that these cases fail on the previous code and pass on the fix; that old-vs-fix run was not repeated for this ADR.
- Neither test runs in CI: a grep of `.github/`, `evaluate.py` and `build.py` for both names found nothing (2026-10-23). This ADR claims no gate until they do.
- Field: Sergeant D's confirmation on his own setup (TASK-1316 AC3). The bench unit has no room temperature, so SAT never reaches the CH step there (entry 11, still open).

## Alternatives Considered

- **Amend ADR-150 in place.** Rejected: ADR-150 is Accepted and immutable; its Decision text is the record of what was decided on 2026-06-24. CLAUDE.md requires a superseding ADR for a changed Accepted decision.
- **Follow thermo-nova `heating_control.py:238` literally for CH** (CH from the sent CS after every step, threshold 28.2/21). Rejected on entry 9 (our reading): the low CS that keeps the flame on at minimum modulation during PWM ON (entry 8) would clear CH mid-burn and stop the burner the PWM cycle wants running.
- **Strictly below (`<`) for the cutoff**, as in ADR-150 and Sergeant D's wording ("below 28.2", "lower than the thresholds"). Not chosen for now: thermo-nova uses `<=` at `heating_control.py:72` and `:385`, and he asked to follow thermo-nova apart from the threshold (entry 7). Following thermo-nova's comparison is our choice; he has not confirmed it. If he picks `<`, this ADR is revised before acceptance.
- **One ADR per correction entry** (5, 7, 9, 11, 12). Rejected: the entries describe one cold-state rule; five thin ADRs would split it and cross-reference each other.
- **Do nothing (leave ADR-150 as the record).** Rejected: a later maintainer reading ADR-150 would find a rule (strict `<`, CH from `finalSetpoint`) the code no longer follows, and no record of the two deliberate CH departures from thermo-nova.

## Consequences

**Positive:**

- One definition of "cold" across all six SAT sites, matching Sergeant D's 28.2/21 and thermo-nova's structure.
- The CH bit has one decision point that a host test compiles as it is.
- The two CH departures from thermo-nova are on record with their source and the strength of that source.

**Negative:**

- **Two departures rest on interpretation.** CH=1 during PWM ON rests on "great understanding!" (entry 9), and CH from the requested setpoint on our reading of entry 11. Mitigation: acceptance waits on TASK-1316 AC3.
- **One PWM re-enable path is missing.** thermo-nova also re-enables PWM in `on_cycle_end()` after a continuous cycle classified OVERSHOOT (`pwm.py:219-223`); the firmware only has the sustained-overshoot auto-switch. Mitigation: TASK-1331 tracks the port.
- **CH=0 may not reach the boiler in OT-Direct gateway mode.** Accepted ADR-179 (Open Questions) records that on OT-Direct boards, which have no PIC (gateway microcontroller), in gateway mode the gateway emits its own MsgID 0 status frames alongside the thermostat's, each with a different CH-enable bit, so SAT's `CH=0` is not reliably honoured. ADR-179 moved that MsgID 0 question out of its own scope (it decides MsgID 1 only) to TASK-1238. This ADR decides when SAT wants CH=0, not how the bit reaches the boiler; until TASK-1238 is decided, the cutoff's CH=0 on those boards is an intent, not a verified boiler state. Mitigation: open question below. This finding is recorded for OT-Direct gateway mode only; PIC boards are outside it.
- **The `<=` boundary is unconfirmed** by Sergeant D. Mitigation: open question below.
- **CH cadence differs from thermo-nova** (on change plus a refresh every 300 s, `SAT_CMD_REFRESH_MS`, instead of every control step, whose interval `iControlInterval` defaults to 30 s), kept without his explicit OK. Mitigation: the 5-minute refresh resyncs a rebooted PIC (the gateway's microcontroller) or a drifted boiler; TASK-1324 covers the boot-time resend gap.
- **Fixed constants.** An installation with atypical radiators or underfloor cannot retune them without a firmware change (carried over from ADR-150, Alternative C).
- **No CI gate.** The tests can drift unnoticed until wired into CI.

## Open Questions

- [x] Sergeant D confirms the cutoff boundary: at or below (`<=`, thermo-nova) or strictly below (`<`, his wording)? — **Answered 2026-10-24 by User: Robert van den Breemen:** Settled: at or below (<=), as thermo-nova. Sergeant D answered 'keep the thermo-nova solution' on the cutoff (docs/sat/sergeant-d-corrections.md entry 12, 2026-10-23); the firmware uses <= since TASK-1323.
- [x] Sergeant D confirms CH=1 during the PWM ON phase whatever CS keeps the flame on (entry 9 point 1), on his own setup (TASK-1316 AC3). — **Answered 2026-10-24 by User: Robert van den Breemen:** Deferred: the rule is decided (entry 9); the remaining item is Sergeant D's confirmation on his own installation, tracked as TASK-1316 AC3. It does not change this decision.
- [x] TASK-1327 lands, including the reset of the PWM disable guard timers on the cutoff. Done in commit `c3aa767a0` (`satPwmDisableOnHeaterOff()`, `satCycleResetSustainGuards()`).
- [x] TASK-1238 decides how SAT's CH=0 reaches the boiler in OT-Direct gateway mode (MsgID 0, see ADR-179 Open Questions); until then the cutoff's CH=0 is not reliably honoured there. — **Answered 2026-10-24 by User: Robert van den Breemen:** Deferred to TASK-1238: this ADR decides when SAT wants CH=0, not how the bit reaches the boiler in OT-Direct gateway mode (ADR-179 Open Questions).
- [x] Whether a user-forced PWM mode (`bForcePWM`) stays PWM through a cold cutoff: resolved by Sergeant D entry 13 (2026-10-23), who removed the Force PWM option completely (TASK-1332). PWM is switched off on the cutoff unconditionally, as thermo-nova does (`heating_control.py:230`).

## Related Decisions

- **ADR-150 (SAT per-heating-system cold setpoint)**: this ADR is its proposed successor; `adr supersede ADR-150 --by ADR-195` runs on acceptance, not before. Its per-system values, its fixed-constant choice and its SAT-disabled scope carry over; its strict `<` cutoff, its CH derivation and its three-effect list are replaced.
- **ADR-085 (SAT integration)**: the umbrella under which SAT is an opt-in master controller; unchanged.
- **ADR-179 (MsgID 1 ownership)**: SAT's CS=10 on the cutoff still goes through `satEnqueueOwnCS`; unchanged. Its Open Questions record that SAT's CH=0 is not reliably honoured in OT-Direct gateway mode (MsgID 0, TASK-1238); this ADR inherits that limitation and does not resolve it.
- **ADR-110 (SAT PV (photovoltaic) surplus setpoint boost)**: composes with the cutoff as ADR-150 states; a boost that lifts the requested setpoint above the cold setpoint re-enables heating; unchanged.
- **ADR-080 (Binding ADR rules must have a CI gate)**: this ADR is guideline-level and claims no gate.

## References

- `docs/sat/sergeant-d-corrections.md` entries 5, 7, 9, 11 and 12 (2026-10-23), with the Discord links quoted in Context.
- thermo-nova at `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb`, `custom_components/sat`: `const.py:34-35`; `heating_control.py:69-72`, `:148-152`, `:229-238`, `:280-282`, `:317-319`, `:385`, `:388-389`, `:398`, `:441`, `:525-528`; `pwm.py:106-109`, `:219-223`; `coordinator/serial/__init__.py:212-214`.
- Firmware at HEAD `c3aa767a0`: `src/OTGW-firmware/SATchEnable.h:26-29` (`satColdCutoff`), `:46-52` (`satChEnableDecide`); `src/OTGW-firmware/SATcontrol.ino:57-58`, `:266-270` (`satGetColdSetpoint`), `:353`, `:385-393`, `:432-441`, `:704-710`, `:906-936`, `:951`, `:966-975` (`satPwmReset`), `:997-1006` (`satPwmDisableOnHeaterOff`), `:2250`, `:4850-4858`, `:4978-4993`, `:5034`, `:5052-5069`; `src/OTGW-firmware/SATcycles.ino:147-152` (`satCycleResetSustainGuards`), `:197-209` (`satCycleInit`), `:951` (`satCycleCheckAutoSwitch`).
- Tasks: TASK-891.2 (original cutoff), TASK-1316 (CH rule, commit `812cca580`, 2.0.0-alpha.471), TASK-1323 (solar offset, `<=`, `satChEnableDecide`, commit `9ec1b87e2`, 2.0.0-alpha.474), TASK-1324 (CH/MM resend gap), TASK-1327 (PWM off and reset on the cutoff, commit `c3aa767a0`, Done), TASK-1331 (`on_cycle_end` PWM re-enable, open), TASK-1238 (CH enable via MsgID 0 in OT-Direct gateway mode, open).
- Tests: `tests/test_sat_ch_enable.cpp`, `tests/test_sat_pwm_override.py`.

The decision has no declarative code rule that a regex could check, and the host tests are not in CI, so it has no Enforcement block: it is guideline-level per ADR-080.
