---
id: TASK-968
title: 'v2 parity: FSexplorer link + PIC firmware flash entry in Advanced'
status: To Do
assignee: []
created_date: '2026-06-30 23:09'
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
- [ ] #1 v2 Advanced links to /FSexplorer
- [ ] #2 PIC flash entry present on pic-capable boards, hidden on OTGW32
<!-- AC:END -->
