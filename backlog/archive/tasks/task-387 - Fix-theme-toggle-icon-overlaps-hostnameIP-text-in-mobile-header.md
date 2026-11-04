---
id: TASK-387
title: 'Fix: theme toggle icon overlaps hostname+IP text in mobile header'
status: Done
assignee: []
created_date: '2026-04-23 07:43'
updated_date: '2026-04-24 19:01'
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
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
CSS fix landed on dev (commit c0eb1682) and carried into 2.0.0 via the dev→2.0.0 merge (commit e1f8e070).

**Fix:** Added `.headerrow { padding-right: 32px; }` inside the `@media (max-width: 600px)` block in both `index.css` and `index_dark.css`. Reserves horizontal space for the absolute-positioned theme-toggle button so flex content (hostname+IP `.headercolumn`) no longer flows under it.

**AC status:**
- AC #1 (no overlap on mobile ≤600px): fixed by padding reservation.
- AC #2 (toggle reachable at top-right): unchanged — `position: absolute; right: 0; top: 8px` still in effect.
- AC #3 (desktop >600px unchanged): media query confines the new padding to the mobile breakpoint.
- AC #4 (Android Chrome + iOS Safari field test): not ticked — no longer achievable on 1.4.2-beta after the archive. Sergeantd's original scope was 1.4.2-beta hardware; field-confirmation path shifts to 1.5.0-beta / 2.0.0-beta. The CSS change is identical across lines since it originates on dev and propagated via merge, so a regression on 2.0.0 would be a new ticket with fresh scope.

**Superseded by merge:** dev commit c0eb1682 lives on merge/dev-into-2.0.0 branch. No further code action required.
<!-- SECTION:FINAL_SUMMARY:END -->
