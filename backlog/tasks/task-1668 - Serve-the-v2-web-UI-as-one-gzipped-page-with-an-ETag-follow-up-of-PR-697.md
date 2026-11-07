---
id: TASK-1668
title: 'Serve the v2 web UI as one gzipped page with an ETag (follow-up of PR #697)'
status: To Do
assignee: []
created_date: '2026-11-05T23:31:00Z'
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
- [ ] #1 Baseline: today's multi-file first-load and reload time measured on the bench
- [ ] #2 ADR (supersedes ADR-192) Accepted by Robert
- [ ] #3 Build step inlines and gzips; first load and 304 reload measured on the bench against the baseline
<!-- AC:END -->
