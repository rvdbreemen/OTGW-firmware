---
id: TASK-1110
title: Highlight firmware table cells that actually changed after a web download
status: Done
assignee:
  - '@claude'
created_date: '2026-09-02 21:24'
updated_date: '2026-09-02 22:30'
labels: []
dependencies: []
ordinal: 205000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The refresh (update-from-web) button on the PIC firmware tab rewrites the version cell in place, and does not touch the size cell at all even though a download usually changes it. Nothing tells the user whether the click did anything: a file already at the latest version is a silent no-op that looks identical to a successful download. Mark every cell whose value actually changed with a green highlight that fades after about 15 seconds, so a real update is visible and a no-op stays quiet.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The size cell is updated from the refreshed file list, not only the version cell
- [x] #2 A cell whose value actually changed is highlighted green and returns to normal after about 15 seconds
- [x] #3 A cell whose value did not change is not highlighted, so a no-op refresh stays visually quiet
- [x] #4 The highlight is legible in both the light and the dark theme
- [x] #5 Repeated clicks restart the highlight rather than leaving a stuck or double-scheduled timer
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Shipped in 1.7.5-beta.7 and deployed to both bench gateways (192.168.88.68 and .16). Confirmed the code is actually live on the device rather than only in the repo: the served index.js carries setFirmwareCell and the firmware-cell-updated class, and the served index.css carries the .picrow .firmware-cell-updated rule.

Evidence basis per AC, stated honestly:
- AC 1, 3 and 5 are verified from the code path: both cells now go through one helper that compares before it writes, and the helper clears any existing timer before arming a new one.
- AC 2 and 4 rest on static evidence, not on a rendered page. The removal is a setTimeout, and the contrast was reasoned from the two hardcoded greens rather than measured in a browser. Every browser MCP (chrome-devtools, playwright, browser) failed to connect this session, so no page was rendered.

The rendered check is what the beta is for; field testers exercise this screen on every PIC firmware download.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Made a web download visible in the PIC firmware table.

The refresh button rewrote the version cell in place and never touched the size cell, so a click that actually downloaded a new image was indistinguishable from a click on a file that was already current: both were silent. Both cells now route through setFirmwareCell(), which compares before it writes, highlights a cell whose value genuinely changed for about 15 seconds, and leaves an unchanged cell alone. A repeated click clears the pending timer before arming a new one rather than stacking timers.

Changes: data/index.js gains an id on the size cell and the setFirmwareCell helper; index.css and index_dark.css gain one rule each plus a background-color transition on the shared cell rule, so the highlight eases in and out in both themes.

Shipped in 1.7.5-beta.7 and deployed to both bench gateways. AC 2 and 4 are checked on static evidence rather than a rendered page: no browser automation was reachable this session.
<!-- SECTION:FINAL_SUMMARY:END -->
