---
id: TASK-718
title: 'chore(backlog): archive verified Done task backlog'
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-26 15:52'
updated_date: '2026-05-26 15:56'
labels:
  - backlog
  - maintenance
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Archive completed tasks after their delivered change has been included in a published release. This keeps Done as the completed-but-not-yet-released queue and Archived as released historical record. Apply the rule to the current Done column: archive eligible released tasks through the Backlog CLI, retaining TASK-709.1 and any other Done work not yet demonstrably in a published release. TASK-709 itself may be archived because its original fixed-IP feature shipped in v1.6.0-beta.23; the responsive correction is tracked separately by TASK-709.1.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Preserve or restore backlog auto_commit configuration after batching archive operations into a coherent commit.
- [ ] #2 Final notes record the number archived, remaining Done holdback, branch, coding agent, and verification state.
- [ ] #3 Archive Done tasks whose delivered change is demonstrably included in a published release, leaving unreleased Done tasks visible.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Apply an archive criterion: tasks may leave Done once completion is verified and their implementation is committed/released or intentionally obsolete; retain tasks tied to current uncommitted follow-up work or unresolved proof.
2. Archive the completed historical Done set through the Backlog CLI, retaining TASK-709 and TASK-709.1 while the fixed-IP correction remains in the working tree.
3. Verify Done/archive counts, restore any CLI configuration changed for batching, record the result, and close/archive this maintenance task.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Archive policy applied on branch dev by Codex (GPT-5): archive verified completed work once landed/released or intentionally obsolete; retain tasks linked to current uncommitted changes or unresolved required evidence. TASK-709 and TASK-709.1 are held because the fixed-IP follow-up UI and AGENTS.md changes remain in the working tree.

Policy refinement from user discussion: use release publication as the ordinary archive threshold. Rationale: Done remains visible until delivery is public; archive clears delivered history without losing task auditability. A regression follow-up does not block archiving the shipped parent when the corrective work is separately tracked.
<!-- SECTION:NOTES:END -->
