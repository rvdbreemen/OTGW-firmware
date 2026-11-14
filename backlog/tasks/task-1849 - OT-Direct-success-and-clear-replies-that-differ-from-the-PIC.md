---
id: TASK-1849
title: 'OT-Direct: success and clear replies that differ from the PIC'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-12T00:04:00Z'
updated_date: '2026-11-14T17:25:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 928000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N15), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- SC=: PIC two-digit hours `SC: 09:05/3` (G:4374-4388, PrintDecimal G:2086-2101); OT-Direct %d (O:3188).
- SR=: PIC `18:1/230`, `70:0/14` (G:4856-4859); OT-Direct echoes input (O:3693).
- FT=: PIC prints D, S, R, C or I (G:4705-4727); OT-Direct echoes any character (O:3901-3905).
- VS= clear: PIC `VS: -` (G:4253-4254); OT-Direct echoes input (O:3158-3160).
- CL= clear: see N3.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 host test per reply, byte-exact; red on current code
- [ ] #2 one line per item in the final summary
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): client impact; none found.

Part of the PIC 6.8 parity audit, parent TASK-1851.

2026-11-13 delta parity check (TASK-1851, comms, read-only, OT-Direct at origin/dev 479e49391 vs PIC 6.8 gateway.asm; G=gateway.asm, O=OTDirect.ino): partly fixed: VS clear (1d4b8fdac) and CL clear (60c0fbc50) replies match now. Still open: SR= echo (O:3999), FT= echo of any character (O:4211-4214), SC hours printed with %d (O:3497).

2026-11-14 18:16 (95) build 2.0.0-beta.254+f12af9f from lane-95-1848 HEAD f12af9f1d on origin/dev ef0c0884b (30d80a30c TASK-1848 beta.252, e4b8362a2 TASK-1849 beta.253, f12af9f1d TASK-1940 beta.254): exit 0, 5 min, 4 Compiling, 1 ESP32S3 image line (warm), RAM 130316 B, Flash 2149603 B; sha256 ino d2c427fbdc521cd9..., littlefs 1e43423a3acc1a6e..., zip f82a7690674d93f2.... Host tests on this HEAD: test_otd_pr_reports_pyotgw 15/15, test_otd_reply_texts_pic 4/4, test_pic_temp_sensor_port 8/8, test_otd_sc_clock 8/8. Old-vs-fix before the rebase: PR test 8 new FAIL on origin/dev OTDirect.ino; reply texts 3 FAIL on the 1848-only code; pic temp 7 FAIL on origin/dev (the setting-off case is trivially green there). evaluate --quick 86/0. Bench requested from 09 (.63 for 1940 + no regression, .61 for 1848/1849).
<!-- SECTION:NOTES:END -->
