---
id: TASK-785
title: Fix unsupported OT message overflow
status: To Do
assignee: []
created_date: '2026-05-31 16:28'
labels:
  - webui ui
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Long unsupported OpenTherm status messages, such as 'Boiler does not implement' with many message IDs, currently overflow the badge/table area. Make the Web UI present these messages cleanly and understandably without breaking the table layout.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Long 'Boiler does not implement' / unsupported OT messages do not overflow the table or badge area on normal desktop widths.
- [ ] #2 If inline wrapping is insufficient, unsupported OT messages are grouped in a clear separate area below the table.
- [ ] #3 Existing OT support/status table behavior remains intact.
- [ ] #4 Relevant implementation is validated with a focused local check.
<!-- AC:END -->
