---
id: TASK-1391
title: >-
  OT support bitmap: PIC-built A frames can count as boiler evidence (unpaired
  A, blacklist/UI= Unknown)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-25T13:44:00Z'
updated_date: '2026-11-04T21:24:00Z'
labels:
  - opentherm
  - pic
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 530000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the boiler-capability research 2026-10-25 (docs/research/2026-10-25-boiler-capability-discovery.md, M3; read in code only). B and A pair only with the same ID within 500 ms (OTGW-Core.ino:5326-5328); an unpaired A keeps bAnswerOverride false (:5336) and counts as boiler evidence (:5401): in T(57) R(Y) B(Y) A(57 ack) that A sets boilerAckedRead for 57 (:5402-5406), and a UI= or blacklist A(X Unknown) sets boilerUnsupported for X (:5438-5450). Only B frames are boiler truth. Needs a capture first.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Bench or field capture shows (or refutes) a PIC-built A frame setting the boiler bitmap
- [x] #2 If confirmed: only B frames set/retract boiler support bits (host test red on HEAD)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: partly: only a genuine B retracts (OTGW-Core.ino:5564/5576); unpaired PIC A can still set (:5530/5545). Needs a capture.

2026-11-04 (otgw-firmware-6c). AC1 confirmed in a field capture (bench .63 has no boiler/thermostat, OTbus offline, so no A frames there): gruijter transcript 20261030-092718 (alpha.549, 350042 lines, local %LOCALAPPDATA%/OTGW-capture/field/gruijter-20261030-092718). Per MsgID: 9 = 349 T + 348 A Read-Ack, 0 B; 16 = 347 T + 347 A Write-Ack, 0 B; 24 = 347 T + 347 A Unknown-Data-Id, 0 B; 57 = 347 T + 347 A Read-Ack, 0 B; 115 = 49 T + 49 A Unknown-Data-Id, 0 B; 28 = 49 T, 1 B Unknown, 48 A Read-Ack. Every one of those A frames follows a B of another id (T57 R26 B26 A57 pattern: the PIC replaced the request). The published otgw-firmware/boiler/unsupported_msgids (transcript line 149167) reads 15R,28R,49R,57R,99R,115R,117R,118R,119R,121R,122R,14W,24W,57W: 24W and 115R come from PIC answers only (caveat: retained value, could partly date from earlier runs; the mechanism matches). Primary sources: otgw-6.6 gateway.asm:2490-2496 prints 'A' for a response the PIC transmits itself (nothing in monitor mode); OTGW-documentation/OTGW-Protocol.md:92 'A = modified response being sent to the thermostat'. AC2: OTGW-Core.ino bitmap block now only enters the slave branch for rsptype OTGW_BOILER (was: every slave frame except an answer-override A). /ot-boiler.json goes to format 3; format 1/2 files are not loaded (start fresh once, same pattern as TASK-1185) so the wrong 24W/115R bits do not stay forever (retraction needs a B that never comes). C4 doc c4-code-otgw-core.md updated. ADR-103 is unaffected (it routes values on MQTT, not support evidence). Side effect for SAT (no design change): a PIC A(28 Unknown) no longer sets boilerUnknown28ThisBoot, matching the SAT rule's own text 'a boiler frame of this boot confirmed Unknown-DataID' (SATcontrol.ino:2586). Host test tests/test_boiler_support_bitmap_b_only.py compiles the real block: against origin/dev b1fe73d2e 3 of 5 FAIL (57/16/9 acked, 24W/115R unsupported, u28 set from an A), on the fix 5 of 5 OK. Related tests OK: test_sat_no_return_startup, test_sat_flow_return_validity, test_pic_pm_chain (40), test_sat_max_tset, test_fs_update_keep. evaluate --quick 82/0/0. NOT host-tested: the format-3 load/save (LittleFS); bench check after build: telnet shows 'ot-support: /ot-boiler.json is format 1/2 ... starting fresh' once, /ot-boiler.json then starts with {"v":3 after the 15-min save.
<!-- SECTION:NOTES:END -->
