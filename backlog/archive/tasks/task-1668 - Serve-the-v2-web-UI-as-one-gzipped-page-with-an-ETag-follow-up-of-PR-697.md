---
id: TASK-1668
title: 'Serve the v2 web UI as one gzipped page with an ETag (follow-up of PR #697)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-05T23:31:00Z'
updated_date: '2026-11-07T09:33:00Z'
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
2026-11-07: duplicate of TASK-1640 (PR #698 by André Brait, landed as 14d304af9 in beta.111 with the TASK-1674 review fixes). Every AC here is the scope of TASK-1640/TASK-1674: ADR-215 (Accepted 2026-11-06 by Robert, partly supersedes ADR-139, ADR-190 and ADR-192); its measurements are first load 1.1-1.4 s against 2.9-4.2 s for the separate assets, and a 304 reload in about 0.25-0.35 s. Today's beta.118 OTA on .63 served / as v2.html.gz, 170522 bytes, 466751 unpacked. ACs left unticked here because the evidence belongs to TASK-1640/TASK-1674, not to this task.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed as a duplicate of TASK-1640 (Done, PR #698, ADR-215 Accepted). No work in this task.
<!-- SECTION:FINAL_SUMMARY:END -->
