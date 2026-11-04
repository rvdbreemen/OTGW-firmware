---
id: TASK-1636
title: Decide on scaling the build, bench and Discord host
status: To Do
assignee: []
created_date: '2026-11-04T22:46:00Z'
labels:
  - process
  - agentic
dependencies:
  - TASK-1633
priority: low
ordinal: 763000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Scenario C of docs/process/agentic-setup.md. Options: a second bench unit, a self-hosted build runner so the build lock does not hang on one workstation, and an always-on small host for discord-mcp and the webhook. Only worth it when the TASK-1633 figures show the bench or the machine is the real bottleneck.
Decision task: present the options with costs and the measured figures to Robert (AskUserQuestion); create implementation tasks for what he picks.
Context: docs/process/agentic-setup.md (finding numbers F1-F7) and docs/process/agentic-improvements-handoff-2026-11-04.md. Parent: TASK-1624.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Follow-up tasks exist for each chosen option, or the task is closed as 'not now' with the reason
- [ ] #2 Robert decided, with the TASK-1633 figures in front of him; decision recorded in the notes
<!-- AC:END -->
