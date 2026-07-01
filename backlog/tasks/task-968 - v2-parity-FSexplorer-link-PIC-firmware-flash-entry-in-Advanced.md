---
id: TASK-968
title: 'v2 parity: FSexplorer link + PIC firmware flash entry in Advanced'
status: To Do
assignee: []
created_date: '2026-06-30 23:09'
updated_date: '2026-07-01 04:52'
labels: []
dependencies: []
ordinal: 180000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verified gap (grep 0 hits in v2): Classic Advanced links to /FSexplorer and the PIC firmware flash page. v2 exposes neither. PIC flash N/A on OTGW32 (gate behind HAS_PIC).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 v2 Advanced links to /FSexplorer
- [ ] #2 PIC flash entry present on pic-capable boards, hidden on OTGW32
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Done: FSexplorer + Flash-Utility (/update) links added to the v2 Debug tab Tools section (verified on .39: links present, /FSexplorer + /update both 200). DEFERRED (AC#2): the PIC firmware flash UI is an in-Classic-UI hex-upload flow with no standalone URL to link to, and it is pic-only (N/A on the OTGW32 test hardware). Porting the full PIC-flash upload flow into v2 is a separate feature — recommend a dedicated task rather than a link stub.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2 parity: added a Tools section to the Debug tab with links to the File System explorer (/FSexplorer) and the Flash Utility (/update) — the admin-links gap vs the Classic Advanced menu. Verified on .39 (links render, targets 200, 0 errors). PIC firmware flash deferred: it is a pic-only hex-upload flow with no deep-linkable URL; porting the full flow to v2 is tracked as a separate follow-up (not applicable on OTGW32).
<!-- SECTION:FINAL_SUMMARY:END -->
