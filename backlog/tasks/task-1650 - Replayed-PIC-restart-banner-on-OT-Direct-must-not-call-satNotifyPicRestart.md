---
id: TASK-1650
title: Replayed PIC restart banner on OT-Direct must not call satNotifyPicRestart
status: To Do
assignee: []
created_date: '2026-11-05T06:58:00Z'
labels:
  - bug
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 774000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by TASK-1412.20 (R-4): OTGW-Core.ino:6188 calls satNotifyPicRestart() for a replayed banner before the !isOTDirectEnabled() guard. Robert 2026-11-05: bug task.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 On OT-Direct a replayed PIC banner does not call satNotifyPicRestart(); on a PIC board a real banner still does
- [ ] #2 Host test red on old / green on fix; evaluate --quick green
<!-- AC:END -->
