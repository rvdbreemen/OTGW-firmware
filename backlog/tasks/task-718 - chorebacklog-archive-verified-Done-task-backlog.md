---
id: TASK-718
title: 'chore(backlog): archive verified Done task backlog'
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-26 15:52'
updated_date: '2026-05-26 15:54'
labels:
  - backlog
  - maintenance
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The Done column contains a large accumulated set of completed tasks. Archive completed historical tasks through the Backlog CLI using this criterion: completion is verified and the implementation is already committed/released or intentionally obsolete. Retain TASK-709 and TASK-709.1 until the current fixed-IP correction and AGENTS.md edits are packaged, because the child correction is still uncommitted and the parent remains useful board context. Batch archive changes coherently rather than producing one archive commit per task.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Preserve or restore backlog auto_commit configuration after batching archive operations into a coherent commit.
- [ ] #2 Final notes record the number archived, remaining Done holdback, branch, coding agent, and verification state.
- [ ] #3 Archive completed historical Done tasks through the backlog CLI while leaving TASK-709 and TASK-709.1 visible until their current working-tree follow-up is packaged.
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
<!-- SECTION:NOTES:END -->
