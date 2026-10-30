---
id: "ADR-197"
title: "SAT PID v3 controller (proposed successor to ADR-069)"
status: "Accepted"
date: "2026-10-29"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes:
  - "ADR-069"
superseded_by: null
related:
  - "ADR-204"
topics:
  - "sat"
  - "pid"
  - "deadband"
  - "integral"
  - "derivative"
  - "thermo-nova"
aliases:
  - "SAT PID"
  - "PID v3"
  - "pid.py"
  - "PID deadband"
  - "integral clamp"
  - "PID cadence"
components:
  - "SAT PID controller"
symbols:
  - "satPidService"
  - "satPidStep"
  - "satPidOutput"
  - "satPidCurveOnly"
  - "satPidGatherInputs"
  - "satPidNoteRoom"
  - "satPidRestoreState"
  - "satPidGetPersistState"
  - "satPidReset"
  - "satResetIntegral"
  - "satPidError"
  - "_pidUpdateIntegral"
  - "_pidUpdateDerivative"
  - "_pidCalculateGains"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-197 SAT (Smart Autotune Thermostat) PID (proportional-integral-derivative) v3 controller (proposed successor to ADR-069)

## Status

Accepted, 2026-10-29.

This ADR is the proposed successor to ADR-069 (SAT PID v3 Controller Implementation, Accepted). ADR-069 stays Accepted and its file is untouched while this ADR is Proposed. The `supersedes` list in the front matter is therefore empty: adr-kit writes both sides of a supersession at once (`adr supersede ADR-069 --by ADR-197`), and running it now would retire ADR-069 before this decision is accepted; `adr lint --strict` also rejects a one-sided link. That command runs on acceptance, which needs the maintainer's explicit OK (TASK-1355).

Guideline-level per ADR-080: no CI (continuous integration) gate checks the PID against `pid.py`.

## Status History

```yaml
status_history:
  - date: 2026-10-24
    status: Proposed
    changed_by: "Agent: Claude"
    reason: Initial proposal; proposed successor to ADR-069, which states wrong values (SAT port audit 2026-10-24, finding 69)
    changed_via: adr-kit
  - date: 2026-10-29
    status: Accepted
    changed_by: Robert van den Breemen
    reason: Maintainer OK in session 2026-10-29 (TASK-1355); open questions answered by Sergeant D
    changed_via: adr-kit lifecycle
  - date: 2026-10-29
    status: Accepted
    changed_by: Robert van den Breemen
    reason: Maintainer OK in session 2026-10-29 (TASK-1355)
    changed_via: adr-kit lifecycle
  - date: 2026-10-30
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-204
    changed_via: adr-kit lifecycle
```

## Context

SAT (Smart Autotune Thermostat) is the firmware's port of the thermo-nova Home Assistant component. Its PID (proportional-integral-derivative) controller trims the weather-compensated heating curve with the room-temperature error. Ground truth for SAT behaviour is, in order (ADR-196): Sergeant D's recorded decisions in `docs/sat/sergeant-d-corrections.md`, then thermo-nova at commit `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb` (local copy `other-projects/SAT-releases-thermo-nova/`, verified identical to that commit on 2026-10-22; paths below are relative to `custom_components/sat`), then the specs, then the firmware. Firmware citations are at the `dev` branch head (HEAD) `2d01d3297` (`src/OTGW-firmware/`); `git diff 2d01d3297 47a0d7e59 -- src` is empty, so the line numbers also hold at `47a0d7e59`.

Sergeant D's entry 28 (2026-04-05 to 2026-10-03) settles the PID: outside the deadband it is P + heating curve + derivative with the integral at 0; inside the deadband it is P + heating curve + integral with the derivative frozen at its last value; the integral compensates for other heat sources (sun, cooking, activity); the PID must be identical to `pid.py`; sensor noise is handled by a low-pass filter on the raw room temperature before the PID (TASK-894), not by changing the PID. On 2026-10-03 he said of the filter chain: "No, don't change anything. Everything looks good now".

ADR-069 (dated 2026-04-09) describes this controller, but the SAT port audit of 2026-10-24 (`docs/sat/sat-port-audit-2026-10-24.md`, finding 69) found that several of its statements do not match the code or thermo-nova. Each was checked again for this ADR:

1. **Deadband.** ADR-069: "default 0.25°C". Code: the setting default `fDeadband = 0.1f` (`SATtypes.h:466`), which the PID reads (`SATpid.ino:151`, `:192`); the unused constant `SAT_PID_DEADBAND_DEFAULT` (`SATpid.ino:20`) is 0.1 as well. thermo-nova: `DEADBAND = 0.1` (`const.py:16`).
2. **Integral clamp.** ADR-069: "clamped to `[0, curveValue]`". Code: symmetric, `[-curveValue, +curveValue]` (`SATpid.ino:175-176`), then a symmetric absolute cap of ±20 (`SATpid.ino:179-181`). thermo-nova: `clamp_to_range(self._integral, self._heating_curve.value)` (`pid.py:192`), which is `clamp(value, -range_limit, range_limit)` (`helpers.py:148-150`).
3. **Interval independence.** ADR-069: the fixed 60 s step "provides consistent integral behavior independent of the configured control interval". False (audit finding 14). The PID runs from `satControlLoopRun()` (`SATcontrol.ino:5070`) on the control timer `timerSATControl` (`SATcontrol.ino:582`, checked at `:4844`), whose interval `iControlInterval` defaults to 30 s (`SATtypes.h:467`) and is settable 10-300 s (`settingStuff.ino:1004`). Each run that passes the gates adds `fKi * error * 60` (`SATpid.ino:169`). The gates are: at least 10 s since the last update (`SATpid.ino:21`, `:279-283`), and an error change of at least 0.001 unless 60 s have passed (`SATpid.ino:286-288`). The room EMA (exponential moving average) moves the error on almost every tick (audit finding 14). thermo-nova runs `control_pid` on a fixed 60 s timer (`climate.py:712-716`, `PID_UPDATE_INTERVAL = 60` at `pid.py:20`), plus once at start-up or when Home Assistant has started (`climate.py:137`, `:143`). The audit estimates that inside the deadband the integral grows about 2x faster at a 30 s interval and up to 6x faster at 10 s (estimate, not measured). **Resolved by TASK-1368 (entries 42, 28, 111):** the PID now runs on its own 60 s timer `timerSATPid` (`SATpid.ino`, `satPidService()`), independent of the control tick, plus one start-up step; the sample gates are gone. The text above describes the code before that change.
4. **Reset on coefficient change.** ADR-069: `satPidReset()` is "called when SAT is disabled, safety trips, or the heating curve coefficient changes significantly". Code: `satPidReset()` has exactly three callers: `satDisable` (`SATcontrol.ino:2381`), `satFlushShortLivedData` (`:2417`) and `initSAT` (`:3905`). The "disabled" and "safety trips" parts of ADR-069's statement hold through `satDisable`, which resets unconditionally and is reached from the two safety trips (`bSafetyTripped = true` at `SATcontrol.ino:4905-4910`, too many invalid room readings, and `:5239-5244`, PIC (the gateway's PIC microcontroller) unavailable), the MQTT-fallback lift (`:4745`), the disabled-or-flashing bail (`:4800`) and the user switch-off (`settingStuff.ino:989`). Only the coefficient-change part is wrong. The coefficient is written by the settings path (`settingStuff.ino:1001`) and by the firmware auto-tune (`SATcontrol.ino:4699`); neither calls a reset. A coefficient change takes effect at the next update because the gains are recomputed on every update (`SATpid.ino:130-141`, called at `:291`).

ADR-069 has further statements that no longer hold:

5. **Control interval.** ADR-069: "Control cycles run every 30-600 seconds". Code: default 30 s, range 10-300 s (`SATtypes.h:467`, `settingStuff.ino:1004`).
6. **Platform.** ADR-069 motivates the design with "ESP8266 has ~40KB usable RAM (random-access memory)". The `dev` line is ESP32-S3 only (CLAUDE.md, Branches). Static state is still used; the reason is no longer ESP8266.
7. **Gains.** ADR-069: gains are "not stored as tunable parameters". Code: a manual mode exists, `bAutoGains` (default true) with `fKpManual`, `fKiManual`, `fKdManual` (`SATtypes.h:540-543`), passed through at `SATpid.ino:122-128`. thermo-nova has the same switch (`pid.py:64-65`, `:76-77`, `:87-88`).
8. **Solar freeze.** ADR-069: integral accumulation is suspended when `bSolarGainActive` is set. Code: the whole integral update is skipped, reset included, and only when the freeze option `bSolarFreezeIntegral` (default true, `SATtypes.h:549`) is on as well (`SATpid.ino:157`). This follows thermo-nova (`climate.py:499-502`, `pid.py:167-168`) and Sergeant D's entry 11, answer 3.
9. **Sample gate.** ADR-069: updates are skipped "where the error has not changed by more than 0.001°C". Code: that skip applies only while less than 60 s have passed (`SATpid.ino:286`).

Accepted ADRs are not edited (CLAUDE.md, ADR Guidelines; maintainer directive 2026-10-24: ADRs with wrong values are superseded by new ADRs). This ADR therefore restates the decision with values cited to the current code and thermo-nova.

## Decision

The SAT PID follows thermo-nova `pid.py` at `7beaaf5` term by term (entry 28), implemented in `src/OTGW-firmware/SATpid.ino`; where the firmware still differs from `pid.py`, the decided rule is the one in the entries cited below (see "Resolved questions" under Open Questions) and the implementing task is named as pending.

The decided behaviour, each point with the firmware and the thermo-nova location:

**1. Output.** `output = heatingCurveValue + P + I + D` (`SATpid.ino:312`; `pid.py:132`), with `P = Kp * error` (`SATpid.ino:300`; `pid.py:101`), `I = integral` (`SATpid.ino:301`) and `D = Kd * rawDerivative` (`SATpid.ino:302`; `pid.py:114`). The error is `target - room` rounded to 3 decimals (`satPidError`, `SATpid.ino:63-66`), as thermo-nova rounds `TemperatureState.error` (`round(self.setpoint - self.current, 3)`, `temperature/state.py:16-18`; TASK-1195). The PID output is the requested setpoint before the multi-zone block, the solar offset and the clamps (ADR-195). **Rounding (decided, pending in TASK-1423):** every PID value is rounded to 3 decimals, the output included (entry 95, Q28a: "Round all the values to 3 decimals not only the Output"; entry 78). This is his deliberate deviation from `pid.py`, which rounds the gains to 6 decimals (`pid.py:71`, `:82`, `:93`), P, I, D and the raw derivative to 3 (`pid.py:101`, `:106`, `:114`, `:119`) and the output to 1 (`pid.py:132`). The firmware does not round the gains, terms or output today (`SATpid.ino`); only the error is rounded (`satPidError`).

**2. Automatic gains** (when `bAutoGains` is true, the default): `Kp = coefficient * curveValue / divisor` with divisor 4 for underfloor and 3 otherwise; `Ki = Kp / 8400`; `Kd = 0.07 * 8400 * Kp` (`SATpid.ino:130-141`, constants at `:25`, `:28-29`; `pid.py:70-71`, `:82`, `:93`). The gains are recomputed on every update, so a change of the coefficient or the curve value retunes them at the next update without a reset. With `bAutoGains` false the manual gains pass through (`SATpid.ino:122-128`; `pid.py:64-65`, `:76-77`, `:87-88`).

**3. Integral, deadband 0.1.** The deadband is a fixed 0.1 °C, as `DEADBAND = 0.1` (`const.py:16`): no user setting and no widening during room-temperature estimation (entry 76, Q26: "fixed 0.1"). **Pending in TASK-1417:** today the PID still reads the setting `settings.sat.fDeadband` (`SATpid.ino:151`, `:192`, default 0.1 at `SATtypes.h:466`, range 0.05-2.0), and the estimation block widens it by 0.5 °C per hour (`SATcontrol.ino:4977-4981`); the constant `SAT_PID_DEADBAND_DEFAULT = 0.1f` (`SATpid.ino:20`) is the intended single source. The firmware-only absolute integral cap of ±20 (`SATpid.ino:179-181`) and the 10 s minimum between updates are not part of the rule: `pid.py:191-192` clamps only to ± the curve value and `update()` has no minimum interval (entry 75, Q25: "follow thermo-nova"); removal is pending in TASK-1416. The Ki-near-0 early return (`SATpid.ino:166`) also goes: thermo-nova always adds and clamps (`pid.py:191-192`; entry 95, Q28b: "Yes, match thermo-nova"; pending in TASK-1423). Outside it (`|error| > deadband`) the integral is set to 0 (`SATpid.ino:160-163`; `pid.py:187-189`). Inside it, the integral adds `Ki * error * 60`, a fixed 60 s step (`SATpid.ino:169`; `pid.py:191`), and is then clamped symmetrically to `[-curveValue, +curveValue]` (`SATpid.ino:175-176`; `pid.py:192`, `helpers.py:148-150`). A negative integral is allowed so the PID can lower the flow below the curve when the room is above target. **Cadence (TASK-1368, entries 42, 28, 111):** the step runs on its own 60 s timer `timerSATPid` (`DECLARE_TIMER_SEC(timerSATPid, 60, SKIP_MISSED_TICKS)`, typed explicitly and never re-set by an untyped `CHANGE_INTERVAL_SEC`, entry 109), as thermo-nova's `control_pid` (`climate.py:712-716`, `pid.py:20`), plus one start-up step after `satPidReset()` (`climate.py:137`, `:143`). `satPidService()` runs on every loop pass while SAT is enabled, not tripped, not flashing and with a live OpenTherm link, before the control tick's early returns (hot water, boiler fault, thermal-safe fallback, summer simmer, valves closed): it stops only when SAT is off (an OpenTherm loss turns SAT off). `pid.update()` has no time gate and no unchanged-error gate (`pid.py:159-171`); the firmware's 10 s gate and unchanged-error skip are removed. The output is computed on every control tick from the current curve, `curve + kp(curve) * held error + I + kd(curve) * held raw derivative` (`pid.py:60-132`, read every loop at `climate.py:375`); only the error, the integral, the raw derivative and the last temperature are held between steps. Without a room temperature there is no integration: the output is the curve with P, I and D reset (entries 38, 46). Exception at boot (entry 115): while no valid room temperature has been seen since the boot or the last reset, the integral and raw derivative restored from flash are kept, only P and D are 0, and the PID state is not saved over the restored one; the first valid reading takes a PID step at once (restarting the 60 s timer), so the output uses the restored I and the new error from that pass. The saved state is the controller's held state (`satPidGetPersistState()`), never the published copies. The zone PIDs take their integral step on the same 60 s timer (`area.py:183-190`, entry 111) and their output on every tick.

**4. Solar freeze.** While solar gain is active and `bSolarFreezeIntegral` is on, the whole integral update is skipped, the outside-deadband reset included (`SATpid.ino:157`; `climate.py:499-502`, `pid.py:167-168`; entry 11, answer 3).

**5. Derivative from temperature, frozen inside the deadband.** Inside the deadband the derivative keeps its last value and only its timestamp moves (`pid.py:205-207`). The previous temperature `_last_temperature` is overwritten on every PID update (`pid.py:171`), also when the derivative step is skipped, and "no change" means exactly `temperature_delta == 0.0` (`pid.py:209-213`) (entry 77, Q27: "match thermo-nova"). **Pending in TASK-1418:** the firmware still keeps a separate reference that moves only with the derivative timestamp (`SATpid.ino:49-56`, `:202-207`) and tests `fabsf(tempDelta) < 0.001f` (`SATpid.ino:212`). Outside it, the raw slope is `-(roomTemp - reference) / deltaTime` (`SATpid.ino:211`, `:226`; `pid.py:209`, `:220`): a rising room temperature gives a negative D, which damps the output and avoids a kick on a target change. A change within 60 s of the derivative timer is skipped (`SATpid.ino:222`; `pid.py:217-218`). A raw slope of magnitude 5 or more is capped and stored without filtering (`SATpid.ino:229-236`; `pid.py:222-225`); otherwise it passes a low-pass filter with `alpha = deltaTime / (60 + deltaTime)` and a final cap of ±5 (`SATpid.ino:240-247`; `pid.py:228-230`). The derivative timer runs on when the room temperature changed, not on when the PID ran, as thermo-nova times it by the sensor's `last_changed` (`SATpid.ino:194-197`, `:259-264`; `pid.py:206`, `:215`; TASK-1195).

**6. Room filter before the PID.** The room temperature is low-pass filtered before it reaches the PID (TASK-894): a time-based EMA with `alpha = dt / (45 + dt)`, time constant `SAT_ROOM_FILTER_TAU_S = 45` s, seeded from the first valid reading (`SATcontrol.ino:4916-4943`). The code comment in the filter block says MQTT (Message Queuing Telemetry Transport) `sat/room_temp` still publishes the raw reading (`:4923`); the publish site itself was not opened for this ADR. This is not in thermo-nova, which reads the Home Assistant entity; Sergeant D asked for it and confirmed the filter chain on 2026-10-03 (entry 28).

**7. State.** PID state lives in static file-scope variables in `SATpid.ino` (`:32-56`), with telemetry copies: `state.sat.fPidP/I/D`, `fError`, `fRawDerivative` and `fPidOutput` at `SATpid.ino:305-313`, and `state.sat.fKp/Ki/Kd` at `SATpid.ino:124-126` (manual) and `:139-141` (auto). **Persistence (decided, pending in TASK-1415):** as thermo-nova, five values are saved and restored: integral, last error, raw derivative, last temperature and the derivative timestamp (`pid.py:150-155`, `:243-249`), with no age check and no NTP (Network Time Protocol) gate on the restore (entry 74, Q24: "follow thermo-nova"). The write cadence is the maintainer's checkpoint rule, recorded in the TASK-1415 notes: at most once every 15 minutes and only when a stored value changed; a planned restart and SAT disable flush at once. After a boot without a room reading yet, the restored integral and raw derivative are kept and not saved over until the first valid reading (entry 115, Q56 A; `climate.py:480-482`; implemented in TASK-1368). Today the firmware restores only the integral and the raw derivative (`satPidRestoreState`, `SATpid.ino:75-83`, called at `SATcontrol.ino:2239`), and only when the saved state is at most 30 minutes old (`SAT_PID_STALE_SEC = 1800`, `SATcontrol.ino:2190`, checked at `:2230`).

**8. Resets (decided, pending in TASK-1414).** As thermo-nova `reset_control_state`, a reset zeros the integral and the raw derivative, and nothing else (`pid.py:134-137`), on every target change, preset change and clear, window open and close (through the target path), HVAC (heating, ventilation and air conditioning) mode change (HEAT or `OFF`) and the reset_integral service, for the main PID and the zone PIDs (`climate.py:641-660`, `:461-464`, `:570-583`, `:808-832`, `:217-220`; entry 73, Q23: "follow thermo-nova"). A stale room sensor is a separate path (entries 38, 46, 115). There is no reset on a coefficient change. **What the code does today:** `satPidReset()` (`SATpid.ino:94-114`) zeros the controller state on SAT disable, on a flush and at init (`SATcontrol.ino:2381`, `:2417`, `:3905`). `satResetIntegral()` (`SATpid.ino:86-91`) clears only the integral, on window open and close (`SATcontrol.ino:1316`, `:1336`), the MQTT command (`MQTTstuff.ino:620`) and REST (`restAPI.ino:1349`, `:1600`). A preset change and a preset clear set only the published copy `state.sat.fPidI = 0.0f` under a "Reset PID integral" comment (`SATcontrol.ino:1169`, `:1177`) and leave `_pid_integral` untouched, so the controller's integral is not reset there (audit finding 15), and a target set directly calls no reset (`satHandleTargetTemp`, `SATcontrol.ino:2100-2131`). TASK-1414 closes these gaps and must not reintroduce the first-tick 0 output of TASK-1270.

## Decision Contract

### Must

- Keep the deadband a fixed 0.1 °C, matching `const.py:16`, with no user setting and no widening (entry 76; code change pending in TASK-1417).
- Clamp the integral symmetrically to ± the heating curve value, as `pid.py:192`.
- Set the integral to 0 outside the deadband, and freeze the derivative inside it.
- Add `Ki * error * 60` per integral update, as `pid.py:191`.
- Derive the gains from the coefficient and the curve value on every update when `bAutoGains` is on.
- Skip the whole integral update during solar gain when the freeze option is on.
- Cite `pid.py` and the entry number for any change to the PID; follow ADR-196 for conflicts.

### Must Not

- Clamp the integral to `[0, curve]`.
- Accumulate the integral outside the deadband (reverted TASK-8, entry 28).
- Change the PID to fight sensor noise; filter the sensor instead (entry 28).
- Claim that the PID is independent of the control interval while it runs on the control tick.

### Exceptions

- Until the implementing tasks are Done, the code keeps these differences from the decided rule: no 3-decimal rounding and the Ki-near-0 return (TASK-1423), the ±20 integral cap and 10 s minimum (TASK-1416), the deadband setting and widening (TASK-1417), the derivative reference and no-change test (TASK-1418), the narrower persistence (TASK-1415) and the narrower resets (TASK-1414). Each is decided by Sergeant D's entry named in the Decision and listed under "Resolved questions"; none is an open question.

### Verification

- `python tests/test_sat_pwm_override.py` compiles the real `_pidUpdateIntegral()` from `SATpid.ino` and checks the solar freeze path (TASK-1323). `python tests/test_sat_pid_cadence.py` (TASK-1368) compiles the whole `SATpid.ino` with the real `safeTimers.h` and the real zone PID, and checks the 60 s cadence: step count and integral independent of the control tick, a 200 s stall giving one step, the millis wrap, the output following the current curve between steps, curve-only output without a room temperature, and the order of `satPidService()` against the tick's early returns (structural). No host test exercises the gains, clamp and derivative details beyond that.
- The test is not run in CI. This ADR claims no gate.
- Every value in this ADR was read from the code at `2d01d3297` and from thermo-nova `pid.py`, `const.py`, `climate.py`, `helpers.py` and `temperature/state.py` in this session. A second reader checked them on 2026-10-24 and refuted no core value; the omissions, mislabels and imprecise citations that reader found are corrected in this text (TASK-1355 acceptance criterion 1).

## Alternatives Considered

- **Amend ADR-069 in place.** Rejected: ADR-069 is Accepted and immutable, and the maintainer directed on 2026-10-24 that ADRs with wrong values are superseded by new ADRs.
- **Restate ADR-069 with only the four values from audit finding 69 corrected.** Rejected: the re-read found five more statements that do not hold (interval range, platform, manual gains, solar freeze condition, sample gate). A successor that keeps them would be wrong again.
- **Record the current firmware behaviour as the decision, extras included** (control-tick cadence, ±20 cap, 10 s gate, settable deadband, deadband widening). Rejected: entry 28 requires the PID to be identical to `pid.py`, and ADR-196 forbids treating the firmware as ground truth. Sergeant D was asked about the ±20 cap and the 10 s gate on 2026-06-21 and did not answer, so they stay open.
- **Do nothing (leave ADR-069 as the record).** Rejected: a maintainer reading ADR-069 would tune against a 0.25 deadband, expect a non-negative integral, assume interval independence, and expect a reset on a coefficient change. None of these is true.

## Consequences

**Benefits**

- The PID record matches the code at `2d01d3297` and thermo-nova, with a citation per value.
- The departures from `pid.py` are listed in one place, each tied to an audit finding or an entry, so they can be put to Sergeant D one at a time (`docs/sat/open-questions.md`).

**Trade-offs**

- The record states a decision ("identical to `pid.py`") that the code does not fully meet yet. Mitigation: each gap is decided by a Sergeant D entry ("Resolved questions") and has an implementing task (TASK-1414 to TASK-1418, TASK-1423; the cadence gap, Q10, is closed by TASK-1368, Done).
- No CI gate. The values can drift from the code unnoticed. Mitigation: guideline-level per ADR-080; the next audit re-checks them.

**Risks and mitigations**

- *Risk*: the integral grows faster than in thermo-nova while the PID runs on the 30 s control tick (audit estimate: about 2x at 30 s, up to 6x at 10 s), with more risk of oscillation around the target. *Mitigation*: Q10, answered by entry 42 and implemented in TASK-1368 (60 s timer, independent of the tick).
- *Risk*: after a target or preset change the frozen D term carries over into the new target (audit finding 15 estimates about -2 °C at Kp 17.35). *Mitigation*: entry 73 decides that the PID resets on every target change as thermo-nova (`climate.py:641-660`, `reset_control_state` at `:461-464`); pending in TASK-1414.
- *Risk*: the user can set the deadband from 0.05 to 2.0, and the estimation fallback widens it by 0.5 °C per hour, both unlike thermo-nova's constant. *Mitigation*: entry 76 decides a fixed 0.1 with no setting and no widening; pending in TASK-1417.

## Open Questions

None open. Every question that was listed here has been answered by Sergeant D and recorded in `docs/sat/sergeant-d-corrections.md`; see "Resolved questions". The firmware differences from `pid.py` that some of these answers remove are implemented by the tasks named below. Until a task is Done, the code still shows the old difference; the Decision text states the decided rule and names that task as pending.

## Resolved Questions

- **PID cadence** (audit finding 14, Q10): fixed 60 s as thermo-nova (`climate.py:712-716`). Entry 42: "follow thermo-nova, a fixed 60 s PID update"; entry 111 (Q55) added the zone PIDs and stepping while the control tick returns early; entry 115 (Q56 A) keeps the restored PID state until the first room reading after boot. Implemented in TASK-1368 (Done).
- **Absolute integral cap ±20 and 10 s minimum between updates** (Q25): remove both. Entry 75: "Q25: follow thermo-nova". To be implemented in TASK-1416 (To Do).
- **PID resets** (audit finding 15, Q23): reset the integral and the raw derivative on every target, preset, window and HVAC (heating, ventilation and air conditioning) mode change and on the reset_integral service, as thermo-nova `reset_control_state`. Entry 73: "Q23: follow thermo-nova". To be implemented in TASK-1414 (To Do).
- **Deadband as a setting and its widening** (audit finding 38, Q26): a fixed 0.1, no user setting, no widening. Entry 76: "Q26: fixed 0.1". To be implemented in TASK-1417 (To Do).
- **Integral skipped when Ki is about 0** (Q28b): match thermo-nova, which always adds and clamps (`pid.py:191-192`). Entry 95: "Q28b: Yes, match thermo-nova" (follow-up of entry 78). To be implemented in TASK-1423 (To Do).
- **Derivative "no change" test** (audit finding 38, Q27): exactly `temperature_delta == 0.0` as `pid.py:211`. Entry 77: "Q27: match thermo-nova". To be implemented in TASK-1418 (To Do).
- **PID persistence** (Q24): save and restore integral, last error, raw derivative, last temperature and the derivative timestamp, with no age check. Entry 74: "Q24: follow thermo-nova". To be implemented in TASK-1415 (To Do). Write cadence (maintainer decision, recorded in the TASK-1415 notes, answers the wear point on entry 74): at most once every 15 minutes and only when a stored value changed.
- **Derivative reference** (audit table row 40, Q27): the previous temperature is overwritten on every PID update, as `pid.py:171`; the separate firmware reference goes. Entry 77: "Q27: match thermo-nova". To be implemented in TASK-1418 (To Do).
- **Output rounding** (Q28a): every PID value is rounded to 3 decimals, the output included. Entry 95: "Q28a: Round all the values to 3 decimals not only the Output" (follow-up of entry 78). This is his deliberate deviation from thermo-nova for the gains (6 decimals there) and the output (1 decimal there). To be implemented in TASK-1423 (To Do).

## Related Decisions

- **ADR-069 (SAT PID v3 Controller Implementation)**: superseded by this ADR on acceptance. Its controller structure, auto-gain formula, fixed 60 s integral step, deadband rules, temperature-based derivative, adaptive alpha and ±5 cap carry over; its 0.25 deadband, `[0, curve]` clamp, interval-independence claim, coefficient-change reset, 30-600 s interval, ESP8266 motivation and "gains are not tunable" statement are replaced.
- **ADR-196 (SAT behaviour ground truth)**: the source precedence this ADR follows.
- **ADR-195 (SAT cold state and CH-enable rule)**: consumes the PID output as the requested setpoint; unchanged.
- **ADR-071 (SAT heating curve algorithm)**: supplies `curveValue` and the coefficient; also under supersession in TASK-1355 for wrong values.
- **ADR-085 (SAT integration)**: the umbrella SAT architecture; unchanged.
- **ADR-080 (Binding ADR rules must have a CI gate)**: this ADR is guideline-level and claims no gate.

## References

- `docs/sat/sergeant-d-corrections.md` entries 11 (answer 3) and 28 (sources include 2026-04-05 19:42 UTC: https://discord.com/channels/812969634638725140/1105556725714649128/1490436555050057789; and the 2026-10-03 08:42 UTC reply: https://discord.com/channels/812969634638725140/1514720723980259460/1555862708765593692).
- thermo-nova pinned source: https://github.com/Alexwijn/SAT/tree/7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb/custom_components/sat
- `docs/sat/sat-port-audit-2026-10-24.md` findings 14, 15, 38 and 69, table row 40, correction F5 (`:1101-1103`); `docs/sat/open-questions.md` Q10.
- thermo-nova at `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb`, `custom_components/sat`: `const.py:16`; `pid.py:20`, `:64-114`, `:132-137`, `:141-157`, `:159-192`, `:199-232`, `:239-251`; `helpers.py:148-150`; `temperature/state.py:16-18`; `climate.py:137`, `:143`, `:217-220`, `:461-464`, `:470-478`, `:499-502`, `:570-583`, `:641-660`, `:712-716`, `:808-832`.
- Firmware at HEAD `2d01d3297`, `src/OTGW-firmware/`: `SATpid.ino:20-56`, `:63-66`, `:75-114`, `:120-182`, `:190-250`, `:254-323`; `SATtypes.h:466-467`, `:540-543`, `:549`; `settingStuff.ino:1000-1004`; `SATcontrol.ino:582`, `:1169`, `:1177`, `:1316`, `:1336`, `:2100-2131`, `:2190`, `:2230`, `:2239`, `:2381`, `:2417`, `:3905`, `:4699`, `:4745`, `:4800`, `:4844`, `:4905-4910`, `:4916-4943`, `:4957`, `:4977-4981`, `:5005`, `:5020`, `:5070`, `:5132-5135`, `:5239-5244`; `settingStuff.ino:989`; `MQTTstuff.ino:620`; `restAPI.ino:1349`, `:1600`.
- Tasks: TASK-1355 (this supersession), TASK-894 (room EMA), TASK-1195 (derivative timer, error rounding), TASK-1323 (solar freeze), TASK-8 (integral outside the deadband, reverted).
- Tests: `tests/test_sat_pwm_override.py` (real `_pidUpdateIntegral()`, freeze path only; not in CI).

The decision has no declarative rule a regex could check, and its only host test is not in CI, so it has no Enforcement block: it is guideline-level per ADR-080.
