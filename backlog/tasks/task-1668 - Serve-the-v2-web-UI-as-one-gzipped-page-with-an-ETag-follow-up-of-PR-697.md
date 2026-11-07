---
id: TASK-1668
title: 'Serve the v2 web UI as one gzipped page with an ETag (follow-up of PR #697)'
status: Done
assignee: []
created_date: '2026-11-05T23:31:00Z'
updated_date: '2026-11-07T23:06:00Z'
labels:
  - webui
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 791000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-06: after the PR #697 request queue lands, serve / as one /v2.html.gz (CSS, JS, fonts inlined) with Content-Encoding gzip and an ETag, as Andre measured (PR #697 comment 6670567252: 170,558 B, first load 1.17 s, reload 304 in 0.13 s). Needs its own ADR superseding ADR-192; FSexplorer stays self-contained; the safeboot update page is out of scope.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Baseline: today's multi-file first-load and reload time measured on the bench
- [x] #2 ADR (supersedes ADR-192) Accepted by Robert
- [x] #3 Build step inlines and gzips; first load and 304 reload measured on the bench against the baseline
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-08: covered by PR #698 (André Brait), landed as 14d304af9 in 2.0.0-beta.111 (TASK-1640/TASK-1674), and ADR-215 (Accepted 2026-11-06, partly supersedes ADR-139, ADR-190 and ADR-192). / is served as one v2.html.gz with CSS, JS and fonts inlined, Content-Encoding gzip and ETag "<githash>-/v2.html.gz" (seen on .63 beta.130 OTA: 170631 bytes, ETag af8ebb5-/v2.html.gz). AC1/AC3 measurements are André's from PR #697 on one ESP32-S3 Combo unit (ADR-215 table): beta.94 loader cold load 2.9-4.2 s vs one gzipped file 1.1-1.4 s, 304 reload about 0.25-0.35 s; our bench added the two-tab test (beta.118 notes: median tab load 1.4-1.5 s with the send cap). Not re-measured by us as a single-tab before/after on our own bench: the multi-file baseline build no longer exists on dev.
<!-- SECTION:NOTES:END -->
