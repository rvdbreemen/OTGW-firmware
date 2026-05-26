---
id: TASK-718
title: 'chore(backlog): archive verified Done task backlog'
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-26 15:52'
updated_date: '2026-05-26 15:53'
labels:
  - backlog
  - maintenance
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The Done column contains a large accumulated set of completed tasks. Archive the verified historical Done tasks through the Backlog CLI while retaining TASK-709.1 until its associated working-tree implementation and AGENTS.md edits are packaged. Batch the archive changes coherently rather than producing one archive commit per task.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Archive the previously reviewed eligible Done tasks through the backlog CLI while leaving TASK-709.1 unarchived.
- [ ] #2 Preserve or restore backlog auto_commit configuration after batching archive operations into a coherent commit.
- [ ] #3 Final notes record the number archived, remaining Done holdback, branch, coding agent, and verification state.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Apply an archive criterion: tasks may leave Done once completion is verified and their implementation is committed/released or intentionally obsolete; retain tasks tied to current uncommitted follow-up work or unresolved proof.
2. Archive the completed historical Done set through the Backlog CLI, retaining TASK-709 and TASK-709.1 while the fixed-IP correction remains in the working tree.
3. Verify Done/archive counts, restore any CLI configuration changed for batching, record the result, and close/archive this maintenance task.
<!-- SECTION:PLAN:END -->
