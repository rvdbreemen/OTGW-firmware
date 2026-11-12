---
id: TASK-1848
title: 'OT-Direct: PR= reports that are not the PIC''s text'
status: To Do
assignee: []
created_date: '2026-11-12T00:02:00Z'
updated_date: '2026-11-12T00:11:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 927000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N14), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

| Letter | PIC | OT-Direct |
|---|---|---|
| P | Low power / Medium power / High power (G:2261-2268, :5499-5502) | fixed P=M (O:3508-3511); library knows the level (OpenTherm.cpp:540-544) |
| R | D by default; S, R, C, I (G:2270-2280) | fixed R=I (= iSense) (O:3512-3515); FT= ignored |
| O | lowercase t/c until thermostat accepted; n while clearing (G:2201-2226) | always uppercase (O:3446-3475) |
| C | C=4 MHz (G:5506-5507) | C=240 (O:3520-3523) |
| B | HH:MM DD-MM-YYYY (tools/build.tcl:83) | __DATE__ (O:3516-3519) |
| L | default FXOMPC (G:5462-5468) | default XXXXXX (O:537) |
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 host test per letter, byte-exact; red on current code
- [ ] #2 PR=P follows the real power level
- [ ] #3 decision for C, B and L recorded (PIC text or agreed OT-Direct value)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): which clients parse P, R, C, B; pyotgw only stores them.

Part of the PIC 6.8 parity audit, parent TASK-1851.
<!-- SECTION:NOTES:END -->
