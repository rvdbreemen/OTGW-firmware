---
id: TASK-785
title: Fix unsupported OT message overflow
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-31 16:28'
updated_date: '2026-05-31 16:32'
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Locate the Web UI code that renders unsupported OT / 'Boiler does not implement' messages and identify why long lists overflow. 2. Implement the smallest layout/data change: prefer inline wrapping, and if the existing table cannot support it cleanly, group unsupported OT messages below the table in a readable separate panel. 3. Validate with focused local checks for the affected Web UI assets and update the task AC/DoD/final summary before closure.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Started on branch dev as @codex. User-provided screenshot shows the unsupported OT warning/badge overflowing horizontally instead of presenting a readable list.

Located the issue in src/OTGW-firmware/data/index.html, index.css/index_dark.css, and refreshBoilerSupport() in index.js. The long unsupported list is rendered inside the statistics footer as nested spans, so the generic .ot-log-footer span display rule keeps the dynamic list from wrapping cleanly.
<!-- SECTION:NOTES:END -->
