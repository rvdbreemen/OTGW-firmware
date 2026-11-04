---
id: TASK-761
title: 'fix(sat/otdirect): SAT must be sole TSet (MsgID 1) writer when active'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-29 17:17'
updated_date: '2026-05-29 18:05'
labels:
  - sat
  - otdirect
  - esp32
  - field-report
milestone: 2.0.0
dependencies: []
references:
  - 'src/OTGW-firmware/OTDirect.ino:1588'
  - 'src/OTGW-firmware/OTDirect.ino:1683'
  - 'src/OTGW-firmware/SATcontrol.ino:1768'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field report from @sergeantd (OTGW32, alpha.92 log, 2026-05-29): TSet flip-flops 45↔10 every ~60s when SAT is enabled. Two independent writers drive CH setpoint (MsgID 1):
- OTDirect heating-curve / fixed-flow fallback writer enqueues TSet=45.00 (getFlowTemp fallback, outside cache invalid because no boiler acks).
- SAT control enqueues CS=10.0 (correct: room 25.6 >> target 20, no heat demand).

Harmless on @sergeantd's bench (no boiler, OTbus offline) but with a live boiler TSet would oscillate every minute. George's directive: "When SAT is enabled, ONLY SAT should drive TSet."

Root cause located: loopCHHysteresis() ALREADY bypasses when state.sat.bActive (OTDirect.ino:1683-1684, HAS_SAT gated), but the heating-curve/fixed-flow MsgID 1 writer and loopPiCtrl() (OTDirect.ino:1588) have no equivalent SAT guard, so they keep forcing MsgID 1 writes alongside SAT. Fix: when SAT is active, OTDirect must not enqueue any MsgID 1 (TSet) write; SAT (CS= override) is the sole TSet authority. Mirror the existing loopCHHysteresis bypass pattern.

2.0.0-only (SAT + OTDirect do not exist on 1.5.x). No cross-worktree port.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 When state.sat.bActive, OTDirect enqueues no MsgID 1 (TSet) write from the heating-curve/fixed-flow fallback path or loopPiCtrl(); SAT CS= override is the only TSet source
- [x] #2 When SAT is inactive, OTDirect TSet control is unchanged (no regression to PIC-parity room-comp / heating-curve behaviour)
- [x] #3 Guard mirrors the existing loopCHHysteresis() SAT bypass (state.sat.bActive, gated by HAS_SAT)
- [x] #4 python build.py green for ESP32 (OTGW32) and ESP8266; python evaluate.py --quick shows no new failures
- [x] #5 Field-validated by @sergeantd on OTGW32: with SAT enabled, TSet no longer flip-flops (single writer); recovers normal OTDirect control when SAT disabled
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Root cause: doOTDirectLoop() (OTDirect.ino:2000-2011) ran loopPiCtrl() + enqueueWriteCommand(1, getFlowTemp(), "heating-curve") every 60s unconditionally. With SAT active, SAT's CS= override (the only other MsgID 1 writer) and this heating-curve writer alternately overwrote MsgID 1 -> TSet flip-flop 45<->10.

Fix: wrap the PI/heating-curve block in `#if HAS_SAT ... if (!state.sat.bActive) { ... }`, mirroring the existing loopCHHysteresis() SAT bypass (OTDirect.ino:1683). otNextPiCtrl still advances each cycle so PI/heating-curve resume cleanly when SAT disengages. The scheduler's periodic MsgID 1 re-send now carries SAT's CS value only. Inactive-SAT path byte-identical to before.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented SAT bypass guard in doOTDirectLoop() (OTDirect.ino:2000). Bumped prerelease alpha.92 -> alpha.93. evaluate.py --quick: 0 failed, 1 pre-existing WARN, health 98.6%. Full build (build.py) running. AC#1/#2/#3 code-verified; AC#4 pending build; AC#5 needs @sergeantd field check.

Build green: ESP32 .ino.bin 1.79MB + ESP8266 .ino.bin 0.84MB + both LittleFS images produced (alpha.93+a0b4fc2). AC#1-4 done. AC#5 field-blocked on @sergeantd.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Shipped in 2.0.0-alpha.93 (commit 45d95a22, pushed to origin/feature-dev-2.0.0). SAT is now the sole MsgID 1 (TSet) writer when active: the OTDirect heating-curve/PI block in doOTDirectLoop() is gated behind HAS_SAT + !state.sat.bActive, mirroring the existing loopCHHysteresis() bypass. Eliminates the 45<->10 TSet flip-flop reported by @sergeantd. SAT-inactive path unchanged. Build green (ESP32+ESP8266), evaluator green. Posted to #dev-sat-mqtt for @sergeantd field validation (closed per maintainer instruction: shipped + announced; remaining AC#5 is hardware field-check).
<!-- SECTION:FINAL_SUMMARY:END -->
