---
id: TASK-1185
title: >-
  OT-Direct: gateway-made frames still count as boiler evidence outside the
  unsupported bitmaps
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-30 14:32'
updated_date: '2026-09-30 20:34'
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
- [x] #1 Each of the four consumers either ignores locally made frames (the TASK-1086 bLocalAnswer mark) or is documented as deliberately counting them, with a host proof for every change
- [x] #2 The maintainer decision on stale /ot-boiler.json verdicts is recorded, and implemented if a migration is chosen
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

Build after commit 5bc508e47: build.bat --target all, firmware and filesystem SUCCESS for esp32, esp32-classic and esp32-combo, 3 images, 18 fresh 2.0.0-alpha.394+5bc508e artifacts; flash use 79.5%, 77.2%, 81.4% (unchanged). python evaluate.py --quick: exit 0, health 100%, ESP abstraction boundary clean. Host suite on the tree: 33/34, the failure is the known adr governance item (TASK-1183). Not bench-validated: the OTGW32 is still in its WiFi provisioning portal.

UI impact check (review finding): after this change more ids end up with thermostat-sent set and no boiler evidence (every thermostat id in loopback, master-mode writes, SR= ids). Web UI read-through:
- Classic OT Support tab (index.js refreshOtSupport, from /api/v2/otgw/ot-support): the Boiler column lists R-ack/W-ack/'no read support'/'rejects write' and shows '-' when none apply. Such an id now shows a neutral '-' where it showed a false R-ack/W-ack; there is no boiler-blaming wording.
- v2 support map (v2.js): 'Thermostat only' ('the boiler never answers, your boiler may not implement it') comes from the live T/R and B/A log lines in the browser, not from ot-support, so a gateway-made A still counts as an answer there; unchanged by this task.
- Statistics banner 'Boiler does not implement these OpenTherm messages' (index.js, /api/v2/otgw/boiler-support) reads the unsupported bitmaps, which TASK-1086 already cleaned; unchanged.
- No web asset keys on the OT log suffix strings.

Maintainer decisions (Robert, 2026-09-30, asked in session):
- Item 4, replay: replayed frames are NOT boiler evidence. They get their own queue source; decoding, publishing and boiler_connected stay as they are, but they set no unsupported verdict and no acknowledged bit, and do not trip the SAT simulation hook.
- AC#2, stale verdicts: one-time migration. /ot-boiler.json gets a format version; the first boot on the new version ignores the bitmaps of an old-format file and starts clean; real verdicts come back from live OT traffic.

Implemented (alpha.395), item 4 and AC#2 per the maintainer decisions:
- Replay: dispatchOTGWInputLine() tags its lines OTFRAME_SRC_REPLAY (3). drainOTFrameQueue() blinks the LED as before and passes replayed=true, and OTdata.bReplayed rides the one-frame delay like bLocalAnswer. processOT() folds both flags into one boilerEvidence condition for the four boiler bitmaps (set and retract). tBoilerLastSeen, which drives boiler_connected, keeps counting replayed B frames, as decided. A new stamp, state.otBus.tRealBoilerLastSeen, is set only by B frames that are neither local nor replayed, together with satNotifyBoilerFrameSeen().
- The SAT gate had to change as well. satControlLoop() has a backstop: `if (settings.sat.bSimulation && satBoilerHardwarePresent()) satOnBoilerDetected();`. It would still switch simulation off during a replay through bBoilerState, which keeps counting replayed frames. So satBoilerHardwarePresent() now reads otRealBoilerSeenRecently() (that stamp within 30 s) on the PIC path, while OT-Direct is not the transport. That gives the decision ("the replay does not touch the SAT simulation hook") its effect, and keeps ADR-117 section 2's rule that the gate reads a signal synthetic traffic does not set.
- Migration: /ot-boiler.json is format 2 ("v":2). A format-1 file is not loaded; the load sets boilerFileDirty so the next save writes format 2. The MQTT connect handler already republishes the (then empty) unsupported list, retained, which replaces the stale one. /ot-thermo.json stays format 1. After a downgrade, an older build finds no "v":1 in the boiler file and starts its boiler bitmaps empty.
- Harness updates: test_boiler_unsupported_origin.cpp and test_local_frame_consumers.cpp now feed live PIC lines to enqueueOTFrame(..., OTFRAME_SRC_PIC), as the PIC task does, since dispatchOTGWInputLine() is now the replay's entry point. The processOT() double in test_raw_passthrough.cpp takes the fifth parameter. The TASK-1086 driver's state double gained tRealBoilerLastSeen.

Evidence (this session, main tree, after the alpha.395 bump):
- python test/host/test_local_frame_consumers.py --old-rev de6b13ebe (the tree before TASK-1185): RESULT PASS, every check yes.
  - 20 defect cases fail on OLD and differ from FIX in exactly their parts: combo A1-A5, L1-L3, N1-N2, H1-H2, R1-R7, and classic R8. The only trace difference allowed is the replay source tag (0 on OLD, 3 on FIX).
  - 22 control cases hold.
  - 8 mutants each fail exactly their cases: MA, MB, ML, MN, MH, MG (gate back on bBoilerState: R7, R8), MR (replay tagged PIC again: R1-R8) and ME (bitmaps count replay again: R1-R5).
  - Red phase before the change: R1-R8 failed and the other 34 passed.
  - The R7 dump shows the decision holding: OLD N=1 H=1, FIX N=0 H=0, and boiler:1 on both sides.
- python test/host/test_ot_support_migration.py --old-rev de6b13ebe: RESULT PASS.
  - M1 (a format-1 file is not loaded and is marked for rewrite), M2 (the next save writes format 2 with empty boiler arrays) and M3 (format 2 loads) fail on OLD and pass on FIX.
  - Controls hold: C1 round trip, differing only in the file's version; C2 thermostat format 1; C3 no file; C4 corrupt file.
  - Mutants MX1, MX2 and MX3 each fail exactly their cases.
  - Red phase: M1-M3 failed on the old loader.
- Slice audits: 225 parts (consumer harness) and 4 parts (migration harness) are verbatim in their sources and brace-complete.
- test_boiler_unsupported_origin.py passes on the working tree; its old-vs-fix mode shows only the documented D6, D7 and S1 differences. test_raw_passthrough.py passes in both modes.
<!-- SECTION:NOTES:END -->
