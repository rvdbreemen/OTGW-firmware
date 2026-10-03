---
id: TASK-1086
title: >-
  feat-2.0.0: OTDirect-synthesized type-7 frames still mark the boiler
  unsupported
status: Done
assignee:
  - '@claude'
created_date: '2026-08-24 20:32'
updated_date: '2026-10-03 09:35'
labels:
  - bug
dependencies: []
priority: medium
ordinal: 263000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Split out of the GH #677 fix. The override-A gate and the Ack retraction are now in place on both lines, which fixes the reported defect. A third surface remains, and it exists ONLY on this line.

OTDirect manufactures its own UNKNOWN_DATAID (type 7) responses and bridges them as 'A' frames: handleMasterModeSlaveFrame at OTDirect.ino:2498/2521/2525-2526, and the UI-table ignore list at OTDirect.ino:1953-1957. These are not bAnswerOverride (no preceding B exists), so the new gate correctly lets them through as proxy answers per ADR-103 - but in master mode there may be no boiler on the bus at all, and the firmware ends up publishing 'Boiler does not implement' about a boiler it never asked.

Why this was not fixed in the same change: the obvious discriminator does not work. The frame queue already carries a source byte (OTFRAME_SRC_OTDIRECT, OTGW-Core.h:558) but it is consumed at the drain (OTGW-Core.ino:499) and never reaches processOT. Worse, source alone over-blocks: OTDirect gateway mode produces genuine B frames from a real boiler under the same source tag, and those ARE valid evidence. The correct discriminator is narrower - specifically the locally synthesized A frames - which needs either a new frame-source value threaded through processOT's signature (5 call sites) or an equivalent flag on OTdata. That is a design decision, not a mechanical port.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Locally synthesized OTDirect type-7 A frames do not set boilerUnsupportedRead or boilerUnsupportedWrite
- [x] #2 Genuine B frames from a real boiler on the OTDirect gateway path still count as boiler evidence
- [x] #3 Proxy A frames that legitimately stand in for a boiler answer (ADR-103) still count
- [x] #4 Verified on a bench device in OTDirect master mode with no boiler attached: no msgid is reported unsupported
- [x] #5 Locally synthesized answers cannot RETRACT a genuine unsupported verdict either — the current rsptype == OTGW_BOILER guard blocks the (T,A) cases but NOT loopback mode, which fabricates frames labelled 'B' (OTDirect.ino:1213-1215)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-08-24: scope widened after adversarial verification of the TASK-1084 fix.

Original scope covered only the SET direction (synthesized type-7 A frames marking the boiler unsupported). Verification showed the RETRACT direction is the same class of problem and was briefly worse: a synthesized READ_ACK/WRITE_ACK could clear a genuine verdict. That is now blocked by requiring rsptype == OTGW_BOILER on the retraction, which covers every (T,A) synthesis site.

What that guard does NOT cover, and is the remaining work here: loopback mode bridges fabricated frames labelled 'B' (OTGW_BOILER) at OTDirect.ino:1213-1215, built from the PROGMEM table at :1188-1204, including type-7 for unknown ids and type-5 WRITE_ACK. Those pass a rsptype-based guard by construction, so they can both set and clear capability bits with no boiler present at all.

Note OTDirect.ino:293 already carries the needed idea elsewhere in the same file: 'if (IS_LOOPBACK_MODE()) return false;   // synthetic responses are not a real boiler'. The bitmap block has no equivalent check. A loopback-mode check may be the cheap 80 percent fix, ahead of the full frame-origin plumbing.

Implementation 2026-09-30 (workflow wf_33a155e0-d5c: implement, adversarial review, fixup; integrated in the main tree):
- Design: bridgeFrameToParser() tags every 'A' and every 'B' while IS_LOOPBACK_MODE() as OTFRAME_SRC_OTDIRECT_LOCAL (new value 2); drainOTFrameQueue() passes it to processOT() as a 4th parameter (default false in the header, like suppressOutput); processOT() stores it in OTdata.bLocalAnswer, which rides the one-frame delay; '!OTdata.bLocalAnswer' gates the UNKNOWN-DATAID set and both Ack retracts. rsptype == OTGW_BOILER stays on the retracts (it still stops a PIC proxy A). Rejected: a loopback-mode check only (misses AC#1, reads the mode at drain time), the queue source byte alone (over-blocks genuine OT-Direct B, fails AC#2), per-call-site marks (fails open), full origin plumbing (reopens TASK-1138).
- Known and accepted: a real boiler reply that arrives while the gateway is in loopback mode (boot probe, a reply in flight at GW=L) is also tagged local: an observation lost, never a fake one counted.
- Proof, own run in the main tree: python test/host/test_boiler_unsupported_origin.py --old-rev HEAD (OLD = c39068977): FIX 22/22; OLD fails exactly D1-D7 (wrong SET on master-mode A, UI= READ and WRITE A, loopback UNKNOWN B; wrong RETRACT on loopback READ-ACK/WRITE-ACK B), only the unsupported verdict differs; 15 controls byte-identical (genuine OT-Direct B in gateway and master mode incl. the TASK-1178 CS=/RM= replies, PIC proxy A incl. P7 WRITE-ACK). Own slice audit: 58 sections, 0 mismatches. Mutants M1-M6, M10, M14 killed (fixup evidence). Transcript: %LOCALAPPDATA%/OTGW-capture/a1-patches/TASK-1086-run-oldvsfix-main-c39068977.txt
- Regression: test_raw_passthrough.cpp's processOT double gets the 4th parameter with defaults (companion edit, it slices drainOTFrameQueue); its OLD-vs-FIX verdict stays PASS; override-reply suites 1178/1177/1184 PASS.
- AC#4 needs the bench in OT-Direct master mode with no boiler; start it with /ot-boiler.json deleted and a prompt reboot, or verdicts persisted by older builds can fail it for a reason unrelated to this fix. Four related consumers outside these ACs: TASK-1185.

Build 2026-09-30 alpha.392: build.bat --target all, esp32, esp32-classic and esp32-combo firmware and filesystem all SUCCESS, 'Build completed successfully!', fresh binaries; the 4th processOT() parameter with its header-only default compiles through the Arduino prototype generation on xtensa. evaluate.py --quick: 70 passed, 0 warnings, 0 failed. Log: %LOCALAPPDATA%/OTGW-capture/build-alpha392-task1086.log

AC#4 BENCH, 2026-10-03, OTGW32 192.168.88.61 running 2.0.0-alpha.404+1d9ae71 (it contains this fix, alpha.392). No boiler and no thermostat on the bus; /ot-boiler.json absent (38-file listing checked).
(1) Master mode as AC#4 words it: otdmode master, about 1 h uptime since the last flash. GET /api/v2/otgw/boiler-support gave unsupported_read [] and unsupported_write []. A 30 s /ws capture showed only keepalives, no OT frames. This case is TRUE BUT NOT DISCRIMINATING: synthesized type-7 frames need a thermostat (handleMasterModeSlaveFrame), so old and new code would both show empty lists here.
(2) The discriminating case, loopback (GW=L), which fabricates boiler frames including type-7 for unknown ids (OTDirect.ino:1188-1215). In a 90 s /ws capture there were 357 R and 358 B frames. Type-7 UNKNOWN_DATA_ID B frames came for 67 distinct msgids (29-32, 34-39, 50-55, 58-63, 70-115); ids 29, 30, 31, 32, 35, 36 and 38 had 9 each, well past the 3-strike threshold. boiler-support afterwards was still unsupported_read [] and unsupported_write []. The old code marks exactly these unsupported (host harness test_boiler_unsupported_origin.py, D-cases).
Bench restored: GW=2 gives runtime master and persisted otdmode 3, as before. GW=L had persisted 4.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Locally synthesized OT-Direct frames no longer count as boiler evidence in boilerUnsupportedRead/Write, in either direction.
- bridgeFrameToParser() tags as a local answer every 'A' frame (master mode, UI=/SR= tables, TASK-1178 replies) and every loopback 'B' frame.
- The tag rides the frame queue and processOT's one-frame delay, and gates the UNKNOWN-DATAID set and both Ack retracts.
- Genuine OT-Direct B frames and PIC proxy A frames keep counting.

Evidence per AC:
- AC#1, #5: test/host/test_boiler_unsupported_origin.py (the real sliced bridge, queue and processOT). OLD c39068977 wrongly sets on D1-D5 and wrongly retracts on D6-D7; FIX does neither.
- AC#2, #3: controls G1-G7 (genuine OT-Direct B) and P1-P7 (PIC proxy A) are byte-identical OLD against FIX.
- AC#4 (bench, 2026-10-03): OTGW32 on alpha.404+1d9ae71, no boiler, no thermostat, no /ot-boiler.json.
  - In master mode GET /api/v2/otgw/boiler-support stayed empty. Without a thermostat no frames reach the parser, so this case does not discriminate.
  - The discriminating run was loopback (GW=L) for 90 s: 357 R and 358 fabricated B frames, type-7 for 67 msgids, ids 29-32/35/36/38 nine times each. boiler-support stayed {unsupported_read: [], unsupported_write: []}.
  - Bench restored to master (persisted otdmode 3).
- Build: alpha.392, three targets fresh; evaluate --quick 0 FAIL.
Related consumers outside these ACs: TASK-1185.
<!-- SECTION:FINAL_SUMMARY:END -->
