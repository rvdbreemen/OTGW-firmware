---
id: TASK-387
title: 'Fix: theme toggle icon overlaps hostname+IP text in mobile header'
status: To Do
assignee: []
created_date: '2026-04-23 07:43'
updated_date: '2026-04-23 07:43'
labels:
  - bug
  - ui
dependencies: []
references:
  - 'Discord #beta-testing'
  - user sergeantd
  - '2026-04-23 07:11Z'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Discord #beta-testing (sergeantd, 2026-04-23 07:11Z, screenshot attached): on mobile (<=600px viewport), the theme toggle icon in the header row sits on top of the rightmost .headercolumn text (hostname + IP). Verified via screenshot from c46861c8 build: the moon/sun icon overlaps the closing paren of '10.0.254.112)'. Root cause: @media (max-width: 600px) makes .theme-toggle-btn 'position: absolute; right: 0; top: 8px', so it pins to the top-right of the header while the hostname+IP flex-item also ends up there. User reports this is a regression but the .theme-toggle-btn CSS last changed 2026-03-26 and no recent CSS commits touched header layout; still, the bug is visible and fixable.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Theme toggle no longer overlaps the hostname/IP text on mobile viewports (<=600px)
- [ ] #2 Toggle remains reachable and tappable at the same visual location (top-right of header)
- [ ] #3 Desktop layout (>600px) unchanged
- [ ] #4 Tested on Android Chrome and iOS Safari
<!-- AC:END -->
