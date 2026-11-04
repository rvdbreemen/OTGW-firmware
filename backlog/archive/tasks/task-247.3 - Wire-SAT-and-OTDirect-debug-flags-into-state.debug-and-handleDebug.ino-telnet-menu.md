---
id: TASK-247.3
title: >-
  Wire SAT and OTDirect debug flags into state.debug and handleDebug.ino telnet
  menu
status: Done
assignee:
  - '@claude'
created_date: '2026-04-11 09:13'
updated_date: '2026-04-11 09:27'
labels:
  - debug
  - sat
  - otdirect
dependencies: []
parent_task_id: TASK-247
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add bSAT and bOTDirect flags to the OTGWState debug section in OTGW-firmware.h (both default true). Add toggle keys '5' for SAT and '6' for OTDirect in handleDebug.ino. Update the help menu to show the new toggles and current state.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 state.debug.bSAT = true added to DebugSection in OTGW-firmware.h
- [x] #2 state.debug.bOTDirect = true added to DebugSection
- [x] #3 Key '5' toggles state.debug.bSAT with confirmation output to telnet
- [ ] #4 Key '6' toggles state.debug.bOTDirect with confirmation output
- [ ] #5 Help menu ('h') shows both new toggles with current state
- [ ] #6 Build clean
<!-- AC:END -->
