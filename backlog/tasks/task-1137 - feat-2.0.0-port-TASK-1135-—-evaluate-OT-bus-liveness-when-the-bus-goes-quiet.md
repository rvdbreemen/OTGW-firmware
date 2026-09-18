---
id: TASK-1137
title: 'feat-2.0.0: port TASK-1135 — evaluate OT-bus liveness when the bus goes quiet'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-18 04:54'
updated_date: '2026-09-18 05:17'
labels:
  - bug
dependencies: []
priority: medium
ordinal: 281000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of TASK-1135 from the 1.x line (otgw-1.x.x commit 020e8198). The 2.0.0 tree carries the same defect with the same shape.

OTGW-Core.ino:4828 computes state.otBus.bBoilerState = (now < (state.otBus.tBoilerLastSeen+30)) inside the OT-message processing path, with the thermostat equivalent just below and the on-change publish guard after it. If the source stops delivering frames entirely, that code never runs, so the 30 second liveness timeout cannot fire in the one case it exists for. The presence flags keep their last value until reboot.

Two things make this heavier here than on 1.x, and they are why this is a port and not a copy:

1. There is a second source. Besides the PIC path there is OTDirect, with its own topics (otgw-otdirect/boiler_connected, otgw-otdirect/thermostat_connected). Whatever drives tBoilerLastSeen on that path needs the same treatment, and the port must establish whether one evaluation covers both sources or each needs its own.

2. A stale flag feeds a control decision, not just a display. SATcontrol.ino:1171 does if (state.otBus.bBoilerState) return true;. On 1.x a stale flag only misleads Home Assistant; here it can keep SAT believing a boiler is present that is not. Check what that call site concludes and whether the fix changes its behaviour in a way that needs its own note.

Also differs: presence values publish under the generic namespace per ADR-084, not the otgw-pic subtree, and the state lives in state.otBus rather than state.otgw.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The liveness timeout is evaluated on a periodic tick independent of frame arrival, for every source that feeds the presence flags
- [x] #2 Evaluation is suppressed while a PIC or ESP flash is in progress so an update does not flap the entities
- [x] #3 The OTDirect path is explicitly covered or explicitly ruled out, with the reason recorded
- [x] #4 The effect on SATcontrol.ino:1171 is assessed and recorded: does a correctly-falling bBoilerState change any SAT decision, and is that change wanted
- [x] #5 Any coupled publishes on a thermostat transition (hvac mode/action equivalents) are preserved
- [x] #6 Build green for the relevant target and evaluate.py --quick shows no new failures
- [ ] #7 Hardware verification by the maintainer with the source absent
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read 1.x reference commit 020e8198, then verify every structural assumption against the 2.0.0 tree instead of copying.
2. Extract the liveness evaluation out of processOT() into evaluateOTBusLiveness(OTBusLivenessTrigger).
3. Call it from processOT() (Frame/FirstFrame) and from doTaskEvery3s() (Tick), above the picSettingsCycleActive early return.
4. Suppress during isFlashing() (covers ESP and PIC on 2.0.0).
5. Establish whether OTDirect needs its own evaluation; record the answer.
6. Assess the SATcontrol.ino:1171 call site; record whether a correctly-falling bBoilerState changes a SAT decision and whether that is wanted.
7. Build via build.bat (verify artifact freshness + per-env SUCCESS, not exit code) and run evaluate.py --quick.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## OTDirect (AC #3) — covered by one evaluation, no separate path needed

The task description's premise is partly stale. Two findings:

1. The topics otgw-otdirect/boiler_connected and friends DO NOT EXIST on 2.0.0. MQTTstuff.ino:1731-1736 records that ADR-084 removed the hardware-specific otgw-pic/* and otgw-otdirect/* duplicates; presence publishes only to the generic OTGW/value/<uniqueId>/<label> namespace via publishBoilerConnectedState() / publishThermostatConnectedState() / publishOTGWConnectedState().

2. ONE evaluation covers both sources for the two link flags. OTDirect's bridgeFrameToParser() (OTDirect.ino:702) does not call processOT() directly; it calls enqueueOTFrame(..., OTFRAME_SRC_OTDIRECT), and drainOTFrameQueue() feeds processOT() in loop() context. So both the PIC path and OTDirect write the SAME two stamps, state.otBus.tBoilerLastSeen and tThermostatLastSeen (OTGW-Core.ino:4809/4815). Evaluating those stamps on a tick covers both sources by construction.

The OTDirect-specific part is bOnline, which has a SECOND writer. OTDirect.ino sets state.otBus.bOnline directly at ~860, ~866 (setup probe), ~1230 (loopback), ~1284 (handleMasterResponse success) and ~1394 (MsgID 0 failure). The failure path clears bOnline within one probe interval, long before the 30 s stamp expires. A tick that re-derived bOnline from (bBoilerState || bThermostatState) would undo that every 3 s for up to half a minute, flapping the MQTT entity, the OTDirect 'OTDirect: disconnected' WebSocket message and OTDirect's own busOffline scheduling decision (OTDirect.ino:1426). Hence Trigger::Tick may only LOWER bOnline, never raise it.

That rule is not a workaround, it is the correct semantics: a liveness window only ever expires. Raising a presence flag needs evidence, the evidence is a frame, and a frame runs processOT(). bBoilerState / bThermostatState need no such rule because processOT() is their only writer and their stamps cannot move forward between frames.

Also dropped the three shadow statics the 1.x patch promoted to file scope. Dedup now compares against the live state.otBus flags. For the two link flags this is behaviour-identical (single writer, so shadow and live were always equal). For bOnline it is strictly more correct: a shadow would desynchronise from OTDirect's direct writes and could suppress a publish that is owed.

Deliberately NOT under OTStateLock. Three independent single-word bools cannot tear; the lock exists so multi-field REST/webhook snapshot readers see a consistent set, and OTDirect already writes bOnline lock-free at the five sites above. Taking the non-recursive mutex on a path processOT() may already hold it on would be the novel risk.

## SATcontrol.ino:1171 (AC #4) — the change is wanted, and one existing mechanism starts working because of it

SATcontrol.ino:1167-1177 is satBoilerHardwarePresent(). It is an AVAILABILITY gate, not a heat-demand path: it answers 'is a real boiler on the bus', and a true answer BLOCKS SAT simulation. The bBoilerState read is behind #if HAS_PIC, so on a fixed OTGW32 it is not consulted at all (otDirectBoilerPresent() covers that board).

Callers: restAPI.ino:1645 (409 on enabling sim), settingStuff.ino:1050 (reject on settings write), SATcontrol.ino:4135 (teardown when sim is on and a boiler appears), SATcontrol.ino:1997 (boilerHwPresent snapshot field, which sat.js:359 mirrors as sim_available).

Effect of a correctly-falling bBoilerState: 30 s after the boiler stops answering, satBoilerHardwarePresent() returns false, so SAT simulation may be enabled again. That is exactly the documented intent. SATcontrol.ino:1188-1190 already says re-enabling needs boiler-absence again, 'reboot with no boiler, OR THE BUS LAYER CLEARS ITS PRESENCE SIGNAL'. The code anticipated this clearing; the defect was that it never happened. Today a bench rig that once saw a boiler refuses simulation until reboot.

It does not enable anything by itself: satOnBoilerDetected() is one-way and only turns sim OFF. The gate only blocks an explicit enable. A false negative (real boiler merely quiet) is self-correcting: on a live OT bus the thermostat polls about every second, so a 30 s gap means the bus really is dead, and the next real B frame trips satNotifyBoilerFrameSeen() plus the SATcontrol.ino:4135 backstop within about 1 s.

Second, larger effect, and it is an improvement: SATcontrol.ino:4141-4145 (TASK-565) resets the write-on-change command cache on an OT-bus offline->online edge, so a recovering boiler immediately receives the current CS/MM/CH/TC instead of waiting out the staleness window. On the PIC path bOnline could never fall, so that edge could NEVER FIRE. With the fix it fires exactly once per genuine recovery — the clear-only rule for Trigger::Tick guarantees bOnline is only ever raised by a real frame, so the edge cannot be retriggered by tick churn.

Conclusion: no heating behaviour changes in an unwanted direction. Two mechanisms that were dead now work as designed. No note needed in SATcontrol.ino itself; the existing comment at :1188 already states the expectation this fix satisfies.

Pre-existing and out of scope: OTDirect loopback mode bridges synthetic B frames, so on a combo board with HAS_PIC true it can raise bBoilerState from simulated traffic. otDirectBoilerPresent() explicitly excludes loopback for precisely this reason; the bBoilerState branch does not. That predates this task and this change only makes the flag fall again when the synthetic traffic stops.

## Evaluator

.build-venv python evaluate.py --quick: 76 checks, 68 passed, 1 warning, 0 FAILED, health 98.7%, exit 0.
The single warning is 'STATUS_BURST_COOLDOWN_MS bound: boards.h not found' — pre-existing and unrelated (this change touches no boards.h and no burst constant).

## Implementation

- src/OTGW-firmware/OTGW-Core.h:584-596 — OTBusLivenessTrigger enum class (FirstFrame / Frame / Tick) + evaluateOTBusLiveness() prototype. Prototyped in the header because the sketch file is concatenated first and doTaskEvery3s() calls the function defined later in OTGW-Core.ino.
- src/OTGW-firmware/OTGW-Core.ino:4760-4837 — new evaluateOTBusLiveness(). isFlashing() suppression at the top (covers both ESP and PIC on this branch, OTGW-firmware.h:828).
- src/OTGW-firmware/OTGW-Core.ino:4863-4867 — processOT() now calls it with FirstFrame on cntOTmessagesprocessed==1, else Frame. The three shadow statics and the 23-line inline block are gone.
- src/OTGW-firmware/OTGW-firmware.ino:739-751 — doTaskEvery3s() calls it with Tick, above the picSettingsCycleActive early return. The DUE(timer3s) ladder already sits inside loop()'s if (!isFlashing()) block, so the suppression is belt and braces.

No MQTTstuff.ino change: the isPICEnabled() gate the 1.x patch had to remove from the presence publishes does not exist on 2.0.0 (sendMQTTstateinformation, MQTTstuff.ino:1758-1766, gates only gateway_mode behind bGatewayModeKnown).

publishHvacMode(false) / publishHvacAction(false) are preserved on the thermostat transition (AC #5), so the HA climate entity cannot hold a stale mode when the thermostat goes away.

## Build (AC #6)

build.bat (the sanctioned wrapper, not python build.py direct), one solo run, all six PlatformIO envs green:

  esp32          SUCCESS   00:03:05.134   (firmware)   RAM 35.3%  Flash 79.3% (1974087 / 2490368)
  esp32          SUCCESS   00:00:27.497   (buildfs)
  esp32-classic  SUCCESS   00:02:23.904   (firmware)   RAM 34.4%  Flash 77.1% (1919251 / 2490368)
  esp32-classic  SUCCESS   00:00:20.490   (buildfs)
  esp32-combo    SUCCESS   00:02:18.741   (firmware)   RAM 35.8%  Flash 81.3% (2024183 / 2490368)
  esp32-combo    SUCCESS   00:00:18.283   (buildfs)

Literal 'Build completed successfully!' banner at build3.log:1047, wrapper exit 0. Not trusting the exit code alone: fresh artifacts confirmed by mtime, e.g. OTGW-firmware-esp32-combo-2.0.0-alpha.364+3c7a3b7.ino.bin at 07:15:25 and its .littlefs.bin at 07:15:45, with the esp32 and esp32-classic sets at 07:09-07:10 and 07:12-07:13. The concatenated sketch was recompiled (Compiling .pio/build/<env>/src/OTGW-firmware.ino.cpp.o in each env), so the change is genuinely in these binaries.

Note: esp32-combo fits here at 81.3%, so the known partition-overfit condition did not trigger on this build.

Zero compile errors. The only warnings in the log are pre-existing AsyncTCP deprecation notices and the LTO serial-compilation note.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ports the 1.x OT-bus liveness fix (otgw-1.x.x 020e8198) to the 2.0.0 line, adapted to this tree rather than copied.

## The defect

The 30 s window that turns 'heard recently' into 'gone' was computed inside processOT(), so it was only evaluated when a frame arrived. A source that stopped delivering left the window unevaluated forever: the timeout could not fire in the one case it exists for, and boiler_connected / thermostat_connected / otgw_connected held their last value until reboot.

## The change

evaluateOTBusLiveness(OTBusLivenessTrigger) in OTGW-Core.ino:4760-4837, called from processOT() (FirstFrame / Frame) and from doTaskEvery3s() (Tick, above the picSettingsCycleActive early return). Suppressed while isFlashing(), which on this branch covers both an ESP and a PIC flash. Enum + prototype in OTGW-Core.h:584-596, because the sketch file is concatenated first.

## Four deliberate divergences from the 1.x patch

1. No MQTTstuff.ino change. Half of the 1.x fix was removing an isPICEnabled() gate from the presence publishes; on 2.0.0 that gate is already absent, and ADR-084 already removed the otgw-pic/* and otgw-otdirect/* duplicate topics the task description still named.
2. One evaluation covers both sources. OTDirect's bridgeFrameToParser() enqueues via enqueueOTFrame() rather than calling processOT() directly, so both paths write the same two stamps. Nothing OTDirect-specific is left to evaluate.
3. Trigger::Tick may only LOWER bOnline. A liveness window only ever expires, and raising needs a frame, which runs processOT(). The rule also keeps the tick from fighting OTDirect, a second writer of bOnline that clears it on an unanswered MsgID 0 probe well before the stamp expires; re-deriving from the OR would flap the entity, the OTDirect WebSocket status line and OTDirect's busOffline scheduling for up to 30 s.
4. The three shadow statics are gone; change detection compares against the live state.otBus flags. Behaviour-identical for the two link flags (single writer), strictly more correct for bOnline, where a shadow would desynchronise from OTDirect's writes and suppress a publish that is owed.

Not under OTStateLock, deliberately: three single-word bools cannot tear, the lock exists for multi-field snapshot readers, and OTDirect already writes bOnline lock-free. Taking a non-recursive mutex on a path processOT() may already hold it on would be the novel risk.

## SAT impact (AC #4)

satBoilerHardwarePresent() (SATcontrol.ino:1167-1177) is an availability gate that blocks SAT simulation, never a heat-demand path. A correctly-falling bBoilerState lets a bench rig enable simulation again 30 s after the boiler goes away, exactly what the comment at SATcontrol.ino:1188 already anticipates. It also revives the TASK-565 offline-to-online edge at SATcontrol.ino:4141, which resets the write-on-change command cache so a recovering boiler gets the current CS/MM/CH/TC immediately; on the PIC path that edge could never fire because bOnline could never fall. Both effects are wanted.

## Verification

build.bat, one solo run, all six PlatformIO envs SUCCESS, literal 'Build completed successfully!' banner, fresh artifacts by mtime, zero compile errors. evaluate.py --quick: 76 checks, 0 failed, health 98.7% (its one warning, boards.h not found, is pre-existing and unrelated).

Committed locally as a3e9a7fd with OTGW_BUMP_HOOK_DISABLE=1. NOT pushed: the maintainer asked to review first, and a prerelease bump is still owed before this ships to testers (bin/bump-prerelease.sh rewrites ~43 files and stages them all, which is unsafe in this dirty worktree alongside other agents).

## Open

AC #7 only: confirming the entities flip with the source physically absent needs a bench device.
<!-- SECTION:FINAL_SUMMARY:END -->
