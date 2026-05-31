---
id: TASK-773
title: 'fix(webui): expose full boiler unsupported list from truncated badge'
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-31 08:47'
updated_date: '2026-05-31 08:48'
labels:
  - webui
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Make the yellow 'Boiler does not implement' badge remain compact while still letting users discover the full unsupported message list when it is visually truncated. Prefer a small tooltip-style/readout interaction using the existing WebUI patterns and avoid broad layout changes.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The compact yellow badge can still truncate long lists without expanding the Statistics footer layout.
- [ ] #2 A user can discover and read the full unsupported list from the badge, including keyboard/focus access where practical.
- [ ] #3 The empty-list behavior remains unchanged: the badge stays hidden when unsupported_read and unsupported_write are both empty.
- [ ] #4 The implementation is limited to the existing WebUI files needed for the badge behavior and does not introduce a new framework.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Inspect the current badge markup, styling, and update function around #boilerUnsupportedLine.
2. Add a narrow tooltip/readout path that exposes the full generated text while preserving the compact ellipsis badge.
3. Verify the relevant source snippets and, where practical, run the existing design-system/UI checks for the touched files.
4. Update acceptance criteria and final summary when complete.
<!-- SECTION:PLAN:END -->
