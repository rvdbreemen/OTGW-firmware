---
id: TASK-1852
title: >-
  Docs: reference specification of the PIC firmware 6.8
  (docs/pic/pic-firmware-6.8-spec.md)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-12T00:14:00Z'
updated_date: '2026-11-12T00:19:00Z'
labels:
  - docs
  - pic
  - parity
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 931000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Coordinator's order for Robert (2026-11-12): a reference specification of the PIC firmware 6.8 at docs/pic/pic-firmware-6.8-spec.md, based on the PIC 6.8 parity audit (TASK-1851, docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md) and on gateway.asm 6.8 itself (OTGW-other-projects/otgw-6.8, repo 770aa45, read-only). Style as docs/sat/thermo-nova-spec.md: every behavioural claim with gateway.asm:line, gaps as 'not found in the code'. Content in the audit's order (a)-(g) plus 6.6 -> 6.8 changes. Describes the PIC, not OT-Direct. A second reader checks 15 random citations and tries to refute them.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 docs/pic/pic-firmware-6.8-spec.md covers (a) frame relay and rewritten fields per MsgID, (b) every serial command with syntax, range, error replies, EEPROM persistence and reply text, (c) PR= and PS=0/1, (d) modes, GW=, alternative slots, PM=, boiler store, Data-Invalid vs Unknown-DataID, blacklisting and MandatoryID, (e) timing, (f) error and status lines, (g) start-up and reset, and a 6.6 -> 6.8 section
- [x] #2 Every behavioural claim carries a gateway.asm:line (or tests/*.stc) citation of a line opened while writing; gaps are listed per section as 'not found in the code / unclear'
- [x] #3 A second reader on the standard model checked 15 randomly drawn citations against gateway.asm and tried to refute them; the result is recorded in the spec and in this task, and any partly-right or refuted item is corrected
- [x] #4 No em dashes, no OTGW32, English
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Method: three readers (general-purpose subagents, one per area: (a)+(d), (b), (c)+(e)-(h)) read gateway.asm 6.8 and the tests/*.stc with the Read tool and wrote sections with citations of lines they opened; the audit was background only. Results: (a)+(d) 343 gateway.asm citation tags + 20 test citations; (b) 52 commands, 466 citations (406 gateway.asm, 60 to 35 test scripts), checked by script to lie in opened ranges; (c),(e)-(h) 457 citations. Assembled by otgw-docs: 1154 lines, 1167 gateway.asm references.

Two code readings that look like PIC defects, found independently by two readers and re-opened in this session: (1) C2= / MsgID8Change calls setbyte3 without loading controlsetpt3 (gateway.asm:2935-2938; setbyte3 writes W at :2688-2691; W = DataID 8 from movfw byte2 at :2539-2540; MsgID 1 loads controlsetpt1 at :2824-2825; same in 6.6 :2927-2930). (2) the fail-safe check after a watchdog reset (gateway.asm:1070-1073) runs with bank 1 selected (:1050) while conf/NoFailSafe is in Bank0data (:315, :516, :522), and FS=0 sets NoFailSafe (:5294-5299) which keeps fail-safe, the reverse of the bit's comment. Both are listed in the spec as questions for Schelte Bron, not as established bugs; no test covers them, nothing was run.

Second reader (separate general-purpose agent, default model, not Fable): 15 citations drawn at random (seed 20261112) from 1131 candidates in sections (a)-(g). Result: 14 confirmed, 1 partly, 0 refuted. Partly: #15 gateway.asm:1594-1596, PrintError runs for errornum non-zero except 0xFF (cleared at :1591-1593), and the 10 ms idle time comes from IDLETIME/PRELOADT0 (:190-191, :203, :775-776). Fixed in the spec after re-opening those lines. Full table in the spec's Verification section.

Conflicts with OTGW-Commands.md found by the (b) reader are listed in spec section b.13 (TT/TC range not enforced, OT limits, CS=0 has no 10-degree default, CL=0 holds 0 %, SB=0 gives SE, RT missing from the reference, and others).

Not verified: nothing was run on a PIC, in gpsim or on a bench; the open points per section stand as written. The FS= polarity and the C2= frame need Schelte or a bench capture.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
docs/pic/pic-firmware-6.8-spec.md (1154 lines): sections (a)-(g) plus (h) 6.6->6.8, open questions for the PIC author (C2= MsgID 8 high byte; FS= bank/polarity), Verification. AC1: all sections present. AC2: 1167 gateway.asm references, each section ends with 'Not found in the code / unclear'. AC3: second reader 14 confirmed / 1 partly / 0 refuted on 15 random citations; the partly item fixed. AC4: grep shows no em dash and no OTGW32.
<!-- SECTION:FINAL_SUMMARY:END -->
