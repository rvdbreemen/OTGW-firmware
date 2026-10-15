---
id: TASK-1227
title: 'v2: deeper OT log buffer or capture mode'
status: To Do
assignee: []
created_date: '2026-10-15 05:07'
labels:
  - webui
  - 2.0.0
  - v2
milestone: m-2
dependencies: []
priority: medium
ordinal: 365000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up port from the classic UI (decided 2026-10-15: core ports first, these after; not blocking TASK-1223). Gap audit docs/research/2026-10-15-classic-vs-v2-gap-audit.md row 21: classic keeps 2000 lines (capture mode up to 1M sized to storage quota); v2 keeps 600 (v2.js:821) and downloads only those.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 v2 keeps at least 2000 log lines by default and offers a capture mode for longer sessions
- [ ] #2 Download saves the whole buffer
- [ ] #3 Bench: works on the combo unit with 0 console errors (screenshot + console log)
<!-- AC:END -->
