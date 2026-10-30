---
id: "ADR-199"
title: "SAT cold state and CH-enable rule (supersedes ADR-195)"
status: "Accepted"
date: "2026-10-29"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes:
  - "ADR-195"
superseded_by: null
related:
  - "ADR-203"
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
  - "cold boundary"
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
  - "satControlLoopRun"
  - "satDisable"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-199 SAT (Smart Autotune Thermostat) cold state and CH-enable rule (supersedes ADR-195)

## Status

Accepted, 2026-10-29.

Guideline-level per ADR-080: no CI (continuous integration) gate. Acceptance needs the maintainer's explicit OK (TASK-1355). This ADR supersedes ADR-195 once accepted. The front-matter `supersedes` list stays empty while it is Proposed: adr-kit writes both sides of a supersession at once (`adr supersede ADR-195 --by ADR-199`), and adr-lint fails a one-sided link. Until then ADR-195 stays Accepted and its file is untouched.

## Status History

```yaml
status_history:
  - date: 2026-10-24
    status: Proposed
    changed_by: "Agent: Claude"
    reason: Initial proposal, supersedes ADR-195 (SAT port audit 2026-10-24, finding 69; TASK-1355)
    changed_via: adr-kit
  - date: 2026-10-29
    status: Proposed
    changed_by: Robert van den Breemen
    reason: Maintainer OK in session 2026-10-29 (TASK-1355); open questions answered by Sergeant D
    changed_via: adr-kit lifecycle
  - date: 2026-10-29
    status: Accepted
    changed_by: Robert van den Breemen
    reason: Maintainer OK in session 2026-10-29 (TASK-1355); open questions answered by Sergeant D
    changed_via: adr-kit lifecycle
  - date: 2026-10-30
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-203
    changed_via: adr-kit lifecycle
```

## Context

SAT (Smart Autotune Thermostat) is the firmware's port of the thermo-nova Home Assistant component. Ground truth for SAT behaviour is, in order: Sergeant D's recorded decisions in `docs/sat/sergeant-d-corrections.md`, then thermo-nova at commit `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb` (local copy `other-projects/SAT-releases-thermo-nova/`; thermo-nova paths below are relative to `custom_components/sat`), then the specs, then the firmware (ADR-196). Firmware paths below are relative to `src/OTGW-firmware/` at `HEAD` `2d01d3297`; `git diff 2d01d3297 c0599581f -- src/OTGW-firmware` is empty, so the line numbers also hold at `c0599581f`.

ADR-195 (Accepted 2026-10-24) records the cold state and the CH (central heating) enable rule. Two statements in it are wrong (SAT port audit 2026-10-24, finding 69, `docs/sat/sat-port-audit-2026-10-24.md`). The maintainer directive of 2026-10-24 is that an ADR with wrong statements is superseded, not edited.

1. **The `<=` boundary was not settled when ADR-195 was written (settled since, entry 98).** ADR-195 closes its boundary question as "settled by entry 12". Entry 12 answered a different question: our 18:10 UTC question was whether SAT should switch PWM (pulse-width modulation) off and reset the setpoint on the cutoff, as thermo-nova does, and "yes keep the thermo-nova solution" answers that. Entries 7 and 11 both list "strictly below or at or below" as still open: he wrote "below 28.2 not 22" (entry 7) and "If the requested setpoint is lower than the thresholds then we send CH=0 CS=10" (entry 11). thermo-nova uses at or below: the heater state is ON only when `requested_setpoint > COLD_SETPOINT` (`heating_control.py:72`) and the PWM setpoint override returns when `requested_setpoint <= COLD_SETPOINT` (`heating_control.py:385`). The firmware follows thermo-nova (`satColdCutoff`, `SATchEnable.h:26-29`). At exactly 28.2 or 21.0 his wording gives CH=1, the firmware gives CH=0. The corrections file contradicts itself here: the "Settled" bullet of entry 12 reads "When the requested setpoint drops to the cold setpoint or below, SAT switches PWM off", which records at-or-below as settled, while entries 7 and 11 list it as open. That bullet is our summary, not his words ("yes keep the thermo-nova solution"), and is the likely origin of ADR-195's "settled by entry 12". Resolved: Sergeant D answered "at or below" (entry 98, which corrects our strict-`<` reading in entry 96); entry 12 stands and the firmware already does this.
2. **"CH is decided only in `satChEnableDecide`" does not hold.** `satControlLoopRun()` (`SATcontrol.ino:4766`) has three early-return paths that queue the CH bit themselves before the main control block is reached, and `satDisable()` queues CH=1 since TASK-1346:
   - thermal estimation safe fallback: CH=1 (`SATcontrol.ino:4960-4976`, CH at `:4972`);
   - summer simmer: CS=10, CH=0 (`SATcontrol.ino:4996-5006`, CH at `:5002`);
   - all valves closed: CS=10, CH=0 (`SATcontrol.ino:5009-5021`, CH at `:5016`);
   - `satDisable()`: CS=0, then CH=1 (`SATcontrol.ino:2391-2395`, CH at `:2395`; TASK-1346, Done), then MM=T (`SATcontrol.ino:2396-2400`; TASK-1344, In Progress) to hand the boiler back (`SATcontrol.ino:2368-2400`).

Since ADR-195 two more decisions were recorded that touch this rule. Entry 17 (2026-10-24, https://discord.com/channels/812969634638725140/1514720723980259460/1563464620956450840) quotes his answer to our question 3: "Yes this is how it works. When we are in PWM `OFF` m, CS=10 CH=0". His own words confirm the PWM `OFF` half on his installation (CS=10 with CH=0). That CH stays 1 during the PWM ON phase is in the entry's "Settled" bullet but depends on the wording of our question 3, which entry 17 does not quote; it is our reading of his "Yes", not a statement in his words. ADR-195 rested the PWM ON point on our reading of "great understanding!" (entry 9). Entry 32 (2026-04-06, https://discord.com/channels/812969634638725140/1105556725714649128/1490819012899377444) records valves closed as CS=10, idle: "when all valves are closed we keep the CS=10 not 0".

thermo-nova's cold sites, opened in this session: `COLD_SETPOINT = 22`, `MINIMUM_SETPOINT = 10.0` (`const.py:34-35`); heater state (`heating_control.py:65-72`, OFF when the HVAC (heating, ventilation and air conditioning) mode is not heat, no valve is open, or the requested setpoint is at or below `COLD_SETPOINT`); the heater-off branch of `update()`: `self._pwm.disable()`, `_reset_pwm_disable_guards()`, `_control_setpoint = MINIMUM_SETPOINT`, maximum relative modulation (`heating_control.py:229-234`), with `disable()` = `reset()` plus `_enabled = False` (`pwm.py:106-109`); the CH bit after every step, ON only when `_control_setpoint > COLD_SETPOINT` (`heating_control.py:238`), sent as the CH enable bit by the serial coordinator (`coordinator/serial/__init__.py:210-212`); the PWM enable guard (`heating_control.py:280-282`); the PWM disable guards (`heating_control.py:317-319`); the PWM override gate (`heating_control.py:385`); the flame-off hold floor `clamp(return + offset, COLD_SETPOINT, maximum_setpoint)` (`heating_control.py:398`); the guard reset (`heating_control.py:525-528`).

## Decision

While SAT is the active controller, one per-heating-system cold setpoint replaces thermo-nova's `COLD_SETPOINT` 22 at every ported place thermo-nova uses it (point 1 lists the sites, and the four that are not ported), and the CH bit is set in exactly the places listed in point 2 below, each with its reason. This keeps every part of ADR-195 that is still right and corrects its two wrong statements.

Terms: CS (Control Setpoint) is the gateway command for the CH water target, CH the enable bit command, MM (Maximum relative Modulation) the modulation cap command, PWM (pulse width modulation) SAT's on/off duty-cycle mode, OT (OpenTherm) the boiler protocol, DHW (domestic hot water) the tap water function. The requested setpoint is the PID (proportional-integral-derivative) output after the multi-zone block and the solar offset, clamped, stored as `_sat_requestedSetpoint` (`SATcontrol.ino:5137-5152`).

**1. Cold setpoint and cutoff (kept from ADR-195, boundary decided: `<=`).**

- `satGetColdSetpoint()` (`SATcontrol.ino:339-343`) returns 28.2 C for radiators (`SAT_COLD_SETPOINT_RAD`, `SATcontrol.ino:130`) or 21.0 C for underfloor (`SAT_COLD_SETPOINT_FLOOR`, `SATcontrol.ino:131`). The values come from entry 7 and entry 11 answer 2; neither says anything about constants versus user settings. The fixed-constant choice comes from ADR-150, carried over through ADR-195.
- It is used at every `COLD_SETPOINT` site in thermo-nova's `heating_control.py` (`:72`, `:238`, `:280`, `:317`, `:385`, `:398`, the sites entry 11 answer 2 lists) and in `cycles/classifier.py` (`:51`, `:85`): the cutoff (`SATcontrol.ino:5209`), the PWM override gate (`satApplyPWM`, `SATcontrol.ino:1023`) and its mirror for the CH rule (`satPwmDrivesSetpoint`, `SATcontrol.ino:1010-1014`), the 3 s suppression refresh (`SATcontrol.ino:1055`), the flame-off hold floor (`satPwmFlameOffHold`, `SATcontrol.ino:808-814`, floor at `:811`), the auto-switch call (`SATcontrol.ino:5188-5190`) and the cycle classifier (`SATcycles.ino:653`, compared with `<` at `:662` and `:692`, as `classifier.py:51`, `:85`).
- thermo-nova has four more `COLD_SETPOINT` sites with no firmware counterpart found: the per-area heating demand `self.pid.output > COLD_SETPOINT` (`area.py:175`), the per-area cooling cap and its floor `max(COLD_SETPOINT, cap)` (`area.py:322`, `:331`), and `did_overshoot_at_flame_off`, which ignores a setpoint below `COLD_SETPOINT` (`device/status.py:121`). A grep of `src/OTGW-firmware` for `satGetColdSetpoint()` finds only the sites above (`SATcontrol.ino:811`, `:1013`, `:1023`, `:1055`, `:5188`, `:5209`, `:5223`; `SATcycles.ino:653`), and a grep for `did_overshoot`, `COOLING_HEADROOM` and `area.py` finds no ported counterpart. This ADR records them as not ported; it does not decide whether they should be.
- The cutoff comparison is `<=` (`satColdCutoff`, `SATchEnable.h:26-29`): a requested setpoint equal to the cold setpoint is cold. Decided by Sergeant D in entry 98 (2026-10-28), "at or below", in answer to Q20 (`docs/sat/open-questions.md`); this agrees with thermo-nova `heating_control.py:72` and `:385` and with entry 12 ("at the cold setpoint or below"), which stands. Entry 98 withdraws our strict-`<` reading in entry 96; entry 71 gave the values only. No code change is needed.
- On the cutoff (`SATcontrol.ino:5209-5214`) SAT sets the final setpoint to `SAT_MIN_SETPOINT` 10 (`SATcontrol.ino:129`) and calls `satPwmDisableOnHeaterOff()` (`SATcontrol.ino:1100-1109`), as thermo-nova's heater-off branch (`heating_control.py:229-234`, `pwm.py:106-109`) and entry 12: leave PWM mode for continuous, reset the PWM phase, ignition wait, hold and effective boiler temperature (`satPwmReset`, `SATcontrol.ino:1070-1079`), clear the sustained overshoot, underheat and saturation timers (`satCycleResetSustainGuards`, `SATcycles.ino:147-152`, as `heating_control.py:525-528`), set the held CS to 10, and set the modulation to `settings.sat.iMaxRelModulation`. CS=10, MM and CH=0 then go out through the write-on-change helpers (`SATcontrol.ino:5217-5223`).

**2. Where the CH bit is set.** Every place in SAT that queues CH is in this list. A new one needs a new or superseding ADR.

| Path | CH | Where | Source |
|---|---|---|---|
| Main control block | `satChEnableDecide` | `SATcontrol.ino:5223`, rule at `SATchEnable.h:46-52` | entries 5, 9, 11, 17; `heating_control.py:238` with the departures below |
| Thermal estimation safe fallback: runs only when `state.sat.bFallbackActive && state.sat.iLastKnownRoomMs > 0` and `SAT_THERMAL_MAX_EST_MS` (2 h, `SATcontrol.ino:158`) has elapsed; `bFallbackActive` is set only by the MQTT-loss takeover (`SATcontrol.ino:4732`), not by a loss of room data, so the path is probably unreachable in normal use (audit finding 63) | 1 | `SATcontrol.ino:4960-4976` (`:4972`) | firmware path (Task #21); this ADR claims no thermo-nova counterpart |
| Summer simmer active | 0, with CS=10 | `SATcontrol.ino:4996-5006` (`:5002`) | firmware path; entry 32 keeps the feature; thermo-nova's heater state has no summer term (`heating_control.py:65-72`) |
| All valves closed | 0, with CS=10 | `SATcontrol.ino:5009-5021` (`:5016`) | entry 32; `heating_control.py:69-70`, `:233`, `:238` |
| SAT disabled, safety trip or fallback lifted (`satDisable`) | 1, after CS=0, then MM=T | `SATcontrol.ino:2368-2400` (CH at `:2391-2395`, MM=T at `:2396-2400`) | CH=1: TASK-1346 (Done), hand back the CH override flag with the boiler. MM=T: TASK-1344 (In Progress), not part of this decision |

`satChEnableDecide` (kept from ADR-195):

- requested setpoint at or below the cold setpoint gives CH=0 (point 1);
- PWM OFF phase while PWM drives the CS (`satPwmDrivesSetpoint`) gives CH=0 with CS=10 (`satApplyPWM` returns 10 at `SATcontrol.ino:1026-1029`; entries 5 and 17);
- everything else gives CH=1: the PWM ON phase whatever CS keeps the flame on, continuous mode, and a PWM tick that keeps the previous CS.

Paths that send no CH and leave the last sent bit in place: DHW active (`SATcontrol.ino:4865-4882`), the boiler fault flag (`SATcontrol.ino:4946-4949`), an invalid room temperature before the safety trip (`SATcontrol.ino:4899-4913`), a tripped SAT (`SATcontrol.ino:4811`) and a control tick that is not due (`SATcontrol.ino:4844`).

Two deliberate departures from thermo-nova `heating_control.py:238`, kept from ADR-195:

- **PWM ON.** thermo-nova would clear CH whenever its suppression CS (flow minus the offset, `heating_control.py:441`) is at or below the threshold during a burn. The firmware keeps CH=1. Entry 17 supports this: his answer confirms the PWM OFF half in his own words, and the PWM ON half rests on his "Yes" to our question 3, as the entry reads it (Context). ADR-195 rested it on our reading of entry 9 alone.
- **Requested, not sent.** thermo-nova compares the sent `_control_setpoint`; the firmware compares the requested setpoint. Example: overshoot protection off, held CS 24, requested setpoint 30 gives CH=1 in the firmware. That this follows from entry 11 answer 1 is our reading (TASK-1323 notes).

**3. Flame-off hold (kept).** The PWM flame-off hold is clamped to [cold setpoint, maximum setpoint] (`satPwmFlameOffHold`, `SATcontrol.ino:808-814`), as `heating_control.py:398` with 28.2/21 for 22 (entry 7).

**4. CH cadence (kept).** CH is sent on change plus every 300 s (`satEnqueueIfChangedCH`, `SATcontrol.ino:512-520`; `SAT_CMD_REFRESH_MS`, `SATcontrol.ino:427`), not after every control step as `heating_control.py:238`. Entry 7 records that we named this and he did not answer. Open question Q13 asks it. The early-boot resend gap of ADR-195 is closed (TASK-1324, Done).

**Scope (kept).** Only while SAT is the active controller. SAT's CS goes through `satEnqueueOwnCS` (`SATcontrol.ino:457-461`), so ADR-179's MsgID 1 ownership is unchanged. CH goes through `addCommandToQueue` (`SATcontrol.ino:516`). How CH reaches the boiler in OT-Direct gateway mode is not settled (TASK-1238, To Do; ADR-179 Open Questions).

## Decision Contract

### Must

- Use `satGetColdSetpoint()` (28.2 radiators, 21.0 underfloor) at every ported site where thermo-nova's `heating_control.py` or `cycles/classifier.py` uses `COLD_SETPOINT` (Decision point 1), and at any `area.py` or `device/status.py` site that is ported later.
- Compare through `satColdCutoff` only, so the `<=` boundary of entry 98 lives in one place.
- On the cutoff and on all valves closed: CS=10, CH=0, MM at the configured maximum, and `satPwmDisableOnHeaterOff()`.
- Queue the CH bit only from the paths in the table of Decision point 2.
- Send SAT's CS through `satEnqueueOwnCS` (ADR-179).

### Must Not

- Turn the cold setpoints into user settings, or reintroduce the global 22 at a `COLD_SETPOINT` site. Not covered: the literal `satGetFlowTemp() < 22.0f` that labels the published `sat/modulation_state` "`COLD`" (`SATcontrol.ino:3255`). It is not a `COLD_SETPOINT` site: thermo-nova's `COLD` modulation state is `setpoint <= MINIMUM_SETPOINT` (`heating_control.py:142`). It is an existing display-only divergence this ADR does not decide.
- Clear CH during the PWM ON phase because the flame-keeping CS is low.
- Derive CH from the sent CS in the main control block.
- Change the cutoff to strictly below (`<`); entry 98 decided `<=`.
- Change the CH part of the SAT-disabled release (CS=0, then CH=1; TASK-1346). The MM=T release belongs to TASK-1344 (In Progress) and is not fixed by this ADR.

### Exceptions

- None.

### Verification

- `python tests/test_sat_pwm_override.py`: ran 2026-10-24 in this session, 70 tests, OK. It compiles the real PWM block of `SATcontrol.ino` (including `satPwmReset()` and `satPwmDisableOnHeaterOff()`) and checks the cutoff and closed-valves resets. It does not reach the CH queueing.
- `tests/test_sat_ch_enable.cpp` compiles the real `SATchEnable.h` and checks the boundary at exactly 28.2 and 21.0, the PWM phases and continuous mode. **Not run for this ADR:** no `g++` on this host.
- Neither test runs in CI. No test checks that CH is queued only from the listed paths; that list was built by reading `satControlLoopRun()` and `satDisable()` and by a grep of `src/OTGW-firmware/*.ino` for `CH=` (2026-10-24).

## Alternatives Considered

- **Edit ADR-195 in place.** Rejected: ADR-195 is Accepted and immutable, and the maintainer directive of 2026-10-24 is to supersede an ADR with wrong statements.
- **Keep "CH only in `satChEnableDecide`" and move the early-return CH sends into it.** Not chosen here: that is a code change with boiler effect (the summer and safe-fallback paths have no thermo-nova counterpart to follow). This ADR records the rule the code follows. If the maintainer wants one decision point, that is a task, and the table in point 2 shrinks with it.
- **Switch the cutoff to strictly below (`<`), as his wording.** Rejected: thermo-nova uses `<=` (`heating_control.py:72`, `:385`), and when asked (Q20) Sergeant D answered "at or below" (entry 98, which withdraws our strict-`<` reading in entry 96).
- **Follow `heating_control.py:238` literally for CH.** Rejected on entries 9 and 17: a low flame-keeping CS during PWM ON would clear CH mid-burn.
- **Do nothing.** Rejected: ADR-195 would keep telling readers that the boundary is decided and that CH has one decision point.

## Consequences

**Positive:**

- The record matches the code: every CH send is listed with its line, and the `<=` boundary is recorded as decided (entry 98).
- The PWM CH rule has Sergeant D's confirmation on his own installation (entry 17): the PWM OFF half in his own words, the PWM ON half through his "Yes" to our question as the entry reads it.
- ADR-195's still-correct parts (per-system 28.2/21, cutoff actions, flame-off hold floor, CH departures, cadence, scope) carry over unchanged.

**Negative:**

- **Five CH paths, not one.** A new path can be added without anyone checking it against this ADR. Mitigation: the Must rule and the table; review.
- **The summer simmer path does not reset PWM.** It sends CS=10 and CH=0 but does not call `satPwmDisableOnHeaterOff()` (the comment at `SATcontrol.ino:1097-1098` says so), unlike the cutoff and valves-closed paths. Mitigation: open question below.
- **One PWM re-enable path is missing.** thermo-nova also re-enables PWM in `on_cycle_end()` after a continuous cycle classified OVERSHOOT (`pwm.py:219-223`). A grep of `src/OTGW-firmware` for `on_cycle_end` finds only the comment at `SATcontrol.ino:1096`. Mitigation: TASK-1331 (In Progress; entries 15 and 18).
- **CH=0 may not reach the boiler in OT-Direct gateway mode** (ADR-179 Open Questions, TASK-1238, To Do). This ADR decides when SAT wants CH=0, not how the bit reaches the boiler.
- **CH cadence differs from thermo-nova** (300 s refresh). Mitigation: Q13.
- **Fixed constants.** An atypical installation cannot retune 28.2/21 without a firmware change (carried over from ADR-150 via ADR-195).
- **No CI gate.**

## Open Questions

None open. Every question that was listed here has been answered and recorded; see "Resolved questions". The two implementation tasks that used to be listed here are under "Follow-up work".

## Resolved Questions

- **Cold cutoff boundary** (Q20): equality is cold, so the comparison is `<=`, as thermo-nova `heating_control.py:72`, `:385`. Entry 98 (2026-10-28, corrects entry 96): Sergeant D "at or below". Entry 71 gave the values 28.2 / 21 and left the boundary open. No code change: `satColdCutoff()` already returns `<=` (`SATchEnable.h:26-29`).
- **Port the `COLD_SETPOINT` sites in `area.py` and `device/status.py`** (maintainer): answered 2026-10-24 by Robert van den Breemen: port them with the per-system cold setpoint (28.2 / 21, entries 7 and 11). `device/status.py:121` now (TASK-1361); `area.py:175`, `:322` and `:331` together with the zone port, which waits for Sergeant D (Q18).
- **Summer simmer calls `satPwmDisableOnHeaterOff()`** (maintainer): answered 2026-10-24 by Robert van den Breemen: yes, as the cold cutoff and the valves-closed path (TASK-1362).
- **CS/MM/CH refresh cadence** (Q13): follow thermo-nova, CS/MM/CH on every control update. Entry 44; implemented in TASK-1370.

## Follow-up Work (does not change this decision)

This ADR decides when SAT wants CH=0 and where the CH bit is queued. The tasks below concern how the bit reaches the boiler and a PWM re-enable path; neither changes those rules.

- TASK-1238 (To Do): how SAT's CH=0 reaches the boiler in OT-Direct gateway mode (MsgID 0); see also ADR-179 Open Questions.
- TASK-1331 (In Progress): port of `on_cycle_end` (`pwm.py:219-227`).

## Related Decisions

- **ADR-195 (SAT cold state and CH-enable rule)**: superseded by this ADR on acceptance. Kept: per-system 28.2/21, the cutoff actions, the flame-off hold floor, the two CH departures, the cadence, the scope. Replaced: the boundary marked as settled, and "CH only in `satChEnableDecide`".
- **ADR-150 (SAT per-heating-system cold setpoint)**: superseded by ADR-195; its per-system values and fixed-constant choice carry over through ADR-195.
- **ADR-196 (SAT behaviour ground truth)**: the source order this ADR follows.
- **ADR-179 (MsgID 1 ownership)**: SAT's CS goes through `satEnqueueOwnCS`; its Open Questions hold the OT-Direct CH limitation (TASK-1238).
- **ADR-085 (SAT integration)**: umbrella; unchanged.
- **ADR-110 (SAT PV surplus setpoint boost)**: a boost above the cold setpoint re-enables heating; unchanged.
- **ADR-080 (Binding ADR rules must have a CI gate)**: this ADR is guideline-level.

## References

- `docs/sat/sat-port-audit-2026-10-24.md`, finding 69 ("ADR drift").
- `docs/sat/sergeant-d-corrections.md` entries 5, 7, 9, 11, 12, 13, 17 and 32, with their Discord links.
- `docs/sat/open-questions.md` Q13, Q20.
- thermo-nova at `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb`, `custom_components/sat` (opened 2026-10-24 in the local copy, which CLAUDE.md records as identical to that commit): `const.py:34-35`; `heating_control.py:65-72`, `:229-234`, `:236-238`, `:280-282`, `:317-319`, `:142`, `:385`, `:388-389`, `:397-398`, `:441`, `:525-528`; `pwm.py:106-109`, `:219-227`; `coordinator/serial/__init__.py:210-212`; `cycles/classifier.py:51`, `:85`; `area.py:175`, `:322`, `:331`; `device/status.py:121`.
- Firmware at `HEAD` `2d01d3297`: `src/OTGW-firmware/SATchEnable.h:26-29`, `:46-52`; `src/OTGW-firmware/SATcontrol.ino:129-131`, `:339-343`, `:427`, `:457-461`, `:465-474`, `:512-520`, `:808-814`, `:1010-1014`, `:1016-1040`, `:1055`, `:1070-1079`, `:1096-1098`, `:1100-1109`, `:158`, `:2368-2400`, `:3255`, `:4732`, `:4766`, `:4811`, `:4844`, `:4865-4882`, `:4899-4913`, `:4946-4949`, `:4960-4976`, `:4996-5006`, `:5009-5021`, `:5137-5152`, `:5188-5190`, `:5209-5223`; `src/OTGW-firmware/SATcycles.ino:147-152`, `:197-209`, `:653`, `:662`, `:692`, `:952`.
- `docs/sat/sat-port-audit-2026-10-24.md`, finding 63 (thermal-estimate fallback unreachable in the normal room-loss case).
- Tasks: TASK-1355 (this supersession), TASK-1316, TASK-1323, TASK-1324 (Done), TASK-1327, TASK-1331 (In Progress), TASK-1332, TASK-1341, TASK-1344 (In Progress), TASK-1346 (Done), TASK-1238 (To Do).
- Tests: `tests/test_sat_ch_enable.cpp`, `tests/test_sat_pwm_override.py`.

The decision has no rule a regex can check, and the host tests are not in CI, so it has no Enforcement block: it is guideline-level per ADR-080.
