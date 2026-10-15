---
id: TASK-1230
title: 'v2: settings safety: fixed-IP validation, board-mode confirm, webhook OFF test'
status: To Do
assignee: []
created_date: '2026-10-15 05:09'
labels:
  - webui
  - 2.0.0
  - v2
milestone: m-2
dependencies: []
priority: medium
ordinal: 368000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up port from the classic UI (decided 2026-10-15: core ports first, these after; not blocking TASK-1223). Gap audit docs/research/2026-10-15-classic-vs-v2-gap-audit.md rows 44, 45, 48: a typo in a static IP locks the user out; changing Board mode wrongly leaves the gateway non-functional; webhook test only has ON.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Fixed-IP fields are validated per octet and prefilled from the current lease
- [ ] #2 Changing Board mode asks for confirmation, and the webhook OFF test call exists
- [ ] #3 Bench: works on the combo unit with 0 console errors (screenshot + console log)
<!-- AC:END -->
