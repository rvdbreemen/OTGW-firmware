---
id: TASK-1086
title: >-
  feat-2.0.0: OTDirect-synthesized type-7 frames still mark the boiler
  unsupported
status: To Do
assignee: []
created_date: '2026-08-24 20:32'
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
<!-- AC:END -->
