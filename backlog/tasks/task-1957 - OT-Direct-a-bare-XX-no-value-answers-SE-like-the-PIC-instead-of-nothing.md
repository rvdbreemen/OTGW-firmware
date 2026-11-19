---
id: TASK-1957
title: 'OT-Direct: a bare XX= (no value) answers SE like the PIC instead of nothing'
status: Done
assignee:
  - '@claude'
created_date: '2026-11-15T04:22:00Z'
updated_date: '2026-11-15T10:28:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1001000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by 09 on the TASK-1859 bench (beta.294+67e3f51, .61, item 3): 'CS=' gets no reply on OT-Direct; the effect is right (nothing written). Cause: handleOTDirectCommand() returns false at 'if (len < 4) return false;' (OTDirect.ino, entry) before any branch, so every bare 'XX=' is silent. The PIC answers SE for a missing value where its parser needs one (tests/command-4.stc: 'CS=' -> SE) and clears for commands whose empty value means clear (OT=, MM=, CL=: ValueCleared). The TASK-1859 host test fed value='' into the cut-out branch and did not go through this entry check.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A bare XX= reaches the command's branch: SE where the PIC parser refuses an empty value (CS=, C2=, TT=, TC=, SB=, SW=, SH=, BS=, RT=), the clear reply where the PIC clears (OT=, MM=, CL=), per command with the gateway.asm citation
- [x] #2 Host test through handleOTDirectCommand() itself (not a cut-out branch): red on the current code, green on the fix
- [x] #3 Build, evaluate --quick, bench on .61 by 09
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
95 (2026-11-15, lane-95-next): handleOTDirectCommand() now accepts len 3 ('XX=') and lets it through only for otdPicBareValueCmd(): CS, C2, BS, CC, RT, TT, TC, SB, SW, SH (their PIC parser answers an empty value with SE: GetDecimal :5045-5049 needs a digit; tests/command-4.stc 'CS=' -> SE) and OT, MM, CL (they clear: ClrOutsideT :4263, ClrMaxModLevel :4806, ClrCoolLevel :4463, 'XX: -'). Other bare commands stay unanswered as before (not audited per PIC handler; out of scope). The list is a switch, not a PROGMEM table: a PROGMEM string in the shared otdPicFloat block broke every harness that compiles it (MSVC 'kCmds unknown size'). Test tests/test_otd_bare_commands.py compiles handleOTDirectCommand() from its first line (entry checks included) through OT=, plus CL= and SB=: 4/4 green; on origin/dev c2cd68b16 2 FAIL (bare CS= etc. no SE, OT=/MM=/CL= no clear) = the 09 bench finding. Sweep 50/50 (test_otd_*, sat_command_delivery, pm_chain, prototype_hoisting, test_evaluate); evaluate --quick 86/0. AC3 (build + bench) pending: no firmware push before the 10:00 release; build slot after d3's 295.

Bench 09 on .61 (OT-Direct, no boiler, no thermostat), beta.298+bb38dc5 (bench-only build of lane-95-next on c2cd68b16, 2026-11-15 07:25, exit 0, RAM 130572 B, Flash 2156679 B): items 1-5 PASS: CS= -> SE, TT=/SW= -> SE, OT= -> 'OT: -', MM= -> 'MM: -', GW= stays silent. evaluate --quick 86/0.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
beta.296. AC1-AC3: tests/test_otd_bare_commands.py 4/4 through the real handleOTDirectCommand entry (2 FAIL on the old code); bench 09 .61 beta.298 items 1-5 PASS (CS= -> SE, TT=/SW= -> SE, OT= -> 'OT: -', MM= -> 'MM: -', GW= silent) and re-checked on beta.301. Pushed origin/dev 2e4ca9c05..3e29faa63; zip beta.301+2b3d9a2 posted (Discord 1571471374038470711); final build exit 0 'Build completed successfully', RAM 130572 B, Flash 2157483 B; sweep 88/88, evaluate --quick 86/0; bench 09 on .61 beta.301+2b3d9a2 PASS (smoke + re-check).
<!-- SECTION:FINAL_SUMMARY:END -->
