---
id: TASK-1365
title: >-
  SAT: on lost OpenTherm hand the boiler back once (CS=0, CH=1, MM=T), then send
  nothing (entries 39, 47)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-24T21:41:00Z'
updated_date: '2026-10-25T20:02:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 500000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-24 (entry 39): if the software has no valid OT, SAT stops: CH=0, CS=0 once, then no more messages to the boiler. Compare the firmware OT-offline handling (entry 39 cites it).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 On OT loss SAT sends CS=0, CH=1, MM=T exactly once (entry 47), then nothing to the boiler while the loss lasts (host test red on old, green on fix)
- [ ] #2 What counts as lost (30 s, Data-Invalid) and whether SAT resumes by itself are answered by Sergeant D (Q33, Q34) and recorded before commit
- [x] #3 No false hand-back on PIC PS=1 enter/leave, a late first loop pass, or millis wrap (host tests)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
TASK-1365: corrected plan for Sergeant D entry 39 ("no valid OT: CH=0, CS=0 once, then stop sending to the boiler"). Second-reader review. Read-only: no files were edited.

Verdict: most of the plan holds. I opened every SAT/OTDirect/OTGW-Core line it cites and every thermo-nova citation, and they match. Two design errors block it as written (A and B below). The question to George also starts from a wrong premise (C).

A. BLOCKING. The detection signal causes a false stop at NTP sync.
- bBoilerState is computed as time(nullptr) < tBoilerLastSeen + 30 (OTGW-Core.ino:5150).
- The comment at OTGW-Core.ino:5145-5148 says time() counts up from 0 until NTP syncs.
- B frames start within about a second of boot, so SAT arms before NTP syncs. When NTP sets the clock, time() jumps forward about 1.7e9 s.
- The next 3 s liveness Tick (doTaskEvery3s, OTGW-firmware.ino:821) then computes bBoilerState = false. It stays false until the next B frame arrives. A grep of src/OTGW-firmware found no hook that re-stamps tBoilerLastSeen on a time sync.
- satControlLoopRun runs on every loop pass, so it would see that transient false. It would send STOP (CH=0, CS=0), then RESUME. That is a spurious boiler stop on a large share of boots, and on any later clock step.
- otRealBoilerSeenRecently() (OTGW-Core.ino:5182-5184) has the same flaw.
- Fix: decide on a millis()-based loss duration, not on the wall-clock flag.
  - Option 1, preferred: add a uint32_t tBoilerLastSeenMs to OTBusState (OTBustypes.h, next to tBoilerLastSeen, :34). Stamp it beside tBoilerLastSeen in processOT (OTGW-Core.ino:5243). SAT then decides "lost" as (now - tBoilerLastSeenMs) >= 30000.
  - Option 2: SAT keeps its own millis "silent since" stamp and stops only when bBoilerState has been false continuously for more than one Tick (> 3 s).
- Either way the host test gets a case "clock jump while alive: no STOP".

B. BLOCKING. The latched CH=0 is not harmless on the PIC, and it conflicts with entry 26.
- PIC MsgID 0 (gateway.asm:2711-2723): bit0 (CH enable) is overridden with NOT CHModeOff in two cases:
  - a user CS is active (SysCtrlSetpoint clear);
  - HeatDemand is set, meaning an on/off thermostat is short-circuiting the line (gateway.asm:1763-1764).
- In the on/off case the PIC also writes TSet = max (gateway.asm:2799-2812).
- So on a Classic with an on/off thermostat, SAT's latched CH=0 still holds after CS=0, and it keeps the thermostat from heating. Entry 26 says the thermostat must remain the fallback, and avoiding exactly this is why satDisable() sends CH=1 (TASK-1346, SATcontrol.ino:2392-2396).
- With auto-resume this is bounded: SAT re-sends CH when the boiler returns. With "stay stopped until re-enabled" it blocks heating.
- This goes to George as part of the question (see georgeSummary). It is not "harmless".

C. CORRECTION. CH=0 before CS=0 does not "make CH=0 count".
- On both boards CH= acts on the bus only while a CS override is active:
  - PIC: SysCtrlSetpoint test at gateway.asm:2721-2723. CS=0 runs ClrCtrlSetPoint at :4199.
  - OT-Direct: otdEffectiveMasterFlags, OTDirect.ino:771-775. CS=0 runs clearWriteOverride(1) at :2783.
- So in either order, CH=0's bus effect ends when CS=0 lands. Sending CH first buys at most one MsgID 0 frame with CH off. During an outage the boiler is not answering anyway.
- What lasts is the latch:
  - PIC: CHModeOff, which matters with an on/off thermostat (B).
  - OT-Direct: otChModeOff, which matters for a later CS override.
- On OT-Direct the result is the opposite of "CH off":
  - After CS=0, clearWriteOverride(1) also sets the MsgID 1 write entry's valueSet = false (OTDirect.ino:2307-2314), so the gateway stops writing TSet.
  - bit0 falls back to otMasterStatusFlags, whose default is 0x01, CH on (OTDirect.ino:62, :2471).
  - So in master mode with no thermostat, after the stop the gateway asks the boiler for CH with no TSet written. Which setpoint the boiler then uses is boiler-dependent and not verified.
  - The original plan's claim "master mode leaves the house unheated" is not supported by the code. It is withdrawn.
- Keep the order CH=0 then CS=0, because it is his literal order and costs nothing. Do not justify it as necessary.

What holds, re-verified this run (time order of today's code):
1. thermo-nova (other-projects/SAT-releases-thermo-nova/custom_components/sat, pinned 7beaaf5; the clone HEAD is snapshot commit c5211afe, so the pin was not git-verified):
   - async_control_heating_loop returns only when the outside temperature is None or HVAC is OFF (climate.py:544-545).
   - Otherwise update() always ends with CS, MM and CH (heating_control.py:236-238).
   - The MQTT coordinator publishes CS= then PM=25 (coordinator/mqtt/opentherm.py:195-197), CH=1|0 (:211-212) and MM=, with TP= first for Immergas (:216-220).
   - _publish_command has no deduplication and no link check (coordinator/mqtt/__init__.py:134-149).
   - MAX_BOILER_TEMPERATURE_AGE = 60 (const.py:40) is used nowhere (grep).
   - The serial sensors' `available` (coordinator/serial/sensor.py:164-166) only affects entity display; it is not a control gate.
   - The only CS=0 is _graceful_disconnect, which sends CS 0 and MM "-" and no CH (coordinator/serial/__init__.py:228-231, called from :189). It runs on entity removal and on EVENT_HOMEASSISTANT_STOP (climate.py:150, :152-162).
   - A stop on link loss is not found in the code. Entry 39 is his own decision and an exception to entry 29.
2. Firmware, liveness:
   - processOT stamps tBoilerLastSeen for every B frame, loopback and replay included (OTGW-Core.ino:5243).
   - tRealBoilerLastSeen is stamped only when the frame is neither a local answer nor replayed (:5251-5252).
   - evaluateOTBusLiveness runs on every frame and on a 3 s Tick (OTGW-firmware.ino:821). bBoilerState drops 30 s after the last B frame (OTGW-Core.ino:5150-5153).
   - bOnline = boiler OR thermostat (:5166). On a Tick it can only be cleared (:5167-5171).
   - On OT-Direct one MsgID 0 non-reply clears bOnline at once (OTDirect.ino:1514-1523). A Data-Invalid reply is bridged as 'B' (OTDirect.ino:1510-1512). OT-Direct tags only 'A' frames, and 'B' frames in loopback, as local (OTDirect.ino:752).
   - state.pic.bAvailable is set at OTGW-Core.ino:824, :1050 (find(ETX) after a reset) and :5636, and never cleared at runtime.
   - 0x26 is the external I2C watchdog (OTGW-Core.ino:62-65). The task text does not mention 0x26; drop the original plan's sentence that claimed it did.
3. Firmware, SAT today (SATcontrol.ino), in pass order:
   - satMqttFallbackStep (call at :4794; lift with satDisable at :4745-4746).
   - The TASK-565 bOnline edge cache reset (:4785-4792).
   - The enable gate, which calls satDisable only if bActive (:4799-4803).
   - Simulation and thermal learning (:4807-4809).
   - The trip return (:4812).
   - The deferred boot CS=0 (:4815-4819).
   - The PIC keepalive (:4839; function at :487-496, returns at :488 when the cache is invalid).
   - The PWM suppression refresh (:4842; it sends through satEnqueueIfChangedCS at :1065).
   - The control tick (DUE at :4845), which sets bActive and CONTINUOUS (:4847-4849). Inside the tick:
     - SW= on DHW entry (:4877);
     - CS/MM/CH in the thermal-safe, summer-simmer and valves-closed blocks (:4971-4973, :5001-5003, :5015-5017);
     - the main block CS/MM/CH (:5221-5225), TP= (:5231) and TC (:5235).
   - The interface-loss trip after 5 ticks (:5240-5248) sets bSafetyTripped and calls satDisable, which sends CS=0, CH=1 and MM=T (:2391-2401).
   - initSAT sends CS=0 at boot (:3929-3932) and MI=500 (:3952).
   - The external CS= refusal while SAT owns the setpoint applies on PIC (OTGW-Core.ino:3738-3742) and on OT-Direct (OTDirect.ino:2771-2775). The OT-Direct heating curve is gated off (OTDirect.ino:2212-2225) and the CS expiry is skipped (:2170-2175).
   - Every SAT sender listed above sits after :4812 except the boot CS=0 sends (:3930, :4817). A hook placed right after :4812 therefore silences all of them. No other SAT sender exists (grep of addCommandToQueue/satEnqueue* in SAT*.ino).

Corrected design:
- Pure decision function between begin/end markers in SATtypes.h, following the satLastCycleExpired pattern (SATtypes.h:429-436):
  - satOtLinkDecide(armed, lostMs, simulation, stopped) -> {NONE, ARM, STOP, RESUME}.
  - lostMs is millis-based (fix A).
  - Arm on the first live reading after SAT starts regulating; no stop before that (boot).
  - simulation (settings.sat.bSimulation) is exempt.
  - The signal counts loopback and replay B frames, as bBoilerState does. That keeps the benches running. On a real installation it is identical to "real boiler".
- Transient flags in state.sat (SATtypes.h, near bSafetyTripped :234): bOtLinkArmed and bOtLinkStopped.
- Split satDisable (:2369-2409):
  - satStopRegulation(): the state teardown at :2373-2386.
  - The existing command block, unchanged for satDisable.
  - satDisable also clears bOtLinkStopped and bOtLinkArmed.
- satOtLinkStop(), in this order:
  1. satStopRegulation()
  2. addCommandToQueue("CH=0")
  3. satEnqueueOwnCS("CS=0")
  4. satResetCmdCache()
  5. bOtLinkStopped = true
  6. narrate the stop
  - Whether to add MM=T, and CH=1 versus CH=0, waits on George (B, entry 21, TASK-1344).
- Hook right after the trip return at :4812, before the deferred boot CS=0 at :4815. While bOtLinkStopped is set, every pass returns there.
- Extend the enable gate at :4800 to (bActive || bOtLinkStopped), so a user switch-off still sends the satDisable hand-back.
- satMqttFallbackStep's lift (:4745-4746) can send CS=0/CH=1/MM=T during an outage. Treat it like a user switch-off; this is listed as an open point.
- Resume: recommended automatic, on the first B frame (the frame trigger sets bBoilerState at once, OTGW-Core.ino:5172-5174). The next tick re-sends CS/MM/CH with a reset cache plus TC. This is not settled by entry 39; it goes to George.
- Entry 44's 5 s cadence (still to be filed) must sit behind the same gate.

Timeline after the fix:
- t0: last B frame.
- t0+30 s: lostMs reaches 30000. The next satControlLoopRun pass sends CH=0, then CS=0, once, and returns.
- From then on there is no SAT traffic: no keepalive, suppression refresh, CS/MM/CH, TC, SW= or TP=.
- On OT-Direct the gateway's own MsgID 0 offline probe continues (OTDirect.ino:1559-1578), so recovery can be seen. That probe is gateway traffic, not SAT.
- On a PIC without a thermostat the PIC sends its own standalone messages (gateway.asm:1791-1800), so B frames return too.
- First B frame: RESUME. Fix A guarantees that a clock jump alone never gives STOP.

Files:
- SATtypes.h: decision function, enum, flags.
- OTBustypes.h and OTGW-Core.ino: the millis stamp (fix A, option 1).
- SATcontrol.ino: split, stop function, hook, gate.
- tests/test_sat_ot_link_stop.py, new. It defines its own OTGW_SATTYPES_H override; only OTGW_SAT_INO exists today (test_sat_pwm_override.py:37). It reuses compile_cpp (test_sat_pwm_override.py:340).
- docs/sat/firmware-sat-spec.md and docs/c4/c4-code-sat.md.
- docs/sat/sergeant-d-corrections.md: his answer, recorded before implementing; entry 39 listed as an exception to entry 29.
- bin/bump-prerelease.sh.

Verification:
- Host test on the real cut-out function, in this sequence:
  1. Boot not armed: NONE.
  2. Alive: ARM.
  3. lost < 30 s: NONE.
  4. lost >= 30 s: STOP exactly once.
  5. Further ticks: NONE.
  6. Alive: RESUME.
  7. Simulation: NONE.
  8. Clock jump while alive: NONE (fix A).
- Source-order checks:
  - CH=0 comes before CS=0 in satOtLinkStop.
  - The hook's return precedes :4815, :4839, :4842 and :4845.
  - The gate includes bOtLinkStopped.
  - tests/test_otd_ch_override.py stays green: satDisable keeps CH=1 (:126).
- Red on HEAD: the function is absent, which is a weak red. Say so.
- Bench, OTGW32 OT-Direct: GW=L, then leave loopback or pull the wire. Capture 10 min of telnet. HEAD keeps sending; the fix sends exactly CH=0, CS=0, then nothing.
- Plus one reboot with NTP on and the boiler live: no stop at NTP sync.
- Not verifiable here: a PIC Classic with a dead boiler and a live thermostat, and an on/off thermostat with the latched CH=0 (Classic-S3 bench).

Sergeant D entries:
- 39: implemented; questions open (see georgeSummary).
- 26: conflict with the latched CH=0 on a PIC with an on/off thermostat (B). Agrees with CS=0 handing back to an OpenTherm thermostat.
- 29: conflict, recorded as an explicit exception (thermo-nova has no link stop).
- 21 and TASK-1344: MM=0 in PWM stays latched without MM=T.
- 32: agrees (CH=0 on idle). It is a different case (valves closed gives CS=10).
- 23: the 3 s refresh is silenced while stopped. This is consistent because PWM is reset.
- 27, 38: a lost room temperature keeps heating; a lost OT link stops SAT. No conflict.
- 28: PID reset on the stop versus keeping the integral is open.
- 7 and 44: the CH cadence changes to every update. The new sender must respect the stop.
- TASK-1346 is reconciled by the two stop paths.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-25: plan (second-read) stored; waiting for Sergeant D's OK on the open question (open-questions Q29/Q30/Q31) before implementation, per CLAUDE.md SAT rules.

2026-10-25 04:33 UTC Sergeant D answered Q30 (entry 47): option B, final message CS=0, CH=1, MM=T (same as satDisable), then nothing. Remaining plan points (no-valid-OT = no boiler reply for 30 s; automatic resume) are our design, stated in the plan; ready to implement.

2026-10-25: two workflows ran on this task (orphaned wf_73ecbf70 from before /clear wrote the code; wf_4c6959ba reviewed it). Review findings A (PS=1 exemption PIC-only, hold not resume), D (boot baseline seed), F (wrap-proof stamp resume) are in the tree; E documented; G (MsgID 1 ownership while lost) and H (satDisable wipes PID/HCR) go into Q34. Blocking: Q33/Q34 to Sergeant D; test_sat_ot_loss.py needs the psleave/pslost/otdps/late/wrap cases.

2026-10-25 Robert decided (interactive): commit provisionally now, mark the behaviour provisional in docs/task, keep the task In Progress until Q33/Q34 are answered; a different answer from Sergeant D becomes a follow-up change. Verification this session: test_sat_ot_loss 21/21 OK on fix, HEAD FAILED (failures=10, errors=2); all tests/test_sat_*.py + test_otd_ch_override OK; evaluate --quick exit 0, 0 failed.

2026-10-25 bench alpha.501+6948ad5 on OTGW32 (OT-Direct, no boiler, SAT on, simulation off), app then fs over OTA, 177 settings kept. (1) Boot without boiler: ot_link_lost true ~30 s after SAT's first pass (09:55:11), 'OpenTherm lost: boiler handed back', OT-Direct write overrides empty afterwards (CS and MM released), 0 OTD commands for the next 2 min. (2) OT replay start (POST /api/v2/simulate/start): 09:59:01 'OpenTherm back: SAT resumes', CS=23.0/MM=100/CH=1 re-sent. (3) Replay stop ~09:59:48: 10:00:18 (30 s later) CS=0, CH=1, MM=T once, then 0 OTD commands until capture end 10:01:48. Logs in the session scratchpad (telnet501.txt, telnet501b.txt). AC#2 open: Q33 asked, Q34 queued.

2026-10-25 correction: the bench unit at 192.168.88.61 (MAC 10:20:BA:21:B4:F8) used for this task's evidence was NOT the OTGW32 (Robert: OTGW32 not connected in the 48 h before 2026-10-25 evening). It was an ESP32-S3 without PIC and without boiler running the combo image in OT-Direct mode (ADR-160 bench default). The evidence stands as 'combo image, OT-Direct mode, no boiler'; any 'OTGW32' wording in earlier notes is wrong.
<!-- SECTION:NOTES:END -->
