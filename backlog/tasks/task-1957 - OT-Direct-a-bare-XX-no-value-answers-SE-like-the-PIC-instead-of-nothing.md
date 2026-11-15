---
id: TASK-1957
title: 'OT-Direct: a bare XX= (no value) answers SE like the PIC instead of nothing'
status: To Do
assignee: []
created_date: '2026-11-15T04:22:00Z'
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
- [ ] #1 A bare XX= reaches the command's branch: SE where the PIC parser refuses an empty value (CS=, C2=, TT=, TC=, SB=, SW=, SH=, BS=, RT=), the clear reply where the PIC clears (OT=, MM=, CL=), per command with the gateway.asm citation
- [ ] #2 Host test through handleOTDirectCommand() itself (not a cut-out branch): red on the current code, green on the fix
- [ ] #3 Build, evaluate --quick, bench on .61 by 09
<!-- AC:END -->
