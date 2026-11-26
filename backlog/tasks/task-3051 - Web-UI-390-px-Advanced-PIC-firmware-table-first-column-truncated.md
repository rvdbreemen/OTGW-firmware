---
id: TASK-3051
title: 'Web UI 390 px: Advanced PIC firmware table first column truncated'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-25T10:04:00Z'
updated_date: '2026-11-26T09:10:00Z'
labels:
  - webui
  - ux
milestone: 2.0.0
dependencies: []
ordinal: 1154000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen in the bench screenshot S12 (OTGW-field-data/bench-63/2026-11-25_beta-candidate/screenshots/S12-light-390.png): at 390 px the file table on Advanced > PIC firmware cuts the first column (FIRMWARE NAME) to one letter plus an ellipsis (d..., g..., i...) so the files cannot be told apart. Not a beta blocker; existing behaviour.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 At 390 px the PIC firmware file names are readable (wrap, scroll or stack) without hiding the version and the action buttons
- [x] #2 Browser test at 390 px; screenshot light and dark
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fix: v2.css, @media max-width 719px stacks .picrow (name on its own wrapping line, version/size/3 buttons under it, column heading row hidden). Root cause: six fixed columns left the name column 30 px at 390 px. Test tests/webui/pic-table-390-3051.test.mjs: 4 pass on the fix, 2 fail on origin/dev data (name column 30 px, name cut), 1400 px green on both. evaluate --quick 88 pass/0 fail; webui suite 109 files fail 0. Host screenshots (synthetic data, headless Chrome) in OTGW-field-data/webui-3051-host-shots/{before,after}. AC2 screenshots on the bench unit still open (bench-tester).

R1 bench .63: 390 px light/dark screenshots S12-{light,dark}-{390,1400}.png in OTGW-field-data/bench-63/2026-11-25_cand-0827/screenshots-3051 (synthetic data).
<!-- SECTION:NOTES:END -->
