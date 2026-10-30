---
id: "ADR-203"
title: "SAT sends CS MM and CH on every control update (supersedes ADR-199 decision 4)"
status: "Accepted"
date: "2026-10-30"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-199"
topics:
  - "sat"
  - "opentherm"
  - "command-cadence"
  - "ch-enable"
  - "control-setpoint"
aliases:
  - "CS MM CH refresh cadence"
  - "write-on-change"
  - "SAT_CMD_REFRESH_MS"
  - "every control update"
  - "5 s resend"
  - "Q13"
  - "CH cadence"
components:
  - "SAT control-tick command output"
symbols:
  - "satEnqueueIfChangedCS"
  - "satEnqueueIfChangedMM"
  - "satEnqueueIfChangedCH"
  - "satEnqueueOwnCS"
  - "satPicCSKeepalive"
  - "satPwmSuppressionRefresh"
  - "SAT_CMD_REFRESH_MS"
  - "satControlLoopRun"
  - "satRestartStep"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-203 SAT (Smart Autotune Thermostat) sends CS MM and CH on every control update (supersedes ADR-199 decision 4)

## Status

Accepted, 2026-10-30. Guideline-level per ADR-080: no CI (continuous integration) gate.

The cadence is Sergeant D's decision in entry 44 (Q13, "Follow thermo-nova": CS, MM and CH every 5 s, a 5 s floor for every command). There is no open point on the cadence. Revised the same day on a maintainer decision (2026-10-30): an earlier draft of this ADR chose "every SAT control tick (10-300 s)" and left a question open, which conflicted with entry 44.

This ADR supersedes **decision 4 ("CH cadence (kept)") of ADR-199 only**. ADR-199 decisions 1, 2 and 3, its Decision Contract and its scope stay in force. adr-kit has no partial-supersession form: `adr supersede ADR-199 --by ADR-203` would mark all of ADR-199 Superseded, `Accepted -> Amended` is not a legal lifecycle transition in the kit, and `adr relate` writes only a generic `related` cross-reference. The front-matter `supersedes` list therefore stays empty and ADR-199 is not touched by this proposal. **When this ADR is accepted, ADR-199 needs a pointer to it**: run `adr relate ADR-203 --to ADR-199` (reference bookkeeping, allowed on an Accepted record) and do not run `adr supersede`.

## Status History

```yaml
status_history:
  - date: 2026-10-30
    status: Proposed
    changed_by: "Agent: Claude"
    reason: Initial proposal; resolves the ADR-199 decision 4 versus entry 44 contradiction (maintainer decision 2026-10-30)
    changed_via: adr-kit
  - date: 2026-10-30
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-199
    changed_via: adr-kit lifecycle
  - date: 2026-10-30
    status: Accepted
    changed_by: Robert van den Breemen
    reason: Maintainer OK in session 2026-10-30 (TASK-1370); Sergeant D entry 44 Q13 'Follow thermo-nova'
    changed_via: adr-kit lifecycle
```

## Context

SAT (Smart Autotune Thermostat) is the firmware's port of the thermo-nova Home Assistant component. Ground truth for SAT behaviour is, in order: Sergeant D's recorded decisions (`docs/sat/sergeant-d-corrections.md`), then thermo-nova at commit `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb`, then the specs, then the firmware (ADR-196). thermo-nova paths below are relative to `custom_components/sat` in the local copy `other-projects/SAT-releases-thermo-nova/`. That checkout reports git `HEAD` `c5211afee` and does not contain the object `7beaaf531`; CLAUDE.md records its content as identical to the pinned commit (verified 2026-10-22). That identity was not re-verified for this ADR. Firmware paths are relative to `src/OTGW-firmware/` at `HEAD` `6e02216bf` (`src/` clean).

Terms: CS (Control Setpoint, MsgID 1) is the gateway command for the CH water target, MM (Maximum relative Modulation, MsgID 14) the modulation cap, CH (central heating) the enable-bit command, TC the remote room setpoint override, OT (OpenTherm) the boiler protocol, PIC (Microchip peripheral interface controller) the coprocessor on the OTGW (OpenTherm Gateway) board, PM (priority message) the PIC command that requests one OT message, OT-Direct the PIC-less boards where the ESP32 drives the bus itself, MI (minimum interval) the OT-Direct minimum gap between two frames, PWM (pulse width modulation) SAT's on/off duty-cycle mode, PID (proportional-integral-derivative) SAT's controller, DHW (domestic hot water) the tap water function, HVAC (heating, ventilation and air conditioning) the Home Assistant climate mode, MQTT (Message Queuing Telemetry Transport) the broker protocol thermo-nova's coordinator publishes on, EEPROM (electrically erasable programmable read-only memory) the PIC's non-volatile memory. `HEAD` is the current git commit; CLAUDE.md is the project's agent instruction file.

**The contradiction.** ADR-199 (Accepted 2026-10-29) says two incompatible things about the same cadence:

- Decision 4 (`ADR-199...md:130`) and its Negative consequence (`:184`): CH is sent on change plus every 300 s, "not after every control step as `heating_control.py:238`", with Q13 still open.
- Resolved Questions (`:197`): Q13 is answered, "follow thermo-nova, CS/MM/CH on every control update. Entry 44; implemented in TASK-1370."

**Entry 44** (2026-10-24, https://discord.com/channels/812969634638725140/1514720723980259460/1563663056087556187) quotes Sergeant D: "Q13: Follow thermo-nova." Q13 asked "CS/MM/CH refresh: every 5 s as thermo-nova, or on change + 300 s?". Entry 44 reads the answer as the first option, states "This answer sets a 5 s floor for every command", and says ADR-199 decision 4 must be updated. The maintainer decided on 2026-10-30 to resolve this with a superseding ADR for decision 4 only, at the 5 s cadence. The `docs/sat/open-questions.md` row Q13 is marked answered by entry 44, with TASK-1370.

Entry 44 left one design point open: whether "every 5 s" is its own timer or replaces the control tick, to go to Sergeant D "if it changes SAT control behaviour". This ADR picks a separate 5 s timer that only repeats the values the last control tick computed. It changes no control computation (the PID step, the setpoint, the CH decision and the tick interval stay as they are), so that condition is not met and no new question is needed. The same split exists in thermo-nova: a 5 s command loop and a separate 60 s PID timer.

**What thermo-nova does** (opened in this session), in time order:

- `_register_event_listeners` starts a timer that calls `schedule_heating_control_loop` every 5 s (`climate.py:706-710`, `timedelta(seconds=5)` at `:708`).
- `schedule_heating_control_loop` is debounced: if no run is pending, it schedules `async_control_heating_loop` 1 s later (`climate.py:524-530`, `async_call_later(self.hass, 1, ...)` at `:530`).
- `async_control_heating_loop` returns without sending anything while the outside temperature is unknown or HVAC mode is OFF (`climate.py:544-545`); otherwise it calls `HeatingControl.update` (`:547-555`).
- At the end of every `update`, whichever branch ran, it calls `async_set_control_setpoint`, `async_set_control_max_relative_modulation` and `async_set_heater_state` (`heating_control.py:236-238`), with no change check.
- The MQTT coordinator publishes `CS=` (then `PM=25`), `CH=` and `MM=` on every call (`coordinator/mqtt/opentherm.py:195-197`, `:211-212`, `:216-220`).
- The PID steps on its own 60 s timer (`climate.py:712-716`, `PID_UPDATE_INTERVAL = 60` in `pid.py:20`). So the commands repeat about every 5 s while the PID output behind them changes at most once a minute.

**What the firmware does today** (write-on-change, TASK-565): `satEnqueueIfChangedCS` (`SATcontrol.ino:464`), `satEnqueueIfChangedMM` (`:497`) and `satEnqueueIfChangedCH` (`:511`) send only when the value changed or `SAT_CMD_REFRESH_MS` = 300000 ms (`:426`) has passed. They are called from four places in the control tick, which is gated by `timerSATControl` (`:581`, `:5655`) with interval `settings.sat.iControlInterval`, default 30 s (`SATtypes.h:476`), range 10-300 s (`settingStuff.ino:1030`):

| Path | Calls | Lines |
|---|---|---|
| Thermal estimation safe fallback | CS, MM (configured max), CH=1 | `SATcontrol.ino:5764-5766` |
| Summer simmer | CS=10, MM (configured max), CH=0 | `SATcontrol.ino:5791-5793` |
| All valves closed | CS=10, MM (configured max), CH=0 | `SATcontrol.ino:5806-5808` |
| Main control block | CS, MM, CH from `satChEnableDecide` | `SATcontrol.ino:5985-5989` |

Two tick paths send nothing: DHW active (`SATcontrol.ino:5682-5704`, returns at `:5703`) and the boiler fault flag (`:5740-5745`, returns at `:5744`).

Two refresh paths run before the tick gate, on every pass of the SAT loop: `satPicCSKeepalive()` (`:5642`, body `:486-495`) resends the last CS when 30 s passed since the last CS send, on PIC boards only, because the PIC drops a CS of 8 C or more after about 63-126 s without a new one (TASK-1236; `other-projects/otgw-6.6/gateway.asm:1619-1629`); `satPwmSuppressionRefresh()` (`:5645`, body `:1243-1262`) refreshes the PWM suppression CS every 3 s through `satEnqueueIfChangedCS` (entry 23).

**Gates before every SAT sender in `satControlLoopRun`** (`SATcontrol.ino:5559`), in order: the MQTT fallback step (`:5586`); SAT disabled or a flash update running (`:5591`), the firmware's counterpart of HVAC OFF; a safety trip (`:5612-5613`); OpenTherm link lost (`satOtLinkStep`, `:5617`); and `satRestartStep` (`:5620`, body `:3122-3171`), which holds SAT quiet after a restart until a boiler reply and a valid outside temperature exist (entry 121) and hands back and stays quiet while the outside temperature is missing (entries 56, 57), the counterpart of thermo-nova's "outside temperature unknown" skip.

## Decision

While SAT is the active controller, a 5 s resend timer sends the latest CS=, MM= and CH= values again, unchanged values included, as thermo-nova's 5 s heating control loop (entry 44); the control tick (`iControlInterval`, default 30 s) still computes the values and sends all three at once, without a change check, on every tick that reaches one of its four send paths.

This replaces ADR-199 decision 4. Scope and limits:

1. **The control tick computes, the 5 s timer repeats.** The four tick send paths (thermal safe fallback, summer simmer, all valves closed, main control block) send CS=, MM= and CH= on every tick they run, with no change check and no 300 s window. Between ticks the 5 s timer resends the values in the last-sent cache (`fLastSentCS`, `iLastSentMM`, `iLastSentCH`). It sends directly (`satEnqueueOwnCS()` for CS, `addCommandToQueue` for MM and CH), not through the change-checked helpers, which would turn the resend into a no-op. A tick that sends restarts the 5 s window. The resend itself never repeats sooner than 5 s after the last send, but a tick sends at once and can land less than 5 s after a resend, so the rate is 3 commands per 5 s plus at most one extra set per control interval. Which value each path sends is unchanged and stays governed by ADR-199 decision 2 and its table.
2. **The resend runs only where thermo-nova would run its loop and SAT may send.** It sits in `satControlLoopRun` after all the gates listed under Context, next to `satPicCSKeepalive()`, so nothing is resent while SAT is disabled or flashing (HVAC OFF), after a safety trip, while the OpenTherm link is lost, during the restart quiet period (entry 121), or while the outside temperature is missing (thermo-nova `climate.py:544-545`). It also needs `bLastSentValid`: every hand-back and every `satResetCmdCache()` clears it, so the resend stays silent until the next tick has sent.
3. **No-send tick paths stop the resend (firmware choice).** A tick that ends on DHW active or on the boiler fault flag disarms the resend until a tick sends again, and the resend also checks the live DHW and fault bits (`SlaveStatus & 0x05`), as `satPwmSuppressionRefresh` does (`:1250`), so a draw that starts between ticks is not overwritten. Without this the timer would resend the pre-draw CS/MM/CH every 5 s where today nothing goes out, which would change ADR-199 decision 2. This ADR claims no thermo-nova parity for those two paths.
4. **TC= stays event-driven** (entries 14 and 16, `satEnqueueIfChangedTC`, TASK-1272). Entry 44 covers CS/MM/CH only.
5. **The 3 s PWM suppression refresh keeps its change-checked helper** (`satPwmSuppressionRefresh` calling `satEnqueueIfChangedCS`, entry 23). Entry 44 says entry 23 is unchanged. The resend repeats `fLastSentCS`, which the suppression refresh keeps current, so the two never send different setpoints.
6. **The PIC CS keepalive stays** (`satPicCSKeepalive`, TASK-1236). It fires only when 30 s passed since the last CS send, so it never fires while the 5 s resend runs. While the resend is disarmed (DHW active, boiler fault) it keeps resending CS every 30 s on PIC boards, as it does today.
7. **SAT's own CS= still goes through `satEnqueueOwnCS()`** (`SATcontrol.ino:456-460`, ADR-179). MM= and CH= keep going through `addCommandToQueue`.
8. **Every send updates the last-sent cache.** Tick sends and resends write `fLastSentCS`/`iLastSentCSMs`, `iLastSentMM`/`iLastSentMMMs`, `iLastSentCH`/`iLastSentCHMs` and `bLastSentValid`. `satPicCSKeepalive` reads the CS fields (`:486-495`) and the suppression refresh's change check compares against `fLastSentCS` (`:464-476`).
9. **Out of scope:** `PM=25` after every CS and the Immergas `TP=11:12` value (entry 44, "Out of scope"); the SAT-disabled release in `satDisable()`; how CH reaches the boiler in OT-Direct gateway mode (TASK-1238, ADR-179 Open Questions).

Implementation is TASK-1370.

## Decision Contract

### Must

- On each of the four control-tick send paths, enqueue CS=, MM= and CH= on every tick that reaches the path, without a change check or refresh window.
- Between ticks, resend the cached CS, MM and CH every 5 s while SAT is the active controller, with a tick send restarting the 5 s window.
- Place the resend after every gate in `satControlLoopRun` (MQTT fallback, disabled or flashing, safety trip, OpenTherm link lost, `satRestartStep`) and require `bLastSentValid`.
- Disarm the resend after a tick that ends on DHW active or the boiler fault flag, and skip it while the live DHW or fault bit is set.
- Keep updating the last-sent CS, MM and CH cache fields and `bLastSentValid` on every tick send and every resend.
- Send SAT's CS= through `satEnqueueOwnCS()` (ADR-179).
- Keep `satPicCSKeepalive()` and its 30 s condition on PIC boards.
- Keep the CH bit values and the list of CH-queueing paths of ADR-199 decision 2.

### Must Not

- Apply a change check or the 300 s refresh to CS/MM/CH on the four control-tick paths or in the 5 s resend.
- Recompute the PID output, the setpoint or the CH decision in the 5 s resend, or change the control tick interval.
- Resend anything during the restart quiet period, after a safety trip, while SAT is disabled, or while the outside temperature is missing.
- Make TC= periodic (entries 14, 16).
- Make the 3 s PWM suppression refresh unconditional, or remove it (entry 23).
- Bypass `satEnqueueOwnCS()` for SAT's CS=.

### Exceptions

- None.

### Verification

- `tests/test_sat_cmd_cache.py` compiles the real command helpers cut out of `SATcontrol.ino` (`satResetCmdCache`, `satEnqueueOwnCS`, `satEnqueueIfChangedCS/MM/CH/TC`, `satPicCSKeepalive`). It covers the write-on-change behaviour of TASK-1324. Under TASK-1370 it must show, red on the old code and green on the change: the per-tick send on the four paths, the 5 s resend of unchanged values, no resend without `bLastSentValid` or after a no-send tick, and the keepalive staying silent while the resend runs. Not run for this ADR; it does not run in CI.
- Bench check before shipping (entry 44, "Open"): a telnet or capture transcript over at least three control ticks on a PIC board and on an OT-Direct board, showing CS, MM and CH about every 5 s, nothing during the restart quiet period, and no PIC command-queue or OT-Direct frame-queue growth.
- Source anchors: the four call sites in the table under Context; the gates at `SATcontrol.ino:5586-5620`; `heating_control.py:236-238`; `climate.py:708`, `:530`, `:544-545`.

## Alternatives Considered

- **Send on every control tick only (10-300 s), no 5 s timer.** This was the earlier draft of this ADR. Rejected: it never reaches entry 44's 5 s floor, not even at the 10 s minimum interval, and thermo-nova repeats the commands every 5 s (`climate.py:708`).
- **Run the whole control tick every 5 s.** Rejected: the tick carries the sustained timers, the DHW guard and the cycle statistics, which are built on the configured interval (entries 86, 95, 109), so a 5 s tick would change control behaviour and would need Sergeant D. thermo-nova itself keeps the 5 s command loop apart from the 60 s PID step (`climate.py:706-716`).
- **Keep write-on-change plus the 300 s refresh (ADR-199 decision 4 as written).** Rejected: Sergeant D decided "Follow thermo-nova" for Q13 (entry 44), and thermo-nova sends all three on every update without a change check (`heating_control.py:236-238`). Keeping it also leaves ADR-199 contradicting itself.
- **Send only CH on the new cadence and leave CS/MM write-on-change.** Rejected: entry 44 covers CS, MM and CH together, as Q13 asked.
- **Edit ADR-199 decision 4 in place.** Rejected: ADR-199 is Accepted and immutable; the maintainer chose a superseding ADR (2026-10-30).
- **Do nothing.** Rejected: ADR-199 would keep stating both cadences, and TASK-1370 would implement against a record that says the opposite.

## Consequences

**Positive:**

- The record has one cadence for CS/MM/CH, matching entry 44 and the ADR-199 Resolved Questions entry, with no open point.
- A PIC that rebooted, or a boiler whose state drifted, is resynced within about 5 s instead of within 300 s.
- CS, MM and CH follow thermo-nova's 5 s loop (`climate.py:708`, `heating_control.py:236-238`), while the PID and the control tick keep their own cadence.
- On PIC boards the CS override can no longer expire while SAT is heating: a 5 s resend is far inside the PIC's ~63 s expiry, whatever the control interval.

**Negative:**

- **PIC serial traffic.** 3 commands per 5 s, 0.6 commands/s, when the control interval is a multiple of 5 s (the 30 s default); the worst case is an 11 s interval, 3 sets in 11 s or about 0.82 commands/s. That is against about 0.003/s for an unchanged value under the 300 s refresh. The TASK-565 rationale for write-on-change ("cutting steady-state OT traffic ~20x", `SATcontrol.ino:421-425`) is given up. Mitigations: `addCommandToQueue` replaces a pending command with the same two-letter prefix in place (`OTGW-Core.ino:3803-3818`; only `TP=` is append-only), so a CS/MM/CH still waiting for its PIC reply is overwritten, not stacked. `handleCommandQueue` runs once a second (`OTGW-firmware.ino:762-764`) and sends every due entry in that pass (`OTGW-Core.ino:3879-3897`); a sent entry is retried every 5 s (`OTGW_CMD_INTERVAL_MS`, `:3674`) until the PIC replies.
- **No PIC EEPROM wear.** On the PIC, `CS=` writes `controlsetpt1/2` and sets `HeartbeatCS` (`gateway.asm:4187-4198`), `CH=` sets or clears `CHModeOff` (`:4372-4378`) and `MM=` writes `MaxModLevel` (`:4752-4763`). All three are RAM (random access memory); none of these handlers calls `WriteEpromData`/`StoreEpromData` (`:4592-4595`). Checked in this session for these three handlers only.
- **OT bus on PIC boards.** The PIC holds CS and MM as stored overrides applied to the thermostat's frames, so an unchanged resend writes the same value and adds no OT frames. Verified for the CS path (`gateway.asm:1619-1629`); the MM and CH frame handling in the PIC was not traced.
- **OT-Direct.** OT-Direct has no PIC command queue: `addCommandToQueue` calls `handleOTDirectCommand` synchronously (`OTGW-Core.ino:3761-3767`). CS= and MM= each enqueue a `WRITE` frame (`OTDirect.ino:2785-2796` for MsgID 1, `:2912-2921` for MsgID 14); CH= only sets `otChModeOff` and sends no frame (`:3020-3025`). The resend therefore adds 2 `WRITE` frames per 5 s, 0.4 frames/s, into the 12-slot frame queue (`:313`), which coalesces a pending frame with the same MsgID in place (`:349-367`). The OT-Direct schedule already rewrites MsgID 1 and 14 every 15 s (`OT_WRITE_INTERVAL_MS`, `:59`; entries `:135`, `:138`), and a command does not reset that schedule (`updateWriteCache`, `:603-611`), so the extra cost is 0.4 frames/s on top of about 0.13 frames/s.
- **Interaction with MI=500 on OT-Direct.** For Ideal, Intergas and Nefit SAT sends `MI=500` at start-up (`SAT_QUIRK_MI_500_BOOT`, `SATcontrol.ino:202`, `:204`, `:206`, `:4679-4680`), and the OT-Direct scheduler then sends at most one frame per 500 ms (`OTDirect.ino:1547`), about 2 frames/s. Estimate, not measured: MsgID 0 already takes up to 1.25 frames/s (every 800 ms, `:55`), so the resend's 0.4 frames/s uses about a fifth of that ceiling, and pending commands go before scheduled polls (`:1549-1551`), so reads are delayed rather than the resend. The bench check above must include an MI=500 unit or a forced `MI=500`.
- **No CI gate.** Mitigation: the host test above, extended under TASK-1370, plus review against the Decision Contract.

## Open Questions

None. The cadence is settled by entry 44 (5 s, CS/MM/CH, unchanged values included).

## Related Decisions

- **ADR-199 (SAT cold state and CH-enable rule)**: this ADR supersedes its decision 4 only. Decisions 1-3, the CH path table and the Decision Contract stay in force. ADR-199 needs a pointer to this ADR on acceptance (see Status).
- **ADR-196 (SAT behaviour ground truth)**: the source order this ADR follows.
- **ADR-179 (MsgID 1 ownership)**: SAT's CS= goes through `satEnqueueOwnCS()`; unchanged.
- **ADR-080 (Binding ADR rules must have a CI gate)**: this ADR is guideline-level.

## References

- `docs/sat/sergeant-d-corrections.md` entry 44 (Q13, "Follow thermo-nova", the 5 s floor; it changes entry 7's CH cadence point), entries 14, 16 (TC= event-driven), 23 (3 s suppression refresh), 56, 57 (outside temperature missing) and 121 (restart quiet period), with their Discord links.
- `docs/sat/open-questions.md` row Q13.
- `docs/adr/ADR-199-sat-cold-state-and-ch-enable-rule-supersedes-adr-195.md:130`, `:184`, `:197`.
- thermo-nova (local copy, see Context): `heating_control.py:229-238`; `climate.py:524-530`, `:540-555`, `:706-710`, `:712-716`; `pid.py:20`; `coordinator/mqtt/opentherm.py:195-197`, `:211-212`, `:216-220`.
- Firmware at `6e02216bf`: `src/OTGW-firmware/SATcontrol.ino:202-206`, `:421-426`, `:456-460`, `:464-476`, `:486-495`, `:497-508`, `:511-520`, `:581`, `:1243-1262`, `:3122-3171`, `:4679-4680`, `:5559`, `:5586`, `:5591`, `:5612-5613`, `:5617`, `:5620`, `:5642`, `:5645`, `:5655`, `:5682-5704`, `:5740-5745`, `:5764-5766`, `:5791-5793`, `:5806-5808`, `:5985-5989`; `src/OTGW-firmware/OTGW-Core.ino:3674`, `:3761-3767`, `:3803-3818`, `:3879-3897`; `src/OTGW-firmware/OTGW-firmware.ino:762-764`; `src/OTGW-firmware/OTDirect.ino:55`, `:59`, `:135`, `:138`, `:313`, `:349-367`, `:603-611`, `:1547-1551`, `:2785-2796`, `:2912-2921`, `:3020-3025`; `src/OTGW-firmware/SATtypes.h:476`; `src/OTGW-firmware/settingStuff.ino:1030`.
- `other-projects/otgw-6.6/gateway.asm:1619-1629` (PIC CS expiry), `:4187-4198` (`SetCtrlSetpoint`), `:4372-4378` (`SetCentralHeat`), `:4752-4763` (`SetMaxModLevel`), `:4592-4595` (EEPROM write routines).
- Tasks: TASK-1370 (implementation), TASK-565 (write-on-change), TASK-1236 (PIC CS keepalive), TASK-1268 (suppression refresh), TASK-1272 (TC= event-driven), TASK-1324 (cache reset), TASK-1238 (OT-Direct CH), TASK-1482 (restart gates).
- Test: `tests/test_sat_cmd_cache.py`.

No Enforcement block: the rule is about which calls code paths inside `SATcontrol.ino` make and in which order relative to the gates, while `satPwmSuppressionRefresh` in the same file must keep the change-checked helper, so no path-scoped regex can state it. It is guideline-level per ADR-080.
