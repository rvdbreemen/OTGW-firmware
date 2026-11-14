---
id: TASK-1851
title: PIC 6.8 parity audit (2026-11-12)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-12T00:08:00Z'
updated_date: '2026-11-14T15:32:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 930000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Parent of the tasks filed from the strict PIC 6.8 vs OT-Direct parity audit of 2026-11-12, done read-only on Robert's order (review: docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md; reference spec of the PIC: docs/pic/pic-firmware-6.8-spec.md). The audit found 60 differences: 38 already covered by tasks or ADRs, 6 corrections to existing tasks (notes added on TASK-1801, 1799, 1760, 1750, 1804, 1805, 1800) and 16 new tasks:

- N1: TASK-1831
- N2: TASK-1832
- N3: TASK-1833
- N4: TASK-1834
- N5: TASK-1835
- N6: TASK-1836
- N7: TASK-1839
- N8: TASK-1840
- N9: TASK-1842
- N10: TASK-1844
- N11: TASK-1845
- N12: TASK-1846
- N13: TASK-1847
- N14: TASK-1848
- N15: TASK-1849
- N16: TASK-1850

Successor of the 6.6 audit (TASK-1795, docs/reviews/2026-11-11_otdirect-pic-parity-audit/).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Every listed task is Done, or closed with a recorded reason (decision, duplicate, intentional difference)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13 delta parity check (TASK-1851, comms, read-only, OT-Direct at origin/dev 479e49391 vs PIC 6.8 gateway.asm; G=gateway.asm, O=OTDirect.ino): Commits since the audited fa5790118: matches PIC: c76efbe01 (1832), 1d4b8fdac (1831), 60c0fbc50 (1833), a9df4868f (1840), 9aa81609c (1846), 2216ea923 (1855), 8f6924552 (1842), 60762c994 (1844); partial: e7fbf7bf5 (1845, misses Data-Invalid, new task pending), c4067636a (1835), af1fdb740 (1839, real GW= change still wipes all state O:2920-2921), 20d61556e (1836, wiped on the auto-detect master path, see 1760); intentional deviation: 47f9edd84 (1860, PR=X answers X=6.8, PIC answers BV G:2187). Open children: 1834 still valid (CS/C2 expire at 60 s O:518 vs SC 63.5 s O:525; one PIC timer G:1623-1646); 1839 AC2/AC3 still valid; 1847 still valid (PM= refuses IDs >127, no TSP/string entries O:3916-3935; uint8_t atoi makes PM=300 -> 44); 1848 still valid (P,R,B,C,L O:3795-3820, :623); 1849 partly fixed; 1850 still valid (disconnect only logs, O:3003-3010). Also still valid: 1750 (OH default 0, O:621), 1859 (floatToF88 truncates/clamps, O:503-507), 1861. Untracked from REVIEW: N11 thermostat's own MsgID 99 (G:3253-3271) has no task. Two new gaps verified by comms in both sources: C1 boiler Data-Invalid to MsgID 99 does not start the spoofed DHW push (PIC: Data-Invalid nop falls into ID99Unknown G:3227-3230; O: otdPushOnBoiler99 only called in the success branch O:1865, Data-Invalid branch O:1875-1877, type 6 dropped O:2687) and C2 SC= accepts times/days the PIC refuses (PIC G:4331-4370: hours>23 OR, minutes>59 OR, missing :/ SE, day 0 or >7 BV, trailing chars SE; O:3492 siscanf>=2, masks O:3494). Filing C1/C2 waits until TASK-1905 (main tree) is on origin, to avoid a task-id collision.

2026-11-13 filed from the delta check: TASK-1906 (N11 orphan, thermostat's own MsgID 99), TASK-1907 (C1, boiler Data-Invalid to MsgID 99 does not start the spoofed DHW push), TASK-1908 (C2, SC= accepts times/days the PIC refuses).

Child status 2026-11-14 (comms, stale audit): Done 9 (1831, 1832, 1833, 1836, 1840, 1842, 1844, 1846, + none else); In Review 4 (1834 AC 3/3, 1835 AC 2/3, 1845 AC 3/4, 1907 AC 1/2); In Progress 1 (1839, Robert answered A in DM 1570230662743592961, implementation queued with d3 after TASK-1924 Part 2); To Do without owner 6 (1847 PM TSP/fault history, 1848 PR texts, 1849 success/clear replies, 1850 thermostat (dis)connected lines, 1906 thermostat's own MsgID 99, 1908 SC= validation). AC1 cannot be ticked until those close; the parent stays In Progress.

Correction to the line above: Done is 8, not 9 (1831, 1832, 1833, 1836, 1840, 1842, 1844, 1846); 8 + 4 + 1 + 6 = 19 children incl. 1906-1908.

Assignment of the 6 open children (coordinator 2026-11-14; owners claim them themselves): 95 -> TASK-1848 (PR texts) + TASK-1849 (success/clear replies); ba -> TASK-1850 (thermostat (dis)connected lines) + TASK-1908 (SC= validation), after TASK-1937; d3 -> TASK-1847 (PM TSP/fault history) + TASK-1906 (the thermostat's own MsgID 99), after TASK-1924 Part 2 and TASK-1839.
<!-- SECTION:NOTES:END -->
