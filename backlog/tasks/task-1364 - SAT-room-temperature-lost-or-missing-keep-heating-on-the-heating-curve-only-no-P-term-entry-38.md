---
id: TASK-1364
title: >-
  SAT: room temperature lost or missing - keep heating on the heating curve
  only, no P term (entry 38)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-24T21:41:00Z'
updated_date: '2026-10-25T05:13:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 499000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-24 (Q6, entry 38): keep heating on the heating curve; no proportional term without a valid room temperature. Replaces the firmware trip after ~10 ticks (audit #4). Plan quotes entry 38 and thermo-nova; post to Sergeant D as a notice.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 With no valid room temperature SAT keeps heating on the curve without P (host test red on HEAD: trips; green: curve only); stale-time rule documented
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
TASK-1364 plan, corrected by the second reader (entry 38: a lost or missing room temperature means SAT keeps heating on the heating curve only). No file was edited. I opened every location below in this session. thermo-nova paths are relative to other-projects/SAT-releases-thermo-nova/custom_components/sat (pinned 7beaaf5). Firmware paths are relative to src/OTGW-firmware.

== 0. What the second read changed ==
The thermo-nova reading, the trip chain, and the "no satPidReset / NAN lastError" design all hold. These parts were wrong or missing:
(a) Removing the early return at SATcontrol.ino:4900-4914 makes several blocks run on an invalid room temperature for the first time. "Run as today" was false for three of them:
  - the MQTT-takeover 2 h safe setpoint (:4950-4977);
  - PV boost (:1778-1779);
  - auto-tune (:5025).
  See step 4.2.
(b) state.sat.fError was not handled.
(c) The doc list had wrong lines and missing sites.
(d) Board difference: OT-Direct has no thermostat to hand back to.
(e) The George message bundled three decisions. It is now a notice plus one question.
(f) Verification C named a non-existent command.

== 1. Sergeant D entries ==
- Entry 38 drives the task: "Keep heating on heat curve. Not proportional because it is related to error ... we cannot use Proportional". The plan follows it. "No I/D either" and "resume on recovery" are our reading (entry 38, Interpretation and Open).
- Entry 27 agrees. Its stale-timeout choice stays OPEN. Correction to its comparison, recorded as a new entry and not by rewriting 27: it leaves out two facts. The MQTT/REST source already uses iSensorMaxAgeS (default 21600 s, SATtypes.h:537). A TR= fed Tr never goes stale (SATcontrol.ino:1435-1443).
- Entry 26 agrees. A lost room temperature is not a fault, and SAT stays on.
- Entry 29: entry 38 is an explicit exception. thermo-nova keeps the frozen P; he chose curve only.
- Entry 28 agrees for the recovery path (I only inside the deadband, D frozen there). Resetting I and raw D during the loss follows the thermo-nova stale path, pid.py:134-137. Whether to reset or freeze is the one open question to him (see George).
- Entry 11 applies. The solar offset is still subtracted from the curve-only requested setpoint (:5143-5145). The solar state can stay latched during the loss, see openPoints.
- Entries 7 and 12 agree. Curve at or below the cold setpoint gives CS=10, CH=0 and PWM off (:5211-5216).
- Entries 42 (60 s PID timer) and 44 (CS/MM/CH on every update) are ordering dependencies. The room-validity decision lives in one helper next to the PID call.
- Entry 39 (TASK-1365) is separate. The PIC-fail trip (:5240-5248) is not touched.

== 2. thermo-nova, in time order ==
- The PID runs on its own 60 s timer: climate.py:712-716, PID_UPDATE_INTERVAL = 60 at pid.py:20.
- control_pid (climate.py:466-502) does this:
  1. If sensor_max_value_age > 0 (default "06:00:00", const.py:170) and the inside sensor state is older than that by last_updated (is_state_stale, helpers.py:64-69): pid.reset() and return (climate.py:470-478). reset() zeroes only _integral and _raw_derivative (pid.py:134-137). _last_error and _last_temperature stay.
  2. Otherwise, when temperature_state is None (climate.py:295-306): return with no reset (climate.py:480-482). This covers a sensor that is missing, unknown or unavailable (current_temperature, climate.py:271-279, via _get_entity_state_float, :669-686). I and D stay frozen.
  3. Otherwise pid.update (climate.py:499-502). Solar-gain update and error-history recording happen only in this branch (:485-497).
- The heating loop runs every 5 s (climate.py:706-710) with requested_setpoint = pid.output (climate.py:370-375).
  - pid.output = curve + P + I + D (pid.py:127-132), with P = kp * _last_error (pid.py:96-101).
  - The loop returns early only when the outside temperature is None or HVAC is OFF (climate.py:544-545).
- So on a stale sensor thermo-nova sends curve + frozen P, with I = D = 0. On the None path it sends curve + frozen P + frozen I + frozen D. P is 0 only when _last_error is None (pid.py:98-99), and _last_error is restored at start-up (pid.py:150). A curve-only path: not found in the code.
- Recovery: a new sensor state only refreshes the curve (_async_entity_changed, climate.py:767-773). The PID resumes at the next 60 s control_pid run with no extra reset.
  - The first update recomputes D from the whole gap: delta_time = last_changed minus the old _last_derivative_updated (pid.py:209-232).

== 3. Firmware today (HEAD) ==
satGetRoomTemp() (SATcontrol.ino:1390-1478) checks the sources in this order:
1. Simulation; the sensor_dropout window gives NaN (:1393-1401).
2. BLE: stale 5 min after the last reading (SAT_STALE_TEMP_BLE_MS :140, :1406-1413). A new reading sets it valid again (SATble.ino:895-896). The slot itself also times out after 5 min (SATble.ino:42).
3. Multi-area average: an area is stale after 5 min (:162, :1903-1909). NaN when no area is valid (:1915).
4. MQTT/REST external: stale after iSensorMaxAgeS (default 21600 s, SATtypes.h:537; range 60-86400, settingStuff.ino:1103; check at :1421-1428; reset by satHandleExternalTemp :1691-1694, which accepts -50..100).
5. Thermostat Tr: stale 5 min after the last thermostat report (:141, :1440-1443; stamp set at OTGW-Core.ino:4487-4491). A Tr that only came from a TR= feed never goes stale.
6. Thermal estimate: only when bFallbackActive (MQTT-loss takeover, set only at :4733) and Tr is a real number outside -10..50 (:1447). A NaN Tr fails that comparison, so a missing Tr ends at `return NAN` (:1475). This estimate path is practically unreachable.

Control tick (timerSATControl, iControlInterval default 30 s):
- Invalid means NaN or outside -10..50 (:4900).
- Each invalid tick increments _sat_consecutiveSkips and returns before the EMA, the curve and any CS/MM/CH (:4900-4914).
- PIC boards: satPicCSKeepalive (:4839, :486-496) re-sends the last CS every 30 s, so the last setpoint is held.
- OT-Direct: the last CS is held because OT-Direct skips its own CS expiry while SAT owns MsgID 1 (comment at :482-484).
- At SAT_MAX_SKIP_COUNT = 10 (:143), about 5 min at 30 s: bSafetyTripped, narrate, satDisable() (:4907-4912).
  - satDisable() resets the PID, saves zeros and sends CS=0 (:2382-2391).
  - On PIC boards the thermostat then takes over.
  - On OT-Direct there is no thermostat. The boiler just loses SAT's demand.
- Every later pass returns at :4812 until an explicit enable runs satClearSafetyTrip (:2139-2146) or a reboot runs initSAT (:3915-3916). SAT does not resume by itself.

== 4. Proposed behaviour, in time order ==
- t0: a source stops reporting. The per-source stale limits stay as they are; entry 27 keeps that question open.
- Each control tick while satGetRoomTemp() is invalid (roomValid = false):
  1. Skip the room EMA (:4925-4944), so NaN never enters _roomFiltered. On recovery the EMA catches up through its time-based alpha. After a 5 min gap alpha = 300/345 ≈ 0.87, so it does not fully snap back. This matches HEAD after a skip window.
  2. NEW, a decision: blocks that HEAD never reached with an invalid room temperature.
     - MQTT-takeover thermal block (:4950-4977). With bFallbackActive and no valid room, iLastKnownRoomMs stops moving (it is updated only on a valid room, :4333-4334).
       - Before 2 h it widens settings.sat.fDeadband. That widened deadband reaches the zone PIDs (satZonePidStep reads fDeadband, :2041).
       - After 2 h it sends a fixed 45 °C with CH=1 and returns. That overrides curve-only and conflicts with entry 38.
       - Proposal: skip this block when !roomValid, so curve-only wins. Do this only after Sergeant D's yes; until then it is listed as a conflict. The alternative is to keep 45 °C after 2 h as an extra safety cap.
     - PV boost (satUpdatePvBoost; ceiling check at :1778-1779 is `!isnan(roomTemp) && ...`). With NaN the indoor ceiling can no longer block a boost, so the boost raises effectiveTarget and the curve without any room reading. Proposal: no new boost activation while !roomValid. An active boost runs out on its own timers. This is our call (not a control-law change) and is mentioned in the notice.
     - Auto-tune (satAutoTuneUpdate :5025, body :4608-4650). It would retune gains from cycles run in curve-only mode. Proposal: skip while !roomValid, the same as the error statistics.
  3. Boiler-fault check (:4946-4949), window timer, pressure, solar gain (NaN-safe at :4449-4453, keeps its last rise rate), summer simmer and valves closed run as before.
  4. Compute curveValue as today (:5068).
  5. Replace `pidOutput = satPidUpdate(...)` (:5072) with `pidOutput = satPidCurveOnly(curveValue)`, a new function in SATpid.ino:
     - _pid_integral = 0 and _pid_rawDerivative = 0 (the thermo-nova stale reset, pid.py:134-137);
     - state P, I, D and fRawDerivative = 0, fPidOutput = curveValue;
     - _pid_lastError = NAN, so the unchanged-error gate (SATpid.ino:286) cannot hand back the curve-only output after recovery;
     - state.sat.fError: hold the last value or set NAN, a decision for the implementation. It is published and persisted by satSavePidState every 5 min (:2193-2203, `%.2f`). NAN would be written as "nan" and comes back through atof. The JSON writer emits null for NaN (jsonEmit.h:147). How the MQTT publishIfChangedF handles NaN was not checked (its body was not opened). Recommendation: hold the last value, and add a published curve-only flag (openPoints);
     - keep _pid_initialized, _pid_lastRoomTemp, _pid_roomTempSeen and the derivative stamps;
     - it must NOT call satPidReset(). That function zeroes fPidOutput and clears _pid_initialized (SATpid.ino:94-114). The first tick after recovery would then return 0 (:267-283), which becomes CS=10 and CH=0 for a tick (firmware-sat-spec.md:124 documents this at boot).
  6. Everything after :5072 runs unchanged on pidOutput = curve:
     - multi-zone override (:5079-5132): a zone with its own valid temperature (its own timeout, :2015-2017) still replaces the output with zone curve + P + I;
     - solar offset (:5143-5145);
     - clamp (:5152-5154);
     - continuous at system max (:5159-5162);
     - PWM or continuous (:5169-5178);
     - auto-switch (:5190-5192);
     - MM (:5199-5206);
     - cold cutoff (:5211-5216);
     - CS, MM and CH through satChEnableDecide (:5219-5225). CH=1 above the cold setpoint. This applies on both boards.
  7. Guard with `if (roomValid)`: satUpdateCurveRecommendation (:5028; it reads fError at :3730) and satHCRAddSample (:5031; SATcycles.ino:1211). This mirrors thermo-nova, which records error history only in the update branch (climate.py:496-497).
  8. Narrate and log once on entry ("room temperature lost: heating on the curve only") and once on exit.
- First tick with a valid value again:
  - The EMA restarts (step 1).
  - satPidUpdate runs a normal update:
    - the 10 s gate passes, because _pid_lastUpdateMs is from before the loss;
    - the unchanged-error gate cannot hold, because _pid_lastError is NAN;
    - P comes from the new error;
    - I follows the deadband rule starting from 0 (SATpid.ino:159-176);
    - D is recomputed at once from the temperature change over the whole gap (SATpid.ino:200-240, alpha ≈ 1 for a long dt). This is the same as thermo-nova, but D does not "start from 0": it can be large at once (Kd = 588·Kp).
  - No trip state is involved.
- Remove SAT_MAX_SKIP_COUNT (:143), _sat_consecutiveSkips (:415, :2144, :3916), the "will disengage" log (:4903) and the block comment (:4895-4899). The PIC-fail trip (:5240-5248) stays (TASK-1365).

Boards:
- PIC with a thermostat: Tr from the thermostat goes stale after 5 min. SAT then sends fresh curve-only CS and CH on each tick instead of holding the last CS and then sending CS=0.
- OT-Direct: Tr stays NaN unless it is fed with TR=. A unit with no BLE, area or external sensor used to trip after about 5 min. It will now heat on the curve with no time limit. Entry 38 says "lost or missing", so this is covered, but say so in the notice.

Implementation shape: one helper decides roomValid and selects satPidUpdate or satPidCurveOnly. It sits where thermo-nova's control_pid makes the same choice, so the entry 42 timer can call it later.

== 5. Files ==
- SATcontrol.ino:
  - the room block (:4894-4915) becomes a roomValid flag;
  - EMA guard;
  - :5072 select;
  - guards at :5025, :5028 and :5031;
  - the thermal-block guard (after the decision);
  - PV-boost activation guard;
  - the skip counter is removed.
- SATpid.ino: satPidCurveOnly().
- Optional: a small header (like SATchEnable.h) so the host test compiles the real decision.
- Docs, in the same commit:
  - docs/sat/firmware-sat-spec.md :268 (Phase E step 4), :499, :586, plus a per-source stale table (the AC "stale-time rule documented");
  - docs/sat/sat-comparison.md :81, :107, :175, :176, :263, :264, :329, :332 (line 30 is a heading, not a row);
  - docs/c4/c4-code-sat.md:21;
  - docs/manuals/en/ch05-sat-thermostat.md:219;
  - docs/manuals/nl/h05-sat-thermostaat.md:246;
  - docs/MANUAL.md:1098;
  - docs/adr/ADR-199 (Proposed):110;
  - docs/adr/ADR-197 (Proposed):74, which lists the room trip as a satDisable caller.
- ADR-070:80 is Accepted. It names _sat_consecutiveSkips only as an example. Leave it; Accepted ADRs are immutable, and this is not worth a superseding ADR. Mention it in the final summary.
- docs/sat/sergeant-d-corrections.md: a new entry recording the entry-27 correction, the entry-29 exception and his answer to the I/D question. Old entries are not rewritten.
- Also: a prerelease bump (bin/bump-prerelease.sh), `graphify update src`, and the notice to Sergeant D.

== 6. Verification ==
A. Host test tests/test_sat_room_loss.py, following the tests/test_sat_mqtt_fallback.py pattern:
  - real code cut from the .ino;
  - compile helper from tests/test_sat_pwm_override.py;
  - OTGW_SAT_INO pointed at a `git show HEAD:` copy for the old-vs-fix run.
  Modes:
  - HEAD mode wraps the inline block from "// Validate room temp" to "_sat_consecutiveSkips = 0;".
  - Fix mode compiles the real satPidUpdate, satPidCurveOnly, _pidUpdateIntegral, _pidUpdateDerivative and _pidCalculateGains, plus the decision helper.
  Stubs: radiators, auto gains, target 21.0, curve 45.0, deadband 0.1, a fake millis().
  Scenario:
  1. Three valid ticks, 30 s apart, with room 20.0.
  2. Twelve ticks with room NaN.
  3. Two valid ticks with room 20.5.
  Expected on HEAD (red): bSafetyTripped=1 on tick 10 of the NaN run, satDisable once, no output equals 45.0.
  Expected on the fix (green):
  - every NaN tick gives 45.0 ±0.05, P = I = D = 0, no trip, no satDisable;
  - the first valid tick gives 45.0 + Kp·0.5 + D, where D is the expected gap value computed in the test (not assumed 0);
  - one NaN tick, then valid again within 60 s with the same error, updates the PID and does not stay at 45.0;
  - room 60.0 behaves like NaN;
  - with bFallbackActive, a NaN room for more than 2 h keeps the curve and not 45 °C (once the decision is made).
B. Bench, old vs fix: the simulation sensor_dropout event (SATcontrol.ino:1396-1398, :4283-4293), 10 min, satsimulation on, recorded with capture-mqtt-debug.
  - HEAD: "SAFETY TRIPPED" after about 5 min, and sat/ topics stop.
  - Fix: sat/pid_p, i and d are 0, sat/setpoint follows sat/heating_curve, no trip, and P returns after the dropout.
  - Run it on the combo image on both an OT-Direct and a PIC (Classic-S3) unit if one is available.
C. Gates:
  - build.bat (combo);
  - `python evaluate.py --quick`;
  - the existing SAT host tests: tests/test_sat_mqtt_fallback.py, test_sat_pwm_override.py, test_sat_published_curve.py, test_sat_cmd_cache.py, test_sat_mqtt_state.py, test_sat_ground_truth_block.py, plus tests/test_sat_ch_enable.cpp compiled and run (it is C++, not a python script).
Not verifiable here: real BLE or thermostat loss on a live boiler (the bench uses the simulation dropout), and the 2 h MQTT-takeover case on hardware.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-25: plan (second-read) stored; waiting for Sergeant D's OK on the open question (open-questions Q29/Q30/Q31) before implementation, per CLAUDE.md SAT rules.

2026-10-25 03:56 UTC Sergeant D answered Q29 (entry 46): reset P, I and D to 0 while the room temperature is missing; curve only. Plan open point settled; ready to implement.

2026-10-25 evidence. Host: tests/test_sat_room_loss.py (real SATpid.ino + SATcontrol.ino code cut) 12/12 OK on fix; HEAD FAILED (failures=9, errors=1): trip after 10 invalid ticks, no curve-only output, I not restarting from 0. Other SAT tests OK (published_curve, mqtt_state, pwm_override 70, push_refused, mqtt_fallback, follow_thermostat, cmd_cache, otd_ch_override); evaluate 0 failed; build SUCCESS 05:57. Bench OTGW32 (OT-Direct, no room sensor): old alpha.495 was latched safety_tripped=true; fix alpha.499+ffff5b8, satsimulation on, sensor_dropout 420 s from 05:05:21 UTC: every poll 05:06-05:12 active=true, safety_tripped=false, room_temp_lost=true, pid_output == heating_curve (22.52 -> 20.80), P=I=D=0; recovery 05:13:01 room_temp_lost=false, P 1.84, I 0, D 1.78 (gap derivative, documented as thermo-nova). Opus review: no blocker; M1-M3/L3/L4/L6 fixed (room_temp_lost state REST/MQTT/UI, flag cleared on disable, solar guard for out-of-range, docs). Stale-time per source stays open (entry 27). Bench note: the filesystem OTA from alpha.495 left the unit in safeboot (main app did not return); recovered by uploading the app in safeboot - filed separately.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT no longer switches itself off when the room temperature is lost or missing (Sergeant D entries 38 and 46): it heats on the heating curve only, with P, I and D reset to 0, and the PID resumes when a valid room temperature returns (I from 0, first D over the gap as thermo-nova). New state room_temp_lost (REST sat/status, MQTT sat/room_temp_lost, SAT page hint). Skipped during the loss: room EMA, auto-tune, curve recommendation, PV boost start, error statistics, and the MQTT-takeover 2 h safe setpoint (entries 38/46 outrank it). Evidence: host test 12/12, red on HEAD; bench 7 min sensor dropout without trip, output = curve, P/I/D 0, clean recovery. Open: stale time per source (entry 27), solar gain state during a loss.
<!-- SECTION:FINAL_SUMMARY:END -->
