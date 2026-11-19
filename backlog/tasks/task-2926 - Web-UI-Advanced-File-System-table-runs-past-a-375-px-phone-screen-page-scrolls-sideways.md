---
id: TASK-2926
title: >-
  Web UI: Advanced > File System table runs past a 375 px phone screen (page
  scrolls sideways)
status: To Do
assignee: []
created_date: '2026-11-19T19:18:00Z'
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
- [ ] #1 On a 375 px mobile viewport Advanced > File System has no horizontal page scroll (scrollWidth <= clientWidth) in EN and NL; long file names wrap or the table scrolls inside its own container
- [ ] #2 Web UI test on the real v2.js with mobile emulation: red on the code before, green after
<!-- AC:END -->
