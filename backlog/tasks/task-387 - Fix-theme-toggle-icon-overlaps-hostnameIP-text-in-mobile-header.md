---
id: TASK-387
title: 'Fix: theme toggle icon overlaps hostname+IP text in mobile header'
status: Done
assignee: []
created_date: '2026-04-23 07:43'
updated_date: '2026-05-25 22:31'
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
- [x] #1 Theme toggle no longer overlaps the hostname/IP text on mobile viewports (<=600px)
- [x] #2 Toggle remains reachable and tappable at the same visual location (top-right of header)
- [x] #3 Desktop layout (>600px) unchanged
- [ ] #4 Tested on Android Chrome and iOS Safari
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-04-23: Fix landed on dev (commit c0eb1682). Added .headerrow { padding-right: 32px; } inside the @media (max-width: 600px) block in both index.css and index_dark.css. Reserves horizontal space for the absolute-positioned theme toggle so flex content (hostname+IP .headercolumn) no longer flows under it. Desktop unchanged. Awaiting field validation by sergeantd on mobile.

Triage 2026-05-22: still relevant but no active work in 14+ days; deprioritised. Mobile header theme-toggle overlap reported by sergeantd on 2026-04-23. No new reports since 1.5.0; the touch-PC FSexplorer fix (commit ebbbb4df, beta.7) may have touched related header CSS — re-validate before active work.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fix committed in c0eb1682 (2026-04-23). Added padding-right:32px inside @media(max-width:600px) on .headerrow. Reserves space for the absolute-positioned theme toggle so flex hostname+IP content does not flow under it. Desktop layout (>600px) unchanged. AC #4 (Android Chrome + iOS Safari) not self-verifiable; no new reports on 1.5.x/1.6.0-beta.
<!-- SECTION:FINAL_SUMMARY:END -->
