---
id: TASK-1917
title: >-
  OT-Direct: a CS=/C2= below 8 C expires after the window; the PIC keeps it as a
  permanent override
status: To Do
assignee: []
created_date: '2026-11-14T01:31:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 982000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during TASK-1834 (PIC 6.8 parity audit finding b1-7, docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md). Read this session from gateway.asm 6.8: CommandExpiry :1626-1633 masks the integer part of controlsetpt1 with b'11111000'; when the HeartbeatCS flag is clear AND the value is >= 8 it sets SysCtrlSetpoint (override released); a value 1..7 skips that and stays forever. C2 (controlsetpt3) the same at :1634-1639. CS=0 is the explicit release (SetCtrlSetpoint :4218-4226 -> ClrCtrlSetPoint). PIC test tests/control-4.stc: CS=5 still R10010500 after 74 s. Our docs/pic/pic-firmware-6.8-spec.md:131 and :308 already state 'Values with HB below 8 never expire'. OT-Direct (OTDirect.ino 1-second status timer, the TASK-442 CS/C2 expiry) expires every non-zero CS/C2 after OT_CSC2_EXPIRY_MS, so a CS=5 (e.g. a deliberate low-flow / pump-only hold) is dropped after ~63 s on OT-Direct and kept on the PIC. User manuals (docs/manuals en/nl, docs/api) promise nothing about CS expiry: checked with grep 2026-11-14, no CS/C2 expiry statement found. Open for the implementer: confirm against the PIC command reference (otgw.tclcode.com firmware page) what the below-8 case is meant for before matching it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 host test with a fake clock on the real expiry code: CS=5 and C2=5 at t=0 still active at t=200 s; red on the current code
- [ ] #2 CS=C:/Program Files/Git/C2= >= 8 still expire as in TASK-1834 (not earlier than 63 s, gone by 64 s)
- [ ] #3 the PIC's intent for values below 8 confirmed from the command reference and cited in the task
- [ ] #4 docs/pic/pic-firmware-6.8-spec.md and any OT-Direct doc state the same behaviour
<!-- AC:END -->
