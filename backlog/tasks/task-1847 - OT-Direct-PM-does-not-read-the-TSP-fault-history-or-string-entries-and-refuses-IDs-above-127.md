---
id: TASK-1847
title: >-
  OT-Direct: PM= does not read the TSP, fault-history or string entries, and
  refuses IDs above 127
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-12T00:01:00Z'
updated_date: '2026-11-15T06:32:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 926000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N13), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC: after PM=10 (also 12, 88, 90, 105, 107) the answer's count starts reads of every entry of the next ID (G:3364-3378, :3389-3397, index G:3464-3470); PM=93/94/95 reads every string character (G:3399-3407); range 0-255 (G:5026-5037). tests/tsp-1.stc, str-1.stc. Command ref describes this.
- OT-Direct: one READ with data 0, or a schedule entry forced due; IDs > 127 -> BV (O:3610-3632).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 host test: PM=10 against a mock boiler with 3 TSPs produces the R frames of tsp-1.stc; red on current code
- [x] #2 PM=93 reads all characters
- [x] #3 PM=131 accepted
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): interaction with the PM slot rule of TASK-1572 and TASK-1767.

Part of the PIC 6.8 parity audit, parent TASK-1851.

95 (2026-11-15, lane-95-next): PM slot rule checked first (TASK-1572: one PIC priority slot, the request only in an alternative slot; TASK-1767 open): on OT-Direct the PM= read and its follow-ups go through otCmdEnqueue(), which in gateway mode with a thermostat sends only in an alternative slot (otdAltSlotTaken) and otherwise on the free bus, so the same rule holds. New '// --- otdPmChain' block (otdPmStart, otdPmOnAnswer) mirrors SetPrioMessage :5026-5038, TSPBufferSize :3363-3378 (valid = type bit 5 clear, count > 0), TSPReadEntry :3389-3397, StringCharacter :3399-3407, SendPriorityMsg :3464-3470 (data <index>:0). handleMasterResponse() feeds it the answers to the gateway's own requests only (origin GATEWAY = the PIC's AlternativeUsed), before the origin reset; the next request goes to otCmdEnqueue(). PM= parses GetDecimal: 0-255, SE without a digit or with trailing characters, OR above 255 (the old atoi wrapped PM=256 to PM=0). Test tests/test_otd_pm_chain_tsp.py (real block + real PM= branch, stand-in boiler with the .stc data): 6/6; on origin/dev c2cd68b16 4 FAIL (PM=10 only R000A0000, PM=93 only R805D0000, PM=131 BV, PM=256 -> 0). AC1: frames R000A0000 R800B0000 R000B0100 R000B0200 = tsp-1.stc; AC2: R805D0000 R005D0100 R005D0200 R805D0300 = str-1.stc; AC3: PM=131 accepted. Pins updated: test_sat_no_return_startup (PM=200 now accepted, PM=256 OR), host test_override_reply (otCmdEnqueue stub). Sweep 51 test_otd_*/sat/pm_chain/hoisting/evaluate + 8 other OTDirect slicers exit 0; evaluate --quick 86/0. Not verified: a real boiler's TSP answers (bench .61 has none).

Bench 09 on .61 (OT-Direct, no boiler, no thermostat), beta.298+bb38dc5 (bench-only build of lane-95-next on c2cd68b16, 2026-11-15 07:25, exit 0, RAM 130572 B, Flash 2156679 B): PM=131 -> 'PM: 131' with one READ 131, PM=256 -> OR, PM=10x -> SE: PASS. NOT benched (field / host only): the indexed follow-up reads of PM=10 / PM=93 need a boiler with TSPs or strings; tests/test_otd_pm_chain_tsp.py reproduces tsp-1.stc and str-1.stc.
<!-- SECTION:NOTES:END -->
