---
id: TASK-1800
title: >-
  OT-Direct: the PS=1 summary line is unusable (separators, not on port 25238,
  repeats, wrong MsgID 71/77 fields)
status: To Do
assignee: []
created_date: '2026-11-11T06:38:00Z'
labels:
  - otdirect
  - review-2026-11-11
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 893000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1795 audit (docs/reviews/2026-11-11_otdirect-pic-parity-audit/). PIC ground truth: other-projects/otgw-6.6/gateway.asm. OT-Direct: src/OTGW-firmware/OTDirect.ino at dev f64aba399. Report B, BC2. OTD emitSummaryLine() (OTDirect.ino:1908-2004) against the PIC PrintSummary/SummaryReport (gateway.asm:2341-2358, :1645-1657; 34 fields in StoreProcTable order :1978-2012). (1) The byte pairs 15/48/49 use ',' (OTDirect.ino:1947-1948, :1974-1978) where the PIC prints HB/LB (gateway.asm:2062-2066): 36 commas instead of 33, so the firmware's own processPSSummary drops the line (OTGW-Core.ino:5236-5239). (2) Toutside gets a '+' (:1928); PIC PrintSigned has none (:2039-2041). (3) Field 24 (MsgID 71) reads s.RelativeVentilation, the MsgID 77 field, and field 25 (MsgID 77) reads s.FanSpeed (MsgID 35). (4) The line is emitted after every MsgID 0 (:1665-1667) instead of once per PS=1 (SummaryDone :1656). (5) It never reaches port 25238 (processOT only, :2003), while T/B frames keep flowing to 25238 (:789) where the PIC hides them (PrintMessage :1921-1922). pyotgw get_status() then times out or crashes (commandprocessor.py:72-75, pyotgw.py:769-781). No test covers it: emitSummaryLine is stubbed in tests/test_otd_gateway_alt_slots.py:218.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 PS=1 sends one summary line in the PIC format (33 commas, the right fields, no '+') to port 25238 and to the parser, and processPSSummary accepts it
- [ ] #2 While PS=1 is on, T/B frames are hidden as on the PIC; PS=0 restores them
- [ ] #3 Host test against the real code (not a re-implementation): red on the old code, green on the fix
- [ ] #4 esp32-combo build, evaluate.py --quick, bench check where the hardware allows (name what was not benched)
<!-- AC:END -->
