---
id: TASK-802
title: 'feat-2.0.0: SAT sim F7 — host-side boiler-emulator script'
status: To Do
assignee:
  - '@claude'
created_date: '2026-05-31 22:56'
updated_date: '2026-06-03 17:41'
labels:
  - sat
  - simulation
  - tooling
dependencies: []
ordinal: 76000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up F7 from SAT simulation plan section 12. Host-side TCP-to-OT bridge that replies to MsgID 3 + status polls, so TASK-795 availability-gate ACs (8-10) can be validated without a physical boiler on the bench. Depends on TASK-795. Milestone 2.0.0.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Single-file host script in scripts/ (stdlib only) emits synthetic slave MsgID 3 + status replies over the OTDirect TCP bridge
- [ ] #2 Running it against an OTGW32 in simulation trips the §4.2 edge hook: firmware auto-disables sim within ~1s and persists bSimulation=false
- [ ] #3 Validates TASK-795 availability-gate ACs (edge auto-disable, REST 409, MQTT reject) without a physical boiler
- [ ] #4 Host tooling only (no firmware change, no version bump); Windows/PowerShell-launchable; documented in scripts/README
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Source: plan §12 F7 + §10 step 9 (availability-gate field validation needs a boiler) + §4.2. Depends on TASK-795 (the availability gate it validates).

GOAL: host-side boiler-emulator script so TASK-795 availability-gate ACs (edge-triggered auto-disable, REST 409, MQTT reject) can be validated on the bench WITHOUT a physical boiler — a TCP->OT bridge that replies to MsgID 3 (slave MemberID) + status polls, making the firmware believe a boiler slave is present.

DESIGN:
- Host script (Python, lives in scripts/ alongside capture-mqtt-debug.bat). Connects to the OTGW32 OT-direct TCP bridge (port 25238, the OT-direct telnet/bridge seen in plan §10) OR drives the PIC serial path — pick the OTDirect TCP path first (no extra hardware, ESP32 OTGW32).
- Behaviour: on connect, periodically emit a synthetic slave response frame for MsgID 3 (so iSlaveMemberID becomes non-zero -> satBoilerHardwarePresent() true) and answer status polls. This trips the §4.2 edge hook -> satOnBoilerDetected() within <=1s.
- CLI flags: --host, --port, --member-id, --interval. Single-file, stdlib only (socket), mirror the capture-mqtt-debug.bat single-file ethos. Windows-friendly (PowerShell-launchable) since that is the dev environment.
- This is HOST tooling (scripts/**) — NOT firmware, NO version bump, commit with [no-task] or chore. Does not flash to device.

OPEN QUESTION (morning): emulate via the OTDirect TCP bridge (ESP32, no extra HW) vs a serial PIC emulator (needs a serial loopback / second device). Default: OTDirect TCP bridge first; serial emulator deferred.

VERIFY: run the emulator against an OTGW32 in bSimulation -> firmware logs 'boiler appeared, simulation off' within ~1s, bSimulation flips false + persists, REST 409 on re-enable. (This is the tool that makes TASK-795 AC#8/#9/#10 self-verifiable without hardware.)
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-06-01T09:13:23+02:00: BLOCKED — investigated, host-side emulator cannot work as the plan assumed. FINDING (verified in OTDirect.ino): otBoilerCacheValid[3] (the signal satBoilerHardwarePresent/otDirectBoilerPresent reads) is set ONLY inside handleMasterResponse() — line 1206 (loopback synthetic) and 1261 (real boiler answering on the OT GPIO pins via otMaster async). The port-25238 TCP bridge INPUT (handleOTDirectBridgeStream, line 651) routes host bytes to sendPICSerial()->handleOTDirectCommand() = PIC-style COMMANDS sent OUT toward the boiler; it is NOT a boiler-response injection point. So a TCP client on 25238 cannot make the firmware believe a boiler answered — there is no software path from the network into otBoilerCacheValid[]/bBoilerState. The serial-PIC-emulator alternative needs real hardware (serial loopback or a 2nd device driving the PIC), which defeats 'no extra HW'. 

CONCLUSION: F7 as specified (host TCP->OT bridge trips the §4.2 gate) is not achievable without a firmware change. THREE OPTIONS for maintainer: (A) add a debug-only REST/MQTT command 'force boiler-present' that sets a test flag satBoilerHardwarePresent() ORs in — small firmware hook, makes the gate self-testable, but adds a test backdoor to prod; (B) emulate at the GPIO level with a 2nd ESP/Arduino acting as an OT slave on the bus pins (real HW, most faithful, no firmware change); (C) drop F7, accept the §4.2 gate is hardware-validation-only (the OTGW32 with a real boiler attached). Recommend B for fidelity or A if a guarded #if-DEBUG test hook is acceptable. NOT picking unattended — adds either a prod backdoor (A) or needs HW (B). Reverting to To Do.

2026-06-03: script written (scripts/sat_boiler_emulator.py, stdlib-only) + scripts/README. AC#1 (single-file frame emitter) and AC#4 (host-only, PowerShell-launchable, documented, no fw change) met; dry-run builds a correct MsgID 3 READ-ACK (B40030004 for member-id 4) and MsgID 0 status frame, syntax-clean. FINDING (verified in OTDirect.ino:651-703): port 25238 INPUT is relayed straight to the PIC via sendPICSerial(); it is NOT injected into otBoilerCacheValid[3]. That cache (-> satBoilerHardwarePresent -> satOnBoilerDetected edge) is set only from real OT-bus boiler responses in handleMasterResponse. So the OTDirect-TCP-bridge approach CANNOT trip the TASK-795 availability gate -> AC#2/#3 are NOT achievable via TCP. The plan's deferred 'open question' is answered: a serial-level PIC emulator OR a dedicated firmware test-injection hook is required. Script kept as a correct frame-builder/dry-run reference + documents the dead-end. Task stays In Progress pending a design decision on the emulation path.

2026-06-03: PARKED (maintainer decision, Option C). Options for resuming F7 captured in docs/plan/SAT_SIM_F7_BOILER_EMULATOR_OPTIONS.md: (A) serial-level PIC emulator [faithful, needs hardware], (B) firmware test-injection hook [pure host-side, recommended], (C) park [current]. Frame-builder script scripts/sat_boiler_emulator.py stays as a reference. Resume by picking A or B.
<!-- SECTION:NOTES:END -->
