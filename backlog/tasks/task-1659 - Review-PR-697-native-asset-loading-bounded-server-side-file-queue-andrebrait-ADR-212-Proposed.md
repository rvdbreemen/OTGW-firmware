---
id: TASK-1659
title: >-
  Review PR #697: native asset loading + bounded server-side file queue
  (andrebrait, ADR-212 Proposed)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-05T14:37:00Z'
updated_date: '2026-11-05T16:33:00Z'
labels:
  - github
  - pr
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 783000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
https://github.com/rvdbreemen/OTGW-firmware/pull/697 by andrebrait, base dev, +1658/-476, 72 files. Replaces the v2.html script loader with native asset loading plus a bounded server-side file queue (ADR-212 Proposed). His follow-up (comment 6660042343): queue alone partly fixes; queue + JS retry fine up to ~2 tabs but max free block 7668 B with two tabs (below the 8192 guard); a single file (CSS/JS/fonts inlined) has no 503s and the most free heap; gzipped ~1.1-1.4 s vs ~2.9-4.0 s. Architecture choice is Robert's (queue/ADR-212 vs single gzipped file superseding ADR-192; TASK-1640 gzip was parked). Note: the PR reuses TASK-1657, which on dev is the beta.95 release task (number collision).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert's architecture decision recorded (queue/ADR-212, single gzipped file, or neither)
- [ ] #2 Reviewer team on the PR head (correctness, project rules/ADRs incl. ADR-139/147/165/188/192, tests); findings posted as a PR review
- [ ] #3 Tests + evaluate + combo build green on the PR head in its own worktree; bench page-load under 2 tabs with heap numbers
- [ ] #4 Merge only on Robert's go
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05 Robert (session): wait for andrebrait's gzip+ETag measurement, then choose the direction. PR comment posted (thanks, the ADR-192/139 implications, the two-tab 7668 B finding, the TASK-1657 number collision -> use TASK-1659, merge conflict with dev).

2026-11-05 15:52: PR rebased onto dev, single commit 491c20b98 now uses TASK-1659, GitHub reports MERGEABLE. Direction still waits for the gzip+ETag measurement (Robert's decision).
<!-- SECTION:NOTES:END -->
