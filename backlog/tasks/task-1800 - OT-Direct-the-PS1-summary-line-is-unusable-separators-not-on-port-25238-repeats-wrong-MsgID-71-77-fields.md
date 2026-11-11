---
id: TASK-1800
title: >-
  OT-Direct: the PS=1 summary line is unusable (separators, not on port 25238,
  repeats, wrong MsgID 71/77 fields)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T06:38:00Z'
updated_date: '2026-11-11T13:56:00Z'
labels:
  - otdirect
  - review-2026-11-11
  - bug
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 893000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1795 audit (docs/reviews/2026-11-11_otdirect-pic-parity-audit/). PIC ground truth: other-projects/otgw-6.6/gateway.asm. OT-Direct: src/OTGW-firmware/OTDirect.ino at dev f64aba399. Report B, BC2. OTD emitSummaryLine() (OTDirect.ino:1908-2004) against the PIC PrintSummary/SummaryReport (gateway.asm:2341-2358, :1645-1657; 34 fields in StoreProcTable order :1978-2012). (1) The byte pairs 15/48/49 use ',' (OTDirect.ino:1947-1948, :1974-1978) where the PIC prints HB/LB (gateway.asm:2062-2066): 36 commas instead of 33, so the firmware's own processPSSummary drops the line (OTGW-Core.ino:5236-5239). (2) Toutside gets a '+' (:1928); PIC PrintSigned has none (:2039-2041). (3) Field 24 (MsgID 71) reads s.RelativeVentilation, the MsgID 77 field, and field 25 (MsgID 77) reads s.FanSpeed (MsgID 35). (4) The line is emitted after every MsgID 0 (:1665-1667) instead of once per PS=1 (SummaryDone :1656). (5) It never reaches port 25238 (processOT only, :2003), while T/B frames keep flowing to 25238 (:789) where the PIC hides them (PrintMessage :1921-1922). pyotgw get_status() then times out or crashes (commandprocessor.py:72-75, pyotgw.py:769-781). No test covers it: emitSummaryLine is stubbed in tests/test_otd_gateway_alt_slots.py:218.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 PS=1 sends one summary line in the PIC format (33 commas, the right fields, no '+') to port 25238 and to the parser, and processPSSummary accepts it
- [x] #2 While PS=1 is on, T/B frames are hidden as on the PIC; PS=0 restores them
- [x] #3 Host test against the real code (not a re-implementation): red on the old code, green on the fix
- [x] #4 esp32-combo build, evaluate.py --quick, bench check where the hardware allows (name what was not benched)
- [x] #5 PS=1 output is byte-for-byte equal to the PIC (gateway.asm PS routine): field order, separators ('/' for byte pairs, ','), number formats, and the same moments of output as the PIC; also delivered on port 25238; golden test against the PIC format, red on the old code (Robert 2026-11-11: 'OT-Direct can also be asked for a report with PS=1. That must always be identical to the PIC's answers.')
- [x] #6 The MsgID 71 and MsgID 77 fields read their own sources (BC2), and the firmware's own processPSSummary accepts the OT-Direct line
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Mirror the PIC: (1) a raw store of the 34 summary Data-IDs filled by the StoreValue rules (T WRITE_DATA before overrides, B READ_ACK, own R WRITE_DATA without a thermostat; no Data-Invalid/Unknown/A), hooked in bridgeFrameToParser(); (2) a formatter per StoreProcTable kind with the PIC print routines (PrintStatus/Float/Signed/Bytes/LowByte/Short/SigShort, incl. the 0xFF rounding); (3) PS=1 answers 'PS: 1' and emits ONE line (SummaryRequest/SummaryDone) to port 25238 and processOT; T/B/R/A hidden on 25238 until PS=0 (HideReports); 'PS: BV'/'PS: SE' as SerialCommand prints them. Removes the per-MsgID 0 retrigger and the wrong 71/77 sources (BC2). Golden test from a Python oracle of the PIC routines plus literal pins.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence (wt-cyc09, 2026-11-11, uncommitted until beta.176 lands): tests/test_otd_ps1_summary_pic.py cuts the real bridgeFrameToParser(), store/print block, emitSummaryLine(), PS= branch, synthesizeResponse() and the loopOTDirect emit step; the once-per-PS=1 rule runs on the real gateway loop of the alt-slots harness. Old code (HEAD copy): 8 tests, 7 FAIL: no line on port 25238, 36 commas instead of 33 (processPSSummary drops it), 31 summary lines in 60 s instead of 1, T/B still on 25238 during PS=1, PS=2 answered 'PS: 2'. Fix: 8/8 OK; line equals the PIC oracle, e.g. 00000011/00001010,40.50,...,30/10,...,-4.50,45.25,0.00,-5,60/40,90/20,...,55,40,12345,0,0,0,65535,0,0,0. processPSSummary: 33 commas accepted; u8u8 fields parse 'HB/LB' (parsePSSummaryU8U8 -> splitPSSummaryPair '/', OTGW-Core.ino:4984-4996); MsgID 71 is ot_u8 ControlSetpointVH and 77 ot_u8 RelativeVentilation (OTGW-Core.h:444, :450), which the raw store now feeds. All tests/test_otd_*.py OK; evaluate.py --quick 100%. Docs: c4-code-otdirect.md. Not covered: the global bare-token error format of other commands stays TASK-1804 BC7; the store is not cleared on GW=R (the PIC restarts and clears RAM).

GW=R now clears the PS=1 store (memset after resetTransientState), as the PIC's restart clears its RAM.

Review (refute-only, opus): no format or store bug found. Fixed from it: a GW= mode switch cleared HideReports in resetTransientState(); the PIC changes HideReports only in PrintSummary (gateway.asm:2354, :2357), so it is now cleared only by PS=0 and GW=R (with the store and the pending line). Test gap closed: case A7 (no thermostat: own R WRITE stored; 0x8000 -> -128.00 / -32768, 0xFFFF -> 0.00 wrap); a mutant without the R rule now fails A7. Fix 9/9, old code 8/9 FAIL. Accepted as PIC parity, not changed: processPSSummary now consumes the OT-Direct line (enterPSMode, publish of all 34 fields incl. zeros for Data-IDs never seen), the same as on a PIC board; before, the 36-comma line was dropped. Not covered by a test: the mode-switch rule (resetTransientState not in this harness), read from the code only.

Bench before (.61 on beta.175): PS=1 -> 'PS: 1' and no summary line on port 25238 within 8 s; PS=2 -> 'PS: 2'. The bus on .61 is offline (no boiler, no thermostat), so no T/B frames flowed: the hiding of reports is not observable on this bench.

Bench after (.61 on beta.178): PS=1 -> 'PS: 1' then exactly one line on port 25238 (33 commas, all zero: no boiler, no thermostat), none in the next 5 s; telnet: 'enterPSMode: PS mode auto-detected as ON (comma-separated summary)' and 'processPSSummary: PS=1 summary parsed: 34 fields (v5+ firmware)'; PS=2 -> 'PS: BV'; PS=0 -> 'PS: 0'. Note: the firmware leaves its PS mode on the first raw frame after PS=0 (OTGW-Core.ino:5753-5757), as on a PIC board; on .61 no frames flow, so telnet still shows 'PS: ON'. Not benched: values from a real boiler and the hiding of T/B frames (no bus traffic on .61).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1/AC5 one PIC-format line per PS=1 on port 25238 and to the parser: golden test (oracle of the PIC print routines + literal pins) 9/9, old code 8/9 FAIL; bench .61: 'PS: 1' then one 33-comma line, none in the next 5 s. AC2 T/B/R/A hidden on 25238 until PS=0 (host A3); a GW= mode switch keeps it, GW=R clears it. AC3 real cut code + real gateway loop (1 line instead of 31 per minute). AC6 MsgID 71/77 from the raw store; processPSSummary parsed it on the bench ('PS=1 summary parsed: 34 fields (v5+ firmware)'). AC4 build d5b57f3a0, evaluate 0 FAIL, refute-only review (no format/store bug; 2 points fixed), full run_tests 7 red all explained (harnesses fixed in 64a5f3b88). Pushed d5b57f3a0 (beta.178). Not verified: real boiler values and the T/B hiding on a live bus (.61 has no bus traffic); the bare error format of other commands stays TASK-1804 BC7.
<!-- SECTION:FINAL_SUMMARY:END -->
