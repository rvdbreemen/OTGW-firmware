---
id: TASK-1958
title: >-
  Web UI Overrides panel: clearing the cooling level sends CL=0 (sets 0 %)
  instead of a clear
status: In Review
assignee:
  - '@claude'
created_date: '2026-11-15T04:26:00Z'
updated_date: '2026-11-15T10:26:00Z'
labels:
  - webui
  - otdirect
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1002000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by 09 on the TASK-1859 bench: the Overrides panel clear action for CL sends 'CL=0' (data/v2.js ~3847). On the PIC (SetCoolLevel gateway.asm 6.8:4454-4464, TASK-1833) and on OT-Direct a number, 0 included, is held as the cooling level; only a value that is not a number clears it ('CL: -'). So the UI clear sets 0 % instead of removing the override.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The clear action sends a value that clears (e.g. CL=-) and the panel shows the override gone
- [x] #2 Web UI test (headless Chrome) red on the current v2.js, green on the fix
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 fix: data/v2.js OTD_WRITE_CMD[7].clear 'CL=0' -> 'CL=-'. OT-Direct (OTDirect.ino CL= handler, otdPicFloatF88 with OTD_PIC_PERCENT): a '-' is no sign there, so it is a parse error (parseErr true) and lands in clearWriteOverride(7) with the reply 'CL: -', as the PIC SetCoolLevel clears on a parse error. Evidence: tests/webui/otd-override-clear.test.mjs (headless Chrome, real v2.html/v2.js against a fake OT-Direct device), CLEARS[7] corrected to 'CL=-' (the test pinned the bug): 5 pass on the fix; on the old data/ (git archive HEAD) 1 fail 'MsgID 7 Clear sends CL=-'. AC1 'the panel shows the override gone' is the firmware's clearWriteOverride(7) after the queued command; not seen on a bench.

Correction (same session): AC1 unticked and status In Review: the panel showing the override gone needs the bench (the web UI test proves the command sent, not the firmware clearing it on a unit). To check in the next bench run on .61 (OT-Direct): set CL=50, Clear, the MsgID 7 row disappears.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: the Clear action for MsgID 7 now sends CL=-, which the OT-Direct handler treats as a parse error and clears (clearWriteOverride(7), reply 'CL: -'), as the PIC SetCoolLevel; the row disappearing after the list is re-read was not checked on a bench. AC2: tests/webui/otd-override-clear.test.mjs (headless Chrome) 5 pass on the fix, 1 fail on the old v2.js (MsgID 7 Clear sends CL=-).
<!-- SECTION:FINAL_SUMMARY:END -->
