---
id: TASK-1639
title: >-
  GitHub #694: code size breakdown of the esp32-combo app slot (93.5% full) and
  reduction candidates
status: Done
assignee:
  - '@claude'
created_date: '2026-11-04T23:51:00Z'
updated_date: '2026-11-11T06:01:00Z'
labels:
  - github
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 765000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
https://github.com/rvdbreemen/OTGW-firmware/issues/694 by andrebrait. Measured breakdown of the 2240 KiB app0 slot (esp-idf-size, nm) and ranked reduction candidates, none implemented. Proposal: needs Robert's go before any work.
<!-- SECTION:DESCRIPTION:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05 Robert (session): released: implement the top-3 low-risk candidates, each measured before/after with the size tool and tested separately.

2026-11-05 lead: Robert chose 'only siscanf' (the other andrebrait candidates are medium/high risk or ADR changes; /update minify not now). Patch (opus agent, wt-1639) integrated on wt-int4 as beta.89 051997cc1: 19 sscanf -> siscanf (10 firmware, 9 OTGWSerial), host shims, tests/test_no_float_sscanf.py (4 OK; red on base with 19 hits). Build: app 2,149,424 B vs beta.87 2,158,720 (-9,296 B incl. the small 1645 addition; 93.7% of 2,293,760); nm on the beta.89 ELF: no sscanf / __ssvfscanf_r. Bench .63 (PIC 6.8) beta.89: processOT decode OK (130 lines, MsgID 0 only: no boiler on the bench), picfwversion 6.8 / pic16f1847 unchanged, bootcount +1 over a planned /ReBoot, hardwaremode PIC. Not verified: other MsgIDs on a real boiler, OT-Direct command parsing (SR=27:...) on .61. Still to do: GitHub #694 reply after the push.

2026-11-11 GitHub #694 closed as completed (Robert's OK in session otgw-firmware-09; his closing comment of 2026-11-05 already on the issue).

Robert's OK, 2026-11-11 ~06:3x local, session otgw-firmware-09, AskUserQuestion 'May I close GitHub issues #694, #692 and #154 (...)?' answered verbatim: "Yes, close all three (Recommended)" (option text: 'Post the two comments, then close #694, #692 and #154 as completed. #10 stays open.').
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
siscanf instead of sscanf (19 call sites, integer formats only): app 2,158,720 -> 2,149,424 B at beta.89 (-9.3 KB incl. 1645), nm: no sscanf/__ssvfscanf_r. Evidence: tests/test_no_float_sscanf.py 4 OK (red on base), 19 host harnesses compile, bench .63 PIC 6.8: frame decode, PIC version and boot counter unchanged. Other #694 candidates not done by Robert's choice. Not verified: other MsgIDs on a real boiler, OT-Direct command parsing on hardware.
<!-- SECTION:FINAL_SUMMARY:END -->
