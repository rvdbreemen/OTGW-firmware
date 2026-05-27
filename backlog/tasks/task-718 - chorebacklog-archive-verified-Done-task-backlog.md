---
id: TASK-718
title: 'chore(backlog): archive verified Done task backlog'
status: Done
assignee:
  - '@codex'
created_date: '2026-05-26 15:52'
updated_date: '2026-05-26 16:21'
labels:
  - backlog
  - maintenance
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Archive completed tasks only after their delivered change has been included in a published stable release; public beta prereleases do not count as shipped for this purpose. This keeps Done as completed-but-not-stable-released work and Archived as stable-release history. The current cutoff is GitHub release v1.5.0-fix2, published 2026-05-08. Archive only tasks that were already Done in that stable release snapshot and remain Done now; keep all v1.6.0 beta-cycle and later work visible.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Preserve or restore backlog auto_commit configuration after batching archive operations into a coherent commit.
- [x] #2 Final notes record the number archived, remaining Done holdback, branch, coding agent, and verification state.
- [x] #3 Archive only current Done tasks that were already Done in the published stable v1.5.0-fix2 snapshot or earlier; leave beta-cycle and later Done tasks visible.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Apply the archive threshold confirmed by the user: a public stable GitHub release, excluding beta prereleases; current cutoff is v1.5.0-fix2 (2026-05-08).
2. Derive eligible tasks by intersecting the current Done column with tasks already Done in the v1.5.0-fix2 repository snapshot, then archive only that set through the Backlog CLI.
3. Verify the remaining Done board, record counts/holdbacks and branch/agent evidence, and close this maintenance task without archiving it until a later stable release includes it.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Archive policy applied on branch dev by Codex (GPT-5): archive verified completed work once landed/released or intentionally obsolete; retain tasks linked to current uncommitted changes or unresolved required evidence. TASK-709 and TASK-709.1 are held because the fixed-IP follow-up UI and AGENTS.md changes remain in the working tree.

Policy refinement from user discussion: use release publication as the ordinary archive threshold. Rationale: Done remains visible until delivery is public; archive clears delivered history without losing task auditability. A regression follow-up does not block archiving the shipped parent when the corrective work is separately tracked.

Policy correction confirmed by user: public beta prereleases are test publications, not shipped releases for backlog archive purposes. Stable archive cutoff is v1.5.0-fix2, confirmed through `gh release list` as a non-prerelease GitHub release published on 2026-05-08; v1.6.0-beta.23 remains a pre-release and does not qualify tasks for archive.

Stable-release archive audit: v1.5.0-fix2 contains 50 tasks already marked Done; 48 remain in the current Done column. Excluded as feature-line-only rather than shipped in this stable dev release: TASK-527, TASK-537, TASK-539, TASK-541, TASK-542, TASK-543, TASK-546. TASK-545 is no longer in current Done. Eligible current stable-release archive set (41): TASK-86, TASK-352, TASK-353, TASK-354, TASK-382, TASK-383, TASK-388, TASK-389, TASK-390, TASK-391, TASK-392, TASK-395, TASK-396, TASK-398, TASK-399, TASK-400, TASK-401, TASK-402, TASK-403, TASK-478, TASK-483, TASK-484, TASK-486, TASK-522, TASK-526, TASK-531, TASK-534, TASK-535, TASK-536, TASK-538, TASK-540, TASK-547, TASK-551, TASK-552, TASK-558, TASK-559, TASK-560, TASK-561, TASK-572, TASK-573, TASK-575.

Archive execution result: archived 43 task records representing 42 distinct task IDs delivered in stable release v1.5.0-fix2 or earlier. The record count exceeds the ID count because two distinct dev-line records share ID TASK-538 and both qualified. The first pass moved 41 records; a direct stable-snapshot audit then found the second TASK-538 record and TASK-545, which were also archived. Verification now reports zero eligible stable-release Done records remaining unarchived.

The Backlog CLI automatically updated retained feature-line records TASK-527, TASK-539, and TASK-541 while archiving their dev dependencies/related tasks; those relationship metadata edits are retained as part of the archive operation. Excluded feature-line Done tasks remain visible because v1.5.0-fix2 did not ship their 2.0.0/ESP32/SAT implementation: TASK-527, TASK-537, TASK-539, TASK-541, TASK-542, TASK-543, TASK-546. Current beta-cycle/post-stable work also remains visible.

Packaging and closure verification: archive moves and CLI relationship updates were committed as 74ee1f77 (`chore(backlog): archive stable release tasks (TASK-718)`). `backlog config get autoCommit` returns `true` after batching. A fresh stable-snapshot comparison against v1.5.0-fix2 reports zero qualifying Done records still unarchived. The Done board retains 96 items for beta-cycle/post-stable work and feature-2.0.0-only work, including TASK-709/TASK-709.1 and excluded TASK-527/TASK-537/TASK-539/TASK-541/TASK-542/TASK-543/TASK-546.

Count correction: the archive verification measured 96 remaining Done items before TASK-718 was closed. Closing this maintenance task correctly places it in Done until a future stable release, so the post-closure board count is 97.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Applied the backlog archive policy agreed with the user: archive tasks after they ship in a public stable release, not merely after a beta pre-release. Confirmed through GitHub releases that the stable cutoff is v1.5.0-fix2 (published 2026-05-08) and that v1.6.0-beta.23 is a pre-release only. Archived 43 task records representing 42 distinct task IDs that were Done in the stable release history; TASK-538 had two separate eligible records. The Backlog CLI also updated retained feature-task relationship metadata for TASK-527, TASK-539, and TASK-541. Batched moves were committed in 74ee1f77. Restored `autoCommit=true` and verified that no qualifying stable-release Done record remains unarchived. After closing this maintenance task, 97 Done items remain visible because they are beta-cycle/post-stable, feature-2.0.0-only work, or this unreleased administration task itself. This is backlog administration only; no firmware validation build was required.
<!-- SECTION:FINAL_SUMMARY:END -->
