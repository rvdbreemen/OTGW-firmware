---
id: TASK-1185
title: >-
  OT-Direct: gateway-made frames still count as boiler evidence outside the
  unsupported bitmaps
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-30 14:32'
updated_date: '2026-09-30 19:35'
labels:
  - otdirect
  - bug
dependencies: []
priority: low
ordinal: 308000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1086 (found by its adversarial review, same class, outside that task's ACs). TASK-1086 tags every OT-Direct 'A' and every loopback 'B' as a local answer (OTFRAME_SRC_OTDIRECT_LOCAL) and keeps them out of boilerUnsupportedRead/Write. Four other consumers still read those frames as boiler evidence:
1. boilerAckedRead/boilerAckedWrite take gateway-made Acks (the master-mode WRITE_ACK echo, SR= READ_ACK answers, loopback B), which show as 'acknowledged' in the /api/v2/otgw/ot-support Boiler column.
2. The OT log prints '(boiler does not implement)' on synthesized type-7 A lines (processOT, the masterslave==1 && type-7 suffix); a bench tester can read that as a boiler verdict.
3. satNotifyBoilerFrameSeen() still fires on loopback B frames.
4. On the PIC path, frames replayed from /otgw_simulation.log (B and A) still count as boiler evidence.
Maintainer question (TASK-1086 review item 3): verdicts written by older builds persist in /ot-boiler.json and reload at boot; only a genuine boiler B Ack retracts them, so a unit that ran master or loopback mode on an older build keeps false 'unsupported' entries. Decide whether a one-time migration or a reset path is wanted.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Each of the four consumers either ignores locally made frames (the TASK-1086 bLocalAnswer mark) or is documented as deliberately counting them, with a host proof for every change
- [ ] #2 The maintainer decision on stale /ot-boiler.json verdicts is recorded, and implemented if a migration is chosen
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. boilerAckedRead/Write: skip frames with OTdata.bLocalAnswer, as the unsupported bitmaps already do (PIC frames and ADR-103 proxy A are never marked local, so the PIC path is unchanged). 2. OT log: a type-7 slave frame the gateway made gets ' (gateway answer)' instead of the boiler suffix. 3. processOT B branch: satNotifyBoilerFrameSeen() only when !localAnswer; tBoilerLastSeen keeps counting loopback (TASK-1138 decision). 3b. satBoilerHardwarePresent(): read state.otBus.bBoilerState only while OT-Direct is not the active transport (!isOTDirectEnabled()); in OT-Direct mode otDirectBoilerPresent() decides, which excludes loopback. 4. Replay of /otgw_simulation.log: maintainer decision (document as deliberate, or mark replayed frames). AC#2 (stale /ot-boiler.json verdicts): maintainer decision. Proof: new host harness test/host/test_local_frame_consumers.py reusing the TASK-1086 chain slices (bridgeFrameToParser -> queue -> processOT -> evaluateOTBusLiveness) plus the real satNotifyBoilerFrameSeen/satBoilerHardwarePresent/otDirectBoilerPresent/isOTDirectEnabled, OLD=HEAD vs FIX, with mutants; gate cases per board model (combo, classic, OTGW32). Bump, build all targets, evaluate.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fifth consumer found while tracing item 3: satBoilerHardwarePresent() (SATcontrol.ino) reads state.otBus.bBoilerState under #if HAS_PIC. The combo build compiles HAS_PIC=1 and HAS_DIRECT_OT=1 (boards.h), and bBoilerState counts OT-Direct loopback B frames by the TASK-1138 decision, so on a combo in loopback the SAT availability gate reports a real boiler: the REST 409 guard and the MQTT enable-reject refuse SAT simulation, which the section 4.2 dual-signal rule in the same file says must not happen.

Implemented (alpha.394), consumers 1 to 3 plus the fifth:
1. boilerAckedRead/boilerAckedWrite skip OTdata.bLocalAnswer frames (OTGW-Core.ino bitmap block). PIC frames are never marked local, so ADR-103 proxy answers still count.
2. The OT log prints ' (gateway answer)' on a type-7 slave frame the gateway made, instead of the boiler suffix.
3. processOT()'s B branch calls satNotifyBoilerFrameSeen() only when !localAnswer. tBoilerLastSeen still counts loopback (TASK-1138 decision, unchanged).
5. satBoilerHardwarePresent() reads state.otBus.bBoilerState only while OT-Direct is not the transport (!isOTDirectEnabled()). Classic builds are unchanged (isOTDirectEnabled() is compile-time false there), and OTGW32 builds never read it. This restores ADR-117 section 2: the gate must read a different signal than synthetic traffic.

Evidence: test/host/test_local_frame_consumers.py --old-rev HEAD (HEAD de6b13ebe, before and after the version bump): RESULT PASS.
- 12 defect cases (A1-A5, L1-L3, N1-N2, H1-H2) fail on OLD and pass on FIX. Each differs from FIX in exactly the dump parts its frames touch, with trace and other bitmaps identical.
- 22 control cases over the combo, classic and OTGW32 builds pass on both sides. O1 (OTGW32 loopback) differs only in the acknowledged bitmap and the SAT edge flag, as listed.
- Mutants MA, MB, ML, MN and MH each fail exactly their own cases.
- Red phase: the same harness run on the unfixed tree failed all 12 defect cases for the expected reasons (e.g. H1: gate 1 with bBoilerState 1 in loopback).
- Slice audit: 222 generated parts verbatim in their sources and brace-complete.
- test_boiler_unsupported_origin.py (working tree) still passes. Its old-vs-fix mode now describes TASK-1086 alone (docstring note): against 331550de3^, D6, D7 and S1 also differ in the acknowledged bitmaps.
- H2 is a deliberate behaviour change. A combo in OT-Direct mode whose boiler has sent B frames but not yet answered MsgID 3 no longer counts as present for SAT. That is the OTGW32 rule (control O2).

Still open:
- Item 4: the /otgw_simulation.log replay reaches processOT() with localAnswer=false, so replayed frames set and retract verdicts, acked bits (persisted to /ot-boiler.json) and trip the SAT edge hook. Maintainer decision: document as deliberate, or mark replayed frames.
- AC#2: stale verdicts from older builds. Acked bits have no retraction at all, so stale acked bits from local frames persist too, not only unsupported ones.
- Side finding: GET /api/v2/otgw/ot-support is not documented in docs/api or the manuals.
<!-- SECTION:NOTES:END -->
