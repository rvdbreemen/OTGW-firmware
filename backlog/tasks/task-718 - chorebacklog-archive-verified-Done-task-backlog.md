---
id: TASK-718
title: 'chore(backlog): archive verified Done task backlog'
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-26 15:52'
updated_date: '2026-05-26 15:58'
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
- [ ] #1 Preserve or restore backlog auto_commit configuration after batching archive operations into a coherent commit.
- [ ] #2 Final notes record the number archived, remaining Done holdback, branch, coding agent, and verification state.
- [ ] #3 Archive only current Done tasks that were already Done in the published stable v1.5.0-fix2 snapshot or earlier; leave beta-cycle and later Done tasks visible.
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
<!-- SECTION:NOTES:END -->
