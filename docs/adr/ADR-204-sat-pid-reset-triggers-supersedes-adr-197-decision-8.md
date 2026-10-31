---
id: "ADR-204"
title: "SAT PID reset triggers (supersedes ADR-197 decision 8)"
status: "Accepted"
date: "2026-10-30"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-197"
  - "ADR-205"
topics:
  - "sat"
  - "pid"
  - "integral"
  - "reset"
  - "threading"
aliases:
  - "PID reset triggers"
  - "reset_control_state"
  - "integral reset"
  - "Q23"
  - "target change reset"
components:
  - "SAT PID controller"
symbols:
  - "satPidResetTerms"
  - "satResetControlState"
  - "satApplyControlStateReset"
  - "satResetControlStateIfTargetChanged"
  - "satHandleZoneSetpoint"
  - "satControlLoopRun"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-204 SAT (Smart Autotune Thermostat) proportional-integral-derivative (PID) reset triggers (supersedes ADR-197 decision 8)

## Status

Accepted, 2026-10-30. Guideline-level per ADR-080: no CI (continuous integration) gate.

This ADR replaces **decision 8 ("Resets") of ADR-197 only**. ADR-197 decisions 1 to 7 and its scope stay in force. adr-kit has no partial-supersession form: `adr supersede ADR-197 --by ADR-204` would mark all of ADR-197 Superseded, so the front-matter `supersedes` list stays empty and ADR-197 is not touched by this proposal. **When this ADR is accepted, ADR-197 needs a pointer to it**: run `adr relate ADR-204 --to ADR-197` (reference bookkeeping, allowed on an Accepted record) and do not run `adr supersede`.

## Status History

```yaml
status_history:
  - date: 2026-10-30
    status: Proposed
    changed_by: "Agent: Claude"
    reason: Initial proposal; records the reset triggers implemented in TASK-1414 and the loop-task request flag, which ADR-197 decision 8 left pending
    changed_via: adr-kit
  - date: 2026-10-30
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-197
    changed_via: adr-kit lifecycle
  - date: 2026-10-30
    status: Accepted
    changed_by: Robert van den Breemen
    reason: Maintainer OK in session 2026-10-30 (TASK-1414); Sergeant D entry 73 Q23 'follow thermo-nova'
    changed_via: adr-kit lifecycle
  - date: 2026-10-31
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-205
    changed_via: adr-kit lifecycle
```

## Context

SAT is the firmware's port of the thermo-nova Home Assistant component. Ground truth for SAT behaviour is, in order: Sergeant D's recorded decisions (`docs/sat/sergeant-d-corrections.md`), then thermo-nova at commit `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb`, then the specs, then the firmware (ADR-196). thermo-nova paths below are relative to `custom_components/sat` in the local copy `other-projects/SAT-releases-thermo-nova/`; the lines were opened in this session. Firmware paths are relative to `src/OTGW-firmware/`.

Terms: PID is the controller of SAT, REST (representational state transfer) the HTTP API, MQTT (Message Queuing Telemetry Transport) the broker protocol, HVAC (heating, ventilation and air conditioning) the Home Assistant climate mode. TASK-1270 is the earlier bug where the first PID output after a reset was 0.

ADR-197 decision 8 said: as thermo-nova `reset_control_state`, a reset zeros the integral and the raw derivative and nothing else, on every target change, preset change and clear, window open and close, HVAC mode change and the reset_integral service, for the main PID and the zone PIDs (Sergeant D entry 73, Q23: "follow thermo-nova"). It marked the work as pending in TASK-1414 and described the firmware gaps: a preset change only cleared the published copy `fPidI`, a target set directly reset nothing, and `satResetIntegral()` cleared the integral only.

**What thermo-nova does**, in code:

- `PID.reset()` sets `_integral` and `_raw_derivative` to 0 and nothing else (`pid.py:134-137`).
- `reset_control_state()` resets the main PID and every area PID (`climate.py:461-464`); the `reset_integral` service does the same (`climate.py:217-220`).
- `async_set_target_temperature` returns at once when the target is unchanged (`climate.py:643-644`) and otherwise ends in `reset_control_state()` (`climate.py:660`). A preset change goes through it (`climate.py:616`, `:624`) and returns early when the preset is the same (`climate.py:610-611`). The window switch to Activity and the restore call the preset and temperature setters (`climate.py:819`, `:832`).
- `async_set_hvac_mode` accepts `HEAT` and `OFF` and resets the control state on both (`climate.py:570-583`).
- A changed room-climate setpoint resets the PID of that area only (`self.areas.pids.reset(entity_id)`, `climate.py:794`), after comparing the new setpoint with the stored `_rooms` value (`climate.py:785-788`).

**Threading in the firmware.** The REST handlers and `updateSetting()` run on the `async_tcp` task (core 1, priority 10). The PID read-modify-writes run on the loop task. A zeroing written from REST in the middle of a loop-task update can be overwritten by that update, so the reset is lost. The firmware already has a pattern for loop-task state touched from REST: a volatile flag raised by the handler and consumed on the loop task (`g_mqttRepublishAllPending`, TASK-1176).

## Decision

SAT resets exactly the integral and the raw derivative of the main PID, and the integral of every zone PID, on the triggers below, and nothing else. A reset is requested by raising a flag and applied on the loop task at the start of the next control pass.

1. **What is reset.** `satPidResetTerms()` sets `_pid_integral` and `_pid_rawDerivative` to 0 and mirrors them into `state.sat.fPidI`, `fPidD` and `fRawDerivative`. `satApplyControlStateReset()` also sets every `satZones[i].fPidIntegral` to 0. The update timers, `_pid_initialized`, the start-up flags, the last error and the last room temperature stay, so the next PID step is a normal step and the TASK-1270 first-tick failure cannot return. `satPidReset()` stays the broad reset for SAT disable, flush and boot.
2. **Triggers for the main and zone PIDs.** A target that really changes (SATtargettemp from REST, MQTT or the adopted thermostat setpoint); a preset change or clear; the window-open switch to Activity and the window-close restore; the SAT off to on switch; the `reset_integral` command (MQTT and both REST paths). `satDisable()` (HVAC off) additionally clears the zone integrals.
3. **Interpretation: a target "really changes" at 0.05 C.** thermo-nova compares for equality (`climate.py:643-644`). The firmware already treats a change of 0.05 C or more as a changed target (`updateSetting`, SATtargettemp), so the same threshold decides the reset. A preset or window change that leaves the target unchanged resets nothing, as in thermo-nova.
4. **Interpretation: HVAC off to on only.** thermo-nova resets on every HVAC mode call. The firmware has no mode call: the SATenabled setting can be written again with the same value by another client or by a retained MQTT message. Only the real off to on transition resets, so a repeated write does not zero the integral.
5. **Firmware choice: the zone trigger.** A changed zone setpoint resets the integral of that zone only, comparing with the previous setpoint of the zone. The first setpoint of a zone resets nothing. thermo-nova compares with `_rooms` (`climate.py:785-795`), which has no direct counterpart here. This is not claimed as parity.
6. **The request flag.** `satResetControlState()` only sets `volatile bool _sat_resetControlStatePending`. `satControlLoopRun()` calls `satApplyControlStateReset()` first, on the loop task. The flag is cleared before the zeroing, so a request raised during it is served on the next pass. The call is safe from any task. `satDisable()` uses the same call for the zone integrals, because it also runs on the `async_tcp` task through `updateSetting()`. Zone setpoints arrive by MQTT on the loop task and are zeroed directly.

Implementation is TASK-1414.

## Decision Contract

### Must

- Zero only the main integral, the raw derivative and the zone integrals on the triggers of decision 2.
- Request the reset through `satResetControlState()` and apply it only in `satApplyControlStateReset()` on the loop task.
- Leave the timers, `_pid_initialized`, the start-up flags and the last error untouched by a reset.
- Reset a zone on a changed setpoint for that zone only.

### Must Not

- Write the PID terms directly from a REST handler or from `updateSetting()`.
- Reset on a target change below 0.05 C, on a preset or window change that leaves the target unchanged, or on a repeated SATenabled=1.
- Reset on a coefficient change.
- Use `satPidReset()` for the trigger set of decision 2.

### Exceptions

- None.

### Verification

- `tests/test_sat_pid_resets.py` compiles the real `SATpid.ino` and the real reset and trigger code cut out of `SATcontrol.ino`, `settingStuff.ino` and `MQTTstuff.ino`. Per trigger it checks that the integral, the raw derivative and the zone integrals are 0, that the timers and flags are unchanged and that the 60 s step schedule equals a run without the trigger. It checks that unchanged targets, presets and enables reset nothing, that a request from another task changes nothing until the loop pass and is applied once, and, structurally, the `satDisable()` and REST call sites. It does not run in CI.
- Not verified: the threading claim is argued from the task model and the existing pattern, not measured on a unit. The bench check is a target change from REST during a running PID, with the integral shown to be 0 on the next pass.

## Alternatives Considered

- **Zero the terms directly in the REST handler.** Rejected: a concurrent loop-task update can overwrite the zeroing, so the reset is lost.
- **Reset only the integral, as ADR-197 decision 8 describes for the firmware.** Rejected: thermo-nova resets the raw derivative too (`pid.py:134-137`), and entry 73 says to follow it.
- **Reset with `satPidReset()`.** Rejected: it also clears the timers and the start-up state, which brings back the TASK-1270 first-tick output of 0.
- **Reset on exact target equality as thermo-nova.** Rejected: a float comparison would reset on rounding noise from the clamp and the MQTT round trip; the existing 0.05 C definition is already in use for the preset clear.
- **Edit ADR-197 decision 8 in place.** Rejected: ADR-197 is Accepted and immutable.

## Consequences

**Positive:**

- A target or preset change no longer carries the old integral into the new target, the main and zone PIDs reset together as in thermo-nova, and a window open and close behaves like a preset change.
- A reset requested from REST cannot be lost to a concurrent PID update.

**Negative:**

- A reset takes effect on the next control pass, not at the moment of the request; the delay is at most one pass of the SAT loop.
- Two points are interpretations, not parity: the 0.05 C threshold and the zone comparison. Sergeant D has not confirmed them.
- No CI gate. Mitigation: the host test above and review against the Decision Contract.

## Open Questions

- [x] Should a repeated SATenabled=1 reset the integral, as a thermo-nova mode call does? Decision 4 says no; it is a firmware choice for Sergeant D to confirm. — **Answered 2026-10-30 by Robert van den Breemen:** No, as Decision 4: only the off-to-on transition resets. Maintainer decision 2026-10-30 (session, ADR-204 accepted); recorded as our reading in Sergeant D entry 157, where he can still overrule it.

## Related Decisions

- **ADR-197 (SAT PID v3 controller)**: this ADR supersedes its decision 8 only. Decisions 1 to 7 stay in force. ADR-197 needs a pointer to this ADR on acceptance (see Status).
- **ADR-196 (SAT behaviour ground truth)**: the source order this ADR follows.

## References

- `docs/sat/sergeant-d-corrections.md` entries 73 (decision), 38 and 46 (stale-sensor path, out of scope) and 157 (readings acted on).
- thermo-nova at commit `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb`, `custom_components/sat`: `pid.py:134-137` (`reset`), `climate.py:217-220`, `:461-464`, `:570-583`, `:610-624`, `:643-660`, `:785-796`, `:817-833`; `services.py:31-39`.
- Firmware: `SATpid.ino` `satPidResetTerms()`; `SATcontrol.ino` `satResetControlState()`, `satApplyControlStateReset()`, `satControlLoopRun()`; `settingStuff.ino` `updateSetting()` (`SATtargettemp`, `SATenabled`); `MQTTstuff.ino` `_satResetIntegralCmd`; `restAPI.ino` reset paths.
- Tests: `tests/test_sat_pid_resets.py`.
- Task: TASK-1414.
