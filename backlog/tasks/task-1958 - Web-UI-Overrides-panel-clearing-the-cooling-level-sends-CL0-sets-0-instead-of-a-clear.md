---
id: TASK-1958
title: >-
  Web UI Overrides panel: clearing the cooling level sends CL=0 (sets 0 %)
  instead of a clear
status: To Do
assignee: []
created_date: '2026-11-15T04:26:00Z'
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
- [ ] #2 Web UI test (headless Chrome) red on the current v2.js, green on the fix
<!-- AC:END -->
