---
id: TASK-705
title: >-
  fix(webui): Statistics table column proportions — Description too narrow,
  Value too wide
status: Done
assignee:
  - '@claude'
created_date: '2026-05-25 20:39'
updated_date: '2026-05-25 20:48'
labels:
  - bug
  - webui
  - ux
dependencies: []
references:
  - 'Discord #beta-testing'
  - user Simon Templar
  - '2026-05-25'
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by Simon Templar on #beta-testing (2026-05-25) running beta.21. The Statistics table has a small Description column and an oversized Value column, making it hard to read. Improve column proportions so Description is readable at typical browser widths.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Statistics table Description column is wide enough to show typical sensor names without truncation at 1280px browser width
- [x] #2 Value column does not dominate the table width disproportionately
- [x] #3 Fix uses CSS only (no JS changes); no regressions on mobile/narrow viewport
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Swap Description (200px fixed) and Value (auto) column widths in .ot-stats-table\n2. Description krijgt auto (groeit mee), Value krijgt 180px fixed — logischer want values zijn kort (23.5 C, ON, 107 hrs)\n3. CSS-only change in index.css
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented: swapped Description (was 200px) en Value (was auto) in .ot-stats-table CSS — Description krijgt nu auto (groeit mee met tabel), Value krijgt 180px fixed. index.css alleen.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
CSS-only fix: swapped Description (was 200px fixed → nu auto) en Value (was auto → nu 180px fixed) in .ot-stats-table. Description krijgt nu de flexibele ruimte; values zijn kort genoeg voor een vaste breedte. Commit 00a332db. Build groen, evaluator 100%.
<!-- SECTION:FINAL_SUMMARY:END -->
