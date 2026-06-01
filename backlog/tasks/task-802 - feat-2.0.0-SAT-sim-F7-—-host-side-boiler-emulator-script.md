---
id: TASK-802
title: 'feat-2.0.0: SAT sim F7 — host-side boiler-emulator script'
status: To Do
assignee: []
created_date: '2026-05-31 22:56'
updated_date: '2026-06-01 04:16'
labels:
  - sat
  - simulation
  - tooling
dependencies: []
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
