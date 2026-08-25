---
id: TASK-1086
title: >-
  feat-2.0.0: OTDirect-synthesized type-7 frames still mark the boiler
  unsupported
status: To Do
assignee: []
created_date: '2026-08-24 20:32'
updated_date: '2026-08-24 21:14'
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
- [ ] #1 Locally synthesized OTDirect type-7 A frames do not set boilerUnsupportedRead or boilerUnsupportedWrite
- [ ] #2 Genuine B frames from a real boiler on the OTDirect gateway path still count as boiler evidence
- [ ] #3 Proxy A frames that legitimately stand in for a boiler answer (ADR-103) still count
- [ ] #4 Verified on a bench device in OTDirect master mode with no boiler attached: no msgid is reported unsupported
- [ ] #5 Locally synthesized answers cannot RETRACT a genuine unsupported verdict either — the current rsptype == OTGW_BOILER guard blocks the (T,A) cases but NOT loopback mode, which fabricates frames labelled 'B' (OTDirect.ino:1213-1215)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-08-24: scope widened after adversarial verification of the TASK-1084 fix.

Original scope covered only the SET direction (synthesized type-7 A frames marking the boiler unsupported). Verification showed the RETRACT direction is the same class of problem and was briefly worse: a synthesized READ_ACK/WRITE_ACK could clear a genuine verdict. That is now blocked by requiring rsptype == OTGW_BOILER on the retraction, which covers every (T,A) synthesis site.

What that guard does NOT cover, and is the remaining work here: loopback mode bridges fabricated frames labelled 'B' (OTGW_BOILER) at OTDirect.ino:1213-1215, built from the PROGMEM table at :1188-1204, including type-7 for unknown ids and type-5 WRITE_ACK. Those pass a rsptype-based guard by construction, so they can both set and clear capability bits with no boiler present at all.

Note OTDirect.ino:293 already carries the needed idea elsewhere in the same file: 'if (IS_LOOPBACK_MODE()) return false;   // synthetic responses are not a real boiler'. The bitmap block has no equivalent check. A loopback-mode check may be the cheap 80 percent fix, ahead of the full frame-origin plumbing.
<!-- SECTION:NOTES:END -->
