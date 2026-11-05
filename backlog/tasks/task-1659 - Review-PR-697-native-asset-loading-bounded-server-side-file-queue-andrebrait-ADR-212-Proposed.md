---
id: TASK-1659
title: >-
  Review PR #697: native asset loading + bounded server-side file queue
  (andrebrait, ADR-212 Proposed)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-05T14:37:00Z'
updated_date: '2026-11-05T23:31:00Z'
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

Robert 2026-11-06 ~00:00 (AskUserQuestion, lead session), after Andre's gzip+ETag measurement (PR #697 comment 6670567252: one /v2.html.gz 170,558 B, first load 200+ETag 1.17 s, reload 304 0.13 s, stale ETag 200 full): 'Beide: eerst queue, gzip later'. The request queue of PR #697 (ADR-212) goes first; the single gzipped page is a follow-up with its own ADR (supersedes ADR-192), after measuring today's multi-file load time. Next for 1659: review PR #697 head in its own worktree (tests, evaluate, esp32-combo build, bench), post the review, then ask Robert for the merge itself.
<!-- SECTION:NOTES:END -->
