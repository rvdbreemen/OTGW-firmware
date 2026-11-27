---
id: TASK-2926
title: >-
  Web UI: Advanced > File System table runs past a 375 px phone screen (page
  scrolls sideways)
status: In Progress
assignee:
  - '@otgw-ux-designer'
created_date: '2026-11-19T19:18:00Z'
updated_date: '2026-11-27T05:25:00Z'
labels:
  - webui
  - ui
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1061000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the read-only UI walk of 396 (7e68542) on .63, 2026-11-19, headless Chrome with mobile emulation 375x812: on Advanced > File System the document is 470 px wide in English and 519 px in Dutch (scrollWidth vs clientWidth 375); the file list (name, size, Download column) is not in a horizontal scroll container, so the whole page scrolls sideways and a modal (No boiler detected) is centred on the wider layout and cut off at the right. All other pages and sub-tabs (Home, SAT simple/control/technical, Monitor x6, Settings x11 categories, Advanced PIC/Debug/System) stay at 375 px in EN and NL. Screenshot: ba scratchpad uiwalk/en375/advanced-files.png. Not blocking for 2.0.0 (Advanced page, rarely used on a phone).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 On a 375 px mobile viewport Advanced > File System has no horizontal page scroll (scrollWidth <= clientWidth) in EN and NL; long file names wrap or the table scrolls inside its own container
- [x] #2 Web UI test on the real v2.js with mobile emulation: red on the code before, green after
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Side note (coordinator: no own task): at 375 px the sub-tab strips scroll horizontally but do not bring the active tab into view (Greek UI: the active 'System' tab under Advanced sits off-screen to the right). Seen on the mock and on .61. Same walk on .61 (397, German): Advanced > File System 510 px wide, so the overflow is there in DE too (EN 470, NL 519).

2026-11-27 otgw-ux-designer: fix in v2.css only: .fstable .fn{overflow-wrap:anywhere} plus @media(max-width:480px){.fstable td{padding:8px 6px}}. New test tests/webui/fs-table-375-2926.test.mjs (real v2.html/v2.js/v2.css, synthetic file names incl. a 70-character name, 375x812, EN and NL). Old vs fix: with the css from HEAD (OTGW_DATA_DIR) both fail: scrollWidth 382 > clientWidth 360; with the fix both pass (node --test, 2 pass 0 fail). Screenshots before/after EN and NL in docs/evidence/2026-11-27_ux-polish/2926/. Not verified: desktop width not re-shot for this task; the 'No boiler detected' modal cut-off from the report was not reproduced separately (it followed the wide layout); a real device was not used.

2026-11-27 otgw-ux-designer: desktop check added (test case at 1400 px: no sideways scroll, a short name stays on one line, row height under 60 px). Old css (HEAD copy via OTGW_DATA_DIR): 1400 px PASS (layout unchanged), 375 px EN and NL FAIL. Fix: 3 of 3 PASS. Screenshots at 375 and 1400 px before and after in docs/evidence/2026-11-27_ux-polish/2926/. Waits for the web UI commit with the prerelease bump after the release commit, then a push slot.
<!-- SECTION:NOTES:END -->
